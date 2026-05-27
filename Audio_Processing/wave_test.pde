import ddf.minim.*;
import ddf.minim.signals.*;
import processing.serial.*;

Minim minim;
AudioOutput out;
Serial port;
Sig sig; // 自作の合成波シグナルクラス

void setup() {
  size(600, 300);
  background(0);
  
  minim = new Minim(this);
  out = minim.getLineOut(Minim.MONO, 512);
  
  sig = new Sig();
  out.addSignal(sig); // オーディオ出力に自作信号を追加
  
  // 指定されたシリアルポートに接続
  port = new Serial(this, "/dev/cu.usbmodem34B7DA61F2542", 9600);
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
class Sig implements AudioSignal {
  float f = 0, a = 0, t = 0;
  int endTime = 0;

  // 音を鳴らす処理
  void on(float freq, float amp, int dur) {
    f = freq;
    a = amp * 0.3; // クリップ（音割れ）防止のため全体の音量を調節
    endTime = millis() + dur; // 指定の長さが切れる時刻を記録
  }

  // 音を止める処理
  void off() {
    a = 0;
  }

  // バッファに数式合成した波形を書き込むメイン処理（モノラル）
  void generate(float[] samp) {
    // 指定された長さを過ぎたらエフェクト（ADSR）なしで即座に消音
    if (millis() > endTime) {
      a = 0;
    }
    
    for (int i = 0; i < samp.length; i++) {
      if (a == 0) {
        samp[i] = 0;
      } else {
        float w = 2 * PI * f * t;

        samp[i] = (sin(w) + sin(w*2)*0.5 + sin(w*3)*0.25 + sin(w*4)*0.1) * a;
        
        t += 1.0 / out.sampleRate(); // 位相（時間）を進める
      }
    }
  }

  void generate(float[] l, float[] r) {
    generate(l);
    arraycopy(l, r);
  }
}
