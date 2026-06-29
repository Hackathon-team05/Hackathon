# Arduino単体テストの実行方法

## 事前準備

PlatformIOとグラフ描画用ライブラリを準備します。

```sh
cd ~/program
python3 -m pip install -r test/tools/requirements.txt
```

Arduino UNO R4 WiFiをUSB接続し、楽器とのSPI配線が必要な試験では対象機器にも
電源を入れてください。

## 全テストの実行

```sh
cd ~/program
python3 test/tools/run_tests.py
```

通常の単体テストだけを実行する場合:

```sh
cd ~/program
python3 test/tools/run_tests.py --suite test_server
```

80 BPMメトロノームを使用した10秒間のマイク試験だけを実行する場合:

```sh
cd ~/program
python3 test/tools/run_tests.py --suite test_mic_hardware
```

シリアルポートを明示する場合:

```sh
cd ~/program
python3 test/tools/run_tests.py \
  --suite test_mic_hardware \
  --upload-port /dev/cu.usbmodemXXXX \
  --test-port /dev/cu.usbmodemXXXX
```

## 保存される結果

結果は実行ごとに `test/results/YYYYMMDD_HHMMSS/` へ保存されます。

- `test_server.txt`: 通常テストの端末出力
- `test_server.json`: PlatformIOのJSON形式結果
- `test_server.xml`: JUnit XML形式結果
- `test_mic_hardware.txt`: マイク実機試験の端末出力
- `test_mic_hardware.json`: マイク試験のJSON形式結果
- `test_mic_hardware.xml`: マイク試験のJUnit XML形式結果
- `mic_data.csv`: マイク測定値
- `mic_waveform.png`: 5秒間の音波形
- `bpm_transition.png`: BPMの時間変化
- `clap_detection.png`: ピーク追跡状態と累積拍手回数
- `mic_clap_comparison.png`: 音波形と拍手検出を共通時間軸で並べた比較グラフ

過去のログからグラフだけを再生成する場合:

```sh
cd ~/program
python3 test/tools/plot_mic_results.py \
  test/results/YYYYMMDD_HHMMSS/test_mic_hardware.txt
```
