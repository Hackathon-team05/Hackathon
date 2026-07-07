// サーバーArduino
// 役割: BPM管理・楽曲データ管理（PROGMEM）・I2C Master・マイク入力・シリアル送信
//
// 【通信方式】サーバ⇔楽器間はI2C（SPIから移行済み）。楽器の識別はI2Cアドレス
//   (dev_ctl[].i2c_address = 0x10+instrument_id) で行い、tick pulseはI2Cとは別の
//   専用GPIO(CS_SYNC)で出力する。SDA/SCLには4.7kΩプルアップを付与すること。

#include<Wire.h>
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

uint8_t frame[8][12] = {};

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
const int CMD_CONNECT=100;//I2Cハンドシェイクで楽器へ送る接続確認コマンド
const int ACK_OK=200;//楽器がハンドシェイクに返す正常応答
const int CMD_RESET=0x05;//【今回追加】起動時にlocal_tick等を初期化させるコマンド(i2c_slave.inoのCMD_RESETと値を一致させる)
const int max_try=3;
//ピン設定
const int CS_DEV1=D4;
const int CS_DEV2=D5;
const int CS_DEV3=D6;
const int CS_DEV4=D7;
const int CS_SYNC=D9;
const int MIC_PIN=A1;
//I2C通信の速度は下部の I2C_CONFIG_HZ で定義（i2c_master.ino の Wire.setClock で使用）
const unsigned long STATUS_POLL_INTERVAL_MS=50;
unsigned long last_status_poll_ms=0;
//コマンド生成
uint8_t command_sequence = 0;
//I2C通信
const uint32_t I2C_CONFIG_HZ = 100000;
//サーバー主導の自動輪唱スケジュール（index=instrument_id）
// id0=楽器1, id1=楽器2, id2=楽器3（いずれも同一旋律のメロディ）, id3=ドラム
// 4分音符=16tick なので 8拍=128tick。8拍ごとに1声ずつ入りを増やす。
// ドラムは開始と同時に PLAY で常時再生、メロディは ENTRY_CUE で先頭から入る。
// ※入りタイミングを変えたい場合はこの2配列だけを編集すればよい。
const uint32_t ENTRY_TICK[4] = {0, 128, 256, 0};        //各idが入る global_tick（拍×16）
const uint8_t  ENTRY_CMD[4]  = {0x03, 0x03, 0x03, 0x01};//0x03=ENTRY_CUE(メロディ) / 0x01=PLAY(ドラム)
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
const int BPM_MAX=120;
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
float estimated_interval=60000/DEF_BPM;
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

// i2c_address は楽器側 i2c_slave.ino の I2C_BASE_ADDRESS(0x10) + instrument_id と
// 一致させること。dev index i ↔ instrument_id i ↔ アドレス 0x10+i の対応を保つ。
DeviceStatus dev_ctl[4] = {
    {CS_DEV1, 0x10, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV2, 0x11, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV3, 0x12, 0, false, 0x00, 0x00, false, false, false},
    {CS_DEV4, 0x13, 0, false, 0x00, 0x00, false, false, false}
};

void setup(){
    boot_setup();
    delay(1000);//楽器の起動待ち
    LED_setup();//LEDマトリックスの初期化
    for(int i=0;i<4;i++){
        if(i2c_wait_ack(i)==false){
            dev_ctl[i].failsafe=true;
            failsafe_light(i);
        }else{
            light_up(i);
        }
    }

    //【今回追加】tick初期化(CMD_RESET)の送信。
    //サーバーだけがリセットされた場合、楽器側は電源が入ったままなので
    //前回演奏していたis_playing=true/local_tickが進行した状態を保持している。
    //これを放置すると、サーバー再起動後(global_tick=0から再開)の新しい輪唱に対し
    //楽器側だけ以前のtick位置から再開してしまい「途中参戦」になってしまう。
    //そこでハンドシェイクに成功した楽器全てへCMD_RESETを送り、
    //local_tick=0 / is_playing=false / 鳴っている音の停止 を明示的に行わせてから
    //通常のENTRY_TICKスケジュール(loop側)を開始する。
    {
        ControlCommand cmd;
        InstrumentStatus status;
        for(int i=0;i<4;i++){
            if(dev_ctl[i].failsafe){
                continue;//ハンドシェイク自体に失敗した楽器には送らない
            }
            generate_cmd(CMD_RESET,cmd);
            i2c_send(i,cmd);
            i2c_receive_with_sequence_check(i,cmd,status);
            //ここでの検証失敗はフェイルセーフにしない。
            //通信自体は確立しているため、以後のSTATUS_POLLでエラーが続けば
            //既存のhandle_device_command()が通常通りフェイルセーフを発動する。
        }
    }

    mic_setup();
    tick_setup();
    last_status_poll_ms=millis();
}

void loop(){
    ControlCommand cmd;
    InstrumentStatus status;

    uint32_t tick_count=tick_generate();
    for(uint32_t i=0;i<tick_count;i++){//tick周期ごとにSYNC送信（全楽器へ同一パルスをブロードキャスト）
        global_tick++;
        digitalWrite(CS_SYNC,HIGH);
        delayMicroseconds(10);
        digitalWrite(CS_SYNC,LOW);
    }

    //【サーバー主導の自動輪唱】各楽器が自分のスケジュール拍(ENTRY_TICK)に達したら
    //開始コマンドを送る。人形設置ではなく global_tick で入りを決める。
    for(int i=0;i<4;i++){
        if(dev_ctl[i].failsafe || dev_ctl[i].is_playing){
            continue;//フェイルセーフ済み or 既に開始済みは対象外
        }
        if(global_tick>=ENTRY_TICK[i]){
            generate_cmd(ENTRY_CMD[i],cmd);//メロディ=ENTRY_CUE / ドラム=PLAY
            i2c_send(i,cmd);
            i2c_receive_with_sequence_check(i,cmd,status);
            //応答検証がOK(error_count==0)のときだけ「開始済み」にする。
            //送信/検証に失敗したら is_playing を立てず、次ループで自動的に再送する。
            if(dev_ctl[i].error_count==0){
                dev_ctl[i].is_playing=true;
            }
        }
    }

    //定期的な状態監視とフェイルセーフ（frog_stateは監視のみ、入り判定には使わない）
    unsigned long server_now_ms=millis();
    if(server_now_ms-last_status_poll_ms>=STATUS_POLL_INTERVAL_MS){
        last_status_poll_ms+=STATUS_POLL_INTERVAL_MS;
        for(int i = 0; i < 4; i++){
            if(dev_ctl[i].failsafe){
                continue;
            }
            i2c_receive_without_sequence_check(i,status);//通信監視
            handle_device_command(i, cmd);//error_count>=3ならフェイルセーフ発動
        }
    }

    //拍手によるBPM変更（テンポは従来通り観客がマイクで操作可能）
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
