// 設計書に準拠した楽譜データの構造体（後でLEDテープ制御にも対応可能）
struct NoteData {
  int pitch;      // 音階（MIDIノート番号, 0は休符）
  float duration; // 音符の基準長（1.0 = 4分音符, 0.5 = 8分音符）
};

// カエルの歌(ドラム) 楽譜データ
NoteData score[] = {
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 1-4拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 5-8拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 9-12拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 13-16拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 17-20拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 21-24拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, // 25-28拍
  {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}, {42, 0.5}  // 29-32拍
};

int len = 64;
int idx = 0;

bool isPlaying = false;

unsigned long next_note_time = 0; // 次の音符を鳴らす時刻（millis用）
int current_bpm = 120; // 指揮者から受信するBPM（初期値）

void setup() {
  Serial.begin(115200);
  delay(3000);
  next_note_time = millis();
}

void loop() {
  unsigned long current_time = millis();


if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      isPlaying = true;       // 再生フラグをON
      idx = 0;                // 曲を最初(0番目)にリセット
      next_note_time = millis(); // 押した瞬間の時間を基準にする
    }
  }

  // 再生中でなければ、ここから下の処理はせずに終了
  if (!isPlaying) {
    return;
  }




  if (current_time >= next_note_time) {
    if (idx < len) {
      // BPMと音符基準長から、実際の音符の長さ(ミリ秒)を計算
      unsigned long L_ms = (60000.0 / current_bpm) * score[idx].duration;
      
      // 1. Processingへデータ送信（長さは計算したミリ秒を送る）
      tx(score[idx].pitch, 100, L_ms);


      // 3. 次の音符を鳴らす時刻を更新
      next_note_time = current_time + L_ms;
      idx++;
    } else {
      // 曲の終了（2秒待ってからループ）
      idx = 0;
      next_note_time = current_time + 2000;
    }
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

