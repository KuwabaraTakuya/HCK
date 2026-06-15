#include "turn_table.h"

static int last_shousetsu = -1;  // 前回つまみが指していた小節
static unsigned long last_move_time = 0; // 最後につまみが動いた時間
static bool isOperating = false;     // 今つまみを操作中かどうかのフラグ

int update_conductor_shousetsu(int angle, int bpm, unsigned long currentTime, NoteData* score, int& scoreIdx, int scoreLen, bool& isPlaying, unsigned long& nextNoteTime) {
  
  int current_score = angle / 40;       // つまみの角度から現在の小節を計算
  int start_idx = current_score * 4;    // その小節の先頭の音符インデックス

  // 初回ループ時の初期化
  if (last_shousetsu == -1) {
    last_shousetsu = current_score;
  }

  // つまみが動いた瞬間の検知
  // つまみが別の小節に入ったら操作中とみなす
  if (current_score != last_shousetsu) {
    isOperating = true;
    last_move_time = currentTime; // 動いた時刻を保存
    last_shousetsu = current_score;

    // つまみが指している小節の頭にターゲットをロックし続ける
    scoreIdx = start_idx; 

    // Processingへ休符を送って音を止める
    tx(0, 0, 0);
  }

  // 手を離した場合の検知
  // 最後につまみが動いてから300msたったら再生を再開する
  if (isOperating && (currentTime - last_move_time > 300)) {
    isOperating = false;
    
    // 次の自動演奏を開始させる
    nextNoteTime = currentTime; 
  }

  // 通常の演奏処理の場合
  // つまみを操作していないときだけ自動で時間が進む
  if (isPlaying && !isOperating) {
    if (currentTime >= nextNoteTime) {
      if (scoreIdx < scoreLen) {
        // BPMと音符基準長から，実際の音符の長さ(ミリ秒)を計算
        unsigned long L_ms = (60000.0 / bpm) * score[scoreIdx].duration;
        
        // Processingへデータを送信
        tx(score[scoreIdx].pitch, 100, L_ms);

        nextNoteTime = currentTime + L_ms;
        scoreIdx++;
        
      } else {
        // 曲の終了
        isPlaying = false;
        scoreIdx = 0;
      }
    }
  }

  // 最新の小節番号を逆算して返す（1〜12）
  int current_shousetsu = (scoreIdx / 4) + 1;
  if (current_shousetsu > 12) {
    current_shousetsu = 1;
  }

  return current_shousetsu;
}

// データをシリアル出力する関数
void tx(int p, int v, unsigned long d) {
  Serial.print(p);
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(d);
}