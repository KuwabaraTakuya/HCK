import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
TrumpetSig sig;

void setup() {
  size(600, 300);
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 512);
  sig = new TrumpetSig();
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
class TrumpetSig implements AudioSignal {
  float f=0, a=0, phase=0, filterAlpha=0.1;
  float prevOut=0;
  int endTime=0;

  void on(float freq, float amp, int dur) {
    f = freq;
    a = amp * 0.25;
    endTime = millis() + dur;
    filterAlpha = 0.05; // 最初はこもった音
  }

  void generate(float[] samp) {
    for (int i = 0; i < samp.length; i++) {
      if (millis() < endTime) {
        // 鋸歯状波
        float raw = 2.0 * (phase / TWO_PI - floor(phase / TWO_PI + 0.5));
        
        // 簡易ローパスフィルタ（アタックで明るい音へスイープ）
        float outVal = filterAlpha * raw + (1.0 - filterAlpha) * prevOut;
        prevOut = outVal;
        
        samp[i] = outVal * a;
        phase += TWO_PI * f / out.sampleRate();
        if(filterAlpha < 0.8) filterAlpha += 0.0001; 
      } else {
        samp[i] = 0;
      }
    }
  }
  void generate(float[] l, float[] r) { generate(l); }
  void off() { a = 0; }
}
// --- Processingの既存コードの一番下に追加 ---

// 画面をマウスクリックしたときに自動で呼ばれる関数
void mousePressed() {
  // Arduinoへ「スタート(S)」の合図を送信する
  port.write('S');
  println("Start Signal Sent!"); // 確認用にコンソールに表示
}
