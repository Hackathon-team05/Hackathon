void i2c_setup(){
    Wire.begin();
    Wire.setClock(I2C_CONFIG_HZ);
}

bool i2c_wait_ack(int dev){
    if(dev < 0 || dev >= 4){
        return false;
    }

    const uint8_t address = I2C_DEVICE_ADDRESS[dev];
    int retry_count = 0;
    while(retry_count < max_try){
        Wire.beginTransmission(address);//addressに送信準備
        Wire.write((uint8_t)CMD_CONNECT);//addressにCMD_CONNECT送信
        uint8_t result = Wire.endTransmission();//addressに送信終了.通信成功時に0を返す．

        if(result == 0){
            delay(1);
            Wire.requestFrom(address, (uint8_t)1);//addressに1バイトのデータ要求と受信
            if(Wire.available() > 0){//受信データのバイト数チェック
                uint8_t response = Wire.read();//受信データの読み取り
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

    const uint8_t address = I2C_DEVICE_ADDRESS[dev];
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

void i2c_receved(int dev, ControlCommand& cmd, InstrumentStatus& status) {
    if(dev < 0 || dev >= 4){
        return;
    }

    const uint8_t address = I2C_DEVICE_ADDRESS[dev];
    uint8_t* cmd_box = (uint8_t*)&cmd;
    uint8_t* status_box = (uint8_t*)&status;
    const int cmd_len = sizeof(ControlCommand);
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

    if(received != status_len){//受信バイト数と読み取りバイト数のチェック
        dev_ctl[dev].error_count++;
        return;
    }
    
    verification_status(dev, cmd, status);
}
