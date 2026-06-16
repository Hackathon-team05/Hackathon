#ifndef SERVER_TEST_H
#define SERVER_TEST_H

// server.inoに対するテストをこのファイルへ記述する。

// 通信パケットの構造体サイズをテストする。
void test_server_packet_structure_sizes() {
    // 入力: ControlCommand構造体とInstrumentStatus構造体。
    // 過程: packed指定された各構造体のサイズを取得する。
    // 出力: SPIで送受信する両パケットが5バイトになる。
    log_input("送信パケットと受信パケットの構造体");

    log_process("各構造体のメモリサイズを取得する");
    log_output("送信パケットサイズ=", sizeof(ControlCommand));
    log_output("受信パケットサイズ=", sizeof(InstrumentStatus));

    TEST_ASSERT_EQUAL_UINT32(5, sizeof(ControlCommand));
    TEST_ASSERT_EQUAL_UINT32(5, sizeof(InstrumentStatus));
}

// サーバーで使用する主要な設定値をテストする。
void test_server_configuration_values() {
    // 入力: シリアル、接続確認、BPM、周期に関する定数。
    // 過程: server.inoで宣言された設定値を読み取る。
    // 出力: 仕様で定めた値と一致する。
    log_input("サーバーの通信設定、BPM範囲、サンプリング周期");

    log_process("server.inoの主要な定数を確認する");
    log_output("通信速度=", BAUD);
    log_output("接続コマンド=", CMD_CONNECT);
    log_output("正常応答=", ACK_OK);
    log_output("既定BPM=", DEF_BPM);
    log_output("最小BPM=", BPM_MIN);
    log_output("最大BPM=", BPM_MAX);
    log_output("状態監視周期=", STATUS_POLL_INTERVAL_MS);
    log_output("マイク取得周期=", MIC_SAMPLE_INTERVAL_MS);

    TEST_ASSERT_EQUAL_INT(115200, BAUD);
    TEST_ASSERT_EQUAL_INT(100, CMD_CONNECT);
    TEST_ASSERT_EQUAL_INT(0, DUMMY);
    TEST_ASSERT_EQUAL_INT(200, ACK_OK);
    TEST_ASSERT_EQUAL_INT(3, max_try);
    TEST_ASSERT_EQUAL_INT(70, DEF_BPM);
    TEST_ASSERT_EQUAL_INT(60, BPM_MIN);
    TEST_ASSERT_EQUAL_INT(140, BPM_MAX);
    TEST_ASSERT_EQUAL_UINT32(50, STATUS_POLL_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(10, MIC_SAMPLE_INTERVAL_MS);
}

// 各楽器の管理情報が初期状態へ戻ることをテストする。
void test_server_device_state_reset() {
    // 入力: setUp()によって初期化された4台分の管理情報。
    // 過程: 各楽器のCSピン、エラー、状態フラグを読み取る。
    // 出力: CSピンはD4～D7で、その他の管理情報はすべて初期値になる。
    log_input("4台分のDeviceStatus管理情報");

    log_process("各楽器のCSピンと状態フラグを確認する");
    log_output("楽器1のCSピン=", dev_ctl[0].cs_pin);
    log_output("楽器2のCSピン=", dev_ctl[1].cs_pin);
    log_output("楽器3のCSピン=", dev_ctl[2].cs_pin);
    log_output("楽器4のCSピン=", dev_ctl[3].cs_pin);

    TEST_ASSERT_EQUAL_UINT8(CS_DEV1, dev_ctl[0].cs_pin);
    TEST_ASSERT_EQUAL_UINT8(CS_DEV2, dev_ctl[1].cs_pin);
    TEST_ASSERT_EQUAL_UINT8(CS_DEV3, dev_ctl[2].cs_pin);
    TEST_ASSERT_EQUAL_UINT8(CS_DEV4, dev_ctl[3].cs_pin);

    for (int dev = 0; dev < 4; dev++) {
        TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[dev].error_count);
        TEST_ASSERT_FALSE(dev_ctl[dev].failsafe);
        TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[dev].frog_state);
        TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[dev].prev_frog_state);
        TEST_ASSERT_FALSE(dev_ctl[dev].pending_entry);
        TEST_ASSERT_FALSE(dev_ctl[dev].pending_stop);
        TEST_ASSERT_FALSE(dev_ctl[dev].is_playing);
    }
}

// server.inoに関する全テストを実行する。
void run_server_tests() {
    RUN_TEST(test_server_packet_structure_sizes);
    RUN_TEST(test_server_configuration_values);
    RUN_TEST(test_server_device_state_reset);
}

#endif
