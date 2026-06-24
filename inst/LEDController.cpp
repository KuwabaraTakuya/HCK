#include "LEDController.h"
#include <Adafruit_NeoPixel.h>
#include "notedata.h"

extern Adafruit_NeoPixel strip;
extern bool isPlaying;
extern void tx(int p, int v, unsigned long d);

// --- 関数の仕様：MIDItoLED ---
uint16_t MIDItoLED(int pitch) {
  switch (pitch % 12) {
    case 0:  return 0;   // (ド) -> 赤
    case 2:  return 25;  // (レ) -> 橙
    case 4:  return 45;  // (ミ) -> 黄
    case 5:  return 85;  // (ファ) -> 緑
    case 7:  return 130; // (ソ) -> 水色
    case 9:  return 170; // (ラ) -> 青
    case 11: return 215; // (シ) -> 紫
    default: return 0;   // 休符や該当なしは赤
  }
}

// --- 関数の仕様：LED_vol_bpm ---
void LED_vol_bpm(LEDContext *ctx, int new_bpm, uint8_t new_vol) {
  ctx->current_bpm = new_bpm;     // curennt_bpmを更新する
  ctx->dynamic_vol = new_vol;     // dynamic_volを更新する
}

// --- 関数の仕様：LED_Process ---
void LED_Process(LEDContext *ctx, NoteData *score, int score_len) {
  unsigned long current_time = millis();

  // 現在の時刻とnext_note_timeを比較し、次の音符へ切り替えるか判断する
  // if (current_time >= ctx->next_note_time) {
    if (ctx->note_index < score_len) {
      int current_pitch = score[ctx->note_index].pitch;

      if (current_pitch > 0) { // 発音開始
        ctx->bright = ctx->dynamic_vol;               
        ctx->current_hue = MIDItoLED(current_pitch); // 音階に応じた色相を決定

        // 後続の -1(タイ) を合算した長さで減衰率を計算
        float total_D = score[ctx->note_index].duration;
        for (int j = ctx->note_index + 1; j < score_len && score[j].pitch == -1; j++)
          total_D += score[j].duration;
        unsigned long L_total = (60000.0 / ctx->current_bpm) * total_D;
        float N = (float)L_total / LED_UPDATE_INTERVAL_MS;
        ctx->decay = (N > 0) ? ((float)ctx->dynamic_vol / N) : ctx->dynamic_vol;

      } else if (current_pitch == 0) {
        ctx->bright = 0;
      }

      // 音符持続時間を算出する
      float D_base = score[ctx->note_index].duration;
      unsigned long L_ms = (60000.0 / ctx->current_bpm) * D_base;

      // 音符持続時間から16msで更新するために、減衰率を算出する
      float N = (float)L_ms / LED_UPDATE_INTERVAL_MS;
      ctx->decay = (N > 0) ? ((float)ctx->dynamic_vol / N) : ctx->dynamic_vol;

      // Processingへシリアルデータを送信
      // tx(current_pitch, ctx->dynamic_vol, L_ms);

      // 次の音符切り替え時刻を計算
      ctx->next_note_time = current_time + L_ms;
      ctx->note_index++;
    } else {
      ctx->bright = 0;
      // 楽譜の終端に達したら再生終了
      // isPlaying = false;
      // strip.clear();
      // strip.show();
    }
  // }
}
