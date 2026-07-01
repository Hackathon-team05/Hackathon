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

// NOTE_OFF未着による音詰まりを検知する watchdog
int  watchdogFiredCount  = 0;
long watchdogLastFiredAt = -1;

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

  // NOTE_OFF watchdog
  if (!isRhythmPart() && melodySynth.activePitch != -1) {
    if (millis() - melodySynth.noteOnTime > maxDurationMs) {
      watchdogFiredCount++;
      watchdogLastFiredAt = millis();
      long heldMs = millis() - melodySynth.noteOnTime;
      println("WATCHDOG fired #" + watchdogFiredCount + " pitch=" + melodySynth.activePitch + " held=" + heldMs + "ms");
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

  if (watchdogLastFiredAt >= 0) {
    fill(255, 150, 0);
    text("WATCHDOG: fired " + watchdogFiredCount + " time(s), last at " + watchdogLastFiredAt + "ms", 12, 80);
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

