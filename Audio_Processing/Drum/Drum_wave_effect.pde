import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
DrumNote drumNote;

void setup() {
  size(600, 300);
  background(0);
  
  minim = new Minim(this);
  
  
  out = minim.getLineOut(Minim.MONO, 256);
  
  drumNote = new DrumNote();
  out.addSignal(drumNote);
  
  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(0);
  stroke(0, 255, 0);
  strokeWeight(2);
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i, 0, out.bufferSize(), 0, width);
    float x2 = map(i+1, 0, out.bufferSize(), 0, width);
    float y1 = 150 + out.mix.get(i) * 120;
    float y2 = 150 + out.mix.get(i+1) * 120;
    line(x1, y1, x2, y2);
  }
}

void serialEvent(Serial p) {
  String msg = p.readStringUntil('\n');
  if (msg != null) {
    msg = trim(msg);
    if (msg.equals("RESET")) {
      drumNote.reset();
      return;
    }

    int[] data = int(split(msg, ','));
    if (data.length >= 3) {
      drumNote.noteOn(data[0], data[1] / 127.0, data[2]);
    }
  }
}

// ==========================================
// 1. HarmonicProfile: 打楽器の「胴鳴り」の倍音比率を定義
// ==========================================
class HarmonicProfile {
  int numHarmonics = 3;
  float getAmplitude(int index, float velocity) {
    float baseAmp = 1.0 / (index * index);
    float sharpness = 0.5 + (velocity * 0.5); 
    return baseAmp * sharpness;
  }
}

// ==========================================
// 2. DrumNote: 倍音合成とADSRを備えたドラムシグナル
// ==========================================
class DrumNote implements AudioSignal {
  // --- 状態の定義 ---
  final int STATE_IDLE    = 0; // 停止中
  final int STATE_ATTACK  = 1; // 立ち上がり（カチッという音）
  final int STATE_DECAY   = 2; // 減衰（余韻）
  int envelopeState = STATE_IDLE;

  float baseFreq = 0;
  float velocity = 0;
  int type = 0; // 0:Kick, 1:Snare, 2:Hi-hat
  
  HarmonicProfile profile;
  float[] phases;
  float[] amps;
  
  float adsrLevel = 0; // 現在の音量倍率 (0.0 - 1.0)
  float noiseAmp = 0;
  
  // パラメータ設定
  float attackStep = 0.2;     // アタックの速さ（大きいほど鋭い）
  float decayFactor = 0.9995; // 減衰の速さ（1に近いほど長い余韻）

  DrumNote() {
    profile = new HarmonicProfile();
    phases = new float[profile.numHarmonics];
    amps = new float[profile.numHarmonics];
  }

  void noteOn(int note, float velocity, int duration) {
    this.velocity = velocity;
    this.envelopeState = STATE_ATTACK; // 発音開始
    this.adsrLevel = 0;                // 音量ゼロからスタート
    
    // 楽器タイプの設定
    if (note == 42) { type = 2; noiseAmp = 0.6; decayFactor = 0.998; } // Hi-hat (短め)
    else if (note <= 36) { type = 0; baseFreq = 150; decayFactor = 0.9997; } // Kick (長め)
    else { type = 1; baseFreq = 180; noiseAmp = 0.5; decayFactor = 0.9992; } // Snare

    for (int i = 0; i < profile.numHarmonics; i++) {
      phases[i] = 0;
      amps[i] = profile.getAmplitude(i + 1, velocity);
    }
  }

  void reset() {
    envelopeState = STATE_IDLE;
    adsrLevel = 0;
  }

  void generate(float[] samp) {
    for (int i = 0; i < samp.length; i++) {
      
      // --- 1. ADSRエンベロープの計算 (状態遷移) ---
      if (envelopeState == STATE_ATTACK) {
        adsrLevel += attackStep;
        if (adsrLevel >= 1.0) {
          adsrLevel = 1.0;
          envelopeState = STATE_DECAY; // 最大音量になったら減衰へ
        }
      } 
      else if (envelopeState == STATE_DECAY) {
        adsrLevel *= decayFactor; // 徐々に音を小さくする
        if (adsrLevel < 0.001) {
          adsrLevel = 0;
          envelopeState = STATE_IDLE; // 音が消えたら停止
        }
      }

      // --- 2. 波形合成 (音源部分) ---
      float combinedWave = 0;
      if (envelopeState != STATE_IDLE) {
        if (type == 2) { // Hi-hat
          combinedWave = (random(2.0) - 1.0) * noiseAmp;
        } else { // Kick or Snare
          for (int h = 0; h < profile.numHarmonics; h++) {
            combinedWave += sin(phases[h]) * amps[h];
            phases[h] += TWO_PI * (baseFreq * (h + 1)) / out.sampleRate();
            if (phases[h] > TWO_PI) phases[h] -= TWO_PI;
          }
          if (type == 0) { // Kickのピッチ降下
            baseFreq = max(50, baseFreq * 0.9998);
          } else { // Snareのノイズ
            combinedWave += (random(2.0) - 1.0) * noiseAmp;
          }
        }
      }
      
      // 出力バッファに書き込み
      samp[i] = combinedWave * adsrLevel * velocity * 0.4;
    }
  }
  
  void generate(float[] l, float[] r) { generate(l); }
}

void mousePressed() {
  port.write('S');
  println("Start Signal Sent!");
}
