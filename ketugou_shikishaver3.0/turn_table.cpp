#include "turn_table.h"

static int last_shousetsu = -1;          // 前回つまみが指していた小節
static unsigned long last_move_time = 0; // 最後につまみが動いた時間

int update_conductor_shousetsu(int angle, unsigned long currentTime, bool& isOperating) {
  // 角度（0〜359度）から、カエルの歌の小節番号（0〜7）を計算 (360度 / 8小節 = 45度ずつ)
  int current_shousetsu = angle / 45;  
  if (current_shousetsu > 7) current_shousetsu = 7;

  // 初回ループ時の初期化
  if (last_shousetsu == -1) {
    last_shousetsu = current_shousetsu;
  }

  // つまみが別の小節に入ったら「操作中（スクラッチ中）」とみなす
  if (current_shousetsu != last_shousetsu) {
    isOperating = true;
    last_move_time = currentTime; // 動いた時刻を保存
    last_shousetsu = current_shousetsu;
  }
  if (current_shousetsu > 7) current_shousetsu = 7;

  // つまみから手が離されて、300ms以上動かなければ操作終了（確定）
  if (isOperating && (currentTime - last_move_time > 300)) {
    isOperating = false;
  }

  return current_shousetsu;
}