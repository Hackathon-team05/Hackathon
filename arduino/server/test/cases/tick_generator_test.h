#ifndef TICK_GENERATOR_TEST_H
#define TICK_GENERATOR_TEST_H

// tick_generator.inoに対するテストをこのファイルへ記述する。

// tick初期化時に現在時刻が保存されることをテストする。
void test_tick_setup_stores_current_time() {
    // 入力: last_tick_us=0の未初期化状態。
    // 過程: tick_setup()を呼び出し、呼び出し前後のmicros()と比較する。
    // 出力: last_tick_usが呼び出し前後の時刻範囲内になる。
    log_input("最終tick時刻=0");

    unsigned long before = micros();
    log_process("現在のマイクロ秒時刻を最終tick時刻へ保存する");
    tick_setup();
    unsigned long after = micros();

    log_output("初期化前の時刻=", before);
    log_output("保存された時刻=", last_tick_us);
    log_output("初期化後の時刻=", after);

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(before, last_tick_us);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(after, last_tick_us);
}

// tick周期に達していない場合はtickを生成しないことをテストする。
void test_tick_generate_before_period() {
    // 入力: BPM=120、最終tick時刻を現在時刻に設定する。
    // 過程: 31250マイクロ秒のtick周期が経過する前にtick_generate()を呼ぶ。
    // 出力: 生成tick数が0になる。
    log_input("BPM=120、経過時間はtick周期未満");
    bpm = 120;
    tick_setup();

    log_process("tick周期が経過する前に生成数を計算する");
    uint32_t count = tick_generate();

    log_output("tick周期=", tick_period_us);
    log_output("生成tick数=", count);

    TEST_ASSERT_EQUAL_UINT32(31250, tick_period_us);
    TEST_ASSERT_EQUAL_UINT32(0, count);
}

// tick周期が1回経過した場合に1tick生成することをテストする。
void test_tick_generate_one_period() {
    // 入力: BPM=120、最終tick時刻を1周期と1000マイクロ秒前に設定する。
    // 過程: 経過した完全なtick周期数を計算する。
    // 出力: 生成tick数が1になり、最終tick時刻が1周期進む。
    log_input("BPM=120、経過時間=1周期+1000マイクロ秒");
    bpm = 120;
    const unsigned long expected_period = 60000000UL / (bpm * 16UL);
    unsigned long initial_tick = micros() - expected_period - 1000;
    last_tick_us = initial_tick;

    log_process("経過時間に含まれる完全なtick周期を数える");
    uint32_t count = tick_generate();

    log_output("tick周期=", tick_period_us);
    log_output("生成tick数=", count);

    TEST_ASSERT_EQUAL_UINT32(expected_period, tick_period_us);
    TEST_ASSERT_EQUAL_UINT32(1, count);
    TEST_ASSERT_EQUAL_UINT32(initial_tick + expected_period, last_tick_us);
}

// 複数のtick周期が経過した場合に不足分をまとめて生成することをテストする。
void test_tick_generate_multiple_periods() {
    // 入力: BPM=120、最終tick時刻を3周期と1000マイクロ秒前に設定する。
    // 過程: while処理で経過した完全なtick周期をすべて数える。
    // 出力: 生成tick数が3になり、最終tick時刻が3周期進む。
    log_input("BPM=120、経過時間=3周期+1000マイクロ秒");
    bpm = 120;
    const unsigned long expected_period = 60000000UL / (bpm * 16UL);
    unsigned long initial_tick = micros() - expected_period * 3UL - 1000;
    last_tick_us = initial_tick;

    log_process("未生成のtickを3周期分まとめて計算する");
    uint32_t count = tick_generate();

    log_output("tick周期=", tick_period_us);
    log_output("生成tick数=", count);

    TEST_ASSERT_EQUAL_UINT32(3, count);
    TEST_ASSERT_EQUAL_UINT32(initial_tick + expected_period * 3UL, last_tick_us);
}

// tick_generator.inoに関する全テストを実行する。
void run_tick_generator_tests() {
    RUN_TEST(test_tick_setup_stores_current_time);
    RUN_TEST(test_tick_generate_before_period);
    RUN_TEST(test_tick_generate_one_period);
    RUN_TEST(test_tick_generate_multiple_periods);
}

#endif
