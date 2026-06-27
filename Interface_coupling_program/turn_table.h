#ifndef TURN_TABLE_H
#define TURN_TABLE_H

#include <Arduino.h>


// 楽譜を最小単位(0.5拍)のマス目で表現し、全声部を共通の音長で送る。
//   -1 → Processing側は発音し直さず保持してほしい
//    0 ... 休符 / ノートオフ
//   >0 ... MIDIノート番号で新規発音

struct MusicData {
  int currentBPM;
  int currentShousetsu; // 指揮者が決定した小節番号
};

struct NoteData {
  int pitch;
  float duration;
};

int update_conductor_shousetsu(int angle, int bpm, int volume, int effects, unsigned long currenttime, NoteData* score, int& scoreIdx, int scoreLen, bool& isPlaying, unsigned long& nextNoteTime);
void tx(int p1, int p2, int p3, int v, int e, unsigned long d);

#endif
