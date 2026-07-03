#include "LEDController.h"

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

// --- 関数の仕様：LED_TriggerNote ---
// 1声部(ctx)に対し、楽譜の step_index の音符を適用する。
// 輪唱では各声部の step_index をずらして（声部v: master_step - v*offset）呼び出す。
void LED_TriggerNote(LEDContext *ctx, NoteData *score, int score_len, int step_index) {
  // 範囲外＝この声部はまだ開始前 or 既に終了 → 新たなトリガなし（直前の輝度を減衰させ続ける）
  if (step_index < 0 || step_index >= score_len) return;

  int current_pitch = score[step_index].pitch;

  if (current_pitch > 0) { // 発音開始
    ctx->bright = ctx->dynamic_vol;
    ctx->current_hue = MIDItoLED(current_pitch); // 音階に応じた色相を決定

    // 後続の -1(タイ) を合算した長さで減衰率を計算
    float total_D = score[step_index].duration;
    for (int j = step_index + 1; j < score_len && score[j].pitch == -1; j++)
      total_D += score[j].duration;
    unsigned long L_total = (60000.0 / ctx->current_bpm) * total_D;
    float N = (float)L_total / LED_UPDATE_INTERVAL_MS;
    ctx->decay = (N > 0) ? ((float)ctx->dynamic_vol / N) : ctx->dynamic_vol;

  } else if (current_pitch == 0) { // 休符のみ消灯
    ctx->bright = 0;
  }
  // current_pitch == -1 のときは何もしない（前の音の輝度・減衰を継続）
}
