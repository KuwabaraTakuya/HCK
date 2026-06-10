// ============================================================
// Trumpet_wave.pde
// トランペット音色シンセサイザー + スペクトルアナライザ
// タブ構成: Trumpet_wave.pde（本ファイル）+ SpectrumAnalyzer.pde
// ============================================================

import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim       minim;
AudioOutput out;
Serial      port;
TrumpetSig  sig;

// エフェクト（本ファイルに保持）
BitCrusherT bitCrusher;
LowPassT    lowPass;

// BPM 推定
float bpm           = 0;
int   lastNoteOnTime = 0;

// ─────────────────────────────────────────────────────────────
void setup() {
  size(900, 560);
  minim = new Minim(this);
  out   = minim.getLineOut(Minim.MONO, 1024); // 1024 で周波数分解能向上

  spectrumSetup(); // SpectrumAnalyzer.pde を初期化

  bitCrusher = new BitCrusherT();
  lowPass    = new LowPassT();

  sig = new TrumpetSig();
  out.addSignal(sig);

  port = new Serial(this, "/dev/cu.usbmodemF412FA63BC982", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(30, 30, 30);

  // エフェクト状態ラベルを作成して spectrumDraw へ渡す
  String effLabel = "";
  if (bitCrusher.active) effLabel += "[BitCrusher] ";
  if (lowPass.active)    effLabel += "[LowPass]";

  spectrumDraw("Trumpet", bpm, effLabel); // SpectrumAnalyzer.pde
}

// ─────────────────────────────────────────────────────────────
void serialEvent(Serial p) {
  String msg = p.readStringUntil('\n');
  if (msg != null) {
    msg = trim(msg);
    int[] data = int(split(msg, ','));
    if (data.length >= 3) {
      int pitch = data[0];
      int vel   = data[1];
      int dur   = data[2];

      if (pitch > 0) {
        sig.on(m2f(pitch), vel / 127.0, dur);

        // ノートオン間隔から BPM を推定
        if (lastNoteOnTime > 0) {
          int interval = millis() - lastNoteOnTime;
          if (interval > 100 && interval < 3000) bpm = 60000.0 / interval;
        }
        lastNoteOnTime = millis();
      } else {
        sig.off();
      }
    }
  }
}

float m2f(int m) {
  return 440.0 * pow(2.0, (m - 69) / 12.0);
}

// B キー: BitCrusher トグル / L キー: LowPass トグル
void keyPressed() {
  if (key == 'b' || key == 'B') bitCrusher.active = !bitCrusher.active;
  if (key == 'l' || key == 'L') lowPass.active    = !lowPass.active;
}

void mousePressed() {
  port.write('S');
  println("Start Signal Sent!");
}

// ─── トランペット音色シグナルクラス ──────────────────────────
// 8倍音合成 + ADSR（設計書 表2・表3 トランペットパラメータ）
class TrumpetSig implements AudioSignal {
  float[] harmonicAmps = {0.60, 1.00, 0.80, 0.70, 0.50, 0.40, 0.30, 0.15};
  float normFactor = 0;

  float f = 0, a = 0;
  float[] phases = new float[8];

  // ADSR（Attack:8ms / Decay:25ms / Sustain:0.80 / Release:120ms）
  float attackSec  = 0.008;
  float decaySec   = 0.025;
  float sustainLvl = 0.80;
  float releaseSec = 0.120;

  final int IDLE = 0, ATTACK = 1, DECAY = 2, SUSTAIN = 3, RELEASE = 4;
  int   adsrState  = IDLE;
  float envAmp     = 0;
  float decayRate  = 0, releaseRate = 0;

  int endTime = 0;

  TrumpetSig() {
    for (float h : harmonicAmps) normFactor += h;
  }

  void on(float freq, float amp, int dur) {
    f         = freq;
    a         = amp * 0.4;
    for (int i = 0; i < 8; i++) phases[i] = 0;
    adsrState = ATTACK;
    envAmp    = 0;
    endTime   = millis() + dur;
  }

  void off() { triggerRelease(); }

  void triggerRelease() {
    if (adsrState != IDLE && adsrState != RELEASE) {
      releaseRate = max(envAmp, 0.001) / (releaseSec * out.sampleRate());
      adsrState   = RELEASE;
    }
  }

  void generate(float[] samp) {
    float sr         = out.sampleRate();
    float attackRate = 1.0 / (attackSec * sr);

    if (millis() >= endTime && adsrState != IDLE && adsrState != RELEASE) {
      triggerRelease();
    }

    for (int i = 0; i < samp.length; i++) {
      switch (adsrState) {
        case ATTACK:
          envAmp += attackRate;
          if (envAmp >= 1.0) {
            envAmp    = 1.0;
            decayRate = (1.0 - sustainLvl) / (decaySec * sr);
            adsrState = DECAY;
          }
          break;
        case DECAY:
          envAmp -= decayRate;
          if (envAmp <= sustainLvl) { envAmp = sustainLvl; adsrState = SUSTAIN; }
          break;
        case SUSTAIN:
          break;
        case RELEASE:
          envAmp -= releaseRate;
          if (envAmp <= 0) { envAmp = 0; adsrState = IDLE; }
          break;
      }

      if (adsrState != IDLE || envAmp > 0) {
        // 8倍音正弦波合成: s(t) = (1/normFactor) Σ hk·sin(2π·f0·k·t)
        float wave = 0;
        for (int k = 0; k < 8; k++) {
          wave      += harmonicAmps[k] * sin(phases[k]);
          phases[k] += TWO_PI * f * (k + 1) / sr;
          if (phases[k] >= TWO_PI) phases[k] -= TWO_PI;
        }
        wave /= normFactor;

        float sample = wave * a * envAmp;
        sample = bitCrusher.process(sample); // エフェクト適用
        sample = lowPass.process(sample);
        samp[i] = sample;
      } else {
        samp[i] = 0;
      }
    }
  }
  void generate(float[] l, float[] r) { generate(l); }
}

// ─── ビットクラッシャーエフェクト ───────────────────────────
// 処理式: y[n] = round(x[n]·2^(B-1)) / 2^(B-1)
class BitCrusherT {
  boolean active   = false;
  int     bitDepth = 4;

  float process(float x) {
    if (!active) return x;
    float steps = pow(2, bitDepth - 1);
    return round(x * steps) / steps;
  }
}

// ─── ローパスフィルターエフェクト ───────────────────────────
// 差分方程式: y[n] = α·x[n] + (1-α)·y[n-1],  α = ωc/(ωc+fs)
class LowPassT {
  boolean active   = false;
  float   cutoffHz = 800;
  float   prevOut  = 0;

  float process(float x) {
    if (!active) return x;
    float wc    = TWO_PI * cutoffHz;
    float alpha = wc / (wc + out.sampleRate());
    prevOut = alpha * x + (1 - alpha) * prevOut;
    return prevOut;
  }
}
