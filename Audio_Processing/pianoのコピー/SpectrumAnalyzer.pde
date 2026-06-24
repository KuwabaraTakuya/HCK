// ============================================================
// SpectrumAnalyzer.pde
// 共通スペクトルアナライザモジュール
// 使い方:
//   setup() 内で spectrumSetup() を呼ぶ（グローバル変数 out が必要）
//   draw()  内で spectrumDraw(instrName, bpm, effLabel) を呼ぶ
// ============================================================

import ddf.minim.analysis.*;

// ── 定数 ────────────────────────────────────────────────────
final int   SPEC_BANDS      = 16;   // バンド数
final float SPEC_MIN_DB     = -60;  // Y 軸下限 [dB]
final float SPEC_MAX_DB     =  40;  // Y 軸上限 [dB]
final float SPEC_PEAK_DECAY =  0.3; // ピーク 1 フレームあたり減衰量 [dB]

// レイアウト
final int SPEC_STATUS_H = 50; // ステータスバー高さ
final int SPEC_LEFT_M   = 68; // 左余白（Y 軸ラベル用）
final int SPEC_RIGHT_M  = 18; // 右余白
final int SPEC_BOTTOM_M = 38; // 下余白（X 軸ラベル用）
final int SPEC_TOP_M    = 60; // 上余白（ステータスバー + マージン）

// ── 状態変数 ─────────────────────────────────────────────────
FFT     specFft;
float[] specBandValues = new float[SPEC_BANDS]; // 現在の dB 値
float[] specPeakValues = new float[SPEC_BANDS]; // ピークホールド dB 値
int[]   specBandBins   = new int[SPEC_BANDS + 1]; // バンド境界ビンインデックス
float[] specBandHz     = new float[SPEC_BANDS];   // X 軸ラベル用中心周波数

// ============================================================
// 公開 API
// ============================================================

// setup() から呼ぶ。グローバル変数 out（AudioOutput）を参照する。
void spectrumSetup() {
  specFft = new FFT(out.bufferSize(), out.sampleRate());
  specFft.window(FFT.HAMMING); // スペクトルリーク低減

  specInitBands(50.0, 20000.0); // 50 Hz〜20 kHz を 16 等分（対数）

  for (int i = 0; i < SPEC_BANDS; i++) {
    specBandValues[i] = SPEC_MIN_DB;
    specPeakValues[i] = SPEC_MIN_DB;
  }
}

// draw() から呼ぶ。
//   instrName : 楽器名（ステータスバー左側に表示）
//   bpm       : 推定 BPM（0 以下なら "--" 表示）
//   effLabel  : エフェクト状態文字列（空文字ならヘルプ表示）
void spectrumDraw(String instrName, float bpm, String effLabel) {
  specFft.forward(out.mix);  // エフェクト適用後の mix を FFT
  specComputeBands();
  specDrawStatusBar(instrName, bpm, effLabel);
  specDrawBars();
}

// ============================================================
// 内部処理
// ============================================================

void specInitBands(float fMin, float fMax) {
  float logMin = log(fMin);
  float logMax = log(fMax);

  // 対数スケールでバンド境界周波数 → ビンインデックスに変換
  for (int i = 0; i <= SPEC_BANDS; i++) {
    float freq = exp(logMin + (logMax - logMin) * i / SPEC_BANDS);
    specBandBins[i] = constrain(specFft.freqToIndex(freq), 0, specFft.specSize() - 1);
  }
  // 低域で同一ビンになる場合に単調増加を保証
  for (int i = 1; i <= SPEC_BANDS; i++) {
    if (specBandBins[i] <= specBandBins[i - 1])
      specBandBins[i] = specBandBins[i - 1] + 1;
  }
  // バンド中心周波数（幾何平均）を計算
  for (int i = 0; i < SPEC_BANDS; i++) {
    float lo = specFft.indexToFreq(specBandBins[i]);
    float hi = specFft.indexToFreq(min(specBandBins[i + 1], specFft.specSize() - 1));
    specBandHz[i] = sqrt(lo * hi);
  }
}

void specComputeBands() {
  for (int i = 0; i < SPEC_BANDS; i++) {
    int kLo = specBandBins[i];
    int kHi = min(specBandBins[i + 1] - 1, specFft.specSize() - 1);

    // バンド内 RMS を計算: rms = sqrt(1/K * Σ|X[k]|^2)
    float sum = 0;
    int   cnt = 0;
    for (int k = kLo; k <= kHi; k++) {
      float amp = specFft.getBand(k);
      sum += amp * amp;
      cnt++;
    }
    float rms = (cnt > 0) ? sqrt(sum / cnt) : 0;
    float db  = (rms > 1e-6) ? 20.0 * log(rms) / log(10.0) : SPEC_MIN_DB;
    specBandValues[i] = constrain(db, SPEC_MIN_DB, SPEC_MAX_DB);

    // ピークホールド：超えたら即更新、下なら SPEC_PEAK_DECAY ずつ降下
    if (specBandValues[i] > specPeakValues[i]) {
      specPeakValues[i] = specBandValues[i];
    } else {
      specPeakValues[i] = max(specPeakValues[i] - SPEC_PEAK_DECAY, SPEC_MIN_DB);
    }
  }
}

void specDrawStatusBar(String instrName, float bpm, String effLabel) {
  // 背景
  fill(20, 20, 40);
  noStroke();
  rect(0, 0, width, SPEC_STATUS_H);

  // 楽器名（左）
  fill(255);
  textSize(20);
  textAlign(LEFT, CENTER);
  text(instrName, 18, SPEC_STATUS_H / 2);

  // BPM（中央）
  fill(200);
  textSize(15);
  textAlign(CENTER, CENTER);
  text((bpm > 0) ? "BPM: " + nf(bpm, 1, 0) : "BPM: --", width / 2, SPEC_STATUS_H / 2);

  // エフェクト状態（右）
  boolean effOn = (effLabel != null && effLabel.length() > 0);
  fill(effOn ? color(255, 200, 0) : color(120));
  textSize(13);
  textAlign(RIGHT, CENTER);
  text(effOn ? effLabel : "[B] BitCrusher  [L] LowPass", width - 15, SPEC_STATUS_H / 2);
}

void specDrawBars() {
  int plotX = SPEC_LEFT_M;
  int plotY = SPEC_TOP_M;
  int plotW = width  - SPEC_LEFT_M - SPEC_RIGHT_M;
  int plotH = height - SPEC_TOP_M  - SPEC_BOTTOM_M;
  int barW  = plotW / SPEC_BANDS;

  // プロット背景
  fill(20);
  noStroke();
  rect(plotX, plotY, plotW, plotH);

  // Y 軸グリッドとラベル（10 dB 間隔）
  for (int db = (int)SPEC_MIN_DB; db <= (int)SPEC_MAX_DB; db += 10) {
    float gy = plotY + map(db, SPEC_MIN_DB, SPEC_MAX_DB, plotH, 0);
    stroke(db == 0 ? color(90) : color(48));
    strokeWeight(1);
    line(plotX, gy, plotX + plotW, gy);

    fill(155);
    noStroke();
    textSize(11);
    textAlign(RIGHT, CENTER);
    text(db + "dB", plotX - 5, gy);
  }

  // バーとピークライン
  for (int i = 0; i < SPEC_BANDS; i++) {
    int bx = plotX + i * barW;

    // バー高さ（dB → ピクセル）
    float barH = map(specBandValues[i], SPEC_MIN_DB, SPEC_MAX_DB, 0, plotH);
    barH = constrain(barH, 0, plotH);

    // レベルに応じてグリーン → イエロー → レッドのグラデーション
    float ratio = (specBandValues[i] - SPEC_MIN_DB) / (SPEC_MAX_DB - SPEC_MIN_DB);
    int cr, cg;
    if (ratio < 0.6) {
      cr = (int)(ratio / 0.6 * 220);
      cg = 164;
    } else {
      cr = 255;
      cg = (int)((1.0 - (ratio - 0.6) / 0.4) * 164);
    }
    fill(cr, cg, 0);
    noStroke();
    rect(bx + 2, plotY + plotH - barH, barW - 4, barH);

    // ピークライン（黄色）
    float peakY = plotY + map(specPeakValues[i], SPEC_MIN_DB, SPEC_MAX_DB, plotH, 0);
    peakY = constrain(peakY, plotY, plotY + plotH - 1);
    stroke(255, 255, 0);
    strokeWeight(2);
    line(bx + 2, peakY, bx + barW - 2, peakY);

    // X 軸ラベル（バンド中心周波数）
    noStroke();
    fill(155);
    textSize(10);
    textAlign(CENTER, TOP);
    text(specFreqLabel(specBandHz[i]), bx + barW / 2, plotY + plotH + 4);
  }
}

String specFreqLabel(float f) {
  if (f >= 1000) return nf(f / 1000, 1, 1) + "k";
  return "" + (int)round(f);
}
