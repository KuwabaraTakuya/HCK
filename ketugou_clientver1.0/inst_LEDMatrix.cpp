#include "inst_LEDMatrix.h"

ArduinoLEDMatrix matrix;

int c, r;

byte frame[8][12] = {
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

void initLEDMatrix(){
  matrix.begin();
}

void shift() {
  // 右に1列ずらすため、右側（11列目）から左に向かって処理する
  for (int row = 0; row < 8; row++) {
    for (int col = 11; col > 0; col--) {
      frame[row][col] = frame[row][col - 1];
    }
    // 左端（0列目）は新しい描画のためにリセット
    frame[row][0] = 0;
  }
}

void drawBar(int height, int column) {
  // マトリクスは一番上が行0、一番下が行7
  // 一番下の行から上方向に向かって指定された高さ分だけ1（点灯）にする
  for (int i = 0; i < height; i++) {
    int targetRow = 7 - i;
    if (targetRow >= 0) { // 配列の範囲外アクセス防止
      frame[targetRow][column] = 1;
    }
  }
}

void clearDisplay() {
  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 12; col++) {
      frame[row][col] = 0;
    }
  }
  matrix.renderBitmap(frame, 8, 12);
}

void updateNoteDisplay(int pitch) {
  // 1. 既存の描画データを右に1列スクロール
  shift();

  // 2. 左端の列に新しいバーを書き込む
  if (pitch != -1) {
    int height = ledheight(pitch);
    // 2. 左端の列に新しいバーを書き込む
    if (height > 0) {
      drawBar(height, 0);
    }
  }

  // 3. LEDマトリクスに反映させる
  matrix.renderBitmap(frame, 8, 12);
}

// MIDIの音階(pitch)をLEDの高さ(0〜8)に変換する関数
int ledheight(int pitch) {
  if (pitch == 0) return 0; // 休符
  switch (pitch) {
    case 60: return 1; // ド
    case 62: return 2; // レ
    case 64: return 3; // ミ
    case 65: return 4; // ファ
    case 67: return 5; // ソ
    case 69: return 6; // ラ
    default: return 0; 
  }
}