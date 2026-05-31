void generate_cmd(int command,ControlCommand& cmd){
    if(command==0x00){
        cmd.command_type=0x00;
        cmd.payload=0x00;
        cmd.sequence=tick_generate();
    }else if(command==0x01){
        cmd.command_type=0x01;
        cmd.payload=0x00;
        cmd.sequence=tick_generate();
    }else if(command==0x02){
        cmd.command_type=0x02;
        cmd.payload=0x00;
        cmd.sequence=tick_generate();
    }else if(command==0x03){
        cmd.command_type=0x03;
        cmd.payload=0x00;
        cmd.sequence=tick_generate();
    }else{
        cmd.command_type=0x04;
        cmd.payload=get_bpm();
        cmd.sequence=tick_generate();
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
    uint8_t sum=0;
    for(int i=0;i<sizeof(InstrumentStatus);i++){
        sum+=status_box[i];
    }
    if(sum==0){
        dev_ctl[dev].error_count=0;
        dev_ctl[dev].prev_frog_state=dev_ctl[dev].frog_state;
        dev_ctl[dev].frog_state=status.frog_state;
        if(dev_ctl[dev].prev_frog_state==0x00&&dev_ctl[dev].frog_state==0x01){
            dev_ctl[dev].pending_entry=true;
            dev_ctl[dev].pending_stop=false;//再生フラグ
        }else if(dev_ctl[dev].prev_frog_state==0x01&&dev_ctl[dev].frog_state==0x00){
            dev_ctl[dev].pending_entry=false;
            dev_ctl[dev].pending_stop=true;
            dev_ctl[dev].is_playing=false;//停止フラグ
        }
    }else{
        dev_ctl[dev].error_count++;
    }
}

void handle_device_command(int dev, ControlCommand& cmd){
    if(dev<0||dev>=4||dev_ctl[dev].failsafe){
        return;
    }
    InstrumentStatus status;
    if(dev_ctl[dev].error_count>=3){
        failsafe(dev,cmd,status);
        return;
    }
    if(dev_ctl[dev].pending_stop){
        generate_cmd(0x02,cmd);
        spi_send(dev,cmd,status);
        dev_ctl[dev].pending_stop=false;
        dev_ctl[dev].pending_entry=false;
        dev_ctl[dev].is_playing=false;
        return;
    }
    if(is_entry_boundary()&&dev_ctl[dev].pending_entry&&!dev_ctl[dev].is_playing){
        generate_cmd(0x03,cmd);
        spi_send(dev,cmd,status);
        dev_ctl[dev].pending_entry=false;
        dev_ctl[dev].is_playing=true;
    }
}
