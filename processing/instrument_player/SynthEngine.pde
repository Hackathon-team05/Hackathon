class InstrumentPreset {
  String name;
  float[] harmonicRatios;
  float[] harmonicDecays;
  float[] harmonicSustains;
  float[] freqMultipliers;
  float attackTime;
  float releaseTime;
  float masterGain;

  InstrumentPreset(
    String name,
    float[] harmonicRatios,
    float[] harmonicDecays,
    float[] harmonicSustains,
    float[] freqMultipliers,
    float attackTime,
    float releaseTime,
    float masterGain
  ) {
    this.name             = name;
    this.harmonicRatios   = harmonicRatios;
    this.harmonicDecays   = harmonicDecays;
    this.harmonicSustains = harmonicSustains;
    this.freqMultipliers  = freqMultipliers;
    this.attackTime       = attackTime;
    this.releaseTime      = releaseTime;
    this.masterGain       = masterGain;
  }
}

// ===== 楽器プリセット定義 =====
// instrumentId 0, 1, 2 → メロディ3パート（フルート/クラリネット/トランペット）
// instrumentId 3       → リズム（DrumVoiceが担当するためここでは未使用）

InstrumentPreset[] PRESETS = {

  // id=0: フルート（fl.wav FFT解析値 A5）
  new InstrumentPreset(
    "flute",
    new float[] {1.0000f, 0.0948f, 0.0408f, 0.0030f, 0.0063f, 0.0002f, 0.0001f, 0.0003f},
    new float[] {1.5000f, 5.0000f, 1.5000f, 5.0000f, 5.0000f, 5.0000f, 2.0000f, 2.0000f},
    new float[] {0.9500f, 0.8849f, 0.9500f, 0.3821f, 0.7132f, 0.1462f, 0.0571f, 0.0385f},
    new float[] {1.998428f, 3.997002f, 5.995854f, 7.994412f, 9.992400f, 11.767242f, 13.982242f, 15.988784f},
    0.0638f, 0.1161f, 0.5f
  ),

  // id=1: クラリネット（cl.wav FFT解析値 Bb5）
  new InstrumentPreset(
    "clarinet",
    new float[] {1.0000f, 0.4780f, 0.0960f, 0.1033f, 0.0076f, 0.0067f, 0.0009f, 0.0008f},
    new float[] {5.0000f, 5.0000f, 2.0000f, 2.0000f, 2.0000f, 2.0000f, 2.0000f, 5.0000f},
    new float[] {0.4795f, 0.4733f, 0.4769f, 0.4718f, 0.4674f, 0.4654f, 0.4382f, 0.1485f},
    new float[] {1.000000f, 2.000000f, 3.000000f, 4.000000f, 5.000000f, 6.000000f, 7.000000f, 8.000000f},
    0.2000f, 0.0800f, 0.5f
  ),

  // id=2: トランペット（tp.mp3 FFT解析値 12倍音）
  new InstrumentPreset(
    "trumpet",
    new float[] {1.0000f, 0.9602f, 0.3711f, 0.1191f, 0.1897f, 0.1414f, 0.1721f, 0.0969f, 0.0411f, 0.0174f, 0.0110f, 0.0234f},
    new float[] {2.409f,  1.169f,  1.255f,  2.741f,  2.161f,  1.700f,  1.215f,  0.805f,  2.343f,  0.835f,  1.303f,  3.000f},
    new float[] {0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f, 0.0000f},
    new float[] {1.000000f, 1.998651f, 3.002311f, 4.005993f, 5.002203f, 6.004527f, 7.001956f, 8.011589f, 9.004249f, 9.952665f, 10.953964f, 12.012201f},
    0.0367f, 0.0800f, 0.5f
  )
};


class AdditiveSynthEngine {
  int numHarmonics;
  float[] harmonicRatios;
  float[] harmonicDecays;
  float[] harmonicSustains;
  float[] freqMultipliers;
  float attackTime;
  float releaseTime;
  float masterGain;

  Oscil[] oscillators;
  ADSR[]  envelopes;
  int activePitch = -1;

  // instrumentId を渡してプリセットを選択する
  AdditiveSynthEngine(int instrumentId) {
    // id=3はリズムなのでフルート(0)にフォールバック
    int presetIndex = constrain(instrumentId, 0, PRESETS.length - 1);
    InstrumentPreset p = PRESETS[presetIndex];

    harmonicRatios   = p.harmonicRatios;
    harmonicDecays   = p.harmonicDecays;
    harmonicSustains = p.harmonicSustains;
    freqMultipliers  = p.freqMultipliers;
    attackTime       = p.attackTime;
    releaseTime      = p.releaseTime;
    masterGain       = p.masterGain;
    numHarmonics     = p.harmonicRatios.length;

    println("AdditiveSynthEngine: preset = " + p.name + " harmonics=" + numHarmonics);

    oscillators = new Oscil[numHarmonics];
    envelopes   = new ADSR[numHarmonics];

    float totalRatio = 0;
    for (float ratio : harmonicRatios) totalRatio += ratio;
    float scale = masterGain / max(totalRatio, 0.000001f);

    for (int i = 0; i < numHarmonics; i++) {
      float peak    = harmonicRatios[i] * scale;
      float sustain = peak * harmonicSustains[i];
      oscillators[i] = new Oscil(440.0f, 1.0f, Waves.SINE);
      envelopes[i]   = new ADSR(peak, attackTime, max(harmonicDecays[i], 0.005f), sustain, releaseTime);
      oscillators[i].patch(envelopes[i]);
    }

  }

  void connect(AudioOutput output) {
    for (ADSR envelope : envelopes) {
      envelope.patch(output);
    }
  }

  long noteOnTime = 0;

  void noteOn(int pitch, int velocity) {
    noteOnTime = millis();
    if (activePitch != -1) noteOff(activePitch);

    activePitch = pitch;
    float baseFreq    = midiToFreq(pitch);
    float velocityGain = map(velocity, 0, 127, 0.0f, 1.0f);

    for (int i = 0; i < numHarmonics; i++) {
      oscillators[i].setFrequency(baseFreq * freqMultipliers[i]);
      oscillators[i].setAmplitude(velocityGain);
      envelopes[i].noteOn();
    }
  }

  void noteOff(int pitch) {
    if (activePitch != pitch) return;
    for (ADSR envelope : envelopes) envelope.noteOff();
    activePitch = -1;
  }

  // EffectProcessor.vibratoMultiplier() の戻り値を渡してピッチを変調する
  // draw()ループから毎フレーム呼び出す
  void applyVibratoMult(float mult) {
    if (activePitch == -1) return;
    float base = midiToFreq(activePitch);
    for (int i = 0; i < numHarmonics; i++) {
      oscillators[i].setFrequency(base * freqMultipliers[i] * mult);
    }
  }
}


float midiToFreq(int pitch) {
  return 442.0f * pow(2, (pitch - 69) / 12.0f);
}


// ===== DrumVoice（リズムパート用・変更なし）=====

// ===== ドラムプリセット定義 =====
DrumVoice createKickVoice() {
  return new DrumVoice(
    new float[] {80.0f, 128.0f, 192.0f, 244.0f, 312.0f},
    new float[] {1.000f, 0.067f, 0.084f, 0.075f, 0.041f},
    new float[] {0.0365f, 0.0300f, 0.0300f, 0.0300f, 0.0300f},
    new float[] {480.0f, 588.0f, 808.0f, 976.0f},
    new float[] {0.603f, 1.000f, 0.278f, 0.219f},
    new float[] {0.0204f, 0.0161f, 0.0400f, 0.0331f},
    0.2429f, 0.0377f, 0.3000f, 0.0080f,
    0.0010f, 0.0060f, 1.00f,
    0.50f, 0.060f, 0.40f, 0.00f
  );
}

DrumVoice createSnareVoice() {
  return new DrumVoice(
    new float[] {236.0f, 304.0f, 436.0f, 656.0f, 864.0f},
    new float[] {1.000f, 0.217f, 0.584f, 0.353f, 0.907f},
    new float[] {0.0219f, 0.0196f, 0.0221f, 0.0209f, 0.0350f},
    new float[] {1068.0f, 1260.0f, 1520.0f, 1800.0f, 2096.0f, 2376.0f},
    new float[] {1.000f, 0.583f, 0.849f, 0.594f, 0.634f, 0.721f},
    new float[] {0.0180f, 0.0167f, 0.0180f, 0.0162f, 0.0180f, 0.0175f},
    0.3500f, 0.0377f, 0.2000f, 0.0100f,
    0.0010f, 0.0060f, 0.65f,
    0.12f, 0.035f, 0.76f, 0.00f
  );
}

// クラッシュシンバル
DrumVoice createCrashVoice() {
  return new DrumVoice(
    // cb.mp3 FFT実測: 非整数倍音10本 + 長い減衰（τ=0.5〜1.0s）
    new float[] {255.0f, 489.4f, 748.1f, 1035.7f, 1249.5f, 1471.3f, 1765.8f, 2055.2f, 2307.2f, 2624.1f},
    new float[] {1.000f, 0.277f, 0.078f, 0.268f,  0.434f,  0.206f,  0.126f,  0.098f,  0.391f,  0.486f},
    new float[] {0.150f, 0.250f, 0.300f, 0.220f,  0.180f,  0.260f,  0.270f,  0.250f,  0.270f,  0.260f},
    new float[] {3800.0f, 5200.0f, 7100.0f, 9500.0f, 12000.0f, 14000.0f},
    new float[] {0.500f,  0.400f,  0.300f,  0.200f,  0.100f,   0.060f},
    new float[] {0.180f,  0.150f,  0.120f,  0.090f,  0.060f,   0.040f},
    0.5000f, 0.2500f, 0.1000f, 0.0150f,
    0.0030f, 0.1000f, 0.30f,
    0.03f, 0.03f, 0.80f, 0.30f
  );
}

class DrumVoice {
  Oscil[] bodyOscs;
  ADSR[]  bodyEnvs;
  Oscil[] snapOscs;
  ADSR[]  snapEnvs;
  Noise   noise;
  ADSR    noiseEnv;
  Oscil   clickOsc;
  ADSR    clickEnv;

  float masterGain;
  float bodyGain;
  float snapGain;
  float noiseGain;
  float clickGain;
  float noiseAmp;
  float clickAmp;

  DrumVoice(
    float[] bodyFreqs, float[] bodyAmps, float[] bodyDecays,
    float[] snapFreqs, float[] snapAmps, float[] snapDecays,
    float noiseAmp, float noiseDecay,
    float clickAmp, float clickDecay,
    float attackTime, float releaseTime,
    float masterGain,
    float bodyGain, float snapGain, float noiseGain, float clickGain
  ) {
    this.masterGain = masterGain;
    this.bodyGain   = bodyGain;
    this.snapGain   = snapGain;
    this.noiseGain  = noiseGain;
    this.clickGain  = clickGain;
    this.noiseAmp   = noiseAmp;
    this.clickAmp   = clickAmp;

    bodyOscs = new Oscil[bodyFreqs.length];
    bodyEnvs = new ADSR[bodyFreqs.length];
    for (int i = 0; i < bodyFreqs.length; i++) {
      bodyOscs[i] = new Oscil(bodyFreqs[i], 1.0f, Waves.SINE);
      bodyEnvs[i] = new ADSR(bodyAmps[i] * masterGain * bodyGain, attackTime, bodyDecays[i], 0.0f, releaseTime);
      bodyOscs[i].patch(bodyEnvs[i]);
    }

    snapOscs = new Oscil[snapFreqs.length];
    snapEnvs = new ADSR[snapFreqs.length];
    for (int i = 0; i < snapFreqs.length; i++) {
      snapOscs[i] = new Oscil(snapFreqs[i], 1.0f, Waves.SINE);
      snapEnvs[i] = new ADSR(snapAmps[i] * masterGain * snapGain, attackTime, snapDecays[i], 0.0f, releaseTime);
      snapOscs[i].patch(snapEnvs[i]);
    }

    noise    = new Noise(1.0f);
    noiseEnv = new ADSR(noiseAmp * masterGain * noiseGain, attackTime, noiseDecay, 0.0f, releaseTime);
    noise.patch(noiseEnv);

    if (clickGain > 0.0f && clickAmp > 0.0f) {
      clickOsc = new Oscil(2548.0f, 1.0f, Waves.SQUARE);
      clickEnv = new ADSR(clickAmp * masterGain * clickGain, 0.001f, clickDecay, 0.0f, 0.003f);
      clickOsc.patch(clickEnv);
    }
  }

  void connect(AudioOutput output) {
    for (ADSR e : bodyEnvs)  e.patch(output);
    for (ADSR e : snapEnvs)  e.patch(output);
    noiseEnv.patch(output);
    if (clickEnv != null) clickEnv.patch(output);
  }

  void connect(Summer s) {
    for (ADSR e : bodyEnvs)  e.patch(s);
    for (ADSR e : snapEnvs)  e.patch(s);
    noiseEnv.patch(s);
    if (clickEnv != null) clickEnv.patch(s);
  }

  void trigger(int velocity) {
    float velocityGain = map(velocity, 0, 127, 0.0f, 1.0f);
    for (int i = 0; i < bodyOscs.length; i++) {
      bodyOscs[i].setAmplitude(velocityGain);
      bodyEnvs[i].noteOn();
    }
    for (int i = 0; i < snapOscs.length; i++) {
      snapOscs[i].setAmplitude(velocityGain);
      snapEnvs[i].noteOn();
    }
    noiseEnv.noteOn();
    if (clickEnv != null) {
      clickOsc.setAmplitude(velocityGain);
      clickEnv.noteOn();
    }
  }
}
