import processing.serial.*;
import ddf.minim.*;
import ddf.minim.ugens.*;
import ddf.minim.effects.*;

Serial port;
Minim minim;
AudioOutput out;

int instrumentId = -1;
String partName = "unknown";
String portName = "";
int baudRate = 115200;

AdditiveSynthEngine melodySynth;
DrumVoice kickVoice;
DrumVoice snareVoice;
DrumVoice hihatVoice;  // クラッシュシンバルとして使用

Summer     crashSum;
HighPassSP crashHPF;

EffectProcessor fx;

SerialParser parser = new SerialParser();

ArrayList<NoteEvent> eventQueue = new ArrayList<NoteEvent>();

String lastMessage = "waiting";

int serialOverflowCount = 0;
PrintWriter latencyLog;

// watchdogテスト用
int  watchdogFiredCount  = 0;
long watchdogLastFiredAt = -1;
long watchdogTestNoteOnAt = -1;

long maxDurationMs = 2000;
long CLARINET_VIBRATO_DELAY_MS = 300;  // クラリネットがビブラートをかけ始めるまでの保持時間

void setup() {
  size(560, 120);

  if (!loadConfig()) {
    exit();
    return;
  }

  if (portName == null || portName.length() == 0) {
    println("Config error: portName is empty.");
    exit();
    return;
  }

  println("Available serial ports:");
  printArray(Serial.list());

  port = new Serial(this, portName, baudRate);
  // バイナリフレームを1バイトずつ SerialParser でパースするため buffer(1) を使用
  port.buffer(1);

  minim = new Minim(this);
  out = minim.getLineOut(Minim.STEREO, 512);

  melodySynth = new AdditiveSynthEngine(instrumentId);
  melodySynth.connect(out);

  fx = new EffectProcessor();
  fx.vibratoEnabled = true;

  // 楽器ごとのビブラート方針
  // フルート(0)   : 常にビブラートをかける
  // クラリネット(1): 伸ばした音にのみ（draw()でノート保持時間を見て遅延適用）
  // トランペット(2): ビブラートなし
  if (instrumentId == 0) {
    fx.vibratoDepth = 0.0060f;
  } else if (instrumentId == 1) {
    fx.vibratoDepth = 0.0060f;
    fx.reverbMix    = 0.15f;
  } else if (instrumentId == 2) {
    fx.vibratoDepth = 0.0000f;
  }
  out.addEffect(fx);

  latencyLog = createWriter("latency.csv");
  latencyLog.println("millis,pitch,latency_ms");
  latencyLog.flush();

  kickVoice  = createKickVoice();
  snareVoice = createSnareVoice();
  hihatVoice = createCrashVoice();  // クラッシュシンバルに変更

  kickVoice.connect(out);
  snareVoice.connect(out);

  // クラッシュは専用 Summer → HighPassSP(800Hz) → out
  crashSum = new Summer();
  crashHPF = new HighPassSP(800, out.sampleRate());
  hihatVoice.connect(crashSum);
  crashSum.patch(crashHPF).patch(out);

  println("instrumentId = " + instrumentId);
  println("partName = " + partName);
  println("portName = " + portName);
  println("baudRate = " + baudRate);
}

void draw() {
  processEventQueue();
  updateSong();

  // NOTE_OFF watchdog
  if (!isRhythmPart() && melodySynth.activePitch != -1) {
    if (millis() - melodySynth.noteOnTime > maxDurationMs) {
      watchdogFiredCount++;
      watchdogLastFiredAt = millis();
      long heldMs = millis() - melodySynth.noteOnTime;
      println("WATCHDOG fired #" + watchdogFiredCount + " pitch=" + melodySynth.activePitch + " held=" + heldMs + "ms");
      melodySynth.noteOff(melodySynth.activePitch);
      watchdogTestNoteOnAt = -1;
    }
  }

  // ビブラートをメロディシンセに毎フレーム適用
  if (!isRhythmPart()) {
    float vibMult = fx.vibratoMultiplier();

    // クラリネットは音を伸ばし始めて CLARINET_VIBRATO_DELAY_MS 経過するまでビブラートをかけない
    if (instrumentId == 1) {
      boolean sustained = melodySynth.activePitch != -1 &&
                           (millis() - melodySynth.noteOnTime) >= CLARINET_VIBRATO_DELAY_MS;
      if (!sustained) vibMult = 1.0f;
    }

    melodySynth.applyVibratoMult(vibMult);
  }

  background(20);
  fill(230);
  text("part: " + partName + " / port: " + portName, 12, 28);
  text("last: " + lastMessage, 12, 56);
  text("song: " + (songPlaying ? "playing (note " + (songIndex+1) + "/" + SONG_PITCHES.length + ")" : "stopped (press P)"), 12, 80);

  // watchdogテスト表示
  if (watchdogTestNoteOnAt >= 0 && melodySynth.activePitch != -1) {
    long elapsed = millis() - watchdogTestNoteOnAt;
    long remaining = maxDurationMs - elapsed;
    fill(255, 200, 0);
    text("WATCHDOG TEST: note stuck " + elapsed + "ms / fires in " + max(0, remaining) + "ms", 12, 104);
  } else if (watchdogLastFiredAt >= 0) {
    fill(0, 255, 100);
    text("WATCHDOG OK: fired " + watchdogFiredCount + " time(s), last at " + watchdogLastFiredAt + "ms", 12, 104);
  }
}

void serialEvent(Serial p) {
  if (p.available() > 256) {
    serialOverflowCount++;
    println("WARN: serial buffer near overflow, count=" + serialOverflowCount);
  }

  long tRx = System.nanoTime();
  while (p.available() > 0) {
    int b = p.read();
    SerialFrame frame = parser.pushByte(b);
    if (frame != null) handleFrame(frame, tRx);
  }
}

void handleFrame(SerialFrame frame, long tRxNs) {
  if (frame.isNoteOn()) {
    if (frame.length != 2) { println("Drop NOTE_ON: bad length=" + frame.length); return; }
    int pitch    = frame.payload[0];
    int velocity = frame.payload[1];
    if (!isValidMidiValue(pitch) || !isValidMidiValue(velocity)) {
      println("Drop NOTE_ON: invalid value pitch=" + pitch + " velocity=" + velocity); return;
    }
    if (velocity == 0) {
      eventQueue.add(new NoteEvent(false, pitch, 0, tRxNs));
      println("Queued NOTE_OFF(v=0) pitch=" + pitch); return;
    }
    eventQueue.add(new NoteEvent(true, pitch, velocity, tRxNs));
    println("Queued NOTE_ON pitch=" + pitch + " velocity=" + velocity);

  } else if (frame.isNoteOff()) {
    if (frame.length != 1) { println("Drop NOTE_OFF: bad length=" + frame.length); return; }
    int pitch = frame.payload[0];
    if (!isValidMidiValue(pitch)) { println("Drop NOTE_OFF: invalid pitch=" + pitch); return; }
    eventQueue.add(new NoteEvent(false, pitch, 0, tRxNs));
    println("Queued NOTE_OFF pitch=" + pitch);

  } else {
    println("Drop frame: unknown type=0x" + hex(frame.type, 2));
  }
}

boolean isValidMidiValue(int value) {
  return value >= 0 && value <= 127;
}

void processEventQueue() {
  while (eventQueue.size() > 0) {
    NoteEvent event = eventQueue.remove(0);

    if (event.noteOn) {
      handleNoteOn(event.pitch, event.velocity, event.tRxNs);
    } else {
      handleNoteOff(event.pitch);
    }
  }
}

void handleNoteOn(int pitch, int velocity, long tRxNs) {
  if (isRhythmPart()) {
    triggerDrumByPitch(pitch, velocity);
  } else {
    melodySynth.noteOn(pitch, velocity);
  }

  lastMessage = "NOTE_ON pitch=" + pitch + " velocity=" + velocity;
  println(lastMessage);

  // MOP-4-2 レイテンシ計測（単位: ms）
  long latencyMs = (System.nanoTime() - tRxNs) / 1_000_000;
  latencyLog.println(millis() + "," + pitch + "," + latencyMs);
  latencyLog.flush();
}

void handleNoteOff(int pitch) {
  if (!isRhythmPart()) {
    melodySynth.noteOff(pitch);
  }

  lastMessage = "NOTE_OFF pitch=" + pitch;
  println(lastMessage);
}

boolean isRhythmPart() {
  return instrumentId == 3 || partName.equals("rhythm");
}

void triggerDrumByPitch(int pitch, int velocity) {
  if (pitch == 36 || pitch == 35) {
    kickVoice.trigger(velocity);
  } else if (pitch == 38 || pitch == 40) {
    snareVoice.trigger(velocity);
  } else if (pitch == 42 || pitch == 44 || pitch == 46) {
    hihatVoice.trigger(velocity);
  } else {
    snareVoice.trigger(velocity);
  }
}

boolean loadConfig() {
  JSONObject config = loadJSONObject(sketchPath("config.json"));
  if (config == null) {
    println("Config error: config.json was not found.");
    return false;
  }

  if (config.hasKey("baudRate")) {
    baudRate = config.getInt("baudRate");
  }

  if (!config.hasKey("selectedInstrumentId")) {
    println("Config error: selectedInstrumentId is missing.");
    return false;
  }

  int selectedInstrumentId = config.getInt("selectedInstrumentId");
  JSONArray instruments = config.getJSONArray("instruments");

  if (instruments == null || instruments.size() == 0) {
    println("Config error: instruments is empty.");
    return false;
  }

  for (int i = 0; i < instruments.size(); i++) {
    JSONObject instrument = instruments.getJSONObject(i);
    int currentId = instrument.getInt("instrumentId");

    if (currentId == selectedInstrumentId) {
      instrumentId = currentId;
      partName = instrument.getString("partName");
      portName = instrument.getString("portName");
      return true;
    }
  }

  println("Config error: selectedInstrumentId was not found.");
  return false;
}

void stop() {
  if (latencyLog != null) latencyLog.flush();
  if (port != null) port.stop();
  if (out != null) out.close();
  if (minim != null) minim.stop();
  super.stop();
}

class NoteEvent {
  boolean noteOn;
  int pitch;
  int velocity;
  long tRxNs;

  NoteEvent(boolean noteOn, int pitch, int velocity, long tRxNs) {
    this.noteOn   = noteOn;
    this.pitch    = pitch;
    this.velocity = velocity;
    this.tRxNs    = tRxNs;
  }
}

// ===== テスト用（確認後に削除） =====
// A S D F G H J K → ド レ ミ ファ ソ ラ シ ド（MIDI 60〜72, C4〜C5）
// Q W E → キック / スネア / クラッシュ
int[] TEST_PITCHES = {60, 62, 64, 65, 67, 69, 71, 72};
char[] TEST_KEYS   = {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k'};
int lastTestPitch  = -1;

void keyPressed() {
  for (int i = 0; i < TEST_KEYS.length; i++) {
    if (key == TEST_KEYS[i]) {
      if (lastTestPitch != -1) handleNoteOff(lastTestPitch);
      handleNoteOn(TEST_PITCHES[i], 100, System.nanoTime());
      lastTestPitch = TEST_PITCHES[i];
      return;
    }
  }
  if (key == 'q' || key == 'Q') triggerDrumByPitch(36, 100);
  if (key == 'w' || key == 'W') triggerDrumByPitch(38, 100);
  if (key == 'e' || key == 'E') triggerDrumByPitch(42, 100);
  if (key == 'p' || key == 'P') startSong();
  if (key == 'y' || key == 'Y') selfTestSerialParser();
  // Tキー: noteOffを送らずnoteOnだけ発火 → watchdogのテスト
  if ((key == 't' || key == 'T') && !isRhythmPart()) {
    int testPitch = 60;
    println("WATCHDOG TEST: noteOn pitch=" + testPitch + " (no noteOff sent)");
    melodySynth.noteOn(testPitch, 100);
    watchdogTestNoteOnAt = millis();
  }
}

void keyReleased() {
  for (int i = 0; i < TEST_KEYS.length; i++) {
    if (key == TEST_KEYS[i] && TEST_PITCHES[i] == lastTestPitch) {
      handleNoteOff(lastTestPitch);
      lastTestPitch = -1;
      return;
    }
  }
}

// ===== SerialParser セルフテスト（Yキー・実機なしでパースを検証） =====
void selfTestSerialParser() {
  println("---- SerialParser self-test ----");
  feedFrame(buildFrame(FRAME_TYPE_ON,  new int[] {60, 100}, true));   // 正常 NOTE_ON
  feedFrame(buildFrame(FRAME_TYPE_OFF, new int[] {60},      true));   // 正常 NOTE_OFF
  feedFrame(buildFrame(FRAME_TYPE_ON,  new int[] {62, 90},  false));  // チェックサム破損（破棄）
  println("invalidFrameCount = " + parser.invalidFrameCount);
  println("--------------------------------");
}

int[] buildFrame(int type, int[] payload, boolean validChecksum) {
  int[] frame = new int[3 + payload.length + 1];
  int idx = 0;
  frame[idx++] = FRAME_START;
  frame[idx++] = type;
  frame[idx++] = payload.length;
  int cs = type ^ payload.length;
  for (int i = 0; i < payload.length; i++) { frame[idx++] = payload[i]; cs ^= payload[i]; }
  frame[idx++] = validChecksum ? (cs & 0xFF) : ((cs ^ 0xFF) & 0xFF);
  return frame;
}

void feedFrame(int[] frameBytes) {
  long tRx = System.nanoTime();
  for (int b : frameBytes) {
    SerialFrame frame = parser.pushByte(b);
    if (frame != null) handleFrame(frame, tRx);
  }
}

// ===== テスト曲「かえるの歌」 =====
// かえるの-うたが / きこえてくるよ / クワクワクワクワ / ケロケロケロケロ
int[] SONG_PITCHES = {
  60, 62, 64, 65, 64, 62, 60,   // かえるのうたが
  64, 65, 67, 69, 67, 65, 64,   // きこえてくるよ
  60, 60, 60, 60,               // クワクワクワクワ
  60, 60, 62, 62, 64, 64, 65, 65, 64, 62, 60               // ケロケロケロケロ
};
float[] SONG_DURATIONS = {       // 単位: 拍
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  1.0, 1.0, 1.0, 1.0, 
  0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.25, 0.5, 0.5, 1.0
};

// ===== テスト曲「かえるの歌」ドラムパターン =====
// フレーズごとにメロディ構造に対応（全16拍）
// 36=キック 38=スネア 42=クラッシュ
int[][] DRUM_PITCHES = {
  // かえるのうたが（4拍 = 8ステップ・8分音符）
  {36,38},{38}, {42,38},{38},
  {36,38},{38}, {42,38},{38},
  // きこえてくるよ（4拍 = 8ステップ・8分音符）
  {36,38},{38}, {42,38},{38},
  {36,38},{38}, {42,38},{38},
  // クワクワクワクワ（4拍 = 4ステップ・4分音符）
  {36,38}, {42,38}, {36,38}, {42,38},
  // ケロケロケロケロ（前半2拍は8分・後半2拍は4分で締め）
  {36,38},{38}, {42,38},{38}, {36,38},{42,38}
};
float[] DRUM_DURATIONS = {
  0.5,0.5, 0.5,0.5,  0.5,0.5, 0.5,0.5,
  0.5,0.5, 0.5,0.5,  0.5,0.5, 0.5,0.5,
  1.0,1.0,1.0,1.0,
  0.5,0.5, 0.5,0.5, 1.0,1.0
};

float SONG_BPM = 132.0;

boolean songPlaying        = false;
int     songIndex          = -1;
long    songNextEventTime  = 0;
boolean songPendingNoteOn  = false;
int     songPendingPitch   = -1;

void startSong() {
  if (songPlaying) return;
  println("Song started: かえるの歌");
  songPlaying       = true;
  songIndex         = -1;
  songNextEventTime = millis();
  songPendingNoteOn = false;
  songPendingPitch  = -1;
}

void updateSong() {
  if (!songPlaying) return;
  if (millis() < songNextEventTime) return;

  float beatMs = 60000.0 / SONG_BPM;

  if (isRhythmPart()) {
    // ドラムパート: triggerで即時発音、noteOff不要
    songIndex++;
    if (songIndex >= DRUM_PITCHES.length) {
      songPlaying = false;
      songIndex   = -1;
      println("Song finished");
      return;
    }
    for (int p : DRUM_PITCHES[songIndex]) triggerDrumByPitch(p, 100);
    songNextEventTime = millis() + (long)(DRUM_DURATIONS[songIndex] * beatMs);

  } else {
    // メロディパート
    if (songPendingNoteOn) {
      handleNoteOn(songPendingPitch, 100, System.nanoTime());
      songNextEventTime = millis() + (long)(SONG_DURATIONS[songIndex] * beatMs) - 50;
      songPendingNoteOn = false;
      return;
    }

    if (songIndex >= 0) handleNoteOff(SONG_PITCHES[songIndex]);
    songIndex++;
    if (songIndex >= SONG_PITCHES.length) {
      songPlaying = false;
      songIndex   = -1;
      println("Song finished");
      return;
    }

    int pitch = SONG_PITCHES[songIndex];
    boolean samePitch   = songIndex > 0 && pitch == SONG_PITCHES[songIndex - 1];
    boolean inKerokero  = songIndex >= 18;  // ケロケロセクションは全ノート間にギャップ

    if (samePitch || inKerokero) {
      songPendingNoteOn = true;
      songPendingPitch  = pitch;
      songNextEventTime = millis() + 50;
    } else {
      handleNoteOn(pitch, 100, System.nanoTime());
      songNextEventTime = millis() + (long)(SONG_DURATIONS[songIndex] * beatMs);
    }
  }
}
