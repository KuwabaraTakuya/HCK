#include "inst_LEDMatrix.h"
#include <Adafruit_NeoPixel.h>
#include "LEDController.h"
#include "notedata.h"

#define SIG_PIN     6
#define NUM_LEDS    60

#define DEBUG_FPS

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, SIG_PIN);

LEDContext ledCtx;

// カエルの歌 楽譜データ
NoteData score[] = {
  {60, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {64, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5},
  {64, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {64, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5}, {67, 0.5}, {-1, 0.5}, {69, 0.5}, {-1, 0.5},
  {67, 0.5}, {-1, 0.5}, {65, 0.5}, {-1, 0.5}, {64, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5},
  {60, 0.5}, {60, 0.5}, {62, 0.5}, {62, 0.5}, {64, 0.5}, {64, 0.5}, {65, 0.5}, {65, 0.5},
  {64, 0.5}, {-1, 0.5}, {62, 0.5}, {-1, 0.5}, {60, 0.5}, {-1, 0.5}, { 0, 0.5}, { 0, 0.5}
};

const int len = sizeof(score) / sizeof(score[0]);
int idx = 0;
bool isPlaying = false;

int current_bpm = 120; // 指揮者側から送信されたbpm
float note_progress = 0.0; // 現在の音符の描画進捗（0.25 = 1ピクセル分）
unsigned long last_shift_time = 0;
unsigned long last_led_update = 0;

#ifdef DEBUG_FPS
unsigned long fps_counter = 0;
unsigned long fps_window_start = 0.0;
#endif

void setup() {
  Serial.begin(115200);
  initLEDMatrix();
  
  Serial.setTimeout(10);
  strip.begin();
  strip.clear();
  strip.show();

  // 初期値のセット（初期値の更新にもLED_vol_bpmを利用可能）
  LED_vol_bpm(&ledCtx,  120, 220);
  ledCtx.note_index = 0;
  ledCtx.bright = 0;
  ledCtx.current_hue = 0;
}

void loop() {
  unsigned long current_time = millis();

  // 1. コマンドの受信（Sが来たら再生開始）
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'S') {
      isPlaying = true;       
      idx = 0;                
      note_progress = 0.0;    
      last_shift_time = current_time;

      ledCtx.note_index = 0;
      ledCtx.next_note_time = millis();
      #ifdef DEBUG_FPS
      fps_counter = 0;
      fps_window_start = millis();
      #endif
    }
  }

  // 再生中(isPlaying == true)でなければ、ここで処理を終わらせて何も描画しない
  if (!isPlaying) {
    return;
  }

  // 2. 音とLEDの同期処理
  unsigned long pixel = (60000.0 / current_bpm) / 2; 

  // ピクセルを進める時間になったら
  if (current_time - last_shift_time >= pixel) {
    
    // まだ楽譜が終わっていなければ
    if (idx < len) {
      
      int pitch = score[idx].pitch;
      float duration = score[idx].duration;
      
      //note_progressが0.0のとき，led描画を現在のbpmに合わせて描画する
      if (note_progress == 0.0) {
         unsigned long L_ms = (60000.0 / current_bpm) * duration;
         tx(pitch, ledCtx.dynamic_vol, L_ms); // 
         
         ledCtx.note_index = idx;
         LED_Process(&ledCtx, score, len);
      }

      // 1. LEDの描画
      updateNoteDisplay(pitch); 

      // 2. 描画した分だけ進捗を進める
      note_progress += 0.5;
      
      // 3. 今の音符の長さ分をすべて描ききったら，次の音符へ進む準備
      if (note_progress >= duration - 0.01) {
        idx++;               // 次の音符
        note_progress = 0.0; // 進捗リセット
      }

    } else {
      // 楽譜が最後まで終わったら
      updateNoteDisplay(0); // 空のバーを流す
      ledCtx.note_index = len;
      LED_Process(&ledCtx, score, len);
      
      if (idx >= len) {
        idx++;
        if (idx > len + 12) {
          isPlaying = false;
          clearDisplay();
          strip.clear();
          strip.show();
        }
      }
    }
    
    // 時間を更新
    last_shift_time += pixel; 
  }


  // --- 音符切り替え処理 ---
  // LED_Process(&ledCtx, score, len);

    // --- LED_Update60Hzでの描画処理 ---
  if (isPlaying && (current_time - last_led_update >= LED_UPDATE_INTERVAL_MS)) {
    last_led_update = current_time;

    // 輝度の減衰
    ledCtx.bright -= ledCtx.decay;
    if (ledCtx.bright < 0) ledCtx.bright = 0;

    uint8_t raw_b = (uint8_t)ledCtx.bright;
    uint8_t gamma_b = ((uint16_t)raw_b * raw_b) / 255;

    // 全ピクセルを点灯
    for(uint16_t i=0; i<strip.numPixels(); i++) {
      uint32_t color = strip.ColorHSV(ledCtx.current_hue * 256, 255, gamma_b);
      //uint32_t color = strip.ColorHSV(ledCtx.current_hue * 256, 255, (uint8_t)ledCtx.bright);
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
// データをシリアル出力する関数
void tx(int p, int v, unsigned long d) {
  Serial.print(p);
  Serial.print(",");
  Serial.print(v);
  Serial.print(",");
  Serial.println(d);
}