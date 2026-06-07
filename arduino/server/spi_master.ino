void spi_setup(){
    SPI.begin();

    pinMode(CS_DEV1,OUTPUT);
    pinMode(CS_DEV2,OUTPUT);
    pinMode(CS_DEV3,OUTPUT);
    pinMode(CS_DEV4,OUTPUT);
    pinMode(CS_SYNC,OUTPUT);

    digitalWrite(CS_DEV1,HIGH);
    digitalWrite(CS_DEV2,HIGH);
    digitalWrite(CS_DEV3,HIGH);
    digitalWrite(CS_DEV4,HIGH);
    digitalWrite(CS_SYNC,LOW);
};

void spi_send(int dev,ControlCommand& cmd,InstrumentStatus& status){
    if(dev<0||dev>=4){
        return;
    }
    int dev_pin=dev_ctl[dev].cs_pin;
    digitalWrite(dev_pin,LOW);
    uint8_t* cmd_box=(uint8_t*)&cmd;
    uint8_t* status_box=(uint8_t*)&status;
    int cmd_len=sizeof(ControlCommand);
    int status_len=sizeof(InstrumentStatus);
    for(int i=0;i<cmd_len;i++){
        status_box[i]=SPI.transfer(cmd_box[i]);
    }
    digitalWrite(dev_pin,HIGH);
    verification_status(dev,cmd,status);
}

void failsafe(int dev,ControlCommand& cmd,InstrumentStatus& status){
    if(dev<0||dev>=4){
        return;
    }
    dev_ctl[dev].failsafe=true;
    generate_cmd(0x02,cmd);
    spi_send(dev,cmd,status);
    dev_ctl[dev].error_count=0;
}
