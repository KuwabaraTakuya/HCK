#include <Adafruit_NeoPixel.h>
#include "LEDController.h"

#define SIG_PIN     6
#define NUM_LEDS    60
#define VOL_PIN     A1    // スライドボリューム(SL4515N-A103L15CM)のワイパー

#define DEBUG_FPS

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, SIG_PIN);

LEDContext ledCtx;

// カエルの歌 楽譜データ
NoteData score[] = {
  {60, 1.0}, {62, 1.0}, {64, 1.0}, {65, 1.0}, {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0},
  {64, 1.0}, {65, 1.0}, {67, 1.0}, {69, 1.0}, {67, 1.0}, {65, 1.0}, {64, 1.0}, {0, 1.0},
  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},
  {60, 0.5}, {60, 0.5}, {62, 0.5}, {62, 0.5}, {64, 0.5}, {64, 0.5}, {65, 0.5}, {65, 0.5},
  {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0}
};
const int len = sizeof(score) / sizeof(score[0]);
bool isPlaying = false;
unsigned long last_led_update = 0;

#ifdef DEBUG_FPS
unsigned long fps_counter = 0;
unsigned long fps_window_start = 0.0;
#endif

void tx(int p, int v, unsigned long d);

void setup() {
  Serial.begin(115200);
  Serial.setTimeout(10);
  strip.begin();
  strip.clear();
  strip.show();

  randomSeed(analogRead(A0));

  // 初期値のセット（初期値の更新にもLED_vol_bpmを利用可能）
  //LED_vol_bpm(&ledCtx, 120, 120);
  ledCtx.note_index = 0;
  ledCtx.bright = 0;
  ledCtx.current_hue = 0;
}

void loop() {
  unsigned long current_time = millis();

  // スライドボリュームで音量(LED輝度の最大値)を常時更新
  // analogReadの0-1023を、輝度として使う0-255へ変換
  ledCtx.dynamic_vol = map(analogRead(VOL_PIN), 0, 1023, 0, 255);

  // シリアル通信等でコマンドと値を受け取った想定の処理
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      isPlaying = true;
      ledCtx.note_index = 0;
      ledCtx.next_note_time = millis();
      int random_bpm = random(60, 200);
      // BPMはランダム、音量はスライドボリュームの現在値を使用
      LED_vol_bpm(&ledCtx, random_bpm, ledCtx.dynamic_vol);
      Serial.print("BPM:");
      Serial.println(random_bpm);
#ifdef DEBUG_FPS
      fps_counter = 0;
      fps_window_start = millis();
#endif
    }
  }

  if (!isPlaying) return;

  // --- 音符切り替え処理 ---
  LED_Process(&ledCtx, score, len);

  // --- LED_Update60Hzでの描画処理 ---
  if (isPlaying && (current_time - last_led_update >= LED_UPDATE_INTERVAL_MS)) {
    last_led_update = current_time;

    // 輝度の減衰
    ledCtx.bright -= ledCtx.decay;
    if (ledCtx.bright < 0) ledCtx.bright = 0;

    //uint8_t raw_b = (uint8_t)ledCtx.bright;
    //uint8_t gamma_b = ((uint16_t)raw_b * raw_b) / 255;

    // 全ピクセルを点灯
    for(uint16_t i=0; i<strip.numPixels(); i++) {
      //uint32_t color = strip.ColorHSV(ledCtx.current_hue * 256, 255, gamma_b);
      uint32_t color = strip.ColorHSV(ledCtx.current_hue * 256, 255, (uint8_t)ledCtx.bright);
      strip.setPixelColor(i, color);
    }
    strip.show();

#ifdef DEBUG_FPS
    // 1秒間の描画回数をカウントして実測FPSを出力
    fps_counter++;
    if (current_time - fps_window_start >= 1000.0) {
      Serial.print("FPS:");
      Serial.println(fps_counter);
      fps_counter = 0;
      fps_window_start = current_time;
    }
#endif
  }
}

void tx(int p, int v, unsigned long d) {
  Serial.print(p); Serial.print(",");
  Serial.print(v); Serial.print(",");
  Serial.println(d);
}
