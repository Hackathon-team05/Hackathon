void boot_setup(){
    spi_setup();
    default_bpm();
};

bool wait_ack(int DEV){
    int retry_count=0;
    const int max_try=3;
    while(retry_count<max_try){
        digitalWrite(DEV,LOW);

    }
}