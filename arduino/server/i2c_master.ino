void i2c_setup(){
    Wire.begin();
    Wire.setClock(I2C_CONFIG_HZ);
    pinMode(CS_SYNC,OUTPUT);
    digitalWrite(CS_SYNC,LOW);
}

bool i2c_wait_ack(int dev){
    if(dev < 0 || dev >= 4){
        return false;
    }

    const uint8_t address = dev_ctl[dev].i2c_address;
    int retry_count = 0;
    while(retry_count < max_try){
        Wire.beginTransmission(address);//addressに送信準備
        Wire.write((uint8_t)CMD_CONNECT);//addressにCMD_CONNECT送信
        uint8_t result = Wire.endTransmission();//addressに送信終了.通信成功時に0を返す．
        Serial.print(result);

        if(result == 0){
            delay(1);
            Wire.requestFrom(address, (uint8_t)1);//addressに1バイトのデータ要求と受信
            if(Wire.available() > 0){//受信データのバイト数チェック
                uint8_t response = Wire.read();//受信データの読み取り
                Serial.println(response);
                if(response == ACK_OK){
                    return true;
                }
            }
        }

        while(Wire.available() > 0){//ゴミデータの破棄
            Wire.read();
        }
        retry_count++;
        delay(100);
    }
    return false;
}

void i2c_send(int dev, ControlCommand& cmd){
    if(dev < 0 || dev >= 4){
        return;
    }

    const uint8_t address = dev_ctl[dev].i2c_address;
    uint8_t* cmd_box = (uint8_t*)&cmd;
    const int cmd_len = sizeof(ControlCommand);

    Wire.beginTransmission(address);
    Wire.write(cmd_box, cmd_len);//ControlCommandをすべて送信
    uint8_t result = Wire.endTransmission();
    if(result != 0){//通信成功のチェック
        dev_ctl[dev].error_count++;
        return;
    }
}

bool i2c_read_status_packet(int dev, InstrumentStatus& status){
    // I2CからInstrumentStatusを1パケット読み取る共通処理。
    // コマンド応答の読み取りと、圧力センサ監視の読み取りで共用する。
    // ここでは「バイト数が揃っているか」だけを確認し、
    // checksum、instrument_id、sequence_ack、ack_okの意味検証は呼び出し側で行う。
    if(dev < 0 || dev >= 4){
        return false;
    }

    const uint8_t address = dev_ctl[dev].i2c_address;
    uint8_t* status_box = (uint8_t*)&status;
    const int status_len = sizeof(InstrumentStatus);

    for(int i = 0; i < status_len; i++){
        status_box[i] = 0xFF;//仮データで埋める
    }

    uint8_t received = Wire.requestFrom(address, (uint8_t)status_len);//InstrumentStatusを要求
    int i = 0;
    while(Wire.available() > 0 && i < status_len){
        status_box[i] = Wire.read();//１バイトずつ読み取り
        i++;
    }

    while(Wire.available() > 0){//ゴミデータの破棄
        Wire.read();
    }

    if(received != status_len || i != status_len){//受信バイト数と読み取りバイト数のチェック
        dev_ctl[dev].error_count++;
        return false;
    }

    return true;
}

void i2c_receive_with_sequence_check(int dev, ControlCommand& cmd, InstrumentStatus& status) {
    // i2c_send()直後の応答読み取り用。
    // スレーブ側がack_okやchecksumを更新する時間を少し待ってから読む。
    delay(1);

    if(!i2c_read_status_packet(dev,status)){
        return;
    }

    // コマンド応答なので、既存の検証関数で
    // checksum、instrument_id、sequence_ack、ack_okを確認する。
    verification_status(dev, cmd, status);
}

void i2c_receive_without_sequence_check(int dev, InstrumentStatus& status) {
    // 圧力センサ監視用の読み取り。
    // 送信コマンドがないため、cmd.sequenceに対応するsequence_ackは検証しない。
    if(!i2c_read_status_packet(dev,status)){
        return;
    }

    uint8_t* status_box=(uint8_t*)&status;
    uint8_t sum=0;

    // InstrumentStatus全体のchecksumを確認する。
    for(int i=0;i<sizeof(InstrumentStatus);i++){
        sum+=status_box[i];
    }

    // 読み取り専用監視では、パケット整合性と楽器IDだけを見る。
    if(sum!=0 || status.instrument_id>=4 || status.instrument_id!=(uint8_t)dev){
        dev_ctl[dev].error_count++;
        return;
    }

    // ここから下は既存のverification_status()内のfrog_state更新処理を流用したもの。
    // 0 -> 1 は置かれた、1 -> 0 は外された状態として扱う。
    dev_ctl[dev].error_count=0;
    dev_ctl[dev].prev_frog_state=dev_ctl[dev].frog_state;
    dev_ctl[dev].frog_state=status.frog_state;
    if(dev_ctl[dev].prev_frog_state==0x00 && dev_ctl[dev].frog_state==0x01){
        dev_ctl[dev].pending_entry=true;
        dev_ctl[dev].pending_stop=false;
    }else if(dev_ctl[dev].prev_frog_state==0x01 && dev_ctl[dev].frog_state==0x00){
        dev_ctl[dev].pending_entry=false;
        dev_ctl[dev].pending_stop=true;
        dev_ctl[dev].is_playing=false;
    }
}

void i2c_failsafe(int dev,ControlCommand& cmd,InstrumentStatus& status){
    if(dev<0||dev>=4){
        return;
    }
    dev_ctl[dev].failsafe=true;
    generate_cmd(0x02,cmd);
    i2c_send(dev,cmd);
    i2c_receive_with_sequence_check(dev,cmd,status);
    dev_ctl[dev].error_count=0;
}
