void boot_setup(){
    Serial.begin(BAUD);
    spi_setup();
    default_bpm();
};

bool wait_ack(int dev){
    int retry_count=0;
    while(retry_count<max_try){
        digitalWrite(dev,LOW);
        SPI.transfer(CMD_CONNECT);
        uint8_t response=SPI.transfer(DUMMY);
        if(response==ACK_OK){
            digitalWrite(dev,HIGH);
            return true;
        }
        digitalWrite(dev,HIGH);
        retry_count++;
    }
    return false;
}