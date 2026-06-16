#ifndef BPM_MANAGER_TEST_H
#define BPM_MANAGER_TEST_H

// bpm_manager.inoに対するテストをこのファイルへ記述する。

// BPM管理に使用する変数が初期化されることをテストする。
void test_bpm_setup() {
    // 入力: BPM、拍手回数、前回時刻、マイク入力履歴に初期値以外を設定する。
    // 過程: bpm_setup()を呼び出す。
    // 出力: BPMが既定値になり、拍手情報とマイク入力履歴が0になる。
    log_input("BPM=100、拍手回数=3、前回時刻=1000、マイク入力履歴=500");

    bpm = 100;
    clap_count = 3;
    last_time = 1000;
    for (int i = 0; i < WINDOW_SIZE; i++) {
        mic_window[i] = 500;
    }

    log_process("BPM管理変数を初期値へ戻す");
    bpm_setup();

    log_output("BPM=", bpm);
    log_output("拍手回数=", clap_count);
    log_output("前回時刻=", last_time);

    TEST_ASSERT_EQUAL_UINT16(DEF_BPM, bpm);
    TEST_ASSERT_EQUAL_INT(0, clap_count);
    TEST_ASSERT_EQUAL_UINT32(0, last_time);
    for (int i = 0; i < WINDOW_SIZE; i++) {
        TEST_ASSERT_EQUAL_UINT16(0, mic_window[i]);
    }
}

// 拍手が5回揃うまではBPMを更新しないことをテストする。
void test_bpm_generate_waits_for_five_claps() {
    // 入力: 500ミリ秒間隔の拍手時刻を4回入力する。
    // 過程: 各時刻をbpm_generate()へ渡す。
    // 出力: 戻り値がfalseで、BPMは既定値70のままになる。
    log_input("拍手時刻=0、500、1000、1500ミリ秒");
    bpm_setup();

    log_process("4回分の拍手時刻を保存し、BPM計算を待機する");
    bool result = false;
    result = bpm_generate(0);
    TEST_ASSERT_FALSE(result);
    result = bpm_generate(500);
    TEST_ASSERT_FALSE(result);
    result = bpm_generate(1000);
    TEST_ASSERT_FALSE(result);
    result = bpm_generate(1500);

    log_output("BPM更新結果=", result);
    log_output("拍手回数=", clap_count);
    log_output("BPM=", bpm);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(4, clap_count);
    TEST_ASSERT_EQUAL_UINT16(DEF_BPM, bpm);
}

// 一定間隔の拍手からBPMが計算されることをテストする。
void test_bpm_generate_from_regular_claps() {
    // 入力: 500ミリ秒間隔の拍手時刻を5回入力する。
    // 過程: 平均間隔から120 BPMを算出し、既定値70へ30パーセント反映する。
    // 出力: BPMが85になり、更新結果がtrueになる。
    log_input("拍手時刻=0、500、1000、1500、2000ミリ秒");
    bpm_setup();

    log_process("5回分の拍手間隔からBPMを計算する");
    bpm_generate(0);
    bpm_generate(500);
    bpm_generate(1000);
    bpm_generate(1500);
    bool result = bpm_generate(2000);

    log_output("BPM更新結果=", result);
    log_output("BPM=", bpm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(85, bpm);
}

// 算出したBPMが最小値60に制限されることをテストする。
void test_bpm_generate_clamps_to_minimum() {
    // 入力: 1500ミリ秒間隔の拍手時刻を5回入力する。
    // 過程: 算出値40を最小値60へ制限し、既定値70へ30パーセント反映する。
    // 出力: BPMが67になる。
    log_input("拍手間隔=1500ミリ秒、算出値=40 BPM");
    bpm_setup();

    log_process("算出したBPMを最小値60へ制限する");
    bpm_generate(0);
    bpm_generate(1500);
    bpm_generate(3000);
    bpm_generate(4500);
    bool result = bpm_generate(6000);

    log_output("BPM更新結果=", result);
    log_output("BPM=", bpm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(67, bpm);
}

// 算出したBPMが最大値140に制限されることをテストする。
void test_bpm_generate_clamps_to_maximum() {
    // 入力: 100ミリ秒間隔の拍手時刻を5回入力する。
    // 過程: 算出値600を最大値140へ制限し、既定値70へ30パーセント反映する。
    // 出力: BPMが91になる。
    log_input("拍手間隔=100ミリ秒、算出値=600 BPM");
    bpm_setup();

    log_process("算出したBPMを最大値140へ制限する");
    bpm_generate(0);
    bpm_generate(100);
    bpm_generate(200);
    bpm_generate(300);
    bool result = bpm_generate(400);

    log_output("BPM更新結果=", result);
    log_output("BPM=", bpm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(91, bpm);
}

// 拍手間隔の外れ値がBPM計算から除外されることをテストする。
void test_bpm_generate_ignores_outlier() {
    // 入力: 500、500、1500、500ミリ秒の拍手間隔。
    // 過程: 中央値500から30パーセント以上離れた1500を除外する。
    // 出力: 有効な500ミリ秒間隔からBPMが85になる。
    log_input("拍手間隔=500、500、1500、500ミリ秒");
    bpm_setup();

    log_process("中央値を基準に1500ミリ秒の外れ値を除外する");
    bpm_generate(0);
    bpm_generate(500);
    bpm_generate(1000);
    bpm_generate(2500);
    bool result = bpm_generate(3000);

    log_output("BPM更新結果=", result);
    log_output("BPM=", bpm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT16(85, bpm);
}

// 6回目以降の拍手で古い時刻が履歴から削除されることをテストする。
void test_bpm_generate_shifts_clap_history() {
    // 入力: 500ミリ秒間隔の拍手時刻を6回入力する。
    // 過程: 6回目の入力時に最初の時刻を削除し、残りを前へ移動する。
    // 出力: 履歴が500～2500になり、BPMが85から95へ更新される。
    log_input("拍手時刻=0、500、1000、1500、2000、2500ミリ秒");
    bpm_setup();

    log_process("6回目の拍手で5回分の履歴を更新する");
    bpm_generate(0);
    bpm_generate(500);
    bpm_generate(1000);
    bpm_generate(1500);
    bpm_generate(2000);
    bool result = bpm_generate(2500);

    log_output("BPM更新結果=", result);
    log_output("履歴の先頭時刻=", clap_times[0]);
    log_output("履歴の末尾時刻=", clap_times[CLAP_MAX - 1]);
    log_output("BPM=", bpm);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_UINT32(500, clap_times[0]);
    TEST_ASSERT_EQUAL_UINT32(2500, clap_times[CLAP_MAX - 1]);
    TEST_ASSERT_EQUAL_UINT16(95, bpm);
}

// 既定BPMへの復帰とBPM取得をテストする。
void test_default_and_get_bpm() {
    // 入力: 現在のBPMを120に変更する。
    // 過程: default_bpm()で既定値へ戻し、get_bpm()で値を取得する。
    // 出力: 両関数の戻り値と管理中のBPMが70になる。
    log_input("現在のBPM=120");
    bpm = 120;

    log_process("BPMを既定値へ戻してから現在値を取得する");
    float default_value = default_bpm();
    float current_value = get_bpm();

    log_output("BPM=", bpm);

    TEST_ASSERT_EQUAL_FLOAT(DEF_BPM, default_value);
    TEST_ASSERT_EQUAL_FLOAT(DEF_BPM, current_value);
    TEST_ASSERT_EQUAL_UINT16(DEF_BPM, bpm);
}

// bpm_manager.inoに関する全テストを実行する。
void run_bpm_manager_tests() {
    RUN_TEST(test_bpm_setup);
    RUN_TEST(test_bpm_generate_waits_for_five_claps);
    RUN_TEST(test_bpm_generate_from_regular_claps);
    RUN_TEST(test_bpm_generate_clamps_to_minimum);
    RUN_TEST(test_bpm_generate_clamps_to_maximum);
    RUN_TEST(test_bpm_generate_ignores_outlier);
    RUN_TEST(test_bpm_generate_shifts_clap_history);
    RUN_TEST(test_default_and_get_bpm);
}

#endif
