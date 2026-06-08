#include "LEDMatrix.h"
#include "send_bpm.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>

WiFiUDP Udp;

// 送信先（奏者側）の設定
const IPAddress playerIp(192, 168, 1, 11);
const unsigned int localPort = 3000;

// 時間管理用の変数
unsigned long last_display_time = 0;
unsigned long last_udp_time = 0;

const int LED_hz = 16;  // LEDマトリクス用（約60Hz）
const int UDP_hz = 100; // UDP送信確認用（10Hz）

void setup() {
  Serial.begin(115200);
  initLEDMatrix();
}

void loop() {
  unsigned long current_time = millis();

  int val = analogRead(A1);
  int bpm = map(val, 0, 1023, 60, 180);
  
  // LEDマトリクスの更新（約16msごとに更新）
  if (current_time - last_display_time >= LED_hz) {
    updateDisplay(bpm);
    last_display_time += LED_hz;
  }

  // 奏者側へのBPM送信（100msごとに送信）
  if (current_time - last_udp_time >= UDP_hz) {
    sendbpm(bpm);
    last_udp_time += UDP_hz;
  }
}

void sendbpm(int bpm_value) {
  Udp.beginPacket(playerIp, localPort);

  String message = "BPM" + String(bpm_value);
  Udp.print(message);

  Udp.endPacket();
}
