#ifndef I2C_MASTER_TEST_H
#define I2C_MASTER_TEST_H

// I2C初期化後の同期ピン状態を確認する。
void test_i2c_initial_sync_pin_state() {
    log_input("I2C通信速度=100kHz");

    i2c_setup();

    log_process("I2Cマスターと同期ピンを初期化する");
    log_output("同期ピン=", digitalRead(CS_SYNC));

    TEST_ASSERT_EQUAL_INT(LOW, digitalRead(CS_SYNC));
}

// 各楽器へ割り当てたI2Cアドレスを確認する。
void test_i2c_device_addresses() {
    log_input("楽器番号=0～3");
    log_process("各楽器のI2Cアドレスを確認する");

    TEST_ASSERT_EQUAL_HEX8(0x0A, dev_ctl[0].i2c_address);
    TEST_ASSERT_EQUAL_HEX8(0x0B, dev_ctl[1].i2c_address);
    TEST_ASSERT_EQUAL_HEX8(0x0C, dev_ctl[2].i2c_address);
    TEST_ASSERT_EQUAL_HEX8(0x0D, dev_ctl[3].i2c_address);
    log_output("デバイスアドレス=",dev_ctl[0].i2c_address);
    log_output("，",dev_ctl[1].i2c_address);
    log_output("，",dev_ctl[2].i2c_address);
    log_output("，",dev_ctl[3].i2c_address);
}

// 範囲外の楽器番号では接続確認を開始しないことを確認する。
void test_i2c_wait_ack_rejects_invalid_device() {
    log_input("無効な楽器番号=-1、4");
    log_process("I2C接続確認がfalseを返すことを確認する");
    TEST_ASSERT_FALSE(i2c_wait_ack(-1));
    TEST_ASSERT_FALSE(i2c_wait_ack(4));
}

// 範囲外の楽器番号では送信せず、管理状態を変更しないことを確認する。
void test_i2c_send_rejects_invalid_device() {
    ControlCommand cmd{};
    generate_cmd(0x00, cmd);

    log_input("無効な楽器番号=-1、4");
    log_process("I2C送信後も全楽器のエラー回数が変化しないことを確認する");

    i2c_send(-1, cmd);
    i2c_send(4, cmd);

    for (int dev = 0; dev < 4; dev++) {
        TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[dev].error_count);
    }
}

// 範囲外の楽器番号では状態パケットを要求しないことを確認する。
void test_i2c_read_status_rejects_invalid_device() {
    InstrumentStatus status{};

    log_input("無効な楽器番号=-1、4");
    log_process("状態パケット読み取りがfalseを返すことを確認する");

    TEST_ASSERT_FALSE(i2c_read_status_packet(-1, status));
    TEST_ASSERT_FALSE(i2c_read_status_packet(4, status));
}

// I2Cマスターの単体テストを実行する。
void run_i2c_master_tests() {
    RUN_TEST(test_i2c_initial_sync_pin_state);
    RUN_TEST(test_i2c_device_addresses);
    RUN_TEST(test_i2c_wait_ack_rejects_invalid_device);
    RUN_TEST(test_i2c_send_rejects_invalid_device);
    RUN_TEST(test_i2c_read_status_rejects_invalid_device);
}

#endif
