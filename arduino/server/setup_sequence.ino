const int CMD_CONNECT=100;
const int DUMMY=0x00;
const int ACK_OK=200;
const int max_try=3;


void boot_setup(){
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