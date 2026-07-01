#ifndef TEST_CONFIG_H
#define TEST_CONFIG_H

// =============================================================================
// テスト / 本番モード切替スイッチ
//
//   TEST_BUTTON_MODE 0 : 【本番モード（既定）】
//       I2C でサーバと連携し、SYNC tick 駆動で score_data.h の楽譜を再生する。
//       instrument.ino の setup()/loop() が動作する（serial.ino のボタンテスト
//       setup()/loop() は無効化される）。サーバー・楽器・Processing を全て繋いだ
//       フル統合テストや本番ではこちら。
//
//   TEST_BUTTON_MODE 1 : 【ボタンテストモード】
//       I2C / サーバ / SYNC を一切使わず、タクトスイッチ(D4)を押すたびに
//       「かえるの歌」を Processing へ USB シリアル送信する。
//       serial.ino(旧serial_tx.ino) の setup()/loop() のみが動作し、
//       instrument.ino の I2C 連携 setup()/loop() は無効化される。
//       ・配線: タクトスイッチを D4–GND（INPUT_PULLUP）、USB を PC へ。
//       ・ピッチ変更: 圧力センサ(A0) の重さで ±1オクターブ（軽い +12 / 重い -12）。
//       ・再生中は オンボード LED(D13) 点灯。
//       ※ このモードのまま本番配線すると I2C 連携が動かないので注意。
// =============================================================================
#define TEST_BUTTON_MODE 0

#endif // TEST_CONFIG_H
