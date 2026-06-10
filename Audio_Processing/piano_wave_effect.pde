import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
PianoNote pianoNote; // 設計書に基づき変更

void setup() {
  size(600, 300);
  background(0);
  
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 512);
  
  pianoNote = new PianoNote();
  out.addSignal(pianoNote); 
  
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
    float y1 = 150 + out.mix.get(i) * 150; 
    float y2 = 150 + out.mix.get(i+1) * 150;
    line(x1, y1, x2, y2);
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
        // 関数名と引数名を設計書に準拠
        pianoNote.noteOn(midiToFreq(data[0]), data[1] / 127.0, data[2]);
      } else {
        pianoNote.noteOff(); //
      }
    }
  }
}

// 設計書の関数名に準拠
float midiToFreq(int midiNote) {
  return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}

// ==========================================
// 3.3 クラス: HarmonicProfile（ピアノの音色の定義）
// ==========================================
class HarmonicProfile {
  int numHarmonics = 10; 

  // 強さに応じた倍音の振幅調整
  float getAmplitude(int index, float velocity) {
    float baseAmp = 1.0 / (index * index); 
    float sharpness = 0.2 + (velocity * 0.8 * (index * 0.5));
    return baseAmp * sharpness;
  }

  // 高域の倍音ほど減衰を早く設定
  float getDecayTime(int index) {
    return 1.0 - (0.00002 * index); // 減衰係数として実装
  }
}

// ==========================================
// 3.3 クラス: PianoNote（設計書の仕様に基づく実装）
// ==========================================
class PianoNote implements AudioSignal {
  // --- 1. 状態の定義（ピアノの動作フェーズ） ---
  final int STATE_IDLE    = 0; // 無音
  final int STATE_ATTACK  = 1; // 鍵盤を叩いた瞬間（音量が急上昇）
  final int STATE_SUSTAIN = 2; // 鍵盤を押している間（緩やかに減衰）
  final int STATE_RELEASE = 4; // 鍵盤を離した後（素早く消音）
  int currentState = STATE_IDLE;

  // --- 2. パラメータ設定 ---
  float currentVolume = 0;   // 現在の音量倍率 (0.0 - 1.0)
  float attackSpeed = 0.5;   // アタックの速さ
  float sustainDecay = 0.9999; // 押している間の減衰（1に近いほど長く響く）
  float releaseDecay = 0.995;  // 離した後の消える速さ
  
  float frequency, velocity;
  HarmonicProfile profile;
  float[] phases;
  float[] amps;

  PianoNote() {
    profile = new HarmonicProfile();
    phases = new float[profile.numHarmonics];
    amps = new float[profile.numHarmonics];
  }

  // 鍵盤が押された
  void noteOn(float frequency, float velocity, int duration) {
    this.frequency = frequency;
    this.velocity = velocity;
    this.currentState = STATE_ATTACK;
    
    // 倍音の初期化
    for (int i = 0; i < profile.numHarmonics; i++) {
      phases[i] = 0;
      amps[i] = profile.getAmplitude(i + 1, velocity) * 0.4;
    }
  }

  // 鍵盤が離された（または音の期限が切れた）
  void noteOff() {
    if (currentState != STATE_IDLE) {
      currentState = STATE_RELEASE;
    }
  }

  void reset() {
    currentState = STATE_IDLE;
    currentVolume = 0;
  }

  void generate(float[] samp) {
    for (int i = 0; i < samp.length; i++) {
      
      // 【ステップA】音量の状態更新（ADSR）
      updateEnvelope();

      // 【ステップB】波形合成（倍音計算）
      float combinedWave = 0;
      if (currentState != STATE_IDLE) {
        for (int h = 0; h < profile.numHarmonics; h++) {
          int hIdx = h + 1;
          combinedWave += sin(phases[h]) * amps[h];
          
          // 位相の更新
          phases[h] += TWO_PI * (frequency * hIdx) / out.sampleRate();
          if (phases[h] > TWO_PI) phases[h] -= TWO_PI;
          
          // ピアノ特有：高域ほど早く減衰する性質
          amps[h] *= profile.getDecayTime(hIdx);
        }
      }

      // 【ステップC】音量と波形を合成して出力
      samp[i] = combinedWave * currentVolume;
    }
  }

  // --- 音量の変化ロジック（ここが分かりやすさのポイント） ---
  void updateEnvelope() {
    switch (currentState) {
      case STATE_ATTACK:
        currentVolume += attackSpeed;
        if (currentVolume >= 1.0) {
          currentVolume = 1.0;
          currentState = STATE_SUSTAIN; // 最大音量になったら持続フェーズへ
        }
        break;

      case STATE_SUSTAIN:
        currentVolume *= sustainDecay; // 押していてもピアノは少しずつ音が小さくなる
        break;

      case STATE_RELEASE:
        currentVolume *= releaseDecay; // 離すと素早く消える
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

void mousePressed() {
  port.write('S');
  println("Start Signal Sent!");
}
