void generate_cmd(int command,ControlCommand& cmd){
    // 【今回追加】0x05=CMD_RESET(tick初期化コマンド)もpayload無しの単純コマンドとして扱う。
    if(command==0x00||command==0x01||command==0x02||command==0x03||command==0x05){
        cmd.command_type=command;
        cmd.payload=0x00;
        cmd.sequence= command_sequence++;
    }else{
        cmd.command_type=0x04;
        cmd.payload=(uint16_t)get_bpm();
        cmd.sequence= command_sequence++;
    }
    generate_checksum(cmd);
}

void generate_checksum(ControlCommand& cmd){
    uint8_t* cmd_box=(uint8_t*)&cmd;
    uint8_t sum=0;
    for(int i=0;i<sizeof(ControlCommand)-sizeof(cmd.checksum);i++){
        sum+=cmd_box[i];
    }
    cmd.checksum=(uint8_t)(0-sum);
}

void verification_status(int dev, ControlCommand& cmd, InstrumentStatus& status){
    // フェイルセーフ済みデバイスは無視
    if(dev_ctl[dev].failsafe){ 
        return;
    }
    uint8_t* status_box=(uint8_t*)&status;
    uint8_t id=status.instrument_id;
    // 範囲外 or 通信相手と自己申告IDが不一致の場合はエラー
    uint8_t sum=0;
    for(int i=0;i<sizeof(InstrumentStatus);i++){
        sum+=status_box[i];
    }
    if(sum==0){
        if(id>=4||id!=(uint8_t)dev){
            dev_ctl[dev].error_count++;
            return;
        }
        // sequence_ackが送信したsequenceと一致するか確認
        if(status.sequence_ack!=cmd.sequence){
            dev_ctl[dev].error_count++;
            return;
        }
    // スレーブがコマンドを正常処理できたか確認
        if(status.ack_ok!=0x01){
            dev_ctl[dev].error_count++;
            return;
        }
        dev_ctl[dev].error_count=0;
        dev_ctl[dev].prev_frog_state=dev_ctl[dev].frog_state;
        dev_ctl[dev].frog_state=status.frog_state;
        //サーバー主導方式では入り/停止を server.ino の ENTRY_TICK スケジュールで決めるため、
        //frog_state は通信監視用に更新するのみで、pending_entry/pending_stop は設定しない。
    }else{
        dev_ctl[dev].error_count++;
    }
}

// サーバー主導方式では入り(ENTRY_CUE/PLAY)は loop() の ENTRY_TICK スケジュールで送るため、
// この関数は「通信エラー3回連続時のフェイルセーフ」のみを担当する。
void handle_device_command(int dev, ControlCommand& cmd){
    if(dev<0||dev>=4||dev_ctl[dev].failsafe){
        return;
    }
    InstrumentStatus status;
    if(dev_ctl[dev].error_count>=3){
        // 通信エラーが続いた楽器へは最終STOPを送り、以降の新規送信を停止する。
        i2c_failsafe(dev,cmd,status);
    }
}
