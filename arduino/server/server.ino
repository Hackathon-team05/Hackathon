// サーバーArduino
// 役割: BPM管理・楽曲データ管理（PROGMEM）・SPI Master・マイク入力・シリアル送信

struct ControlCommand{
    uint8_t command_type;
    uint8_t payload;
    uint8_t sequence;
    uint8_t checksum;
};

struct InstrumentStatus{
    uint8_t instrument_id;
    uint8_t frog_state;
    uint8_t sequence_ack;
    uint8_t ack_ok;
    uint8_t checksum;
};

void setup(){
    boot_setup();
}

void loop(){

}