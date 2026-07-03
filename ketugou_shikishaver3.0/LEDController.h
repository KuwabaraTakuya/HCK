#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>

#define LED_UPDATE_INTERVAL_MS 16

struct NoteData {
  int pitch;
  float duration;
};

struct LEDContext {
  float bright;                // 現在の輝度
  int current_bpm;             // 指揮者から受信した最新のBPM
  uint8_t dynamic_vol;         // 指揮者から受信した最新の音量
  float decay;                 // LED の減衰率
  uint16_t current_hue;        // 現在鳴っている音階の色相 (0 - 255)
  // --- 輪唱(3声3分割)用：担当セグメント ---
  uint16_t led_start;          // この声部が担当するセグメントの先頭LED番号
  uint16_t led_count;          // この声部が担当するLED数
};

uint16_t MIDItoLED(int pitch);
void LED_vol_bpm(LEDContext *ctx, int new_bpm, uint8_t new_vol);
// 楽譜の step_index の音符をこの声部(ctx)に適用し、bright/hue/decay を更新する。
// step_index が範囲外（未開始/終了）のときは何もしない（直前の減衰を継続）。
// 進行タイミングと次ステップ判断は呼び出し側（マスターシーケンサ）が担当する。
void LED_TriggerNote(LEDContext *ctx, NoteData *score, int score_len, int step_index);

#endif
