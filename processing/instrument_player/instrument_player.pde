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

ArrayList<NoteEvent> eventQueue = new ArrayList<NoteEvent>();

String lastMessage = "waiting";

int serialOverflowCount = 0;
PrintWriter latencyLog;

long maxDurationMs = 2000;
long CLARINET_VIBRATO_DELAY_MS = 300;  // クラリネットがビブラートをかけ始めるまでの保持時間

void setup() {
  size(420, 120);

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
  port.bufferUntil('\n');

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
      println("WATCHDOG: forcing release pitch=" + melodySynth.activePitch);
      melodySynth.noteOff(melodySynth.activePitch);
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
}

void serialEvent(Serial p) {
  // Serial バッファ溢れ監視
  if (p.available() > 256) {
    serialOverflowCount++;
    println("WARN: serial buffer near overflow, count=" + serialOverflowCount);
  }

  long tRx = System.nanoTime();
  String line = p.readStringUntil('\n');

  if (line == null) return;
  line = trim(line);
  if (line.length() == 0) return;

  println("RX: " + line);
  parseLine(line, tRx);
}

void parseLine(String line, long tRxNs) {
  String[] fields = split(line, ',');

  if (fields.length == 2) {
    int pitch    = parseIntField(fields[0], -1);
    int velocity = parseIntField(fields[1], -1);

    if (!isValidMidiValue(pitch) || !isValidMidiValue(velocity)) {
      println("Invalid NOTE_ON value: pitch=" + pitch + " velocity=" + velocity);
      return;
    }

    // velocity=0 はNOTE_OFFとして扱う
    if (velocity == 0) {
      eventQueue.add(new NoteEvent(false, pitch, 0, tRxNs));
      println("Queued NOTE_OFF(v=0) pitch=" + pitch);
      return;
    }

    eventQueue.add(new NoteEvent(true, pitch, velocity, tRxNs));
    println("Queued NOTE_ON pitch=" + pitch + " velocity=" + velocity);

  } else if (fields.length == 1) {
    int pitch = parseIntField(fields[0], -1);

    if (!isValidMidiValue(pitch)) {
      println("Invalid NOTE_OFF value: pitch=" + pitch);
      return;
    }

    eventQueue.add(new NoteEvent(false, pitch, 0, tRxNs));
    println("Queued NOTE_OFF pitch=" + pitch);

  } else {
    println("Invalid frame: " + line);
  }
}

int parseIntField(String value, int fallback) {
  value = trim(value);
  if (value.length() == 0) return fallback;

  for (int i = 0; i < value.length(); i++) {
    char c = value.charAt(i);
    if (c < '0' || c > '9') return fallback;
  }

  return int(value);
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

  // MOP-4-2 レイテンシ計測
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

// ===== テスト曲「かえるの歌」 =====
// かえるの-うたが / きこえてくるよ / クワクワクワクワ / ケロケロケロケロ
int[] SONG_PITCHES = {
  60, 62, 64, 65, 64, 62, 60,   // かえるのうたが
  64, 65, 67, 69, 67, 65, 64,   // きこえてくるよ
  67, 65, 64, 62,               // クワクワクワクワ
  60, 60, 60, 60                // ケロケロケロケロ
};
float[] SONG_DURATIONS = {       // 単位: 拍
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 1.0,
  1.0, 1.0, 1.0, 1.0,
  1.0, 1.0, 1.0, 1.0
};

// ===== テスト曲「かえるの歌」ドラムパターン =====
// スネア: 8分音符で全拍  バスドラ: 4分音符で奇数拍  クラッシュ: 偶数拍（バスドラと交互）
// 各ステップ0.5拍・32ステップ = 16拍
// 36=キック 38=スネア 42=クラッシュ
int[][] DRUM_PITCHES = {
  {36,38},{38}, {42,38},{38},   // 拍1〜2
  {36,38},{38}, {42,38},{38},   // 拍3〜4
  {36,38},{38}, {42,38},{38},   // 拍5〜6
  {36,38},{38}, {42,38},{38},   // 拍7〜8
  {36,38},{38}, {42,38},{38},   // 拍9〜10
  {36,38},{38}, {42,38},{38},   // 拍11〜12
  {36,38},{38}, {42,38},{38},   // 拍13〜14
  {36,38},{42,38}               // 拍15〜16（終わり：スネア4分音符）
};
float[] DRUM_DURATIONS = {
  0.5,0.5, 0.5,0.5,  0.5,0.5, 0.5,0.5,
  0.5,0.5, 0.5,0.5,  0.5,0.5, 0.5,0.5,
  0.5,0.5, 0.5,0.5,  0.5,0.5, 0.5,0.5,
  0.5,0.5, 0.5,0.5,  0.5,0.5, 1.0,1.0
};

float SONG_BPM = 60.0;

boolean songPlaying       = false;
int     songIndex         = -1;
long    songNextEventTime = 0;

void startSong() {
  if (songPlaying) return;
  println("Song started: かえるの歌");
  songPlaying       = true;
  songIndex         = -1;
  songNextEventTime = millis();
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
    if (songIndex >= 0) handleNoteOff(SONG_PITCHES[songIndex]);
    songIndex++;
    if (songIndex >= SONG_PITCHES.length) {
      songPlaying = false;
      songIndex   = -1;
      println("Song finished");
      return;
    }
    handleNoteOn(SONG_PITCHES[songIndex], 100, System.nanoTime());
    songNextEventTime = millis() + (long)(SONG_DURATIONS[songIndex] * beatMs);
  }
}
