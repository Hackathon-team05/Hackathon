// EffectProcessor — WBS243: ビブラート・リバーブ実装
// 設計書 表3.4: LFO位相, リバーブバッファ を保持し合成後サンプルにエフェクトを適用する

class EffectProcessor implements AudioEffect {

  // ===== Reverb（Schroeder型: 並列コムフィルタ + 直列オールパスフィルタ）=====
  boolean reverbEnabled = true;
  float   reverbMix     = 0.25f;  // ドライ/ウェット比（0.0=dry, 1.0=全wet）

  // コムフィルタ（4本並列）: 素数に近い遅延長でフラッタエコーを防ぐ
  private float[][] combBuf;
  private int[]     combIdx;
  private final int[]   COMB_LEN   = {1687, 1601, 2053, 2251};
  private final float[] COMB_DECAY = {0.805f, 0.827f, 0.783f, 0.764f};

  // オールパスフィルタ（2本直列）: 拡散処理
  private float[][] apBuf;
  private int[]     apIdx;
  private final int[]   AP_LEN   = {347, 113};
  private final float[] AP_DECAY = {0.7f, 0.7f};

  // ===== Vibrato（正弦波LFO による周波数変調）=====
  boolean vibratoEnabled = false;
  float   vibratoHz      = 5.0f;    // LFO周波数 [Hz]
  float   vibratoDepth   = 0.003f;  // 変調深さ（±0.3%）
  private float vibratoPhase = 0.0f;

  // ===== 初期化 =====
  EffectProcessor() {
    combBuf = new float[COMB_LEN.length][];
    combIdx = new int[COMB_LEN.length];
    for (int i = 0; i < COMB_LEN.length; i++) {
      combBuf[i] = new float[COMB_LEN[i]];
    }

    apBuf = new float[AP_LEN.length][];
    apIdx = new int[AP_LEN.length];
    for (int i = 0; i < AP_LEN.length; i++) {
      apBuf[i] = new float[AP_LEN[i]];
    }
  }

  // ===== Vibrato: draw()ループから毎フレーム呼び出す =====
  // 戻り値の周波数倍率を AdditiveSynthEngine.applyVibratoMult() へ渡す
  float vibratoMultiplier() {
    if (!vibratoEnabled) return 1.0f;
    vibratoPhase += TWO_PI * vibratoHz / frameRate;
    if (vibratoPhase > TWO_PI) vibratoPhase -= TWO_PI;
    return 1.0f + vibratoDepth * sin(vibratoPhase);
  }

  // ===== AudioEffect インタフェース（Minim が出力バッファごとに呼び出す）=====
  void process(float[] samp) {
    if (!reverbEnabled) return;
    for (int i = 0; i < samp.length; i++) {
      samp[i] = applyReverb(samp[i]);
    }
  }

  void process(float[] left, float[] right) {
    if (!reverbEnabled) return;
    for (int i = 0; i < left.length; i++) {
      left[i]  = applyReverb(left[i]);
      right[i] = applyReverb(right[i]);
    }
  }

  // ===== 内部: Schroederリバーブ 1サンプル処理 =====
  private float applyReverb(float x) {
    // 並列コムフィルタで残響密度を生成
    float wet = 0;
    for (int c = 0; c < COMB_LEN.length; c++) {
      float delayed = combBuf[c][combIdx[c]];
      combBuf[c][combIdx[c]] = x + delayed * COMB_DECAY[c];
      combIdx[c] = (combIdx[c] + 1) % COMB_LEN[c];
      wet += delayed;
    }
    wet *= 0.25f;

    // 直列オールパスフィルタで拡散
    for (int a = 0; a < AP_LEN.length; a++) {
      float delayed = apBuf[a][apIdx[a]];
      float v = wet + delayed * AP_DECAY[a];
      apBuf[a][apIdx[a]] = v;
      apIdx[a] = (apIdx[a] + 1) % AP_LEN[a];
      wet = delayed - AP_DECAY[a] * v;
    }

    return x * (1.0f - reverbMix) + wet * reverbMix;
  }
}
