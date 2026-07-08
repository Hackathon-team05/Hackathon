void boot_setup(){
    Serial.begin(BAUD);
    i2c_setup();
    default_bpm();
};