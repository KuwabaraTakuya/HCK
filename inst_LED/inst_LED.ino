#include "inst_LEDMatrix.h"

struct NoteData {
  int pitch;      // 音階（MIDIノート番号, 0は休符）
  float duration; // 音符の基準長（1.0 = 4分音符, 0.5 = 8分音符）
};

// カエルの歌 楽譜データ
NoteData score[] = {
  {60, 1.0}, {62, 1.0}, {64, 1.0}, {65, 1.0}, {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0},
  {64, 1.0}, {65, 1.0}, {67, 1.0}, {69, 1.0}, {67, 1.0}, {65, 1.0}, {64, 1.0}, {0, 1.0},
  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},
  {60, 0.5}, {60, 0.5}, {62, 0.5}, {62, 0.5}, {64, 0.5}, {64, 0.5}, {65, 0.5}, {65, 0.5}, 
  {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0}
};

int len = 36;
int idx = 0;

bool isPlaying = false;

int current_bpm = 120; // 指揮者側から送信されたbpm
float note_progress = 0.0; // 現在の音符の描画進捗（0.25 = 1ピクセル分）
unsigned long last_shift_time = 0;

void setup() {
  Serial.begin(115200);
  initLEDMatrix();
  // next_note_timeはもう使わないので消去してOKです
}

void loop() {
  unsigned long current_time = millis();

  // 1. コマンドの受信（Sが来たら再生開始）
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      isPlaying = true;       
      idx = 0;                
      note_progress = 0.0;    
      last_shift_time = current_time;
    }
  }

  // 再生中(isPlaying == true)でなければ、ここで処理を終わらせて何も描画しない
  if (!isPlaying) {
    return;
  }

  // 2. 音とLEDの同期処理
  unsigned long pixel = (60000.0 / current_bpm) / 4; 

  // ピクセルを進める時間になったら
  if (current_time - last_shift_time >= pixel) {
    
    // まだ楽譜が終わっていなければ
    if (idx < len) {
      
      int pitch = score[idx].pitch;
      float duration = score[idx].duration;
      
      //note_progressが0.0のとき，led描画を現在のbpmに合わせて描画する
      if (note_progress == 0.0) {
         unsigned long L_ms = (60000.0 / current_bpm) * duration;
         tx(pitch, 100, L_ms); // Processingへ送信
      }
      
      // 1. MIDIノート(pitch)をLEDの高さに変換
      int height = ledheight(pitch);
      
      // 2. LEDの描画（1列シフトして新しいバーを描き込む）
      updateNoteDisplay(height); 
      
      // 3. 描画した分だけ進捗を進める
      note_progress += 0.25;
      
      // 4. 今の音符の長さ分をすべて描ききったら，次の音符へ進む準備
      if (note_progress >= duration - 0.01) {
        idx++;               // 次の音符
        note_progress = 0.0; // 進捗リセット
      }

    } else {
      // 楽譜が最後まで終わったら
      updateNoteDisplay(0); // 空のバーを流す
    }
    
    // 時間を更新（先輩の魔法！）
    last_shift_time += pixel; 
  }
}

// データをシリアル出力する関数
void tx(int p, int v, unsigned long d) {
  Serial.print(p);
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(d);
}