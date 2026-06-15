#ifndef TURN_TABLE_H
#define TURN_TABLE_H

#include <Arduino.h>

struct MusicData {
  int currentBPM;
  int currentShousetsu; // 指揮者が決定した小節番号
};

struct NoteData {
  int pitch;
  float duration;
};

int update_conductor_shousetsu(int angle, int bpm, unsigned long currenttime, NoteData* score, int& scoreIdx, int scoreLen, bool& isPlaying, unsigned long& nextNoteTime);
void tx(int p, int v, unsigned long d);

#endif