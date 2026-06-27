// ============================================================
// SpectrumAnalyzer.pde
// 共通スペクトルアナライザモジュール
// 使い方:
//   setup() 内で spectrumSetup() を呼ぶ（グローバル変数 out が必要）
//   draw()  内で spectrumDraw(instrName, bpm, effLabel) を呼ぶ
// ============================================================

import ddf.minim.analysis.*;

// ── 定数 ────────────────────────────────────────────────────
final int   SPEC_BANDS      = 35;   // バンド数
final float SPEC_MIN_DB     = 0;  // Y 軸下限 [dB]
final float SPEC_MAX_DB     =  50;  // Y 軸上限 [dB]
final float SPEC_PEAK_DECAY =  0.2; // ピーク 1 フレームあたり減衰量 [dB]

// レイアウト
final int SPEC_STATUS_H = 50; // ステータスバー高さ
final int SPEC_LEFT_M   = 68; // 左余白
final int SPEC_RIGHT_M  = 18; // 右余白
final int SPEC_BOTTOM_M = 50; // 下余白
final int SPEC_TOP_M    = 60; // 上余白

// ── 状態変数 ─────────────────────────────────────────────────
FFT     specFft;
float[] specBandValues = new float[SPEC_BANDS]; 
float[] specPeakValues = new float[SPEC_BANDS]; 
int[]   specBandBins   = new int[SPEC_BANDS + 1]; 
float[] specBandHz     = new float[SPEC_BANDS]; 

// ── 【変更】最高ピークホールド用の状態変数 ──────────────────────
float  specGlobalMaxAmp = 0.001;  // これまでの最大振幅（初期値はノイズ床）
float  specPeakFreq     = 0;      // 最大音量時の周波数 f
float  specPeakMel      = 0;      // 最大音量時のメル尺度 m
String melScaleText     = "ピークメル尺度＝--- mel";

// ============================================================
// 公開 API
// ============================================================

void spectrumSetup() {
  specFft = new FFT(out.bufferSize(), out.sampleRate());
  specFft.window(FFT.HAMMING); 

  specInitBands(0.0, 2000.0); 

  for (int i = 0; i < SPEC_BANDS; i++) {
    specBandValues[i] = SPEC_MIN_DB;
    specPeakValues[i] = SPEC_MIN_DB;
  }
}

void spectrumDraw(String instrName, float bpm, String effLabel) {
  specFft.forward(out.mix); 
  specComputeBands();
  specDrawStatusBar(instrName, bpm, effLabel);
  specDrawBars();
}

// 【追加】新しく曲が始まるときなどに、保持した最高ピーク値をリセットする関数
void spectrumResetPeak() {
  specGlobalMaxAmp = 0.001;
  specPeakFreq = 0;
  specPeakMel = 0;
  melScaleText = "ピークメル尺度＝--- mel";
}

// ============================================================
// 内部処理
// ============================================================

void specInitBands(float fMin, float fMax) {
  float logMin = log(fMin);
  float logMax = log(fMax);
  for (int i = 0; i <= SPEC_BANDS; i++) {
    float freq = exp(logMin + (logMax - logMin) * i / SPEC_BANDS);
    specBandBins[i] = constrain(specFft.freqToIndex(freq), 0, specFft.specSize() - 1);
  }
  for (int i = 1; i <= SPEC_BANDS; i++) {
    if (specBandBins[i] <= specBandBins[i - 1])
      specBandBins[i] = specBandBins[i - 1] + 1;
  }
  for (int i = 0; i < SPEC_BANDS; i++) {
    float lo = specFft.indexToFreq(specBandBins[i]);
    float hi = specFft.indexToFreq(min(specBandBins[i + 1], specFft.specSize() - 1));
    specBandHz[i] = sqrt(lo * hi);
  }
}

void specComputeBands() {
  // 16バンドのスペクトル計算
  for (int i = 0; i < SPEC_BANDS; i++) {
    int kLo = specBandBins[i];
    int kHi = min(specBandBins[i + 1] - 1, specFft.specSize() - 1);
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

    if (specBandValues[i] > specPeakValues[i]) {
      specPeakValues[i] = specBandValues[i];
    } else {
      specPeakValues[i] = max(specPeakValues[i] - SPEC_PEAK_DECAY, SPEC_MIN_DB);
    }
  }

  // ─── 【重要】全ビンの中から「現在のフレームで一番音が大きい場所」を探す ───
  float currentMaxAmp = -1;
  int currentMaxBin = -1;
  for (int k = 0; k < specFft.specSize(); k++) {
    float amp = specFft.getBand(k);
    if (amp > currentMaxAmp) {
      currentMaxAmp = amp;
      currentMaxBin = k;
    }
  }

  // ─── 【重要】現在保持している最高ピーク値より大きな音が来たら更新する ───
  if (currentMaxBin != -1 && currentMaxAmp > specGlobalMaxAmp) {
    specGlobalMaxAmp = currentMaxAmp; // 新しい最高ピーク値（振幅）を保持
    specPeakFreq = specFft.indexToFreq(currentMaxBin); // その時の周波数をfとする
    
    if (specPeakFreq > 0) {
      // 数式に代入: m = 2595 * log10(1 + f / 700)
      specPeakMel = 2595.0 * (float)(Math.log10(1.0 + specPeakFreq / 700.0));
      
      // 画面に表示する文字列を生成
      melScaleText = "ピークメル尺度＝" + nf(specPeakMel, 0, 2) + " mel";
    }
  }
}

void specDrawStatusBar(String instrName, float bpm, String effLabel) {
  fill(20, 20, 40);
  noStroke();
  rect(0, 0, width, SPEC_STATUS_H);

  fill(255);
  textSize(20);
  textAlign(LEFT, CENTER);
  text(instrName, 18, SPEC_STATUS_H / 2);

  fill(200);
  textSize(15);
  textAlign(CENTER, CENTER);
  text((bpm > 0) ? "BPM: " + nf(bpm, 1, 0) : "BPM: --", width / 2, SPEC_STATUS_H / 2);

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

  fill(20);
  noStroke();
  rect(plotX, plotY, plotW, plotH);

  for (int db = (int)SPEC_MIN_DB; db <= (int)SPEC_MAX_DB; db += 5) {
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

  for (int i = 0; i < SPEC_BANDS; i++) {
    int bx = plotX + i * barW;
    float barH = map(specBandValues[i], SPEC_MIN_DB, SPEC_MAX_DB, 0, plotH);
    barH = constrain(barH, 0, plotH);

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

    float peakY = plotY + map(specPeakValues[i], SPEC_MIN_DB, SPEC_MAX_DB, plotH, 0);
    peakY = constrain(peakY, plotY, plotY + plotH - 1);
    stroke(255, 255, 0);
    strokeWeight(2);
    line(bx + 2, peakY, bx + barW - 2, peakY);

    noStroke();
    fill(155);
    textSize(10);
    textAlign(CENTER, TOP);
    text(specFreqLabel(specBandHz[i]), bx + barW / 2, plotY + plotH + 4);
  }


  fill(255, 215, 0); // 目立つゴールドカラー
  textSize(16);
  textAlign(RIGHT, TOP);
  text(melScaleText, plotX + plotW - 15, plotY + 15);
}

String specFreqLabel(float f) {
  if (f >= 1000) return nf(f / 1000, 1, 1) + "k";
  return "" + (int)round(f);
}
