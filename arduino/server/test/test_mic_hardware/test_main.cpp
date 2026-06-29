#include <Arduino.h>
#include <SPI.h>
#include <unity.h>

// Arduino IDEが通常生成する関数プロトタイプを、.ino読込前に補う。
void boot_setup();
bool wait_ack(int dev);
void spi_setup();
void spi_send(int dev, struct ControlCommand& cmd, struct InstrumentStatus& status);
void failsafe(int dev, struct ControlCommand& cmd, struct InstrumentStatus& status);
bool i2c_wait_ack(int dev);
void i2c_send(int dev, struct ControlCommand& cmd);
void i2c_receive_with_sequence_check(
    int dev,
    struct ControlCommand& cmd,
    struct InstrumentStatus& status
);
void i2c_receive_without_sequence_check(int dev, struct InstrumentStatus& status);
void bpm_setup();
bool bpm_generate(unsigned long last_t);
float get_bpm();
float default_bpm();
void tick_setup();
uint32_t tick_generate();
bool is_entry_boundary();
void mic_setup();
bool mic_read();
void generate_cmd(int command, struct ControlCommand& cmd);
void generate_checksum(struct ControlCommand& cmd);
void verification_status(
    int dev,
    struct ControlCommand& cmd,
    struct InstrumentStatus& status
);
void handle_device_command(int dev, struct ControlCommand& cmd);

// 製品側のsetup/loopと、Unityテスト側のsetup/loopの名前衝突を防ぐ。
#define setup server_application_setup
#define loop server_application_loop
#include "../../server.ino"
#undef setup
#undef loop

// マイク読み取り処理と、拍手時刻からBPMを計算する処理をテスト対象にする。
#include "../../bpm_manager.ino"
#include "../../mic_input.ino"

// 80 BPMのメトロノーム音を観測する時間。
const unsigned long TEST_DURATION_MS = 10000;

// テスト環境で再生するメトロノームのBPMと許容誤差。
const uint16_t EXPECTED_BPM = 80;
const uint16_t BPM_TOLERANCE = 5;

// 10秒間では約13拍となる。開始位相と初期ウィンドウ生成を考慮して範囲を持たせる。
const uint16_t MIN_EXPECTED_TICK_COUNT = 10;
const uint16_t MAX_EXPECTED_TICK_COUNT = 15;

// UNO R4のanalogRead()が標準設定で返す12ビットADCの範囲。
const uint16_t ADC_MIN_VALUE = 0;
const uint16_t ADC_MAX_VALUE = 4095;

// 拍手や音による入力変化があったと判断する最小変動幅。
const uint16_t REQUIRED_SIGNAL_RANGE = 20;

// Unityが各テストの直前に呼び出す。
void setUp() {
    analogReadResolution(12);
    bpm_setup();
    mic_setup();
}

// Unityが各テストの直後に呼び出す。
void tearDown() {
}

// mic_read()でA1を10秒間読み取り、80 BPMのメトロノーム検出結果を確認する。
void test_microphone_input_with_mic_read() {
    // 入力: 距離約50cm、音量50%で再生する80 BPMのメトロノーム音。
    // 過程: 製品コードのmic_read()を10秒間呼び出す。
    // 出力: 信号変動、検出拍数、推定BPMを確認する。
    Serial.println();
    Serial.println("[入力] A1へマイクを接続し、80 BPMのメトロノームを再生してください。");
    Serial.println("[処理] 製品コードのmic_read()でA1を読み取ります。");
    Serial.println("[出力] 経過時間(ms), ADC値, ピーク追跡, BPM更新, BPM, 拍手回数");
    Serial.println("MIC_HEADER,time_ms,adc,peak_track,bpm_updated,bpm,clap_count");

    uint16_t minimum_value = ADC_MAX_VALUE;
    uint16_t maximum_value = ADC_MIN_VALUE;
    uint32_t sample_count = 0;
    uint32_t bpm_update_count = 0;
    uint32_t detected_tick_count = 0;
    bool all_values_in_range = true;

    unsigned long start_time = millis();

    while (millis() - start_time < TEST_DURATION_MS) {
        unsigned long previous_sample_time = last_mic_sample_ms;
        unsigned long previous_tick_time = last_time;
        bool bpm_updated = mic_read();

        // clap_countは5で飽和するため、last_timeの更新から総検出拍数を数える。
        if (last_time != previous_tick_time) {
            detected_tick_count++;
        }

        // mic_read()がサンプリング周期前に終了した場合はログへ記録しない。
        if (last_mic_sample_ms == previous_sample_time) {
            continue;
        }

        uint16_t sample = value;
        unsigned long elapsed = millis() - start_time;

        // Pythonで解析できるように、各測定値を固定形式のCSVとして出力する。
        Serial.print("MIC_DATA,");
        Serial.print(elapsed);
        Serial.print(",");
        Serial.print(sample);
        Serial.print(",");
        Serial.print(peak_track ? 1 : 0);
        Serial.print(",");
        Serial.print(bpm_updated ? 1 : 0);
        Serial.print(",");
        Serial.print(get_bpm());
        Serial.print(",");
        Serial.println(clap_count);

        Serial.print("[出力] ");
        Serial.print(elapsed);
        Serial.print(", ");
        Serial.print(sample);
        Serial.print(", ");
        Serial.print(peak_track);
        Serial.print(", ");
        Serial.print(bpm_updated);
        Serial.print(", ");
        Serial.print(get_bpm());
        Serial.print(", ");
        Serial.println(clap_count);

        if (sample < minimum_value) {
            minimum_value = sample;
        }
        if (sample > maximum_value) {
            maximum_value = sample;
        }
        if (sample > ADC_MAX_VALUE) {
            all_values_in_range = false;
        }
        if (bpm_updated) {
            bpm_update_count++;
        }

        sample_count++;
    }

    uint16_t signal_range = maximum_value - minimum_value;

    Serial.println("[処理] 10秒間の測定結果を集計します。");
    Serial.print("[出力] サンプル数=");
    Serial.println(sample_count);
    Serial.print("[出力] 最小値=");
    Serial.println(minimum_value);
    Serial.print("[出力] 最大値=");
    Serial.println(maximum_value);
    Serial.print("[出力] 変動幅=");
    Serial.println(signal_range);
    Serial.print("[出力] BPM計算窓内の拍数=");
    Serial.println(clap_count);
    Serial.print("[出力] 検出した総拍数=");
    Serial.println(detected_tick_count);
    Serial.print("[出力] BPM更新回数=");
    Serial.println(bpm_update_count);
    Serial.print("[出力] 最終BPM=");
    Serial.println(get_bpm());
    Serial.print("MIC_SUMMARY,");
    Serial.print(sample_count);
    Serial.print(",");
    Serial.print(minimum_value);
    Serial.print(",");
    Serial.print(maximum_value);
    Serial.print(",");
    Serial.print(signal_range);
    Serial.print(",");
    Serial.print(clap_count);
    Serial.print(",");
    Serial.print(bpm_update_count);
    Serial.print(",");
    Serial.println(get_bpm());
    Serial.print("METRONOME_SUMMARY,");
    Serial.print(EXPECTED_BPM);
    Serial.print(",");
    Serial.print(detected_tick_count);
    Serial.print(",");
    Serial.print(bpm_update_count);
    Serial.print(",");
    Serial.println(get_bpm());

    TEST_ASSERT_TRUE_MESSAGE(
        all_values_in_range,
        "ADC値が0～4095の範囲外です。A1の電圧と配線を確認してください。"
    );
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(
        TEST_DURATION_MS / MIC_SAMPLE_INTERVAL_MS - 1,
        sample_count
    );
    TEST_ASSERT_TRUE_MESSAGE(
        signal_range >= REQUIRED_SIGNAL_RANGE,
        "入力変動が小さすぎます。マイクの電源、GND、A1配線を確認し、測定中に拍手してください。"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        detected_tick_count >= MIN_EXPECTED_TICK_COUNT
            && detected_tick_count <= MAX_EXPECTED_TICK_COUNT,
        "80 BPMに対する検出拍数が許容範囲外です。"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        get_bpm() >= EXPECTED_BPM - BPM_TOLERANCE
            && get_bpm() <= EXPECTED_BPM + BPM_TOLERANCE,
        "推定BPMが80±5の範囲外です。"
    );
}

// Arduino起動後にマイク実機テストだけを1回実行する。
void setup() {
    Serial.begin(115200);
    delay(2000);

    UNITY_BEGIN();
    RUN_TEST(test_microphone_input_with_mic_read);
    UNITY_END();
}

// テストはsetup()で完了するため、繰り返し処理は行わない。
void loop() {
}
