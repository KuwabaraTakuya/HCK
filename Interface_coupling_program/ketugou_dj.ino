#include "turn_table.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>

WiFiUDP Udp;

const IPAddress leaderIp(192, 168, 1, 10);
const unsigned int localPort = 3000;
bool lastButtonState = HIGH;   // ボタンの直前の状態
uint8_t currentEffects = 0;    // 現在のエフェクトフラグ (0, 1, 2...)
bool myFlag = 0;

struct button{
  const int pin;
  int currentstate;
  int laststate;
};

button syncButton = {2, HIGH, HIGH};
button effectButton = {3, HIGH, HIGH};


// カエルの歌 楽譜データ（8分音符グリッド版）
// 1マス = 0.5拍。4分音符は「発音 + タイ(-1)」の2マスで表す。
// 1小節 = 8マス。全8小節 = 64マス。
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

int len = 64;
int idx = 0;
bool isPlaying = false;
unsigned long next_note_time = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(syncButton.pin, INPUT_PULLUP);
  pinMode(effectButton.pin, INPUT_PULLUP);
}

void loop() {
  unsigned long current_time = millis();

  //デジタル、エフェクトフラグの読み取り
  syncButton.currentstate = digitalRead(syncButton.pin);

  // スイッチが「押された瞬間（HIGHからLOWになった時）」だけ反応させる
  if (syncButton.laststate == HIGH && syncButton.currentstate == LOW) {
    myFlag = !myFlag;
    if(!isPlaying){
      isPlaying = true;
      idx = 0;
      next_note_time = current_time;
    }
    else{
      isPlaying = false;
      idx = 0;
    }
    delay(50); // チャタリング防止
  }
  syncButton.laststate = syncButton.currentstate;

  //エフェクト変更ボタンの処理
  effectButton.currentstate = digitalRead(effectButton.pin);
  if (effectButton.laststate == HIGH && effectButton.currentstate == LOW) {
    currentEffects = (currentEffects + 1) % 4;
    delay(50); // チャタリング防止
  }
  effectButton.laststate = effectButton.currentstate;

  //ターンテーブルの角度読み取りと小節番号の計算
  //ADCのチャンネル間クロストーク対策: 1回目を捨てて2回目を採用する
  analogRead(A0);
  int turn_table_val = analogRead(A0);
  if(turn_table_val > 1020) {
    turn_table_val = 1020;
  }

  int angle = map(turn_table_val, 0, 1020, 0, 359);

  //ユーザーのbpmの操作
  analogRead(A1);
  int bpm_val = analogRead(A1);
  int bpm = map(bpm_val, 0, 1023, 60, 180);

  //スライドボリューム(SL4515N-A103L)による音量操作 0〜255に変換
  analogRead(A2);
  int volume_val = analogRead(A2);
  int volume = map(volume_val, 0, 1023, 0, 255);

  // .cpp側の関数へ楽譜データやフラグ，インデックスを丸ごと渡して同期更新
  int shousetsu = update_conductor_shousetsu(angle, bpm, volume, currentEffects, current_time, score, idx, len, isPlaying, next_note_time);
}
