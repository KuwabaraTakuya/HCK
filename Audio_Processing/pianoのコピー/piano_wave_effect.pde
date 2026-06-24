import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
PianoNote pianoNote;

BitCrusherF bitCrusher;
LowPassF    lowPass;

float bpm            = 0;
int   lastNoteOnTime = 0;

void setup() {
  size(900, 560);

  minim = new Minim(this);
  out   = minim.getLineOut(Minim.MONO, 1024);

  spectrumSetup();

  bitCrusher = new BitCrusherF();
  lowPass    = new LowPassF();
  pianoNote = new PianoNote();
  out.addSignal(pianoNote);

  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(30, 30, 30);
  String effLabel = "";
  if (bitCrusher.active) effLabel += "[BitCrusher] ";
  if (lowPass.active)    effLabel += "[LowPass]";
  spectrumDraw("Piano", bpm, effLabel);
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
// 3.3 クラス: HarmonicProfile（倍音構造の再定義）
// ==========================================
class HarmonicProfile {
  int[] targetHarmonics = {1, 1, 2, 2, 2, 2, 3, 3, 3, 4, 4, 8, 8};
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
// PianoNote（実践編のADSR理論に基づく修正）
// ==========================================
class PianoNote implements AudioSignal {
  // 状態定義
  final int STATE_IDLE    = 0;
  final int STATE_SUSTAIN = 2; // ピアノはAttack直後、すぐこの「ゆっくり減衰」に入る
  final int STATE_RELEASE = 4; 
  int currentState = STATE_IDLE;

  float currentVolume = 0;   
  
  // 【実践編の適用】
  // 押している間はキープせず、ゆーーっくりと0（Sustainレベル=0）に向かって減衰する速度（長めのDecay）
  float sustainDecay = 0.9999; 
  // 鍵盤を離したあと、ブチッと切れずにわずかに余韻を残してスッと消える速度（短めのRelease）
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
    
    // 【実践編の適用】Attackタイムはゼロ
    // 叩いた瞬間に一瞬で最大音量になり、そのまま持続（ゆっくり減衰）フェーズへ突入させる
    this.currentVolume = 1.0;
    this.currentState = STATE_SUSTAIN;
    
    for (int h = 0; h < profile.numHarmonics; h++) {
      int hIdx = profile.targetHarmonics[h];
      phases[h] = 0;
      amps[h] = profile.getAmplitude(hIdx, velocity) * 0.15; // 音割れ防止のため一律調整
    }
  }

  void noteOff() {
    if (currentState != STATE_IDLE) {
      currentState = STATE_RELEASE; // 鍵盤から指を離したらリリース（余韻）フェーズへ
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
        for (int h = 0; h < profile.numHarmonics; h++) {
          int hIdx = profile.targetHarmonics[h];
          combinedWave += sin(phases[h]) * amps[h];
          
          phases[h] += TWO_PI * (frequency * hIdx) / out.sampleRate();
          if (phases[h] > TWO_PI) phases[h] -= TWO_PI;
          
          amps[h] *= profile.getDecayTime(hIdx);
        }
      }

      float sample = combinedWave * currentVolume;
      sample = bitCrusher.process(sample);
      sample = lowPass.process(sample);
      samp[i] = sample;
    }
  }

  void updateEnvelope() {
    switch (currentState) {
      case STATE_SUSTAIN:
        // 鍵盤を押さえている間、一定の音量を維持することなく、ゆーっくり減衰（Sustainレベル=0へ向かう）
        currentVolume *= sustainDecay;
        if (currentVolume < 0.001) {
          currentVolume = 0;
          currentState = STATE_IDLE;
        }
        break;

      case STATE_RELEASE:
        // 鍵盤を離したあと、ブチッと切れずにわずかに余韻を残してスッと消える
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

void keyPressed() {
  if (key == 'b' || key == 'B') bitCrusher.active = !bitCrusher.active;
  if (key == 'l' || key == 'L') lowPass.active    = !lowPass.active;
}

void mousePressed() {
  port.write('S');
  println("Start Signal Sent!");
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
