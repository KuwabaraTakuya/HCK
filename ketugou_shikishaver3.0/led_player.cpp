#include <Adafruit_NeoPixel.h>
#include "LEDController.h"
#include "led_player.h"

// ─── 配線・LED設定（sheetmusic2.ino より移植）───
#define SIG_PIN     9     // LEDテープのデータピン（hack_test02のButton04=6番と衝突するためD7に変更）
#define NUM_LEDS    60

// --- 輪唱(4声4分割)設定 ---
#define NUM_VOICES         4                       // 声部の数
#define LEDS_PER_VOICE     (NUM_LEDS / NUM_VOICES) // 1声部あたりのLED数(60/4=15)
#define VOICE_OFFSET_STEPS 16                      // 声部ごとの開始ずれ(8ステップ=1小節)
#define STEP_PER_MEASURE 8 
// --- 楽器割り当て（順番ボタンとは別に固定）---
#define DRUM_VOICE 3   // この声部番号(0〜3)をドラム扱いにする。メロディではなく一音ループ。
#define DRUM_HUE   0   // ドラムの色相（0=赤）

static Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, SIG_PIN);

// 声部ごとに独立したコンテキストを持つ（同じ楽譜を時間差で再生＝輪唱）
static LEDContext ledCtx[NUM_VOICES];

// カエルの歌 楽譜データ
static NoteData score[] = {
  {60, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {64, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5},
  {64, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {64, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5}, {67, 0.5}, {-1, 0.5}, {69, 0.5}, {-1, 0.5},
  {67, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5}, {64, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {60, 0.5}, {62, 0.5}, {62, 0.5}, {64, 0.5}, {64, 0.5}, {65, 0.5}, {65, 0.5},
  {64, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}
};
static const int score_len = sizeof(score) / sizeof(score[0]);

static bool isPlaying = false;
static unsigned long last_led_update = 0;

// ─── LEDテープの描画頻度(Hz)計測用：1秒間に strip.show() を何回呼んだか ───
static unsigned long tape_fps_count = 0;   // 1秒窓の中での描画回数
static unsigned long tape_fps_window = 0;  // 直近にHzを出力した時刻

// 声部vの登場順スロット（0=最初に登場, 1=次, …。-1=今回は登場しない）
// 順番ボタンで決めた array[] から startLEDPlayback() で作る。
static int entry_slot[NUM_VOICES] = { -1, -1, -1, -1 };

// 輪唱のマスターシーケンサ（全声部を1つのグリッドで同期進行させる）
// 声部vが見るステップ番号 = master_step - v * VOICE_OFFSET_STEPS
static int master_step = 0;              // リード声部(声部0)のステップ番号
static unsigned long next_note_time = 0; // 次のステップへ切り替わる時刻 (ms)

// ─── strip初期化＋全声部のセグメント割り当て ───
void setupLED() {
  strip.begin();
  strip.clear();
  strip.show();

  for (int v = 0; v < NUM_VOICES; v++) {
    ledCtx[v].bright = 0;
    ledCtx[v].decay = 0;
    ledCtx[v].current_hue = 0;
    ledCtx[v].led_start = v * LEDS_PER_VOICE;
    ledCtx[v].led_count = LEDS_PER_VOICE;
  }
}

// ─── 輪唱の頭出し（sheetmusic2 の 'S' コマンド受信時の処理に相当）───
void startLEDPlayback(int bpm, uint8_t vol, const int* order) {
  isPlaying = true;
  master_step = 0;
  next_note_time = millis();

  // 登場順スロットを作る：order[k]=v なら「k番目に登場する声部はv」→ entry_slot[v]=k
  for (int v = 0; v < NUM_VOICES; v++) entry_slot[v] = -1;
  for (int k = 0; k < NUM_VOICES; k++) {
    int v = order[k];
    if (v >= 0 && v < NUM_VOICES) entry_slot[v] = k;
  }

  for (int v = 0; v < NUM_VOICES; v++) {
    ledCtx[v].bright = 0;
    ledCtx[v].decay = 0;
    ledCtx[v].current_hue = 0;
    ledCtx[v].led_start = v * LEDS_PER_VOICE;
    ledCtx[v].led_count = LEDS_PER_VOICE;
    LED_vol_bpm(&ledCtx[v], bpm, vol);
  }
}

// ─── 1声部を master_step の位置に応じてトリガする ───
// メロディ楽器: カエルの歌(score)をそのまま。ドラム: メロディ無視で1拍ごとに赤を一発ループ。
static void triggerVoice(int v, int ms) {
  if (entry_slot[v] < 0) return;                      // この声部は今回登場しない
  int sv = ms - entry_slot[v] * VOICE_OFFSET_STEPS;   // この声部から見たステップ番号
  if (sv < 0) return;                                 // まだ登場前（自分の番が来ていない）

  if (v == DRUM_VOICE) {
    // ドラム：1拍 = VOICE_OFFSET_STEPS/小節ではなく「2ステップ(=8分×2)」ごとに赤を一発鳴らす
    if (sv % 2 == 0) {
      ledCtx[v].bright = ledCtx[v].dynamic_vol;
      ledCtx[v].current_hue = DRUM_HUE;               // 赤
      float beat_ms = 60000.0 / ledCtx[v].current_bpm; // 1拍の長さ(ms)
      float N = beat_ms / LED_UPDATE_INTERVAL_MS;
      ledCtx[v].decay = (N > 0) ? (ledCtx[v].dynamic_vol / N) : ledCtx[v].dynamic_vol;
    }
  } else {
    // メロディ楽器：カエルの歌
    LED_TriggerNote(&ledCtx[v], score, score_len, sv);
  }
}

// ─── 60Hz描画───
void updateLED(int bpm, uint8_t vol, int measure, bool scrubbing) {
  if (!isPlaying) return;

  unsigned long current_time = millis();

  // 指揮者の最新BPM・音量を全声部へ反映（つまみ操作を演奏中もリアルタイム反映）
  for (int v = 0; v < NUM_VOICES; v++) {
    ledCtx[v].current_bpm = bpm;
    ledCtx[v].dynamic_vol = vol;
  }

  // 最終声部が最後の音符を鳴らし終えるまでのステップ数
  const int total_steps = (score_len - 1) + (NUM_VOICES - 1) * VOICE_OFFSET_STEPS;

  if (scrubbing) {
    // ── ターンテーブルで再生位置を直接操作（スクラッチ）──
    // 小節(0〜7) → リード声部のステップ番号（1小節 = VOICE_OFFSET_STEPS ステップ）
    int target_step = measure * STEP_PER_MEASURE;
    if (target_step < 0) target_step = 0;
    if (target_step > total_steps) target_step = total_steps;

    if (target_step != master_step) {
      master_step = target_step;
      // 新しい位置を全声部へ適用（登場順スロットに従ってオフセット＝輪唱を保ったままジャンプ）
      for (int v = 0; v < NUM_VOICES; v++) triggerVoice(v, master_step);
    }
    // スクラッチ中は自動進行を止め、手を離した瞬間からこの位置で進み出すようにする
    next_note_time = current_time;

  } else if (current_time >= next_note_time) {
    // ── 通常の自動進行 ──
    if (master_step <= total_steps) {
      // 各声部を登場順スロットに従ってトリガ（メロディはカエルの歌、ドラムは赤の一音ループ）
      for (int v = 0; v < NUM_VOICES; v++) triggerVoice(v, master_step);

      const float D_base = 0.5; // カエルの歌は全ステップ同じ長さ(8分音符)
      unsigned long L_ms = (60000.0 / ledCtx[0].current_bpm) * D_base;
      next_note_time = current_time + L_ms;
      master_step++;
    } else {
      // 全声部が楽譜終端に達したら再生終了
      isPlaying = false;
      strip.clear();
      strip.show();
      return;
    }
  }

  // --- 60Hz(16ms)ごとの描画処理 ---
  if (current_time - last_led_update >= LED_UPDATE_INTERVAL_MS) {
    last_led_update = current_time;

    // 声部ごとに、担当セグメントを自分の色・輝度で点灯
    for (int v = 0; v < NUM_VOICES; v++) {
      // 輝度の減衰
      ledCtx[v].bright -= ledCtx[v].decay;
      if (ledCtx[v].bright < 0) ledCtx[v].bright = 0;

      uint32_t color = strip.ColorHSV(ledCtx[v].current_hue * 256, 255, (uint8_t)ledCtx[v].bright);
      uint16_t end = ledCtx[v].led_start + ledCtx[v].led_count;
      for (uint16_t i = ledCtx[v].led_start; i < end; i++) {
        strip.setPixelColor(i, color);
      }
    }
    strip.show();

    // 描画頻度(Hz)の計測：1秒ごとに「1秒間の描画回数 = 描画頻度」を表示
    tape_fps_count++;
    if (current_time - tape_fps_window >= 1000) {
      Serial.print("TapeFPS: "); Serial.print(tape_fps_count); Serial.println(" Hz");
      tape_fps_count = 0;
      tape_fps_window = current_time;
    }
  }
}
int GetCurrentMeasure(){
  int m = master_step / STEP_PER_MEASURE;
  return m;
}
