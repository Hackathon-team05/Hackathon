void boot_setup(){
    Serial.begin(BAUD);
    i2c_setup();
    default_bpm();
};

bool wait_ack(int dev){
    int retry_count=0;
    while(retry_count<max_try){
        digitalWrite(dev,LOW);
        SPI.transfer(CMD_CONNECT);
        delayMicroseconds(1000);
        uint8_t response=SPI.transfer(DUMMY);
        Serial.println(response);
        if(response==ACK_OK){
            digitalWrite(dev,HIGH);
            return true;
        }
        digitalWrite(dev,HIGH);
        retry_count++;
        delay(100);//チェック失敗時に待ち時間
    }
    return false;
}
