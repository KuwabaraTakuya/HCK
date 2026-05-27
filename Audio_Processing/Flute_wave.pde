import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
FluteSig sig;

void setup() {
  size(600, 300);
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 512);
  sig = new FluteSig();
  out.addSignal(sig);
  
  // ポート名を指定のものに設定
  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 115200);
  port.bufferUntil('\n');
}

void draw() {
  background(0);
  stroke(0, 255, 0); // 緑色の線で波形を描画
  strokeWeight(2);
  
  // 再生中のオーディオバッファを画面に描画
  for (int i = 0; i < out.bufferSize() - 1; i++) {
    float x1 = map(i, 0, out.bufferSize(), 0, width);
    float x2 = map(i+1, 0, out.bufferSize(), 0, width);
    float y1 = 150 + out.mix.get(i) * 100;
    float y2 = 150 + out.mix.get(i+1) * 100;
    line(x1, y1, x2, y2);
  }
}

// シリアルポートからデータが届いた時の処理
void serialEvent(Serial p) {
  String msg = p.readStringUntil('\n');
  if (msg != null) {
    msg = trim(msg);
    int[] data = int(split(msg, ','));
    if (data.length >= 3) {
      int pitch = data[0];
      int vel = data[1];
      int dur = data[2];
      
      if (pitch > 0) {
        float freq = m2f(pitch); // MIDIノート番号を周波数に変換
        sig.on(freq, vel / 127.0, dur); // 発音
      } else {
        sig.off(); 
      }
    }
  }
}

float m2f(int m) {
  return 440.0 * pow(2.0, (m - 69) / 12.0);
}

// 正弦波の複数合成を行う自作シグナルクラス
class FluteSig implements AudioSignal {
  float f=0, a=0, phase=0, noiseAmp=0;
  int endTime=0;

  void on(float freq, float amp, int dur) {
    f = freq;
    a = amp * 0.4;
    endTime = millis() + dur;
    noiseAmp = 0.2; // アタック時の息ノイズ
  }
  
  void off() { a = 0; }

  void generate(float[] samp) {
    for (int i = 0; i < samp.length; i++) {
      if (millis() < endTime) {
        // 6Hzのヴィブラート
        float vibrato = sin(TWO_PI * 6.0 * (millis()/1000.0)) * 2.0;
        float currentFreq = f + vibrato;
        
        // 正弦波(90%) + 三角波(10%)
        float sine = sin(phase);
        float tri = (2.0/PI) * asin(sin(phase));
        float wave = (sine * 0.9) + (tri * 0.1);
        
        // 息のノイズ（徐々に減衰）
        float noise = (random(2.0) - 1.0) * noiseAmp;
        
        samp[i] = (wave + noise) * a;
        phase += TWO_PI * currentFreq / out.sampleRate();
        noiseAmp *= 0.9995; 
      } else {
        samp[i] = 0;
        a *= 0.9; // 余韻
      }
    }
  }
  void generate(float[] l, float[] r) { generate(l); }
}
// --- Processingの既存コードの一番下に追加 ---

// 画面をマウスクリックしたときに自動で呼ばれる関数
void mousePressed() {
  // Arduinoへ「スタート(S)」の合図を送信する
  port.write('S');
  println("Start Signal Sent!"); // 確認用にコンソールに表示
}
