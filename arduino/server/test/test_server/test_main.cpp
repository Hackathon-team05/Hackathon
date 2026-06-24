#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

// 試験対象の楽器番号。0～3の範囲で指定する。
const int TEST_DEVICE = 0;

// Arduino IDEが通常生成する関数プロトタイプを、.ino読込前に補う。
struct ControlCommand;
struct InstrumentStatus;

// 起動シーケンス: シリアル、SPI、既定BPMの初期化と楽器接続確認を行う。
void boot_setup();
bool wait_ack(int dev);

// SPIマスター: ピン初期化、5バイト送受信、フェイルセーフ停止を行う。
void spi_setup();
void spi_send(int dev, ControlCommand& cmd, InstrumentStatus& status);
void failsafe(int dev, ControlCommand& cmd, InstrumentStatus& status);

// BPM管理: 拍手時刻からのBPM計算と現在値の取得を行う。
void bpm_setup();
bool bpm_generate(unsigned long last_t);
float get_bpm();
float default_bpm();

// tick管理: BPMに応じたtick生成と、演奏開始境界の判定を行う。
void tick_setup();
uint32_t tick_generate();
bool is_entry_boundary();

// マイク入力: 入力初期化と拍手ピーク検出を行う。
void mic_setup();
bool mic_read();

// 通信コマンド: パケット生成、チェックサム、応答検証、状態遷移を行う。
void generate_cmd(int command, ControlCommand& cmd);
void generate_checksum(ControlCommand& cmd);
void verification_status(int dev, ControlCommand& cmd, InstrumentStatus& status);
void handle_device_command(int dev, ControlCommand& cmd);

// 製品側のsetup/loopと、Unityテスト側のsetup/loopの名前衝突を防ぐ。
#define setup server_application_setup
#define loop server_application_loop
#include "../../server.ino"
#undef setup
#undef loop

// SPI処理から参照される製品コードをそのままテスト対象へ組み込む。
#include "../../setup_sequence.ino"
#include "../../spi_master.ino"
#include "../../bpm_manager.ino"
#include "../../tick_generator.ino"
#include "../../entry_que.ino"
#include "../../mic_input.ino"
#include "../../control_command.ino"

// テストへ与える入力を表示する。
void log_input(const char* message) {
    Serial.print("  [入力] ");
    Serial.println(message);
}

// テスト中の処理内容を表示する。
void log_process(const char* message) {
    Serial.print("  [処理] ");
    Serial.println(message);
}

// テストで得られた数値を表示する。
void log_output(const char* label, unsigned long value) {
    Serial.print("  [出力] ");
    Serial.print(label);
    Serial.println(value);
}

// パケットの全バイトを加算する。
// 正常なチェックサムであれば戻り値は0になる。
uint8_t packet_sum(const void* packet, size_t size) {
    const uint8_t* bytes = static_cast<const uint8_t*>(packet);//1バイトずつ引数packetを読み込む
    uint8_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += bytes[i];
    }
    return sum;
}

// 送信コマンドの内容を表示する。
void print_command(const ControlCommand& cmd) {
    Serial.print("  [出力] コマンド種別=");
    Serial.print(cmd.command_type);
    Serial.print(", データ=");
    Serial.print(cmd.payload);
    Serial.print(", シーケンス番号=");
    Serial.print(cmd.sequence);
    Serial.print(", チェックサム=");
    Serial.println(cmd.checksum);
}

// 楽器Arduinoから受信した状態を表示する。
void print_status(const InstrumentStatus& status) {
    Serial.print("  [出力] 楽器ID=");
    Serial.print(status.instrument_id);
    Serial.print(", カエル状態=");
    Serial.print(status.frog_state);
    Serial.print(", 応答シーケンス番号=");
    Serial.print(status.sequence_ack);
    Serial.print(", 正常応答=");
    Serial.print(status.ack_ok);
    Serial.print(", チェックサム=");
    Serial.println(status.checksum);
}

// 選択した楽器Arduinoへ接続確認を行う。
// ACK_OKを受信した場合はtrueを返す。
bool connect_test_device() {
    int cs_pin = dev_ctl[TEST_DEVICE].cs_pin;//dev_ctl配列からcs_pinに該当する場所を抜き取る

    Serial.print("  [入力] 楽器番号=");
    Serial.print(TEST_DEVICE);
    Serial.print(", CSピン=");
    Serial.println(cs_pin);
    log_process("接続コマンド100を送信し、ダミーデータ0の送信中にACKを受信する");

    bool connected = wait_ack(cs_pin);
    log_output("接続結果=", connected);
    return connected;
}

// 各テストの前にサーバーの状態を初期値へ戻す。
void reset_device_state() {
    command_sequence = 0;
    bpm = DEF_BPM;
    clap_count = 0;
    last_time = 0;

    for (int i = 0; i < CLAP_MAX; i++) {
        clap_times[i] = 0;
    }
    for (int i = 0; i < CLAP_INTERVAL_COUNT; i++) {
        clap_intervals[i] = 0;
        clap_sorted_intervals[i] = 0;
    }

    global_tick = 0;
    last_tick_us = 0;
    tick_period_us = 0;
    entry_boundary_reached = false;

    value = 0;
    win_id = 0;
    win_full = false;
    last_mic_sample_ms = 0;
    peak_track = false;
    peak_max = 0;
    peak_time = 0;
    down_count = 0;

    for (int i = 0; i < WINDOW_SIZE; i++) {
        mic_window[i] = 0;
    }

    for (int dev = 0; dev < 4; dev++) {
        dev_ctl[dev].error_count = 0;
        dev_ctl[dev].failsafe = false;
        dev_ctl[dev].frog_state = 0;
        dev_ctl[dev].prev_frog_state = 0;
        dev_ctl[dev].pending_entry = false;
        dev_ctl[dev].pending_stop = false;
        dev_ctl[dev].is_playing = false;
    }
}

// Unityが各テストの直前に呼び出す。
void setUp() {
    reset_device_state();
}

// Unityが各テストの直後に呼び出す。
void tearDown() {
}

// 各inoファイルに対応するテストファイルを読み込む。
#include "../cases/server_test.h"
#include "../cases/spi_master_test.h"
#include "../cases/control_command_test.h"
#include "../cases/integration.h"
#include "../cases/bpm_manager_test.h"
#include "../cases/tick_generator_test.h"
#include "../cases/entry_que_test.h"
#include "../cases/mic_input_test.h"

// Arduino起動後に全テストを1回実行する。
void setup() {
    Serial.begin(115200);
    delay(2000);

    spi_setup();
    delay(1000);

    UNITY_BEGIN();
    run_server_tests();
    run_spi_master_tests();
    run_control_command_tests();
    run_integration_tests();
    run_bpm_manager_tests();
    run_tick_generator_tests();
    run_entry_que_tests();
    run_mic_input_tests();
    UNITY_END();
}

// テストはsetup()で完了するため、繰り返し処理は行わない。
void loop() {
}
