void boot_setup(){
    Serial.begin(BAUD);
    i2c_setup();
    default_bpm();
};

// 旧SPI版の wait_ack() はI2C化により i2c_master.ino の i2c_wait_ack() へ置き換え済み
// （setup() は i2c_wait_ack(i) を呼ぶ）。旧SPIハンドシェイク関数は削除した。
