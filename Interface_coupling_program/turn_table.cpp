#include "turn_table.h"


// 全マスが 0.5拍 で等間隔なので、声部をマス単位でずらすだけで輪唱が成立する。

static const int SLOTS_PER_MEASURE = 8;   // 1小節 = 8マス(0.5拍×8)
static const int NUM_MEASURES      = 8;   // 全8小節
static const float GRID_BEAT       = 0.5; // 1マスの長さ(拍)
static const int RINSHO_OFFSET1    = 16;  // 後続声部1の遅れ(マス) = 8拍 = 2小節
static const int RINSHO_OFFSET2    = 32;  // 後続声部2の遅れ(マス) = 16拍 = 4小節

static int last_shousetsu = -1;       // 前回つまみが指していた小節
static unsigned long last_move_time = 0;
static bool isOperating = false;      // 今つまみを操作中かどうか
static bool wasPlaying = false;       // 前回ループの再生状態(開始エッジ検知用)

// 指定インデックスのピッチを安全に取り出す(範囲外は休符0)
static int pitchAt(NoteData* score, int i, int scoreLen) {
  if (i >= 0 && i < scoreLen) return score[i].pitch;
  return 0;
}

int update_conductor_shousetsu(int angle, int bpm, int volume, int effects, unsigned long currentTime, NoteData* score, int& scoreIdx, int scoreLen, bool& isPlaying, unsigned long& nextNoteTime) {

  int current_score = angle / 45;                 // 0〜7 の小節
  if (current_score >= NUM_MEASURES) current_score = NUM_MEASURES - 1;
  int start_idx = current_score * SLOTS_PER_MEASURE;

  // 再生開始の瞬間(false→true)に内部状態をリセットして頭から綺麗に始める
  if (isPlaying && !wasPlaying) {
    scoreIdx = 0;
    nextNoteTime = currentTime;
    last_shousetsu = current_score;
    isOperating = false;
  }
  wasPlaying = isPlaying;

  if (last_shousetsu == -1) {
    last_shousetsu = current_score;
  }

  // つまみが別の小節に入ったらスクラブ操作とみなす
  if (current_score != last_shousetsu) {
    isOperating = true;
    last_move_time = currentTime;
    last_shousetsu = current_score;
    scoreIdx = start_idx;          // つまみの小節頭にロック
    tx(0, 0, 0, 0, 0,0);             // 休符を送って一旦止める
  }

  // 手を離して300ms経過したら自動演奏を再開
  if (isOperating && (currentTime - last_move_time > 300)) {
    isOperating = false;
    nextNoteTime = currentTime;
  }

  // 自動演奏(操作中でないときだけ時間が進む)
  if (isPlaying && !isOperating) {
    if (currentTime >= nextNoteTime) {
      // 先頭声部が末尾を過ぎても、後続声部が鳴り終わるまで(+RINSHO_OFFSET2)回し続ける
      if (scoreIdx < scoreLen + RINSHO_OFFSET2) {
        // 全マス共通の音長(累積誤差を避けるため nextNoteTime を基準に積む)
        unsigned long L_ms = (unsigned long)((60000.0 / bpm) * GRID_BEAT);

        int Pitch1     = pitchAt(score, scoreIdx, scoreLen);                   // 先頭声部(末尾以降は0=休符)
        int rinshoP1   = pitchAt(score, scoreIdx - RINSHO_OFFSET1, scoreLen);  // 2小節遅れ
        int rinshoP2   = pitchAt(score, scoreIdx - RINSHO_OFFSET2, scoreLen);  // 4小節遅れ

        tx(Pitch1, rinshoP1, rinshoP2, volume, effects, L_ms);

        nextNoteTime += L_ms;   // 予定時刻基準でテンポを保つ
        scoreIdx++;
      } else {
        // 曲の終了(全声部が鳴り終わった)
        isPlaying = false;
        scoreIdx = 0;
        tx(0, 0, 0, 0, 0, 0);      // 念のため全声部ノートオフ
      }
    }
  }

  // 小節番号を逆算して返す(1〜8)
  int current_shousetsu = (scoreIdx / SLOTS_PER_MEASURE) + 1;
  if (current_shousetsu > NUM_MEASURES) {
    current_shousetsu = NUM_MEASURES;
  }
  return current_shousetsu;
}

// データをシリアル出力する関数
void tx(int p1, int p2, int p3, int v, int e, unsigned long d) {
  Serial.print(p1);
  Serial.print(",");
  Serial.print(p2);
  Serial.print(",");
  Serial.print(p3);
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.print(e);
  Serial.print(",");
  Serial.println(d);
}
