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
extern void score_loop_check(volatile uint16_t &local_tick);
extern void score_stop_all();
// 【追加】i2c_slave.ino のI2C受信割り込み(on_i2c_receive)が予約したコマンドを、
// 割り込みの外(loop側)で実際に実行するための関数。重い処理(score_init等)を
// 割り込みハンドラから追い出すための仕組み(instrument_spi_fix_notes.md 指摘4対応)。
extern void process_pending_command();
// 【追加】I2Cで受信したControlCommandの内容をSerialへ出力するための関数
// (i2c_slave.ino側で受信内容を記録し、ここでloop()コンテキストから出力する)。
extern void print_i2c_log();
// 【追加】内蔵12x8ドットマトリクスLEDに担当パート名を表示するための関数
extern void matrix_display_init();

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
    matrix_display_init();

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

        int8_t pitch_offset = pressure_get_pitch_offset(); //追加
        score_step(current_tick, pitch_offset); //変更
        score_loop_check(local_tick);
    }
}

#endif // !TEST_BUTTON_MODE