// ============================================================
// Flute_wave.pde
// フルート音色シンセサイザー + スペクトルアナライザ
// タブ構成: Flute_wave.pde（本ファイル）+ SpectrumAnalyzer.pde
// ============================================================

import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
FluteNote fluteNote;

BitCrusherF bitCrusher;
LowPassF    lowPass;

// 設定フラグ（ピアノと同じ構造）
boolean harmonicActive = true;
boolean adsrActive     = true;

float bpm            = 0;
int   lastNoteOnTime = 0;

void setup() {
  size(900, 560);
  minim = new Minim(this);
  out   = minim.getLineOut(Minim.MONO, 1024);

  spectrumSetup(); // スペクトラムアナライザ初期化

  bitCrusher = new BitCrusherF();
  lowPass    = new LowPassF();
  fluteNote  = new FluteNote();
  out.addSignal(fluteNote);

  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(30, 30, 30);
  
  // ステータスバー用文字列の生成
  String effLabel = "";
  if (bitCrusher.active) effLabel += "[BitCrusher] ";
  if (lowPass.active)    effLabel += "[LowPass] ";
  if (harmonicActive)    effLabel += "[Harmonics] "; else effLabel += "[PureSine] ";
  if (adsrActive)        effLabel += "[ADSR] ";      else effLabel += "[Gate] ";
  
  spectrumDraw("Flute", bpm, effLabel);
}

void serialEvent(Serial p) {
  String msg = p.readStringUntil('\n');
  if (msg != null) {
    msg = trim(msg);
    if (msg.equals("RESET")) {
      fluteNote.reset();
      spectrumResetPeak(); // ピーク表示もリセット
      return;
    }
    int[] data = int(split(msg, ','));
    if (data.length >= 3) {
      if (data[0] > 0) {
        fluteNote.noteOn(midiToFreq(data[0]), data[1] / 127.0, data[2]);
        if (lastNoteOnTime > 0) {
          int interval = millis() - lastNoteOnTime;
          if (interval > 100 && interval < 3000) bpm = 60000.0 / interval;
        }
        lastNoteOnTime = millis();
      } else {
        fluteNote.noteOff();
      }
    }
  }
}

float midiToFreq(int midiNote) {
  return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}

void keyPressed() {
  if (key == 'b' || key == 'B') bitCrusher.active = !bitCrusher.active;
  if (key == 'l' || key == 'L') lowPass.active    = !lowPass.active;
  if (key == 'h' || key == 'H') harmonicActive    = !harmonicActive;
  if (key == 'e' || key == 'E') adsrActive        = !adsrActive;
}

void mousePressed() {
  port.write('S');
  spectrumResetPeak();
}

class HarmonicProfile {
  int[] targetHarmonics = { 1, 1, 1, 3, 3, 5 };
  int numHarmonics = targetHarmonics.length;

  // フルートは基音が強く、高次倍音が少ない丸い音が特徴
  float getAmplitude(int harmonicNum, float velocity) {
    if (harmonicNum == 1) return 1.0;
    if (harmonicNum == 2) return 0.4;
    if (harmonicNum == 3) return 0.2;
    return 0.2 / (harmonicNum * harmonicNum); // 4倍音以降は急速に小さくする
  }
}

class FluteNote implements AudioSignal {
  final int STATE_IDLE    = 0;
  final int STATE_ATTACK  = 1; 
  final int STATE_SUSTAIN = 3; 
  final int STATE_RELEASE = 2; 
  int currentState = STATE_IDLE;
  float currentVolume = 0;   
  
  // ─── 【改善】ご要望に応じたADSRパラメータの設定 ───
  float attackSpeed  = 0.0004; // Attack：少しだけ長めに設定（フワッと立ち上がる）
  float sustainLevel = 0.8;    // Sustain：やや高めに設定（最大音量の80%）
  float releaseDecay = 0.998;  // Release：少し長めに設定（美しい余韻、ノイズ防止）
  
  float frequency, velocity;
  HarmonicProfile profile;
  float[] phases;
  float[] amps;
  
  // フルート特有の表現
  float vibratoPhase = 0;
  float noiseAmp = 0;

  FluteNote() {
    profile = new HarmonicProfile();
    phases = new float[profile.numHarmonics];
    amps = new float[profile.numHarmonics];
  }

  void noteOn(float frequency, float velocity, int duration) {
    this.frequency = frequency;
    this.velocity = velocity;
    
    if (this.currentState == STATE_IDLE) {
      this.currentVolume = 0.0;
    }

    
    this.currentState = STATE_ATTACK;
    this.vibratoPhase = 0;
    this.noiseAmp = 0.0; // 息ノイズ（必要に応じて数値を設定してください）

    for (int h = 0; h < profile.numHarmonics; h++) {
      int hIdx = profile.targetHarmonics[h];

      
      amps[h] = profile.getAmplitude(hIdx, velocity) * 0.2; // 音割れ防止
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
        
        // ── ビブラートの計算 ──
        float currentFreq = frequency;
        vibratoPhase += TWO_PI * 5.0 / out.sampleRate(); // 約5Hzの揺れ
        if (vibratoPhase > TWO_PI) vibratoPhase -= TWO_PI;
        
        // 安定して吹いている時（Sustain時）にビブラートをかける
        if (currentState == STATE_SUSTAIN) {
           currentFreq += sin(vibratoPhase) * 2.5; 
        }
        
        // ── 倍音合成 ──
        int loopLimit = harmonicActive ? profile.numHarmonics : 1;
        for (int h = 0; h < loopLimit; h++) {
          int hIdx = profile.targetHarmonics[h];
          combinedWave += sin(phases[h]) * amps[h];
          
          phases[h] += TWO_PI * (currentFreq * hIdx) / out.sampleRate();
          if (phases[h] > TWO_PI) phases[h] -= TWO_PI;
        }
        
        // ── アタック時の息ノイズ（徐々に減衰） ──
        float noise = (random(2.0) - 1.0) * noiseAmp;
        noiseAmp *= 0.9992;
        combinedWave += noise;
      }

      float sample = combinedWave * currentVolume;
      sample = bitCrusher.process(sample);
      sample = lowPass.process(sample);
      samp[i] = sample;
    }
  }

  void updateEnvelope() {
    if (!adsrActive) {
      if (currentState == STATE_SUSTAIN || currentState == STATE_ATTACK) {
        currentVolume = 1.0;
        currentState = STATE_SUSTAIN;
      } else {
        currentVolume = 0;
      }
      return;
    }

    switch (currentState) {
      case STATE_ATTACK:
        // sustainLevel（0.8）に向かって、少し長めの時間をかけて立ち上げる
        currentVolume += attackSpeed;
        if (currentVolume >= sustainLevel) {
          currentVolume = sustainLevel;
          currentState = STATE_SUSTAIN; // Decayを挟まず直接Sustainへ（Decay: ゼロ）
        }
        break;

      case STATE_SUSTAIN:
        // やや高めに設定した音量をそのまま100%キープ
        currentVolume = sustainLevel;
        break;

      case STATE_RELEASE:
        // 少し長めの設定（0.998）で、フワッと滑らかに音を消していく
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

class BitCrusherF {
  boolean active   = false;
  int     bitDepth = 4;

  float process(float x) {
    if (!active) return x;
    float steps = pow(2, bitDepth - 1);
    return round(x * steps) / steps;
  }
}

class LowPassF {
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
