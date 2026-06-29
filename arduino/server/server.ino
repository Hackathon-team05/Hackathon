// サーバーArduino
// 役割: BPM管理・楽曲データ管理（PROGMEM）・SPI Master・マイク入力・シリアル送信

#include<SPI.h>
#include<Wire.h>

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
//初期セットアップ
const int BAUD=115200;
const int CMD_CONNECT=100;
const int DUMMY=0x00;
const int ACK_OK=200;
const int max_try=3;
//ピン設定
const int CS_DEV1=D4;
const int CS_DEV2=D5;
const int CS_DEV3=D6;
const int CS_DEV4=D7;
const int CS_SYNC=D9;
const int MIC_PIN=A1;
//SPI通信
const SPISettings SPI_CONFIG(1000000,MSBFIRST,SPI_MODE0);//SPI通信の仕様を明示．1MHzで通信．
const unsigned long STATUS_POLL_INTERVAL_MS=50;
unsigned long last_status_poll_ms=0;
//コマンド生成
uint8_t command_sequence = 0;
//I2C通信
const uint32_t I2C_CONFIG_HZ = 100000;
//拍手検出
uint16_t value=0;
const int PEAK_SIGMA_FACTOR=5;
const uint8_t WINDOW_SIZE=50;
uint16_t mic_window[WINDOW_SIZE];
uint8_t win_id=0;
bool win_full=false;
//BPM管理
const int DEF_BPM=70;
const int BPM_MIN=60;
const int BPM_MAX=140;
uint16_t bpm=70;
const unsigned long MIC_SAMPLE_INTERVAL_MS=10;//マイクサンプリング周期
unsigned long last_mic_sample_ms=0;
unsigned long last_time=0;//前回の検出時間  
int clap_count=0;//ピーク検出回数
const int CLAP_MAX=5;//BPM計算に使う拍手の数
unsigned long clap_times[CLAP_MAX];//拍手の時間を保管
const float SMOOTH_FACTOR=0.3;//BPM変化の適用割合
bool peak_track=false;
uint16_t peak_max=0;
unsigned long peak_time=0;
uint8_t down_count=0;
const uint8_t PEAK_DOWN_COUNT=3;
const unsigned long INTERVAL_TIME=250;
const uint8_t CLAP_INTERVAL_COUNT = CLAP_MAX-1;
unsigned long clap_intervals[CLAP_MAX-1];
unsigned long clap_sorted_intervals[CLAP_MAX-1];
const float INTERVAL_OUTLIER_RATIO=0.3;
//tick管理
unsigned long global_tick=0;
unsigned long last_tick_us=0;
unsigned long tick_period_us=0;
bool entry_boundary_reached=false;

struct DeviceStatus{
    uint8_t cs_pin; //ピン番号
    uint8_t i2c_address;//I2Cアドレス
    uint8_t error_count;//エラー回数
    bool failsafe;//フェイルセーフ発動フラグ
    uint8_t frog_state;//受信時のカエル人形の状態
    uint8_t prev_frog_state;//前回受信時のカエル人形の状態
    bool pending_entry;//入り境界で再生するフラグ
    bool pending_stop;//停止コマンド送信待ちフラグ
    bool is_playing;//演奏状態のフラグ
};

DeviceStatus dev_ctl[4] = {
    {CS_DEV1, 0x0A, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV2, 0x0B, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV3, 0x0C, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV4, 0x0D, 0, false, 0x00, 0x00, false, false, false}
};

void setup(){
    boot_setup();
    delay(1000);//楽器の起動待ち
    for(int i=0;i<4;i++){
        if(i2c_wait_ack(i)==false){
            dev_ctl[i].failsafe=true;
        }
    }
    mic_setup();
    tick_setup();
    last_status_poll_ms=millis();
}

void loop(){
    ControlCommand cmd;
    InstrumentStatus status;

    entry_boundary_reached=false;

    uint32_t tick_count=tick_generate();
    for(uint32_t i=0;i<tick_count;i++){//tick周期ごとにSYNC送信
        global_tick++;
        digitalWrite(CS_SYNC,HIGH);
        delayMicroseconds(10);
        digitalWrite(CS_SYNC,LOW);
        entry_boundary_reached |= is_entry_boundary();
    }

    if(entry_boundary_reached){//50ms周期前に入り境界によるコマンド送信
        for(int i=0;i<4;i++){
            handle_device_command(i,cmd);
        }
    }
    unsigned long server_now_ms=millis();
    if(server_now_ms-last_status_poll_ms>=STATUS_POLL_INTERVAL_MS){
        last_status_poll_ms+=STATUS_POLL_INTERVAL_MS;
        for(int i = 0; i < 4; i++){
            if(dev_ctl[i].failsafe){
                continue;
            }
            // 圧力センサ監視は送信なしでInstrumentStatusだけを読む。
            // cmd.sequenceを使う通常応答検証ではなく、読み取り専用の関数を使う。
            i2c_receive_without_sequence_check(i,status);//圧力センサの監視
            handle_device_command(i, cmd);
        }
    }

    if(mic_read()){
        for(int i = 0; i < 4; i++){
            if(dev_ctl[i].failsafe){
                continue;
            }
            generate_cmd(0x04,cmd);
            i2c_send(i, cmd);//BPM送信
            i2c_receive_with_sequence_check(i,cmd,status);
        }
    }
}
