#ifndef LED_PLAYER_H
#define LED_PLAYER_H

#include <Arduino.h>

// ─── LEDテープ（カエルの歌・3声輪唱）プレイヤー ───
// sheetmusic2.ino の setup()/loop() を、hack_test02 から呼べる関数に分解したもの。

void setupLED();                              // strip初期化（setup()から1回呼ぶ）

// 輪唱の頭出し（演奏開始＝PLAYING時に呼ぶ）
//   order : 登場順。order[k] = k番目に登場する声部の番号(0〜3)。-1 はその枠を使わない。
//           （hack_test02 の順番ボタンが押された順 array[] をそのまま渡す）
void startLEDPlayback(int bpm, uint8_t vol, const int* order);

// シーケンサ進行＋60Hz描画（loop()で毎回呼ぶ）
//   bpm      : 再生スピード（A1つまみ）
//   vol      : LED輝度の最大値 0-255（A2つまみ）
//   measure  : ターンテーブルが指す小節 0-7（A0つまみ）
//   scrubbing: true の間はターンテーブルで再生位置を直接操作（スクラッチ）。
//              false なら measure を無視して通常の自動進行を続ける。
void updateLED(int bpm, uint8_t vol, int measure, bool scrubbing);

int GetCurrentMeasure();

#endif
