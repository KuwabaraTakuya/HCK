#include "turn_table.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>

WiFiUDP Udp;

const IPAddress leaderIp(192, 168, 1, 10);
const unsigned int localPort = 3000;

// カエルの歌 楽譜データ
NoteData score[] = {
  {60, 1.0}, {62, 1.0}, {64, 1.0}, {65, 1.0}, {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0},
  {64, 1.0}, {65, 1.0}, {67, 1.0}, {69, 1.0}, {67, 1.0}, {65, 1.0}, {64, 1.0}, {0, 1.0},
  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},  {60, 1.0}, {0, 1.0},
  {60, 0.5}, {60, 0.5}, {62, 0.5}, {62, 0.5}, {64, 0.5}, {64, 0.5}, {65, 0.5}, {65, 0.5}, 
  {64, 1.0}, {62, 1.0}, {60, 1.0}, {0, 1.0}
};

int len = 36;
int idx = 0;
bool isPlaying = false;
unsigned long next_note_time = 0; 

void setup() {
  Serial.begin(115200);
}

void loop() {
  unsigned long current_time = millis();

  //processingの音楽開始フラグ
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if(cmd = 'S') {
      if (!isPlaying) {
        isPlaying = true;       
        idx = 0;                
        next_note_time = current_time;
      }
    }
  }

  //ターンテーブルの角度読み取りと小節番号の計算
  int turn_table_val = analogRead(A0);
  if(turn_table_val > 1020) {
    turn_table_val = 1020;
  }

  int angle = map(turn_table_val, 0, 1020, 0, 359);
  
  //ユーザーのbpmの操作
  int bpm_val = analogRead(A1);
  int bpm = map(bpm_val, 0, 1023, 60, 180);

  // .cpp側の関数へ楽譜データやフラグ，インデックスを丸ごと渡して同期更新
  int shousetsu = update_conductor_shousetsu(angle, bpm, current_time, score, idx, len, isPlaying, next_note_time);

  //実際には指揮者側から送られてきたbpmと小説番号をつかう
  MusicData sendData = { bpm, shousetsu };
  Udp.beginPacket(leaderIp, localPort);
  Udp.write((uint8_t*)&sendData, sizeof(sendData));
  Udp.endPacket();
}
