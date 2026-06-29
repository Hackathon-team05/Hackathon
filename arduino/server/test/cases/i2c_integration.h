#ifndef I2C_INTEGRATION_H
#define I2C_INTEGRATION_H

// I2Cで楽器Arduinoとの接続確認を行う。
void test_i2c_handshake_with_instrument() {
    log_input("I2Cアドレス0x0Aの楽器ArduinoへCMD_CONNECT=100を送信する");
    log_process("最大3回まで再試行し、ACK_OK=200を待つ");

    bool connected = i2c_wait_ack(TEST_DEVICE);

    log_output("接続結果=", connected);
    TEST_ASSERT_TRUE_MESSAGE(
        connected,
        "I2C接続確認に失敗しました。SDA、SCL、GND、アドレスとスレーブ電源を確認してください。"
    );
}

// I2Cで楽器の状態パケットを読み取る。
void test_i2c_status_poll_exchange() {
    log_input("I2Cアドレス0x0Aの楽器状態を読み取る");
    TEST_ASSERT_TRUE_MESSAGE(
        i2c_wait_ack(TEST_DEVICE),
        "状態読み取り前のI2C接続確認に失敗しました。"
    );

    InstrumentStatus status{};

    log_process("5バイトのInstrumentStatusを読み取り、チェックサムと楽器IDを検証する");
    i2c_receive_without_sequence_check(TEST_DEVICE, status);

    print_status(status);
    log_output("受信パケット合計値=", packet_sum(&status, sizeof(status)));
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(TEST_DEVICE, status.instrument_id);
    TEST_ASSERT_EQUAL_UINT8(0, packet_sum(&status, sizeof(status)));
    TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[TEST_DEVICE].error_count);
}

// I2CでBPMコマンドを送信し、楽器Arduinoから応答を受信する。
void test_i2c_bpm_command_exchange() {
    log_input("コマンド種別0x04、BPMデータ120をI2C送信する");
    TEST_ASSERT_TRUE_MESSAGE(
        i2c_wait_ack(TEST_DEVICE),
        "BPMコマンド送信前のI2C接続確認に失敗しました。"
    );

    bpm = 120;
    ControlCommand cmd{};
    InstrumentStatus status{};
    generate_cmd(0x04, cmd);

    log_process("5バイトのコマンドを送信し、5バイトの応答を検証する");
    i2c_send(TEST_DEVICE, cmd);
    i2c_receive_with_sequence_check(TEST_DEVICE, cmd, status);

    print_command(cmd);
    print_status(status);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(0x04, cmd.command_type);
    TEST_ASSERT_EQUAL_UINT16(120, cmd.payload);
    TEST_ASSERT_EQUAL_UINT8(TEST_DEVICE, status.instrument_id);
    TEST_ASSERT_EQUAL_UINT8(cmd.sequence, status.sequence_ack);
    TEST_ASSERT_EQUAL_UINT8(0x01, status.ack_ok);
    TEST_ASSERT_EQUAL_UINT8(0, packet_sum(&status, sizeof(status)));
    TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[TEST_DEVICE].error_count);
}

// I2C通信エラーが3回発生した場合のフェイルセーフ動作を確認する。
void test_i2c_failsafe_after_three_errors() {
    log_input("I2C通信エラー回数=3、再生状態=再生中");
    TEST_ASSERT_TRUE_MESSAGE(
        i2c_wait_ack(TEST_DEVICE),
        "フェイルセーフ確認前のI2C接続確認に失敗しました。"
    );

    dev_ctl[TEST_DEVICE].error_count = 3;
    dev_ctl[TEST_DEVICE].is_playing = true;

    ControlCommand cmd{};

    log_process("停止コマンド0x02をI2C送信し、フェイルセーフを有効にする");
    handle_device_command(TEST_DEVICE, cmd);

    print_command(cmd);
    log_output("フェイルセーフ状態=", dev_ctl[TEST_DEVICE].failsafe);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_TRUE(dev_ctl[TEST_DEVICE].failsafe);
    TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[TEST_DEVICE].error_count);
    TEST_ASSERT_EQUAL_UINT8(0x02, cmd.command_type);
    TEST_ASSERT_EQUAL_UINT16(0, cmd.payload);
    TEST_ASSERT_EQUAL_UINT8(0, packet_sum(&cmd, sizeof(cmd)));
}

//テストの実施
void run_i2c_integration_tests(){
    RUN_TEST(test_i2c_handshake_with_instrument);
    RUN_TEST(test_i2c_status_poll_exchange);
    RUN_TEST(test_i2c_bpm_command_exchange);
    RUN_TEST(test_i2c_failsafe_after_three_errors);
}

#endif
