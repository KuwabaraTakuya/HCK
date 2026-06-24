#ifndef NOTEDATA_H
#define NOTEDATA_H

struct NoteData {
  int pitch;      // 音階（MIDIノート番号, 0は休符）
  float duration; // 音符の基準長（1.0 = 4分音符, 0.5 = 8分音符）
};

#endif