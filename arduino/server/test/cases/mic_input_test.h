#ifndef MIC_INPUT_TEST_H
#define MIC_INPUT_TEST_H

// mic_input.inoに対するテストをこのファイルへ記述する。

// マイク入力初期化時にサンプリング時刻が保存されることをテストする。
void test_mic_setup_stores_sample_time() {
    // 入力: 最終サンプリング時刻=0。
    // 過程: mic_setup()でA1を入力に設定し、現在時刻を保存する。
    // 出力: 最終サンプリング時刻が呼び出し前後の範囲内になる。
    log_input("最終マイクサンプリング時刻=0");

    unsigned long before = millis();
    log_process("マイクピンを入力に設定し、現在時刻を保存する");
    mic_setup();
    unsigned long after = millis();

    log_output("初期化前の時刻=", before);
    log_output("保存された時刻=", last_mic_sample_ms);
    log_output("初期化後の時刻=", after);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(before, last_mic_sample_ms);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(after, last_mic_sample_ms);
}

// サンプリング周期前はマイク入力を処理しないことをテストする。
void test_mic_read_skips_before_sample_interval() {
    // 入力: 最終サンプリング時刻を現在時刻にし、履歴位置を0にする。
    // 過程: 10ミリ秒が経過する前にmic_read()を呼び出す。
    // 出力: falseを返し、履歴位置と最終時刻を変更しない。
    log_input("前回取得からの経過時間が10ミリ秒未満");
    last_mic_sample_ms = millis();
    unsigned long initial_sample_time = last_mic_sample_ms;
    win_id = 0;

    log_process("サンプリング周期に達しているか確認する");
    bool result = mic_read();

    log_output("拍手検出結果=", result);
    log_output("履歴位置=", win_id);
    log_output("最終取得時刻=", last_mic_sample_ms);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT8(0, win_id);
    TEST_ASSERT_EQUAL_UINT32(initial_sample_time, last_mic_sample_ms);
}

// マイク入力履歴が50件で満杯になることをテストする。
void test_mic_read_fills_sample_window() {
    // 入力: 空の50件分マイク入力履歴。
    // 過程: 各回で取得周期を経過させ、mic_read()を50回呼び出す。
    // 出力: 履歴位置が0へ戻り、win_fullがtrueになる。
    log_input("空のマイク入力履歴へ50回分のA1入力を保存する");
    win_id = 0;
    win_full = false;

    log_process("10ミリ秒周期を模擬して50回サンプリングする");
    for (int i = 0; i < WINDOW_SIZE; i++) {
        last_mic_sample_ms = millis() - MIC_SAMPLE_INTERVAL_MS;
        bool result = mic_read();
        TEST_ASSERT_FALSE(result);
    }

    log_output("履歴位置=", win_id);
    log_output("履歴充足状態=", win_full);

    TEST_ASSERT_EQUAL_UINT8(0, win_id);
    TEST_ASSERT_TRUE(win_full);
}

// ピーク終了時にピーク管理状態が初期化されることをテストする。
void test_mic_read_finishes_peak_tracking() {
    // 入力: ピーク追跡中、下降判定が残り1回、前回拍手から250ミリ秒未満。
    // 過程: 現在値がピーク最大値を下回る状態でmic_read()を呼び出す。
    // 出力: BPM更新を行わず、ピーク追跡情報が初期値へ戻る。
    log_input("ピーク追跡中、下降回数=2、前回拍手から250ミリ秒未満");

    win_full = true;
    peak_track = true;
    peak_max = UINT16_MAX;
    peak_time = millis();
    last_time = peak_time;
    down_count = PEAK_DOWN_COUNT - 1;
    last_mic_sample_ms = millis() - MIC_SAMPLE_INTERVAL_MS;

    log_process("3回目の下降を検出し、短すぎる拍手間隔を除外する");
    bool result = mic_read();

    log_output("BPM更新結果=", result);
    log_output("ピーク追跡状態=", peak_track);
    log_output("ピーク最大値=", peak_max);
    log_output("ピーク時刻=", peak_time);
    log_output("下降回数=", down_count);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(peak_track);
    TEST_ASSERT_EQUAL_UINT16(0, peak_max);
    TEST_ASSERT_EQUAL_UINT32(0, peak_time);
    TEST_ASSERT_EQUAL_UINT8(0, down_count);
}

// 取得したA1の値がマイク入力履歴へ保存されることをテストする。
void test_mic_read_stores_analog_value() {
    // 入力: 空の履歴位置0と、Arduino UNO R4のA1入力値。
    // 過程: サンプリング周期経過後にmic_read()でA1を読み取る。
    // 出力: valueと履歴0番が一致し、次の履歴位置が1になる。
    log_input("履歴位置=0、A1から現在のマイク入力値を取得する");

    win_id = 0;
    win_full = false;
    last_mic_sample_ms = millis() - MIC_SAMPLE_INTERVAL_MS;

    log_process("A1の値を読み取り、マイク入力履歴へ保存する");
    bool result = mic_read();

    log_output("拍手検出結果=", result);
    log_output("取得値=", value);
    log_output("履歴に保存した値=", mic_window[0]);
    log_output("次の履歴位置=", win_id);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_UINT16(value, mic_window[0]);
    TEST_ASSERT_EQUAL_UINT8(1, win_id);
    TEST_ASSERT_FALSE(win_full);
}

// mic_input.inoに関する全テストを実行する。
void run_mic_input_tests() {
    RUN_TEST(test_mic_setup_stores_sample_time);
    RUN_TEST(test_mic_read_skips_before_sample_interval);
    RUN_TEST(test_mic_read_fills_sample_window);
    RUN_TEST(test_mic_read_finishes_peak_tracking);
    RUN_TEST(test_mic_read_stores_analog_value);
}

#endif
