#ifndef INTEGRATION_TEST_H
#define INTEGRATION_TEST_H
// 楽器Arduinoとの接続確認をテストする。
void test_spi_handshake_with_instrument() {
    // 入力: TEST_DEVICEで選択した楽器Arduinoと、そのCS配線。
    // 過程: CMD_CONNECT=100を送信し、DUMMY=0送信中にACKを読み取る。
    // 出力: 最大3回以内にACK_OK=200を受信し、接続成功となる。
    log_input("選択した楽器Arduinoの電源を入れ、SPI配線を接続する");
    bool connected = spi_connect_test_device();

    log_process("最大3回まで再試行し、正常応答200を待つ");
    TEST_ASSERT_TRUE_MESSAGE(
        connected,
        "正常応答200を受信できません。楽器の電源、配線、楽器番号、SPIモードを確認してください。"
    );
}

// 状態監視コマンドの送信と状態パケットの受信をテストする。
void test_spi_status_poll_exchange() {
    // 入力: command_type=0x00の定期状態監視コマンド。
    // 過程: CSをLOWにして5バイト送信し、同時にInstrumentStatusを5バイト受信する。
    // 出力: ID、sequence_ack、ack_ok、チェックサムが正しく、error_countが0になる。
    log_input("状態監視コマンド0x00を送信し、同時に楽器状態を受信する");
    TEST_ASSERT_TRUE_MESSAGE(
        spi_connect_test_device(),
        "状態監視前の接続確認に失敗しました。"
    );

    ControlCommand cmd{};
    InstrumentStatus status{};
    generate_cmd(0x00, cmd);

    log_process("CSをLOWにして5バイト送受信し、CSをHIGHに戻して受信状態を検証する");
    spi_send(TEST_DEVICE, cmd, status);
    print_command(cmd);
    print_status(status);
    log_output("受信パケット合計値=", packet_sum(&status, sizeof(status)));
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(TEST_DEVICE, status.instrument_id);
    TEST_ASSERT_EQUAL_UINT8(cmd.sequence, status.sequence_ack);
    TEST_ASSERT_EQUAL_UINT8(0x01, status.ack_ok);
    TEST_ASSERT_EQUAL_UINT8(0, packet_sum(&status, sizeof(status)));
    TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[TEST_DEVICE].error_count);
}

//フェイルセーフの有効化をテスト
void test_verification_status_ignored_during_failsafe() {
    // 入力: フェイルセーフが有効な楽器と、カエル設置状態の正常応答。
    // 過程: verification_status()を呼び出す。
    // 出力: エラー回数とカエル状態を含む管理状態が変更されない。
    log_input("フェイルセーフ状態=有効、受信したカエル状態=1");

    ControlCommand cmd{};
    cmd.sequence = 17;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.frog_state = 0x01;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    dev_ctl[TEST_DEVICE].failsafe = true;
    dev_ctl[TEST_DEVICE].error_count = 2;

    log_process("フェイルセーフ中のため受信状態の検証を行わず終了する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);
    log_output("カエル状態=", dev_ctl[TEST_DEVICE].frog_state);
    log_output("再生開始待ち=", dev_ctl[TEST_DEVICE].pending_entry);

    TEST_ASSERT_EQUAL_UINT8(2, dev_ctl[TEST_DEVICE].error_count);
    TEST_ASSERT_EQUAL_UINT8(0x00, dev_ctl[TEST_DEVICE].frog_state);
    TEST_ASSERT_FALSE(dev_ctl[TEST_DEVICE].pending_entry);
}

// BPMコマンドの送信と楽器Arduinoからの応答をテストする。
void test_spi_bpm_command_exchange() {
    // 入力: 現在BPM=120から生成したcommand_type=0x04のBPMコマンド。
    // 過程: BPMをpayloadへ格納して5バイト送信し、楽器側の処理結果を受信する。
    // 出力: payload=120で、sequence_ack、ack_ok、チェックサムがすべて正常になる。
    log_input("コマンド種別0x04、BPMデータ120を送信する");
    TEST_ASSERT_TRUE_MESSAGE(
        spi_connect_test_device(),
        "BPMコマンド送信前の接続確認に失敗しました。"
    );

    bpm = 120;
    ControlCommand cmd{};
    InstrumentStatus status{};
    generate_cmd(0x04, cmd);

    log_process("BPMデータを格納した5バイトを送受信し、楽器側のACKを検証する");
    spi_send(TEST_DEVICE, cmd, status);
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

// 通信エラーが3回発生したときのフェイルセーフ動作をテストする。
void test_spi_failsafe_after_three_errors() {
    // 入力: 対象楽器の通信エラー回数を3回にし、再生中の状態にする。
    // 過程: handle_device_command()からfailsafe()を呼び出し、停止コマンド0x02を送信する。
    // 出力: フェイルセーフが有効になり、エラー回数が0へ戻る。
    log_input("通信エラー回数=3、再生状態=再生中");
    TEST_ASSERT_TRUE_MESSAGE(
        spi_connect_test_device(),
        "Connnet"
    );

    dev_ctl[TEST_DEVICE].error_count = 3;
    dev_ctl[TEST_DEVICE].is_playing = true;

    ControlCommand cmd{};

    log_process("エラー回数を確認し、停止コマンド0x02を楽器Arduinoへ送信する");
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

// 実機SPI通信およびI2C通信を伴う統合テストを実行する。
void run_spi_integration_tests() {
    RUN_TEST(test_spi_handshake_with_instrument);
    RUN_TEST(test_spi_status_poll_exchange);
    RUN_TEST(test_verification_status_ignored_during_failsafe);
    RUN_TEST(test_spi_bpm_command_exchange);
    RUN_TEST(test_spi_failsafe_after_three_errors);
}

#endif
