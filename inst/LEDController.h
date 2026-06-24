#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include "notedata.h"

#define LED_UPDATE_INTERVAL_MS 16

// --- 設計書 3-3. 構造体定義に準拠 ---
struct LEDContext {
  float bright;                // 現在の輝度
  int current_bpm;             // 指揮者から受信した最新のBPM
  uint8_t dynamic_vol;         // 指揮者から受信した最新の音量
  int note_index;              // 現在再生中の音符番号
  unsigned long next_note_time;// 次の音符へ切り替わる時刻 (ms)
  float decay;                 // LED の減衰率
  uint16_t current_hue;        // 現在鳴っている音階の色相 (0 - 255)
};

uint16_t MIDItoLED(int pitch);
void LED_vol_bpm(LEDContext *ctx, int new_bpm, uint8_t new_vol);
void LED_Process(LEDContext *ctx, NoteData *score, int score_len);

#endif
