// =============================================================================
// serial_tx.ino  （楽器 Arduino 側・Processing への USB シリアル送信部分のみ）
//
// 役割: 設計書 表 3.8 の score_player から呼ばれる
//         serial_tx_note_on(pitch, velocity)
//         serial_tx_note_off(pitch)
//       を提供し、Processing 側 SerialParser が解釈できる SerialFrame を送出する。
//
// ★ このファイルは「Processing とのシリアル通信部分」だけを担当する。
//    譜面参照(score_player) / SYNC tick(sync_isr) / SPI / EEPROM ID 読み出しは
//    青木くんの担当分のコードに任せ、ここからは serial_tx_note_on/off を呼ぶだけ。
//
// -----------------------------------------------------------------------------
// SerialFrame フォーマット（Processing 側 SerialParser.pde と完全一致させること）
//
//   [START] [TYPE] [LENGTH] [PAYLOAD ...] [CHECKSUM]
//     0xAA   0x90    n         n bytes        XOR(TYPE..PAYLOAD末尾)
//
//   START    : 0xAA 固定（スタートバイト）
//   TYPE     : 0x90 = NOTE_ON / 0x80 = NOTE_OFF
//   LENGTH   : PAYLOAD のバイト数  NOTE_ON→2 / NOTE_OFF→1
//   PAYLOAD  : NOTE_ON  = { pitch, velocity }
//              NOTE_OFF = { pitch }
//   CHECKSUM : TYPE・LENGTH・PAYLOAD 各バイトの XOR（下位 8bit）
//
//   ※ボーレートは Processing 側 config.json の baudRate と一致させる（115200）。
//     setup() に Serial.begin(115200); を入れること（青木くん側の setup と統合）。
// =============================================================================

#include <Arduino.h>

// ----- フレーム定数（SerialParser.pde と一致）-----
static const uint8_t FRAME_START    = 0xAA;
static const uint8_t FRAME_TYPE_ON  = 0x90;  // NOTE_ON
static const uint8_t FRAME_TYPE_OFF = 0x80;  // NOTE_OFF

// ----- 内部ヘルパ: 1 フレームを送出 -----
// type と payload を受け取り、START + TYPE + LENGTH + payload + CHECKSUM を送信する。
static void serial_tx_frame(uint8_t type, const uint8_t* payload, uint8_t length) {
  uint8_t checksum = type ^ length;
  for (uint8_t i = 0; i < length; i++) {
    checksum ^= payload[i];
  }

  Serial.write(FRAME_START);
  Serial.write(type);
  Serial.write(length);
  for (uint8_t i = 0; i < length; i++) {
    Serial.write(payload[i]);
  }
  Serial.write(checksum);
}

// ----- 公開関数: NOTE_ON 送信 -----
// 設計書 表 3.8 score_step() から、local_tick が start_tick に一致したときに呼ばれる。
// payload = { pitch, velocity }
void serial_tx_note_on(uint8_t pitch, uint8_t velocity) {
  uint8_t payload[2] = { pitch, velocity };
  serial_tx_frame(FRAME_TYPE_ON, payload, 2);
}

// ----- 公開関数: NOTE_OFF 送信 -----
// 設計書 表 3.8 score_step()（end_tick 一致時）/ score_stop_all()（STOP 受信時）から呼ばれる。
// payload = { pitch }
void serial_tx_note_off(uint8_t pitch) {
  uint8_t payload[1] = { pitch };
  serial_tx_frame(FRAME_TYPE_OFF, payload, 1);
}

void init_serial_tx() {
  Serial.begin(115200);
}

// =============================================================================
// 単体動作確認用スケッチ（このファイル単体で動かすとき用）
//
//   ボタンを押すたびに「かえるの歌」の送信を 開始/停止 する。
//   再生は millis() ベースのノンブロッキング処理で、delay を使わないため
//   再生中でもボタン入力に即応する。
//
//   青木くんのコードに合流させるときは、すでに setup()/loop() が存在するため、
//   下の SERIAL_TX_STANDALONE_TEST ブロックは 0 にして無効化する（衝突防止）。
//   結合後に必要なのは Serial.begin(115200); と serial_tx_note_on/off の呼び出しだけ。
//   楽譜の進行は本実装では青木くんの score_player（SYNC tick 駆動）が担当する。
// =============================================================================
#define SERIAL_TX_STANDALONE_TEST 0

#if SERIAL_TX_STANDALONE_TEST

// ----- ピン設定 -----
static const int BUTTON_PIN = 4;          // タクトスイッチ（GND との間に接続、INPUT_PULLUP）
                                          // ※ SYNC は D2 を使う設計のため D4 を割り当て
static const int LED_PIN    = LED_BUILTIN; // 再生中に点灯

// ----- かえるの歌の楽譜（Processing 側 instrument_player.pde と一致）-----
// 本実装では PROGMEM に Note{start_tick,end_tick,pitch,velocity} で持つが、
// 単体テストでは pitch + 拍数(beats) の簡易シーケンサとする。
static const uint8_t SONG_PITCHES[] = {
  60, 62, 64, 65, 64, 62, 60, // かえるのうたが
  64, 65, 67, 69, 67, 65, 64, // きこえてくるよ
  60, 60, 60, 60,  // クワクワクワクワ
  60, 60, 62, 62, 64, 64, 65, 65, 64, 62, 60 // ケロケロケロケロ
};
static const float SONG_BEATS[] = {       // 各音符の長さ（拍）
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  1.0, 1.0, 1.0, 1.0,
  0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.5, 0.5, 1.0
};
static const int   SONG_LENGTH   = sizeof(SONG_PITCHES) / sizeof(SONG_PITCHES[0]);
static const float SONG_BPM      = 60.0;
static const uint8_t SONG_VELOCITY = 100;

// ----- 再生状態 -----
static bool          songPlaying      = false;
static int           songIndex        = -1;   // 現在発音中の音符インデックス
static unsigned long songNextEventMs  = 0;    // 次の音符イベント時刻
static bool          noteSounding     = false; // 現在 NOTE_ON 済みか

// ----- ボタンのデバウンス -----
static int           lastButtonReading = HIGH;
static int           buttonState       = HIGH;
static unsigned long lastDebounceMs    = 0;
static const unsigned long DEBOUNCE_MS = 50;

static unsigned long beatToMs(float beats) {
  return (unsigned long)(beats * (60000.0 / SONG_BPM));
}

// 再生開始: 先頭から
static void songStart() {
  songPlaying     = true;
  songIndex       = -1;
  noteSounding    = false;
  songNextEventMs = millis();
}

// 再生停止: 鳴っている音があれば NOTE_OFF を送って止める
static void songStop() {
  if (noteSounding && songIndex >= 0 && songIndex < SONG_LENGTH) {
    serial_tx_note_off(SONG_PITCHES[songIndex]);
  }
  songPlaying  = false;
  songIndex    = -1;
  noteSounding = false;
}

// 再生中の進行（毎ループ呼び出し・ノンブロッキング）
static void songUpdate() {
  if (!songPlaying) return;
  if ((long)(millis() - songNextEventMs) < 0) return;  // まだ次イベントの時刻でない

  // 直前に鳴らした音を止める
  if (noteSounding && songIndex >= 0) {
    serial_tx_note_off(SONG_PITCHES[songIndex]);
    noteSounding = false;
  }

  songIndex++;
  if (songIndex >= SONG_LENGTH) {
    songStop();   // 曲末で自動停止
    return;
  }

  // 次の音を鳴らす
  serial_tx_note_on(SONG_PITCHES[songIndex], SONG_VELOCITY);
  noteSounding    = true;
  songNextEventMs = millis() + beatToMs(SONG_BEATS[songIndex]);
}

// ボタン押下（HIGH→LOW エッジ）でトグル
static void buttonUpdate() {
  int reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonReading) {
    lastDebounceMs = millis();   // 状態が変わったらデバウンスタイマをリセット
  }

  if (millis() - lastDebounceMs > DEBOUNCE_MS) {
    if (reading != buttonState) {
      buttonState = reading;
      // INPUT_PULLUP なので押下＝LOW。立ち下がりで開始/停止をトグル
      if (buttonState == LOW) {
        if (songPlaying) songStop();
        else             songStart();
      }
    }
  }

  lastButtonReading = reading;
}

//void setup() {
//  Serial.begin(115200);
//  pinMode(BUTTON_PIN, INPUT_PULLUP);
//  pinMode(LED_PIN, OUTPUT);
//  digitalWrite(LED_PIN, LOW);
//  delay(500);
//}

void loop() {
  buttonUpdate();
  songUpdate();
  digitalWrite(LED_PIN, songPlaying ? HIGH : LOW);  // 再生中は点灯
}

#endif
