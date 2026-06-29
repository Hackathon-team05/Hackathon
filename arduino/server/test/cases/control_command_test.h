#ifndef CONTROL_COMMAND_TEST_H
#define CONTROL_COMMAND_TEST_H

// control_command.inoに対するテストをこのファイルへ記述する。

// SPIパケットのサイズとチェックサムを確認する。
void test_spi_packet_layout_and_checksum() {
    // 入力: 状態監視コマンド0x00、payload=0、最初のsequence=0。
    // 過程: generate_cmd()でパケットを生成し、2の補数チェックサムを付加する。
    // 出力: 両構造体が5バイトで、送信パケットの全バイト合計が0になる。
    log_input("コマンド種別=0x00、データ=0、最初のシーケンス番号=0");

    ControlCommand cmd{};
    generate_cmd(0x00, cmd);

    log_process("5バイトのコマンドと2の補数チェックサムを生成する");
    print_command(cmd);
    log_output("パケット合計値=", packet_sum(&cmd, sizeof(cmd)));

    TEST_ASSERT_EQUAL_UINT32(5, sizeof(ControlCommand));
    TEST_ASSERT_EQUAL_UINT32(5, sizeof(InstrumentStatus));
    TEST_ASSERT_EQUAL_UINT8(0x00, cmd.command_type);
    TEST_ASSERT_EQUAL_UINT16(0, cmd.payload);
    TEST_ASSERT_EQUAL_UINT8(0, cmd.sequence);
    TEST_ASSERT_EQUAL_UINT8(0, packet_sum(&cmd, sizeof(cmd)));
}

// テスト用の受信状態へ2の補数チェックサムを設定する。
void set_status_checksum(InstrumentStatus& status) {
    status.checksum = 0;
    status.checksum = (uint8_t)(0 - packet_sum(&status, sizeof(status)));
}

// 正常な受信状態を検証できることをテストする。
void test_verification_status_valid_response() {
    // 入力: ID、シーケンス番号、ACK、チェックサムがすべて正常な受信状態。
    // 過程: verification_status()で受信状態を検証する。
    // 出力: 通信エラー回数が0になり、受信したカエル状態が保存される。
    log_input("正常な楽器ID、シーケンス番号、ACK、チェックサム");

    ControlCommand cmd{};
    cmd.sequence = 10;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.frog_state = 0x00;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    dev_ctl[TEST_DEVICE].error_count = 2;

    log_process("受信状態のチェックサム、楽器ID、シーケンス番号、ACKを検証する");
    verification_status(TEST_DEVICE, cmd, status);

    print_status(status);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);
    log_output("カエル状態=", dev_ctl[TEST_DEVICE].frog_state);

    TEST_ASSERT_EQUAL_UINT8(0, dev_ctl[TEST_DEVICE].error_count);
    TEST_ASSERT_EQUAL_UINT8(0x00, dev_ctl[TEST_DEVICE].frog_state);
}

// カエルが設置されたときの状態遷移をテストする。
void test_verification_status_frog_placed() {
    // 入力: 前回状態が未設置0、今回の受信状態が設置1。
    // 過程: verification_status()で前回状態と今回状態を比較する。
    // 出力: 再生開始待ちが有効になり、停止待ちが無効になる。
    log_input("前回のカエル状態=0、今回のカエル状態=1");

    ControlCommand cmd{};
    cmd.sequence = 11;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.frog_state = 0x01;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    dev_ctl[TEST_DEVICE].frog_state = 0x00;

    log_process("正常な受信状態を検証し、カエルの設置を検出する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("前回のカエル状態=", dev_ctl[TEST_DEVICE].prev_frog_state);
    log_output("現在のカエル状態=", dev_ctl[TEST_DEVICE].frog_state);
    log_output("再生開始待ち=", dev_ctl[TEST_DEVICE].pending_entry);
    log_output("停止待ち=", dev_ctl[TEST_DEVICE].pending_stop);

    TEST_ASSERT_EQUAL_UINT8(0x00, dev_ctl[TEST_DEVICE].prev_frog_state);
    TEST_ASSERT_EQUAL_UINT8(0x01, dev_ctl[TEST_DEVICE].frog_state);
    TEST_ASSERT_TRUE(dev_ctl[TEST_DEVICE].pending_entry);
    TEST_ASSERT_FALSE(dev_ctl[TEST_DEVICE].pending_stop);
}

// カエルが取り外されたときの状態遷移をテストする。
void test_verification_status_frog_removed() {
    // 入力: 前回状態が設置1、今回の受信状態が未設置0、演奏中。
    // 過程: verification_status()で前回状態と今回状態を比較する。
    // 出力: 停止待ちが有効になり、再生開始待ちと演奏状態が無効になる。
    log_input("前回のカエル状態=1、今回のカエル状態=0、演奏中");

    ControlCommand cmd{};
    cmd.sequence = 12;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.frog_state = 0x00;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    dev_ctl[TEST_DEVICE].frog_state = 0x01;
    dev_ctl[TEST_DEVICE].pending_entry = true;
    dev_ctl[TEST_DEVICE].is_playing = true;

    log_process("正常な受信状態を検証し、カエルの取り外しを検出する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("前回のカエル状態=", dev_ctl[TEST_DEVICE].prev_frog_state);
    log_output("現在のカエル状態=", dev_ctl[TEST_DEVICE].frog_state);
    log_output("再生開始待ち=", dev_ctl[TEST_DEVICE].pending_entry);
    log_output("停止待ち=", dev_ctl[TEST_DEVICE].pending_stop);
    log_output("演奏状態=", dev_ctl[TEST_DEVICE].is_playing);

    TEST_ASSERT_EQUAL_UINT8(0x01, dev_ctl[TEST_DEVICE].prev_frog_state);
    TEST_ASSERT_EQUAL_UINT8(0x00, dev_ctl[TEST_DEVICE].frog_state);
    TEST_ASSERT_FALSE(dev_ctl[TEST_DEVICE].pending_entry);
    TEST_ASSERT_TRUE(dev_ctl[TEST_DEVICE].pending_stop);
    TEST_ASSERT_FALSE(dev_ctl[TEST_DEVICE].is_playing);
}

// チェックサム異常を通信エラーとして扱うことをテストする。
void test_verification_status_invalid_checksum() {
    // 入力: チェックサムだけが不正な受信状態。
    // 過程: verification_status()で受信パケット全体の合計を確認する。
    // 出力: 通信エラー回数が1増え、カエル状態は更新されない。
    log_input("受信状態のチェックサムを正常値から1変更する");

    ControlCommand cmd{};
    cmd.sequence = 13;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.frog_state = 0x01;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);
    status.checksum++;

    log_process("受信パケットの全バイト合計が0か確認する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("パケット合計値=", packet_sum(&status, sizeof(status)));
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);
    log_output("カエル状態=", dev_ctl[TEST_DEVICE].frog_state);

    TEST_ASSERT_NOT_EQUAL(0, packet_sum(&status, sizeof(status)));
    TEST_ASSERT_EQUAL_UINT8(1, dev_ctl[TEST_DEVICE].error_count);
    TEST_ASSERT_EQUAL_UINT8(0x00, dev_ctl[TEST_DEVICE].frog_state);
}

// 通信相手と異なる楽器IDを通信エラーとして扱うことをテストする。
void test_verification_status_wrong_instrument_id() {
    // 入力: 通信対象とは異なる範囲内の楽器ID。
    // 過程: verification_status()で通信対象番号と受信IDを比較する。
    // 出力: 通信エラー回数が1増える。
    log_input("通信対象とは異なる楽器ID");

    ControlCommand cmd{};
    cmd.sequence = 14;

    InstrumentStatus status{};
    status.instrument_id = (TEST_DEVICE + 1) % 4;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    log_process("受信した楽器IDが通信対象番号と一致するか確認する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("受信した楽器ID=", status.instrument_id);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(1, dev_ctl[TEST_DEVICE].error_count);
}

// 範囲外の楽器IDを通信エラーとして扱うことをテストする。
void test_verification_status_out_of_range_instrument_id() {
    // 入力: 有効範囲0～3を超える楽器ID 4。
    // 過程: verification_status()で受信IDの範囲を確認する。
    // 出力: 通信エラー回数が1増える。
    log_input("有効範囲外の楽器ID=4");

    ControlCommand cmd{};
    cmd.sequence = 15;

    InstrumentStatus status{};
    status.instrument_id = 4;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    log_process("受信した楽器IDが0～3の範囲内か確認する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("受信した楽器ID=", status.instrument_id);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(1, dev_ctl[TEST_DEVICE].error_count);
}

// シーケンス番号の不一致を通信エラーとして扱うことをテストする。
void test_verification_status_wrong_sequence() {
    // 入力: 送信番号10に対して応答番号11の受信状態。
    // 過程: verification_status()で送信番号と応答番号を比較する。
    // 出力: 通信エラー回数が1増える。
    log_input("送信シーケンス番号=10、応答シーケンス番号=11");

    ControlCommand cmd{};
    cmd.sequence = 10;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.sequence_ack = 11;
    status.ack_ok = 0x01;
    set_status_checksum(status);

    log_process("送信シーケンス番号と応答シーケンス番号を比較する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("送信シーケンス番号=", cmd.sequence);
    log_output("応答シーケンス番号=", status.sequence_ack);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(1, dev_ctl[TEST_DEVICE].error_count);
}

// ACK異常を通信エラーとして扱うことをテストする。
void test_verification_status_invalid_ack() {
    // 入力: ack_okが正常値1ではなく0の受信状態。
    // 過程: verification_status()でスレーブのコマンド処理結果を確認する。
    // 出力: 通信エラー回数が1増える。
    log_input("ACK処理結果=0");

    ControlCommand cmd{};
    cmd.sequence = 16;

    InstrumentStatus status{};
    status.instrument_id = TEST_DEVICE;
    status.sequence_ack = cmd.sequence;
    status.ack_ok = 0x00;
    set_status_checksum(status);

    log_process("ACK処理結果が正常値1か確認する");
    verification_status(TEST_DEVICE, cmd, status);

    log_output("ACK処理結果=", status.ack_ok);
    log_output("通信エラー回数=", dev_ctl[TEST_DEVICE].error_count);

    TEST_ASSERT_EQUAL_UINT8(1, dev_ctl[TEST_DEVICE].error_count);
}

void test_command_generate_bpm_update(){
    ControlCommand cmd;
    int command=0x04;
    log_input("command=0x04");
    generate_cmd(command,cmd);
    log_process("payloadと内部BPMが一致するか確認する");
    log_output("command=",cmd.payload);
    log_output("BPM=",bpm);
    log_output("payload=",cmd.payload);
    TEST_ASSERT_EQUAL_UINT16(bpm,cmd.payload);
}
// control_command.inoに関する全テストを実行する。
void run_control_command_tests() {
    RUN_TEST(test_spi_packet_layout_and_checksum);
    RUN_TEST(test_verification_status_valid_response);
    RUN_TEST(test_verification_status_frog_placed);
    RUN_TEST(test_verification_status_frog_removed);
    RUN_TEST(test_verification_status_invalid_checksum);
    RUN_TEST(test_verification_status_wrong_instrument_id);
    RUN_TEST(test_verification_status_out_of_range_instrument_id);
    RUN_TEST(test_verification_status_wrong_sequence);
    RUN_TEST(test_verification_status_invalid_ack);
    RUN_TEST(test_command_generate_bpm_update);
}

#endif
