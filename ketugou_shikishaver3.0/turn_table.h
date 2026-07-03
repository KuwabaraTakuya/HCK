#ifndef TURN_TABLE_H
#define TURN_TABLE_H

#include <Arduino.h>

// ─── 奏者側(hack_client02.ino)と100%一致させた4バイトのデータ構造 ───
struct MusicData {
  uint8_t currentBPM = 120;
  uint8_t currentmeasure = 0; // 小節番号 (0〜7)
  uint8_t effects = 0;
  uint8_t volume = 100;       
};

// ターンテーブルつまみの動きを監視し、選択された小節番号を返す関数
int update_conductor_shousetsu(int angle, unsigned long currentTime, bool& isOperating);

#endif