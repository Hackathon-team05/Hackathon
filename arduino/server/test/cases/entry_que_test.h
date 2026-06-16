#ifndef ENTRY_QUE_TEST_H
#define ENTRY_QUE_TEST_H

// entry_que.inoに対するテストをこのファイルへ記述する。

// tickが0の場合は演奏開始境界ではないことをテストする。
void test_entry_boundary_at_zero_tick() {
    // 入力: global_tick=0。
    // 過程: is_entry_boundary()で128tick境界か判定する。
    // 出力: 0は開始境界として扱わずfalseになる。
    log_input("global_tick=0");
    global_tick = 0;

    log_process("0tickを演奏開始境界として扱わないことを確認する");
    bool result = is_entry_boundary();

    log_output("境界判定=", result);
    TEST_ASSERT_FALSE(result);
}

// 128tickの直前は演奏開始境界ではないことをテストする。
void test_entry_boundary_before_128_ticks() {
    // 入力: global_tick=127。
    // 過程: global_tickを128で割った余りを確認する。
    // 出力: 余りが0ではないためfalseになる。
    log_input("global_tick=127");
    global_tick = 127;

    log_process("128tick境界の直前を判定する");
    bool result = is_entry_boundary();

    log_output("境界判定=", result);
    TEST_ASSERT_FALSE(result);
}

// 128tickごとに演奏開始境界となることをテストする。
void test_entry_boundary_at_multiples_of_128() {
    // 入力: global_tick=128と256。
    // 過程: 各tickを128で割った余りを確認する。
    // 出力: どちらも128の倍数なのでtrueになる。
    log_input("global_tick=128、256");

    log_process("128tickごとの演奏開始境界を判定する");
    global_tick = 128;
    bool first_result = is_entry_boundary();
    global_tick = 256;
    bool second_result = is_entry_boundary();

    log_output("128tickの境界判定=", first_result);
    log_output("256tickの境界判定=", second_result);

    TEST_ASSERT_TRUE(first_result);
    TEST_ASSERT_TRUE(second_result);
}

// 128tick境界の直後は演奏開始境界ではないことをテストする。
void test_entry_boundary_after_128_ticks() {
    // 入力: global_tick=129。
    // 過程: global_tickを128で割った余りを確認する。
    // 出力: 余りが1なのでfalseになる。
    log_input("global_tick=129");
    global_tick = 129;

    log_process("128tick境界の直後を判定する");
    bool result = is_entry_boundary();

    log_output("境界判定=", result);
    TEST_ASSERT_FALSE(result);
}

// entry_que.inoに関する全テストを実行する。
void run_entry_que_tests() {
    RUN_TEST(test_entry_boundary_at_zero_tick);
    RUN_TEST(test_entry_boundary_before_128_ticks);
    RUN_TEST(test_entry_boundary_at_multiples_of_128);
    RUN_TEST(test_entry_boundary_after_128_ticks);
}

#endif
