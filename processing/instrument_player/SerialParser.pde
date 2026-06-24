// =============================================================================
// SerialParser.pde
// 設計書 3.3.2 オブジェクト設計（図 3.3 クラス図・表 3.4）／3.3.3 関数設計（表 3.5）に準拠
//
// 楽器 Arduino から USB シリアルで送られる SerialFrame（バイナリフレーム）を、
// バイトストリームから状態機械でパースするクラス群。
//
// 設計書で定義されたクラス:
//   SerialFrame : { int type, int length, byte[] payload, int checksum }
//   SerialParser: { byte[] receiveBuffer, ParseState parseState, SerialFrame parseFrame(byte[]) }
//   ParseState  : WaitingStart / ReadingLength / ReadingPayload / ReadingChecksum
//   SoundData   : { int instrument_id, int pitch, int velocity }  ※NOTE_ON の payload 解析結果
//
// 関数設計（表 3.5）:
//   parseFrame()  : スタートバイト検出・長さ確認・チェックサム検証を行い、フレームを返す
//   handleFrame() : type とペイロード値を検証し、NOTE_ON/NOTE_OFF をキューへ追加。
//                   不正フレームは破棄してログに残す
//
// -----------------------------------------------------------------------------
// フレームフォーマット（バイナリ）
//   [START] [TYPE] [LENGTH] [PAYLOAD ...] [CHECKSUM]
//     0xAA   0x90    n        n bytes        XOR(TYPE..PAYLOAD末尾)
//
//   START    : 0xAA 固定（スタートバイト）
//   TYPE     : 0x90 = NOTE_ON / 0x80 = NOTE_OFF（MIDI 慣例に準拠）
//   LENGTH   : PAYLOAD のバイト数
//                NOTE_ON  → 2（pitch, velocity）
//                NOTE_OFF → 1（pitch）
//   PAYLOAD  : NOTE_ON  = {pitch, velocity}
//              NOTE_OFF = {pitch}
//   CHECKSUM : TYPE・LENGTH・PAYLOAD 各バイトの XOR
//
//   ※スタートバイト値・チェックサム方式は設計書に数値レベルの記載がないため、
//     楽器 Arduino 側 serial_tx_note_on()/serial_tx_note_off()（表 3.8）と
//     合意のうえ上記で確定する。変更時は本ヘッダと Arduino 側を揃えること。
// =============================================================================

// フレーム定数
final int FRAME_START      = 0xAA;
final int FRAME_TYPE_ON    = 0x90;  // NOTE_ON
final int FRAME_TYPE_OFF   = 0x80;  // NOTE_OFF
final int FRAME_MAX_PAYLOAD = 8;    // payload の上限（異常長フレームの暴走防止）

// ===== ParseState（設計書 ParseState 列挙に対応）=====
static final int STATE_WAITING_START    = 0;
static final int STATE_READING_TYPE     = 1;
static final int STATE_READING_LENGTH   = 2;
static final int STATE_READING_PAYLOAD  = 3;
static final int STATE_READING_CHECKSUM = 4;

// ===== SerialFrame（設計書 表 3.4）=====
// type で NOTE_ON / NOTE_OFF を区別。NOTE_ON は payload に SoundData、
// NOTE_OFF は payload に pitch を格納する。
class SerialFrame {
  int    type;
  int    length;
  int[]  payload;
  int    checksum;

  SerialFrame(int type, int length, int[] payload, int checksum) {
    this.type     = type;
    this.length   = length;
    this.payload  = payload;
    this.checksum = checksum;
  }

  boolean isNoteOn()  { return type == FRAME_TYPE_ON; }
  boolean isNoteOff() { return type == FRAME_TYPE_OFF; }
}

// ===== SoundData（設計書 表 3.4）=====
// NOTE_ON 時の発音に必要なノート番号と音量を保持する。音価情報は持たない。
// instrument_id は将来拡張用（現状フレームには含めず -1 を保持）。
class SoundData {
  int instrument_id;
  int pitch;
  int velocity;

  SoundData(int pitch, int velocity) {
    this.instrument_id = -1;
    this.pitch         = pitch;
    this.velocity      = velocity;
  }
}

// ===== SerialParser（設計書 図 3.3）=====
// バイト列から有効な SerialFrame を生成する。状態機械で 1 バイトずつ処理し、
// スタートバイト未検出・長さ不正・チェックサム不一致のフレームは null を返して破棄。
//
// テキストフレーム仕様（parseTextLine）:
//   "pitch,velocity\n" → NOTE_ON  （2フィールド、velocity > 0）
//   "pitch,0\n"        → NOTE_OFF （velocity = 0）
//   "pitch\n"          → NOTE_OFF （1フィールド）
class SerialParser {
  int   parseState = STATE_WAITING_START;  // 設計書 ParseState

  // 解析途中の作業領域
  private int   curType;
  private int   curLength;
  private int[] curPayload;
  private int   payloadIndex;
  private int   runningChecksum;  // TYPE..PAYLOAD の XOR を逐次計算

  // 統計（TPM：不正フレーム検出数）
  int invalidFrameCount = 0;

  // 設計書 図 3.3 のシグネチャ: SerialFrame parseFrame(byte[] bytes)
  // 受信バイト列をまとめて与え、最初に完成した有効フレームを返す。
  // 完成フレームが無ければ null（残りバイトは状態として保持され次回に継続）。
  SerialFrame parseFrame(byte[] bytes) {
    SerialFrame result = null;
    for (int i = 0; i < bytes.length; i++) {
      SerialFrame f = pushByte(bytes[i] & 0xFF);
      if (f != null && result == null) result = f;
    }
    return result;
  }

  // 受信した 1 バイトを与え、フレームが完成したらそれを返す。
  // 未完成・不正の場合は null を返す（呼び出し側はループで全バイトを供給する）。
  // serialEvent() からは 1 バイト到着ごとにこちらを直接呼ぶ。
  SerialFrame pushByte(int b) {
    b &= 0xFF;  // 符号拡張を除去（byte→int 0..255）

    switch (parseState) {

      case STATE_WAITING_START:
        if (b == FRAME_START) {
          parseState      = STATE_READING_TYPE;
          runningChecksum = 0;
        }
        // スタートバイト以外は読み飛ばす（同期回復）
        return null;

      case STATE_READING_TYPE:
        if (b != FRAME_TYPE_ON && b != FRAME_TYPE_OFF) {
          // 不正 type。破棄して同期からやり直す
          invalidFrameCount++;
          println("SerialParser: invalid type=0x" + hex(b, 2) + " -> resync");
          parseState = STATE_WAITING_START;
          return null;
        }
        curType          = b;
        runningChecksum ^= b;
        parseState       = STATE_READING_LENGTH;
        return null;

      case STATE_READING_LENGTH:
        if (b < 1 || b > FRAME_MAX_PAYLOAD) {
          invalidFrameCount++;
          println("SerialParser: invalid length=" + b + " -> resync");
          parseState = STATE_WAITING_START;
          return null;
        }
        curLength        = b;
        runningChecksum ^= b;
        curPayload       = new int[curLength];
        payloadIndex     = 0;
        parseState       = STATE_READING_PAYLOAD;
        return null;

      case STATE_READING_PAYLOAD:
        curPayload[payloadIndex] = b;
        runningChecksum ^= b;
        payloadIndex++;
        if (payloadIndex >= curLength) {
          parseState = STATE_READING_CHECKSUM;
        }
        return null;

      case STATE_READING_CHECKSUM:
        parseState = STATE_WAITING_START;  // 1 フレーム完了。次は必ず START 待ちへ
        if (b != (runningChecksum & 0xFF)) {
          invalidFrameCount++;
          println("SerialParser: checksum mismatch expected=0x"
                  + hex(runningChecksum & 0xFF, 2) + " got=0x" + hex(b, 2) + " -> drop");
          return null;
        }
        // 検証 OK。完成フレームを返す
        return new SerialFrame(curType, curLength, curPayload, b);
    }

    return null;
  }

  // テキストフレームを解析して SerialFrame を返す。
  // 形式: "pitch,velocity" → NOTE_ON  / "pitch" または velocity=0 → NOTE_OFF
  // 不正な行は null を返す。
  SerialFrame parseTextLine(String line) {
    line = trim(line);
    if (line.length() == 0) return null;

    String[] fields = split(line, ',');

    if (fields.length == 2) {
      int pitch    = parseTextInt(fields[0]);
      int velocity = parseTextInt(fields[1]);
      if (pitch < 0 || velocity < 0) {
        invalidFrameCount++;
        println("SerialParser: invalid text NOTE_ON: " + line);
        return null;
      }
      if (velocity == 0) {
        return new SerialFrame(FRAME_TYPE_OFF, 1, new int[]{ pitch }, 0);
      }
      return new SerialFrame(FRAME_TYPE_ON, 2, new int[]{ pitch, velocity }, 0);

    } else if (fields.length == 1) {
      int pitch = parseTextInt(fields[0]);
      if (pitch < 0) {
        invalidFrameCount++;
        println("SerialParser: invalid text NOTE_OFF: " + line);
        return null;
      }
      return new SerialFrame(FRAME_TYPE_OFF, 1, new int[]{ pitch }, 0);

    } else {
      invalidFrameCount++;
      println("SerialParser: invalid text frame: " + line);
      return null;
    }
  }

  // 文字列を 0〜127 の整数に変換。範囲外・非数値は -1 を返す。
  private int parseTextInt(String s) {
    s = trim(s);
    if (s.length() == 0) return -1;
    for (int i = 0; i < s.length(); i++) {
      if (s.charAt(i) < '0' || s.charAt(i) > '9') return -1;
    }
    int v = int(s);
    return (v >= 0 && v <= 127) ? v : -1;
  }
}
