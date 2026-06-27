import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim       minim;
AudioOutput out;
Serial      port;
PianoNote   pianoNote;

// 全エフェクトクラスのインスタンス
BitCrusherF      bitCrusher;
DistortionEffect distortionEff; // 【追加】
LowPassF         lowPass;
CompressorEffect compressorEff; // 【追加】
DelayEffect      delayEff;      // 【追加】
ReverbEffect     reverbEff;     // 【追加】

// ── 倍音構造とADSR制御のON/OFFフラグ（初期設定のまま維持） ──
boolean harmonicActive = true;
boolean adsrActive     = true;

float bpm            = 0;
int   lastNoteOnTime = 0;

// ── 【追加】画面上のGUIボタン用パラメータ（8つのボタン用に最適化） ──────
String[] btnLabels = {"BitCrusher", "Distortion", "LowPass", "Compressor", "Delay", "Reverb", "Harmonics", "ADSR"};
int btnCount   = btnLabels.length;
int btnW       = 92;  // ボタン幅
int btnH       = 25;
int btnY       = 526; // アナライザー下部の配置エリア
int btnSpacing = 10;  // ボタン同士の間隔
int startX     = 50;  // 左端の開始位置

// ─────────────────────────────────────────────────────────────
void setup() {
  size(900, 560);

  minim = new Minim(this);
  // エフェクト増加による処理負荷での音切れを防ぐため、バッファサイズを2048に拡張
  out   = minim.getLineOut(Minim.MONO, 2048);

  spectrumSetup();

  bitCrusher    = new BitCrusherF();
  distortionEff = new DistortionEffect(); // 【追加】
  lowPass       = new LowPassF();
  compressorEff = new CompressorEffect(); // 【追加】
  delayEff      = new DelayEffect();      // 【追加】
  reverbEff     = new ReverbEffect();     // 【追加】
  
  pianoNote  = new PianoNote();
  out.addSignal(pianoNote);

  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(30, 30, 30);
  
  // ── 現在の各機能のON/OFF状態をステータスバー用の文字列にまとめる（追加分も反映） ──
  String effLabel = "";
  if (bitCrusher.active)    effLabel += "[BitCrusher] "; 
  if (distortionEff.active) effLabel += "[Distortion] "; // 【追加】
  if (lowPass.active)       effLabel += "[LowPass] ";
  if (compressorEff.active) effLabel += "[Compressor] "; // 【追加】
  if (delayEff.active)      effLabel += "[Delay] ";      // 【追加】
  if (reverbEff.active)     effLabel += "[Reverb] ";     // 【追加】
  if (harmonicActive)       effLabel += "[Harmonics] "; else effLabel += "[PureSine] ";
  if (adsrActive)           effLabel += "[ADSR] ";       else effLabel += "[Gate] ";
  
  spectrumDraw("Piano", bpm, effLabel);

  // ── 【追加】画面下部へのGUIボタン描画処理 ─────────────────────────────────
  for (int i = 0; i < btnCount; i++) {
    int bx = startX + i * (btnW + btnSpacing);
    boolean isOn = false;
    
    if (i == 0) isOn = bitCrusher.active;
    if (i == 1) isOn = distortionEff.active;
    if (i == 2) isOn = lowPass.active;
    if (i == 3) isOn = compressorEff.active;
    if (i == 4) isOn = delayEff.active;
    if (i == 5) isOn = reverbEff.active;
    if (i == 6) isOn = harmonicActive;
    if (i == 7) isOn = adsrActive;
    
    if (isOn) {
      fill(46, 204, 113); // ONの時は黄緑色
      stroke(39, 174, 96);
    } else {
      fill(60, 60, 60);    // OFFの時は暗い灰色
      stroke(90, 90, 90);
    }
    strokeWeight(1);
    rect(bx, btnY, btnW, btnH, 4);
    
    fill(255);
    textSize(11);
    textAlign(CENTER, CENTER);
    text(btnLabels[i], bx + btnW/2, btnY + btnH/2);
  }
}

void serialEvent(Serial p) {
  String msg = p.readStringUntil('\n');
  if (msg != null) {
    msg = trim(msg);
    if (msg.equals("RESET")) {
      pianoNote.reset();
      return;
    }

    int[] data = int(split(msg, ','));
    if (data.length >= 3) {
      if (data[0] > 0) {
        pianoNote.noteOn(midiToFreq(data[0]), data[1] / 127.0, data[2]);
        if (lastNoteOnTime > 0) {
          int interval = millis() - lastNoteOnTime;
          if (interval > 100 && interval < 3000) bpm = 60000.0 / interval;
        }
        lastNoteOnTime = millis();
      } else {
        pianoNote.noteOff();
      }
    }
  }
}

float midiToFreq(int midiNote) {
  return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}

// ==========================================
// 3.3 クラス: HarmonicProfile（倍音構造の定義）
// ※コード内容の変更なし
// ==========================================
class HarmonicProfile {
  int[] targetHarmonics = {1, 1, 1, 1, 1, 1, 2, 2, 3,  4, 4};
  int numHarmonics = targetHarmonics.length;

  float getAmplitude(int harmonicNum, float velocity) {
    float baseAmp = 1.0 / (harmonicNum * harmonicNum);
    float sharpness = 0.2 + (velocity * 0.8 * (harmonicNum * 0.5));
    return baseAmp * sharpness;
  }

  float getDecayTime(int harmonicNum) {
    return 1.0 - (0.00002 * harmonicNum);
  }
}

// ==========================================
// PianoNote（ADSRおよび倍音合成の制御）
// ※倍音合成やエンベロープ制御ロジックの変更なし
// ==========================================
class PianoNote implements AudioSignal {
  final int STATE_IDLE    = 0;
  final int STATE_SUSTAIN = 2; 
  final int STATE_RELEASE = 4; 
  int currentState = STATE_IDLE;
  float currentVolume = 0;   
  
  float sustainDecay = 0.9999;
  float releaseDecay = 0.99;
  
  float frequency, velocity;
  HarmonicProfile profile;
  float[] phases;
  float[] amps;

  PianoNote() {
    profile = new HarmonicProfile();
    phases = new float[profile.numHarmonics];
    amps = new float[profile.numHarmonics];
  }

  void noteOn(float frequency, float velocity, int duration) {
    this.frequency = frequency;
    this.velocity = velocity;
    this.currentVolume = 1.0;
    this.currentState = STATE_SUSTAIN;

    for (int h = 0; h < profile.numHarmonics; h++) {
      int hIdx = profile.targetHarmonics[h];
      phases[h] = 0;
      amps[h] = profile.getAmplitude(hIdx, velocity) * 0.15; 
    }
  }

  void noteOff() {
    if (currentState != STATE_IDLE) {
      if (adsrActive) {
        currentState = STATE_RELEASE;
      } else {
        currentState = STATE_IDLE;
        currentVolume = 0;
      }
    }
  }

  void reset() {
    currentState = STATE_IDLE;
    currentVolume = 0;
  }

  void generate(float[] samp) {
    for (int i = 0; i < samp.length; i++) {
      
      updateEnvelope();
      float combinedWave = 0;
      
      if (currentState != STATE_IDLE) {
        int loopLimit = harmonicActive ? profile.numHarmonics : 1;
        
        for (int h = 0; h < loopLimit; h++) {
          int hIdx = profile.targetHarmonics[h];
          combinedWave += sin(phases[h]) * amps[h];
          
          phases[h] += TWO_PI * (frequency * hIdx) / out.sampleRate();
          if (phases[h] > TWO_PI) phases[h] -= TWO_PI;
          
          amps[h] *= profile.getDecayTime(hIdx);
        }
      }

      float sample = combinedWave * currentVolume;
      
      // ── エフェクト・パイプライン（追加分のエフェクトを直列に繋ぐ） ──
      sample = bitCrusher.process(sample);
      sample = distortionEff.process(sample); // 【追加】
      sample = lowPass.process(sample);
      sample = compressorEff.process(sample); // 【追加】
      sample = delayEff.process(sample);      // 【追加】
      sample = reverbEff.process(sample);     // 【追加】
      
      samp[i] = sample;
    }
  }

  void updateEnvelope() {
    if (!adsrActive) {
      if (currentState == STATE_SUSTAIN) {
        currentVolume = 1.0;
      } else {
        currentVolume = 0;
      }
      return;
    }

    switch (currentState) {
      case STATE_SUSTAIN:
        currentVolume *= sustainDecay;
        if (currentVolume < 0.001) {
          currentVolume = 0;
          currentState = STATE_IDLE;
        }
        break;

      case STATE_RELEASE:
        currentVolume *= releaseDecay;
        if (currentVolume < 0.001) {
          currentVolume = 0;
          currentState = STATE_IDLE;
        }
        break;

      case STATE_IDLE:
        currentVolume = 0;
        break;
    }
  }

  void generate(float[] l, float[] r) { generate(l); }
}

// ── キーボード操作の割り当てを追加 ─────────────────────
void keyPressed() {
  if (key == 'b' || key == 'B') bitCrusher.active    = !bitCrusher.active;
  if (key == 't' || key == 'T') distortionEff.active = !distortionEff.active; // 【追加】
  if (key == 'l' || key == 'L') lowPass.active       = !lowPass.active;
  if (key == 'c' || key == 'C') compressorEff.active = !compressorEff.active; // 【追加】
  if (key == 'd' || key == 'D') delayEff.active      = !delayEff.active;      // 【追加】
  if (key == 'r' || key == 'R') reverbEff.active     = !reverbEff.active;     // 【追加】
  if (key == 'h' || key == 'H') harmonicActive       = !harmonicActive;
  if (key == 'e' || key == 'E') adsrActive           = !adsrActive;
}

void mousePressed() {
  // ── 【変更】画面上のボタンがクリックされたかの判定 ─────────────────────
  for (int i = 0; i < btnCount; i++) {
    int bx = startX + i * (btnW + btnSpacing);
    if (mouseX >= bx && mouseX <= bx + btnW && mouseY >= btnY && mouseY <= btnY + btnH) {
      if (i == 0) bitCrusher.active    = !bitCrusher.active;
      if (i == 1) distortionEff.active = !distortionEff.active;
      if (i == 2) lowPass.active       = !lowPass.active;
      if (i == 3) compressorEff.active = !compressorEff.active;
      if (i == 4) delayEff.active      = !delayEff.active;
      if (i == 5) reverbEff.active     = !reverbEff.active;
      if (i == 6) harmonicActive       = !harmonicActive;
      if (i == 7) adsrActive           = !adsrActive;
      return; // ボタン領域をクリックした場合はArduinoへのシグナル送信をスキップ
    }
  }

  port.write('S');
  println("Start Signal Sent!");
}

// ============================================================
// エフェクトコンポーネントクラス群
// ============================================================
class BitCrusherF {
  boolean active   = false;
  int     bitDepth = 4;

  float process(float x) {
    if (!active) return x;
    float steps = pow(2, bitDepth - 1);
    return round(x * steps) / steps;
  }
}

// 【追加】ディストーション
class DistortionEffect {
  boolean active     = false;
  float   threshold  = 0.15;
  float   makeupGain = 1.8;

  float process(float x) {
    if (!active) return x;
    float clipped = x;
    if (clipped > threshold)       clipped = threshold;
    else if (clipped < -threshold) clipped = -threshold;
    return clipped * makeupGain;
  }
}

class LowPassF {
  boolean active   = false;
  float   cutoffHz = 100; // 元コードの100Hz設定を維持
  float   prevOut  = 0;

  float process(float x) {
    if (!active) return x;
    float wc    = TWO_PI * cutoffHz;
    float alpha = wc / (wc + out.sampleRate());
    prevOut = alpha * x + (1 - alpha) * prevOut;
    return prevOut;
  }
}

// 【追加】コンプレッサー
class CompressorEffect {
  boolean active     = false;
  float   threshold  = 0.12;
  float   ratio      = 4.0;
  float   makeupGain = 1.3;

  float process(float x) {
    if (!active) return x;
    float absX = abs(x);
    if (absX > threshold) {
      float over = absX - threshold;
      float compressedAbsX = threshold + (over / ratio);
      x = (x >= 0) ? compressedAbsX : -compressedAbsX;
    }
    return x * makeupGain;
  }
}

// 【追加】ディレイ
class DelayEffect {
  boolean active   = false;
  float[] buffer;
  int     writePos = 0;
  float   feedback = 0.40; 
  float   mix      = 0.35; 

  DelayEffect() {
    buffer = new float[12000];
  }

  float process(float x) {
    if (!active) return x;
    float delayedSample = buffer[writePos];
    buffer[writePos] = x + delayedSample * feedback;
    writePos = (writePos + 1) % buffer.length;
    return x + delayedSample * mix;
  }
}

// 【追加】リバーブ
class ReverbEffect {
  boolean active = false;
  float[][] buffers;
  int[] writePositions;
  float mix = 0.25; 

  ReverbEffect() {
    int[] lengths = {1013, 1223, 1453, 1721}; 
    buffers = new float[4][];
    writePositions = new int[4];
    for (int i = 0; i < 4; i++) {
      buffers[i] = new float[lengths[i]];
    }
  }

  float process(float x) {
    if (!active) return x;
    float outSum = 0;
    for (int i = 0; i < 4; i++) {
      float delayed = buffers[i][writePositions[i]];
      buffers[i][writePositions[i]] = x + delayed * 0.55; 
      writePositions[i] = (writePositions[i] + 1) % buffers[i].length;
      outSum += delayed;
    }
    return x + (outSum / 4.0) * mix;
  }
}
