//#include <Wire.h>  // I2C化に伴いSPI.hではなくWire.hを使用する(i2c_slave.ino側でinclude)

// main.ino
#include "score_data.h"
#include "test_config.h"   // TEST_BUTTON_MODE（テスト/本番の切替スイッチ。既定0=本番/I2C）

volatile bool is_playing = false;
volatile uint16_t local_tick = 0;
uint8_t frog_state = 0;

extern void init_serial_tx();
extern void instrument_id_init();
extern uint8_t get_instrument_id();
extern void score_init(uint8_t instrument_id);
extern void init_i2c_slave(); // 【変更】SPI(ソフトウェアスレーブ)→I2Cスレーブ化に伴い改名
extern void sync_init();
extern void pressure_init();
extern uint8_t pressure_read();
extern int8_t pressure_get_pitch_offset();  // 【追加】

extern void score_step(uint16_t local_tick, int8_t pitch_offset); //引数追加
extern void score_stop_all();
// 【追加】i2c_slave.ino のI2C受信割り込み(on_i2c_receive)が予約したコマンドを、
// 割り込みの外(loop側)で実際に実行するための関数。重い処理(score_init等)を
// 割り込みハンドラから追い出すための仕組み(instrument_spi_fix_notes.md 指摘4対応)。
extern void process_pending_command();
// 【追加】I2Cで受信したControlCommandの内容をSerialへ出力するための関数
// (i2c_slave.ino側で受信内容を記録し、ここでloop()コンテキストから出力する)。
extern void print_i2c_log();

// 【テストモード切替】TEST_BUTTON_MODE==1 のときは serial_tx.ino 側のボタンテスト
// setup()/loop() を使うため、本番(I2C)用の setup()/loop() を無効化する。
// （is_playing/local_tick/frog_state 等のグローバルと extern は他ファイルが参照するため
//   ガードの外に残す。無効化するのは setup()/loop() のみ。）
#if !TEST_BUTTON_MODE

void setup() {
    Serial.begin(115200);
    init_serial_tx();
    instrument_id_init();
    score_init(get_instrument_id());
    init_i2c_slave();
    sync_init();
    pressure_init();

    // 【追加】起動完了時に一度だけ表示する。担当パート(instrument_id)が
    // 正しく読み出せているかをここで確認できる。
    Serial.print(F("[BOOT] instrument ready. instrument_id="));
    Serial.println(get_instrument_id());
    Serial.println(F("[BOOT] waiting for server handshake..."));
}

void loop() {
    // 【追加】I2C受信割り込みが予約したコマンド(PLAY/STOP/ENTRY_CUE/BPM_UPDATE)を
    // ここで実処理する。score_init/score_stop_allなど時間のかかる処理は
    // 割り込みハンドラ内では行わず、必ずこのloop()側で実行する。
    process_pending_command();

    // 【追加】直前のI2C通信で受信したControlCommandの内容をSerialへ出力する。
    print_i2c_log();

    // 圧力センサからfrog_state（0 or 1）を更新
    frog_state = pressure_read();

    if (is_playing) {
        noInterrupts();
        uint16_t current_tick = local_tick;
        interrupts();

        // 【今回追加】自分のパートの終了判定。
        // local_tickはCMD_PLAY/CMD_ENTRY_CUE/CMD_RESET受信時に0へリセットされたあと、
        // on_sync_tick(sync_isr.ino)によってtickが来るたびに単純に加算され続ける値
        // (以前あったscore_loop_check()による毎ループ0への巻き戻しはもう行わない)。
        // そのため「1ループ分のtick数(LOOP_MAX_TICK) × 指定ループ回数」を
        // local_tickが超えたかどうかで「自分のパートを指定回数演奏し終わったか」を
        // 判定できる(A回ループさせたいなら LOOP_MAX_TICK×A == local_tick で分岐)。
        uint16_t stop_tick = (uint16_t)(LOOP_MAX_TICK * PART_LOOP_COUNT[get_instrument_id()]);

        if (current_tick >= stop_tick) {
            // 指定回数のループを演奏し終えたので、それ以上ループしないよう再生を終了する。
            // is_playing=falseにすることで、on_sync_tick側もこれ以降local_tickを
            // 加算しなくなり(is_playingを見て加算しているため)、自然に停止し続ける。
            is_playing = false;
            score_stop_all(); // 鳴っている音が残っていれば強制NOTE_OFFして止める

            Serial.print(F("[PART END] instrument_id="));
            Serial.print(get_instrument_id());
            Serial.println(F(" reached loop count. stopped."));
        } else if (frog_state) {
            // 【今回追加】カエル人形が乗っている(frog_state=1)ときだけ発音する。
            int8_t pitch_offset = pressure_get_pitch_offset(); //追加
            // score_step()にはscore_data.hの譜面基準(0〜LOOP_MAX_TICK-1)のtick位置を渡す。
            // local_tick自体はもう毎ループ0に巻き戻らないため、ここで割った余りを渡す。
            score_step((uint16_t)(current_tick % LOOP_MAX_TICK), pitch_offset); //変更
        } else {
            // 【今回追加】人形が乗っていない(frog_state=0)ときは発音しない。
            // 鳴っている音は止める。local_tickはSYNCで進み続けるので(is_playingは維持)、
            // 再び人形を乗せれば輪唱の正しい位置から鳴り復帰する。
            score_stop_all();
        }
    }
}

#endif // !TEST_BUTTON_MODE