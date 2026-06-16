#ifndef SPI_MASTER_TEST_H
#define SPI_MASTER_TEST_H

// spi_master.inoに対するテストをこのファイルへ記述する。

// SPI初期化後の各ピンの状態を確認する。
void test_spi_initial_pin_states() {
    // 入力: 製品仕様のSPI設定（1MHz、MSB first、MODE0）。
    // 過程: spi_setup()でSPIと5本の制御ピンを初期化する。
    // 出力: 各楽器CSは非選択のHIGH、同期信号CS_SYNCはLOWになる。
    log_input("SPI通信設定は1MHz、上位ビット先行、モード0");

    spi_setup();

    log_process("各CSピンがHIGH、同期ピンがLOWになっているか確認する");
    log_output("楽器1のCSピン=", digitalRead(CS_DEV1));
    log_output("楽器2のCSピン=", digitalRead(CS_DEV2));
    log_output("楽器3のCSピン=", digitalRead(CS_DEV3));
    log_output("楽器4のCSピン=", digitalRead(CS_DEV4));
    log_output("同期ピン=", digitalRead(CS_SYNC));

    TEST_ASSERT_EQUAL_INT(HIGH, digitalRead(CS_DEV1));
    TEST_ASSERT_EQUAL_INT(HIGH, digitalRead(CS_DEV2));
    TEST_ASSERT_EQUAL_INT(HIGH, digitalRead(CS_DEV3));
    TEST_ASSERT_EQUAL_INT(HIGH, digitalRead(CS_DEV4));
    TEST_ASSERT_EQUAL_INT(LOW, digitalRead(CS_SYNC));
}

// spi_master.inoに関する全テストを実行する。
void run_spi_master_tests() {
    RUN_TEST(test_spi_initial_pin_states);
}

#endif
