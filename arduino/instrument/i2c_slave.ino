// i2c_slave.ino
// 【役割】サーバArduino(I2Cマスタ)との通信をスレーブ側として処理するファイル。
// 旧 spi_slave.ino からの変更点はここに集約されている。
//
// 【変更理由】
// Arduino UNO R4 WiFi (Renesas RA4M1) は、Arduino核心の SPI ライブラリが
// マスター動作専用であり、ハードウェアSPIスレーブ動作をサポートしていない。
// 旧 spi_slave.ino はこれを digitalRead/digitalWrite によるソフトウェアSPI
// エミュレーションで回避していたが、タイミング制約が厳しく不安定要因になりやすい。
// 一方、UNO R4 WiFi の Wire ライブラリ(I2C)はハードウェアスレーブ動作に対応して
// いるため、本ファイルでは通信方式をSPIからI2Cへ置き換える。
//
// 【維持した点(指示により変更していない部分)】
// ・ControlCommand / InstrumentStatus の構造体定義（フィールド名・サイズとも）
// ・CommandType の値(CMD_STATUS_POLL〜CMD_BPM_UPDATE)、CMD_CONNECT・ACK_OKの値
// ・is_playing, local_tick, frog_state, instrument_id など、他ファイル
//   (instrument.ino, score_player.ino, pressure_sensor.ino等)が参照する変数
// ・チェックサム計算方法(8bitの2の補数チェックサム)
// ・「重い処理は割り込みの外(loop側)で行う」という設計方針
//   (process_pending_command()に処理を委譲する流れは変更していない)
//
// 【ControlCommand/InstrumentStatusのやり取り方法の変更点】
// SPI版は1回のCS LOW区間内でControlCommand送信とInstrumentStatus返送を
// 同時(全二重)に行っていたが、I2Cは半二重のため以下の2フェーズに分かれる。
//   ① マスターがWire.beginTransmission(addr)+write(5バイト)+endTransmission()
//      でControlCommandを送信 → スレーブ側 on_i2c_receive() が呼ばれる
//   ② マスターがWire.requestFrom(addr, 5)でInstrumentStatusを読み出す
//      → スレーブ側 on_i2c_request() が呼ばれる
// この①②はサーバー側(マスター)で連続して呼ぶことで、見た目上は元のSPI版の
// 1トランザクションと同等の意味になる。サーバー側の実装(spi_master.ino等)も
// 合わせてI2Cマスター実装に変更する必要があるが、本ファイルはアップロードされた
// 楽器Arduino側のみを対象としている。
#include "score_data.h"
#include <Wire.h>

// ============================================================================
// 【配線】I2CはSDA/SCLの2線を、サーバArduinoと全楽器Arduinoで共有するバス方式。
// UNO R4 WiFiのハードウェアI2Cピンは固定（基板のSDA/SCL用ピン、A4/A5と共通の
// バスに接続されている）であり、ソフトウェアSPIのように自由なピンへの割り当ては
// 行わない（Wire.begin()が内部で固定ピンを使用する）。
// これにより、旧SPI版で必要だった個別のCSピン(D10-D13)・プルアップ抵抗が不要になる。
// 代わりに、SDA/SCLそれぞれにバス全体で1か所だけ プルアップ抵抗(4.7kΩ程度) を
// 入れること(複数台つける必要はない)。GNDは全Arduinoで共通化する。
// ============================================================================

// 楽器ごとのI2Cアドレスは「ベースアドレス + instrument_id」で自動的に決まる。
// instrument_id(0〜3)はEEPROMから読み出されるため、4台とも同一ファームウェアの
// まま、CSピンのような配線分岐をせずに各楽器を一意に識別できる。
const uint8_t I2C_BASE_ADDRESS = 0x10; // 4台で 0x10〜0x13 を使用する

// コマンド種別 (旧spi_slave.inoのCommandTypeから値を変更していない。
// server.ino側のgenerate_cmd()と対応する)
enum CommandType : uint8_t {
    CMD_STATUS_POLL = 0x00, // 圧力センサ監視用ポーリング
    CMD_PLAY        = 0x01,
    CMD_STOP        = 0x02,
    CMD_ENTRY_CUE   = 0x03,
    CMD_BPM_UPDATE  = 0x04
};

// server.ino の wait_ack() ハンドシェイクと対応する値(旧spi_slave.inoから変更なし)
const uint8_t CMD_CONNECT = 100;
const uint8_t ACK_OK      = 200;

// server.ino の ControlCommand と完全一致させる (5バイト, packed) ※変更なし
struct __attribute__((packed)) ControlCommand {
    uint8_t  command_type;
    uint16_t payload;
    uint8_t  sequence;
    uint8_t  checksum;
};

// server.ino の InstrumentStatus と完全一致させる (5バイト, packed) ※変更なし
struct __attribute__((packed)) InstrumentStatus {
    uint8_t instrument_id;
    uint8_t frog_state;
    uint8_t sequence_ack;
    uint8_t ack_ok;
    uint8_t checksum;
};

extern volatile bool is_playing;
extern volatile uint16_t local_tick;
extern uint8_t frog_state;
extern void score_init(uint8_t instrument_id);
extern void score_stop_all();
extern uint8_t get_instrument_id();

// 送信するInstrumentStatusのバッファ。通信中に随時更新される。
// [0]=instrument_id [1]=frog_state [2]=sequence_ack(前回値) [3]=ack_ok [4]=checksum
volatile uint8_t i2c_tx_buffer[sizeof(InstrumentStatus)];

// 直前に正常受信できたControlCommandのsequence。
// 旧SPI版と同じく、「今回受信したsequenceをそのまま今回の応答に載せる」のではなく、
// 一度 prepare_tx_buffer_head() を経由してから次回の応答に反映する設計を維持する
// (サーバー側 verification_status() の比較ロジックを変更しない前提のため)。
volatile uint8_t last_acked_sequence = 0;

// ----------------------------------------------------------------------------
// 【可視化用】受信したControlCommandの内容をSerialへ出力するためのログ情報。
// 割り込みハンドラ(on_i2c_receive)内ではSerial.print()を直接呼ばず、
// ここに値を保存するだけにとどめ、実際の出力はloop()側で行う
// (割り込み内でのSerial通信はバッファロック等の影響で不安定になりやすいため)。
// ----------------------------------------------------------------------------
volatile bool has_log_entry = false;
volatile uint8_t log_command_type = 0;
volatile uint16_t log_payload = 0;
volatile uint8_t log_sequence = 0;
volatile bool log_checksum_ok = false;

// 【指摘4対応の継続】I2C割り込み内では「どのコマンドが来たか」を記録するだけにし、
// score_init/score_stop_allなどの重い処理は loop() 側の process_pending_command()
// で実行する。これにより割り込みハンドラの実行時間を最小化し、他の割り込み
// (SYNC tick等)への影響を抑える。
volatile bool has_pending_command = false;
volatile uint8_t pending_command_type = 0;
volatile uint16_t pending_payload = 0;

// ハンドシェイク(CMD_CONNECT)を受信した直後、次のonRequestで
// InstrumentStatus(5バイト)ではなくACK_OK(1バイト)を返すためのフラグ。
volatile bool handshake_pending = false;

// ----------------------------------------------------------------------------
// loop() 側から毎周期呼ばれる、保留中コマンドの実処理。(内容は旧spi_slave.inoから変更なし)
// score_init/score_stop_allなど時間のかかる処理はここに集約し、
// 割り込みハンドラ(on_i2c_receive)からは絶対に呼ばない。
// ----------------------------------------------------------------------------
void process_pending_command() {
    if (!has_pending_command) return;

    uint8_t command_type;
    uint16_t payload;

    // 割り込みと競合しないよう、値を取り出す間だけ割り込みを止める
    noInterrupts();
    command_type = pending_command_type;
    payload = pending_payload;
    has_pending_command = false;
    interrupts();

    switch (command_type) {
        case CMD_STATUS_POLL:
            // 圧力センサ監視用ポーリング。状態更新のみで、再生制御は行わない。
            break;
        case CMD_PLAY:
            is_playing = true;
            break;
        case CMD_STOP:
            is_playing = false;
            score_stop_all(); // アクティブな全音符にNOTE_OFFを発行して停止
            break;
        case CMD_ENTRY_CUE:
            noInterrupts();
            local_tick = 0; // 自パート譜面の先頭から演奏を開始するためリセット
            interrupts();
            score_init(get_instrument_id());
            is_playing = true;
            break;
        case CMD_BPM_UPDATE:
            // payload に新しいBPMが入る想定。
            // 楽器側はBPM値そのものを保持しない設計のため、現状は受信のみ。
            (void)payload;
            break;
        default:
            break;
    }
}

// ----------------------------------------------------------------------------
// instrument_id・frog_state・sequence_ack(前回値)を送信バッファの先頭3バイトへ
// 反映させる。I2Cでは「直前の応答(onRequest)が終わった直後」に呼び出すことで、
// 次回の通信に向けて値を更新しておく(旧SPI版のon_cs_falling末尾と同じ役割)。
// ----------------------------------------------------------------------------
void prepare_tx_buffer_head() {
    i2c_tx_buffer[0] = get_instrument_id();
    i2c_tx_buffer[1] = frog_state;
    i2c_tx_buffer[2] = last_acked_sequence; // 前回受理したsequence(旧SPI版と同じ「1回遅れ」の仕様を維持)
}

// ----------------------------------------------------------------------------
// ControlCommand(5バイト)のチェックサムを検証し、ack_ok・checksumを確定する。
// (内容は旧spi_slave.inoから変更なし。呼び出しタイミングのみI2C用に調整)
// ----------------------------------------------------------------------------
void finalize_tx_buffer_tail(const uint8_t* raw_rx) {
    uint8_t sum = 0;
    for (uint8_t i = 0; i < sizeof(ControlCommand); i++) sum += raw_rx[i];
    bool checksum_ok = (sum == 0);

    i2c_tx_buffer[3] = checksum_ok ? 0x01 : 0x00; // ack_ok: 今回コマンドの検証結果

    ControlCommand cmd;
    memcpy(&cmd, raw_rx, sizeof(ControlCommand));

    // 【可視化用】受信内容をログ用変数に記録する(Serial.printはloop()側で行う)
    log_command_type = cmd.command_type;
    log_payload = cmd.payload;
    log_sequence = cmd.sequence;
    log_checksum_ok = checksum_ok;
    has_log_entry = true;

    if (checksum_ok) {
        // 次回の通信でsequence_ackとして返すため記録する(今回の応答にはまだ反映しない)
        last_acked_sequence = cmd.sequence;

        // 実コマンド処理はloop()側へ予約するだけ(指摘4対応、重い処理を割り込み外に出す)
        pending_command_type = cmd.command_type;
        pending_payload = cmd.payload;
        has_pending_command = true;
    }
    // checksum不一致の場合はlast_acked_sequenceを更新しない
    // (= 次回もまだ「直前に成功した値」を返し続ける)

    // InstrumentStatus全体(checksum以外の4バイト)の和から、2の補数チェックサムを計算
    uint8_t out_sum = 0;
    for (uint8_t i = 0; i < sizeof(InstrumentStatus) - 1; i++) out_sum += i2c_tx_buffer[i];
    i2c_tx_buffer[4] = (uint8_t)(0 - out_sum);
}

// ----------------------------------------------------------------------------
// 【可視化用】I2Cで受信したControlCommandの内容をSerialへ出力する。
// loop()側から毎周期呼ばれる。割り込み(on_i2c_receive)内では値の保存のみを
// 行い、実際のSerial.print()はここ(通常コンテキスト)でまとめて行う。
// ----------------------------------------------------------------------------
void print_i2c_log() {
    if (!has_log_entry) return;

    uint8_t command_type;
    uint16_t payload;
    uint8_t sequence;
    bool checksum_ok;

    noInterrupts();
    command_type = log_command_type;
    payload = log_payload;
    sequence = log_sequence;
    checksum_ok = log_checksum_ok;
    has_log_entry = false;
    interrupts();

    if (command_type == CMD_CONNECT) {
        // 【ハンドシェイク専用ログ】起動時のwait_ack()応答が来たことを明示する。
        Serial.println(F("[HANDSHAKE] CMD_CONNECT received -> replied ACK_OK"));
        return;
    }

    Serial.print(F("[I2C RX] cmd="));
    switch (command_type) {
        case CMD_STATUS_POLL: Serial.print(F("STATUS_POLL")); break;
        case CMD_PLAY:        Serial.print(F("PLAY")); break;
        case CMD_STOP:        Serial.print(F("STOP")); break;
        case CMD_ENTRY_CUE:   Serial.print(F("ENTRY_CUE")); break;
        case CMD_BPM_UPDATE:  Serial.print(F("BPM_UPDATE")); break;
        default:              Serial.print(command_type); break;
    }
    Serial.print(F(" payload="));
    Serial.print(payload);
    Serial.print(F(" seq="));
    Serial.print(sequence);
    Serial.print(F(" checksum="));
    Serial.println(checksum_ok ? F("OK") : F("NG"));
}

// ----------------------------------------------------------------------------
// 起動直後など、ControlCommandを介さない場面用の初期送信バッファ作成。
// (内容は旧spi_slave.inoから変更なし)
// ----------------------------------------------------------------------------
void prepare_tx_buffer_idle() {
    prepare_tx_buffer_head();
    i2c_tx_buffer[3] = 0x01; // 初期状態は異常なしとして扱う

    uint8_t sum = 0;
    for (uint8_t i = 0; i < sizeof(InstrumentStatus) - 1; i++) sum += i2c_tx_buffer[i];
    i2c_tx_buffer[4] = (uint8_t)(0 - sum);
}

// ----------------------------------------------------------------------------
// Wireライブラリの onReceive ハンドラ。
// マスターがWire.beginTransmission(addr)+write(...)+endTransmission()で
// ControlCommand(5バイト)、またはハンドシェイク用の1バイト(CMD_CONNECT)を
// 送信してきたときに呼ばれる(I2Cハードウェア割り込みから起動される)。
//
// 【旧SPI版の指摘1(MISO駆動切替)について】
// I2Cはオープンドレイン方式の共有バスであり、Wireライブラリがバスの占有/解放を
// 自動的に行うため、ソフトウェアSPIで必要だったMISOのOUTPUT/INPUT切り替えに
// 相当する処理は不要(対応不要)。
//
// 【指摘4対応の継続】ここでも重い処理(score_init等)は行わず、フラグを立てて
// loop()側のprocess_pending_command()に委譲する。
// ----------------------------------------------------------------------------
void on_i2c_receive(int num_bytes) {
    uint8_t buf[sizeof(ControlCommand)];
    uint8_t n = 0;

    while (Wire.available()) {
        uint8_t b = Wire.read();
        if (n < sizeof(buf)) {
            buf[n] = b;
        }
        n++;
    }

    if (n == 1 && buf[0] == CMD_CONNECT) {
        // 【ハンドシェイク経路】次のonRequestでACK_OK(1バイト)を返す
        handshake_pending = true;

        // 【可視化用】ハンドシェイク受信もログに残す(command_type=CMD_CONNECTとして記録)
        log_command_type = CMD_CONNECT;
        log_payload = 0;
        log_sequence = 0;
        log_checksum_ok = true; // ハンドシェイクにチェックサムは無いため常にtrue扱い
        has_log_entry = true;
        return;
    }

    if (n != sizeof(ControlCommand)) {
        // 想定外のバイト数(ノイズ・通信ミス等)。今回の受信は破棄する。
        return;
    }

    // ここでbuf[0..4] = ControlCommandの5バイトが揃った。
    finalize_tx_buffer_tail(buf); // i2c_tx_buffer[3](ack_ok), [4](checksum)を確定
}

// ----------------------------------------------------------------------------
// Wireライブラリの onRequest ハンドラ。
// マスターがWire.requestFrom(addr, n)を呼んだときに呼ばれる。
// ハンドシェイク直後はACK_OK(1バイト)、それ以外はInstrumentStatus(5バイト)を返す。
// ----------------------------------------------------------------------------
void on_i2c_request() {
    if (handshake_pending) {
        handshake_pending = false;
        Wire.write(ACK_OK);
        return;
    }

    uint8_t tmp[sizeof(InstrumentStatus)];
    memcpy(tmp, (const void*)i2c_tx_buffer, sizeof(tmp));
    Wire.write(tmp, sizeof(tmp));

    // 今回の応答送出が完了したので、次回の通信に向けてhead(instrument_id/
    // frog_state/sequence_ack)を更新しておく。旧SPI版のon_cs_falling末尾と
    // 同じタイミング(=このタイミングで更新することで、sequence_ackが
    // 「直前に成功したsequence」を返す1回遅れの挙動を維持する)。
    prepare_tx_buffer_head();
}



// ----------------------------------------------------------------------------
// I2Cスレーブの初期化。
// instrument_id_init() が先に実行済みであること(=get_instrument_id()が
// 正しい値を返すこと)が前提(instrument.ino の setup() 内の呼び出し順序を維持)。
// ----------------------------------------------------------------------------
void init_i2c_slave() {
    uint8_t addr = (uint8_t)(I2C_BASE_ADDRESS + get_instrument_id());

    Wire.begin(addr);
    Wire.onReceive(on_i2c_receive);
    Wire.onRequest(on_i2c_request);

    prepare_tx_buffer_idle(); // 起動直後の送信バッファを用意

    Serial.print(F("[BOOT] I2C slave address = 0x"));
    Serial.println(addr, HEX);
}
