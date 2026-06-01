// サーバーArduino
// 役割: BPM管理・楽曲データ管理（PROGMEM）・SPI Master・マイク入力・シリアル送信

struct __attribute__((packed)) ControlCommand{
    uint8_t command_type;
    uint16_t payload;
    uint8_t sequence;
    uint8_t checksum;
};

struct __attribute__((packed)) InstrumentStatus{
    uint8_t instrument_id;
    uint8_t frog_state;
    uint8_t sequence_ack;
    uint8_t ack_ok;
    uint8_t checksum;
};

const int CS_DEV1=D4;
const int CS_DEV2=D5;
const int CS_DEV3=D6;
const int CS_DEV4=D7;
const int CS_SYNC=D9;

struct DeviceStatus{
    uint8_t cs_pin; //ピン番号
    uint8_t error_count;//エラー回数
    bool failsafe;//フェイルセーフ発動フラグ
    uint8_t frog_state;//受信時のカエル人形の状態
    uint8_t prev_frog_state;//前回受信時のカエル人形の状態
    bool pending_entry;//入り境界で再生するフラグ
    bool pending_stop;//停止コマンド送信待ちフラグ
    bool is_playing;//演奏状態のフラグ
};

DeviceStatus dev_ctl[4] = {
    {CS_DEV1, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV2, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV3, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV4, 0, false, 0x00, 0x00, false, false, false}
};

void setup(){
    boot_setup();
    for(int i=0;i<4;i++){
        if(wait_ack(dev_ctl[i].cs_pin)==false){
            dev_ctl[i].failsafe=true;
        }
    }
}

void loop(){
    ControlCommand cmd;
    InstrumentStatus status;

    for(int i = 0; i < 4; i++){
        if(dev_ctl[i].failsafe){
            continue;
        }
        generate_cmd(0x00,cmd);//定期的に圧力センサの監視
        spi_send(i, cmd, status);//定期的に圧力センサの監視
        handle_device_command(i, cmd);
    }
}