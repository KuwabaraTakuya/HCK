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

// 倍音構造のON/OFFフラグ（初期値はON）
boolean harmonicActive = true;

// BPM 推定
float bpm           = 0;
int   lastNoteOnTime = 0;

// ─────────────────────────────────────────────────────────────
void setup() {
  size(900, 560);
  minim = new Minim(this);
  
  // 【ノイズ対策】バッファサイズを 2048 に拡張し、描画や通信負荷による音切れを防止
  out   = minim.getLineOut(Minim.MONO, 2048); 

  spectrumSetup(); // SpectrumAnalyzer.pde を初期化

  bitCrusher = new BitCrusherT();
  lowPass    = new LowPassT();

  sig = new TrumpetSig();
  out.addSignal(sig);

  port = new Serial(this,"/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(30, 30, 30);

  // エフェクト状態ラベルを作成して spectrumDraw へ渡す
  String effLabel = "";
  if (bitCrusher.active) effLabel += "[BitCrusher] ";
  if (lowPass.active)    effLabel += "[LowPass] ";
  if (harmonicActive)    effLabel += "[Harmonics] ";
  else                   effLabel += "[PureSine] ";

  spectrumDraw("Trumpet", bpm, effLabel);
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

// B キー: BitCrusher トグル / L キー: LowPass トグル / H キー: Harmonics トグル
void keyPressed() {
  if (key == 'b' || key == 'B') bitCrusher.active = !bitCrusher.active;
  if (key == 'l' || key == 'L') lowPass.active    = !lowPass.active;
  if (key == 'h' || key == 'H') harmonicActive = !harmonicActive;
}

void mousePressed() {
  port.write('S');
  println("Start Signal Sent!");
}

// ==========================================
// クラス: HarmonicProfile（倍音構造の定義）
// ==========================================
class HarmonicProfile {
  // 【バグ修正済み】1〜8倍音を重複なく正しく合成
  int[] targetHarmonics = {1, 1, 1, 2, 2, 3, 3, 4}; 
  int numHarmonics = targetHarmonics.length;
  float[] harmonicAmps = {1.00, 1.00, 0.80, 0.60, 0.60, 0.40, 0.30, 0.30}; // トランペット固有の倍音比
  float normFactor = 0;

  HarmonicProfile() {
    for (float h : harmonicAmps) normFactor += h;
  }

  float getAmplitude(int harmonicNum, float velocity) {
    return harmonicAmps[harmonicNum - 1] / normFactor;
  }

  float getDecayTime(int harmonicNum) {
    return 1.0;
  }
}

// ─── トランペット音色シグナルクラス ──────────────────────────
class TrumpetSig implements AudioSignal {
  float f = 0, a = 0;
  
  HarmonicProfile profile;
  float[] phases;
  float[] amps;

  // ── 【要望反映】ADSRパラメータの設定変更 ──
  float attackSec  = 0.050; // 50ms（やや遅め：吹き込み感を表現）
  float decaySec   = 0.015; // 15ms（やや短め：メリハリを出す）
  float sustainLvl = 0.90;  // 0.90（やや高め：力強い持続音）
  float releaseSec = 0.070; // 70ms（やや短め：スッと歯切れよく消える）

  final int IDLE = 0, ATTACK = 1, DECAY = 2, SUSTAIN = 3, RELEASE = 4;
  int   adsrState  = IDLE;
  float envAmp     = 0;

  // 【ノイズ対策】オーディオスレッドと完全同期して残り時間を数えるサンプルカウンター
  int samplesLeft = 0;

  TrumpetSig() {
    profile = new HarmonicProfile();
    phases = new float[profile.numHarmonics];
    amps = new float[profile.numHarmonics];
  }

  void on(float freq, float amp, int dur) {
    float sr = out.sampleRate();
    f        = freq;
    a        = amp * 0.4;
    
    // 【ノイズ対策】前の音が残っている場合はその音量を引き継いで次のアタックを開始（プチプチ音防止）
    adsrState = ATTACK;
    
    // 指定のミリ秒を正確なサンプル数に変換してセット
    samplesLeft = int((dur / 1000.0) * sr);

    // 【ノイズ対策】波形の連続性を維持するため、phases（位相）はゼロリセットせずそのまま継続
    for (int h = 0; h < profile.numHarmonics; h++) {
      int hIdx = profile.targetHarmonics[h];
      amps[h] = profile.getAmplitude(hIdx, amp);
    }
  }

  void off() { 
    triggerRelease();
  }

  void triggerRelease() {
    if (adsrState != IDLE && adsrState != RELEASE) {
      adsrState   = RELEASE;
      samplesLeft = 0; 
    }
  }

  void generate(float[] samp) {
    float sr         = out.sampleRate();
    float attackRate = 1.0 / (attackSec * sr);
    float decayRate  = (1.0 - sustainLvl) / (decaySec * sr);

    for (int i = 0; i < samp.length; i++) {
      
      // サンプル数ベースでノートの長さを1サンプルずつ減算（正確な時間同期）
      if (adsrState != IDLE && adsrState != RELEASE) {
        if (samplesLeft > 0) {
          samplesLeft--;
        } else {
          triggerRelease(); 
        }
      }

      // ADSR エンベロープ制御
      switch (adsrState) {
        case ATTACK:
          envAmp += attackRate;
          if (envAmp >= 1.0) {
            envAmp    = 1.0;
            adsrState = DECAY;
          }
          break;
        case DECAY:
          envAmp -= decayRate;
          if (envAmp <= sustainLvl) { 
            envAmp    = sustainLvl; 
            adsrState = SUSTAIN;
          }
          break;
        case SUSTAIN:
          break;
        case RELEASE:
          // 現在のエンベロープ音量から滑らかにフェードアウト
          float releaseRate = max(envAmp, 0.001) / (releaseSec * sr);
          envAmp -= releaseRate;
          if (envAmp <= 0) { 
            envAmp    = 0; 
            adsrState = IDLE;
          }
          break;
      }

      float combinedWave = 0;

      if (adsrState != IDLE || envAmp > 0) {
        int loopLimit = harmonicActive ? profile.numHarmonics : 1;
        
        for (int h = 0; h < loopLimit; h++) {
          int hIdx = profile.targetHarmonics[h];
          combinedWave += sin(phases[h]) * amps[h];
          
          phases[h] += TWO_PI * f * hIdx / sr;
          if (phases[h] >= TWO_PI) phases[h] -= TWO_PI;
          
          amps[h] *= profile.getDecayTime(hIdx);
        }

        float sample = combinedWave * a * envAmp;
        sample = bitCrusher.process(sample); // エフェクト適用
        sample = lowPass.process(sample);
        samp[i] = sample;
      } else {
        samp[i] = 0;
      }
    }
  }
  void generate(float[] l, float[] r) { generate(l);
  }
}

// ─── ビットクラッシャーエフェクト ───────────────────────────
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
