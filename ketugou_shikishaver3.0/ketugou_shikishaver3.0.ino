#include "LEDMatrix.h"
#include "turn_table.h"
#include "led_player.h"
#include <WiFiS3.h>
#include <WiFiUdp.h>

// ──────────────────────────────────────────────────────────────
// ★ LEDテープ単体テスト用スイッチ ★
//   この行を有効にすると、WiFi接続をスキップし、起動直後に演奏(PLAYING)状態にして
//   LEDテープが光るかどうかだけを確認できます。
//   本番（WiFiで奏者と同期する）では、必ずこの行を「//」でコメントアウトしてください。
//#define TEST_LED_NO_WIFI
// ──────────────────────────────────────────────────────────────

// ─── hack_test.ino から移植したネットワーク設定 ───
char ssid[] = "TP-Link_E356";
char pass[] = "99692025";
unsigned int localPort = 3000;
IPAddress broadcastIp(192, 168, 1, 255);
IPAddress local_Ip(192, 168, 1, 10); // 指揮者自身の固定IP (奏者側のleaderIpと一致)

//順番だけ決めるタクトスイッチ用の変数を4つ用意
//4つのタクトスイッチで順番だけ決めてその後に同期開始タクトスイッチを押す
//タクトスイッチが押されたものだけ同期処理を開始する
//配列array、タクトスイッチが押された順番でarrayに入り、Udp.beginPacket(playerIps[array[i]], localPort);で送信
int n = 0;

// 同期を行う奏者たちのIPアドレスリスト
IPAddress playerIps[] = {
  IPAddress(192, 168, 1, 11), // 奏者1 (ピアノ: hack_client02のlocalIpと一致)
  IPAddress(192, 168, 1, 12), // 奏者2 フルート
  IPAddress(192, 168, 1, 13), // トランペット
  IPAddress(192, 168, 1, 14)  // 
};
const int numPlayers = 1; // 現在テストする奏者の数
int status = WL_IDLE_STATUS;
WiFiUDP Udp;

// ─── 状態管理 (hack_test.inoより) ───
enum State { SELECT, IDLE, SYNCING, PLAYING };
State systemState = SELECT;

// ─── ピン配置・構造体 (turn_table_A.inoより) ───
struct button {
  const int pin;
  int currentstate;
  int laststate;
};

button Button01 = {4, HIGH, HIGH};     // 順番ボタン（4番ピン）
button Button02 = {5, HIGH, HIGH};     // 順番ボタン（5番ピン）
button Button03 = {6, HIGH, HIGH};     // 順番ボタン（6番ピン）
button Button04 = {7, HIGH, HIGH};     // 順番ボタン（7番ピン）
button Button05 = {8, HIGH, HIGH};     // while文から抜け出すためのボタン（8番ピン)）
int array[4] = {-1, -1, -1, -1};       // 押された順番で奏者番号(0~3)が入る配列
int array_frag[4] = {true, true, true, true};

button syncButton = {2, HIGH, HIGH};     // 同期・演奏開始ボタン（2番ピン）
button effectButton = {3, HIGH, HIGH};   // エフェクトボタン（3番ピン）
const int turnTablePin = A0;             // ターンテーブルつまみ（A0）
const int bpmPin = A1;                   // BPMつまみ（A1）
const int volumePin = A2;                // 音量つまみ (※A3に配線している場合はA3に変えてください)
const int LED_hz = 16;  // LEDマトリクス用（約60Hz）
unsigned long last_display_time = 0;
// 状態管理変数
uint8_t currentEffects = 0;
int effectLastState = HIGH;
unsigned long lastSendTime = 0;
const int sendInterval = 100;            // 100ms（0.1秒）ごとに全員へデータを送信

int lastmeasureupdate = 0;

bool isOperating = false;
bool ledStarted = false;                 // 演奏開始時に1度だけLED輪唱を頭出しするためのフラグ

// ─── LEDマトリクスの更新頻度(Hz)計測用：1秒間にupdateDisplayを何回呼んだか ───
unsigned long matrixCount = 0;           // 1秒窓の中でのマトリクス更新回数
unsigned long lastMatrixHzTime = 0;      // 直近にHzを出力した時刻

// ─── 奏者側からの時刻同期要求（4バイト）に応答する関数 (hack_test.inoより) ───
bool handleNtpRequests() {
  int packetSize = Udp.parsePacket();
  if (packetSize) {
    uint32_t mastertime = millis();
    char packetBuffer[255];
    Udp.read(packetBuffer, 255);
    
    // 要求してきた奏者のIPへ、現在の指揮者マイコンの時刻（4バイト）を即座に返送
    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
    Udp.write((uint8_t*)&mastertime, sizeof(mastertime));
    Udp.endPacket();
    
    Serial.print(" -> Sent MasterTime: "); Serial.println(mastertime);
    return true;
  }
  return false;
}

// ─── 順番に"SYNC_READY"を送り同期を完了させるシーケンス (hack_test.inoより) ───
void startSyncSequence() {
  systemState = SYNCING;
  Serial.println("Starting Sync Sequence...");
  
  for (int i = 0; i < n; i++) {
    if(array[i] != -1){
      Serial.print("Syncing with Player "); Serial.print(i);
#ifndef TEST_LED_NO_WIFI
      int playerIdx = array[i];
      // 奏者に対して順番を送信
      Udp.beginPacket(playerIps[playerIdx], localPort);
      Udp.write(i);
      Udp.endPacket();

      // 奏者が上のhandleNtpRequestsに反応してリクエストを返してくるのを最大2秒待つ
      unsigned long waitStart = millis();
      bool synced = false;
      while (millis() - waitStart < 2000) {
        if (handleNtpRequests()) {
          synced = true;
          break;
        }
      }
      if (synced) {
        Serial.println(" -> Sync Done.");
      } else {
        Serial.println(" -> Timeout.");
      }
      delay(100); // ネットワーク安定のためのわずかなウェイト
#else
      Serial.println(" -> (TESTモード: UDP同期はスキップ)");
#endif
    }
  }

  Serial.println("All Sync Completed! Moving to PLAYING state.");
  systemState = PLAYING; // 同期シーケンスが終わったら自動的に演奏状態へ
}

void setup() {
  Serial.begin(115200);
  delay(100);
  
  pinMode(Button01.pin, INPUT_PULLUP);
  pinMode(Button02.pin, INPUT_PULLUP);
  pinMode(Button03.pin, INPUT_PULLUP);
  pinMode(Button04.pin, INPUT_PULLUP);
  pinMode(Button05.pin, INPUT_PULLUP);
  pinMode(syncButton.pin, INPUT_PULLUP);
  pinMode(effectButton.pin, INPUT_PULLUP);

  setupLED(); // LEDテープ(NeoPixel)の初期化
  initLEDMatrix();
#ifdef TEST_LED_NO_WIFI
  // ★テストモード：WiFiをスキップ。SELECTから開始し、順番ボタンで登場順を決めてから演奏する。
  //   操作: 順番ボタン(4〜7番)で登場順 → 8番ボタンで確定 → 2番(同期)ボタンで演奏開始 → LED点灯
  //   ※ systemState はグローバル初期値 SELECT のまま（ここでは PLAYING にしない）
  Serial.println("\n[TESTモード] WiFi無し。順番ボタンで登場順→8番で確定→2番で演奏開始。");
#else
  // WiFi初期化・接続処理
  WiFi.config(local_Ip);
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(500);
  }
  Serial.println("WiFiに接続完了しました！");

  Udp.begin(localPort);
  Serial.println("\n[サーバ準備完了] 2番ピンのボタンを押すと同期シーケンスが始まります...");
#endif
}

void loop() {
  while(systemState == SELECT){
    Button01.currentstate = digitalRead(Button01.pin);
    Button02.currentstate = digitalRead(Button02.pin);
    Button03.currentstate = digitalRead(Button03.pin);
    Button04.currentstate = digitalRead(Button04.pin);
    Button05.currentstate = digitalRead(Button05.pin);  

      if(array_frag[0] && Button01.laststate == HIGH && Button01.currentstate == LOW){
        Serial.print(n+1);
        Serial.println("番：ピアノ");
        array[n++] = 0;
        array_frag[0] = false;
      }
      if(array_frag[1] && Button02.laststate == HIGH && Button02.currentstate == LOW){
        Serial.print(n+1);
        Serial.println("番：フルート");
        array[n++] = 1;
        array_frag[1] = false;
      }
      if(array_frag[2] && Button03.laststate == HIGH && Button03.currentstate == LOW){
        Serial.print(n+1);
        Serial.println("番：トランペット");
        array[n++] = 2;
        array_frag[2] = false;
      }
      if(array_frag[3] && Button04.laststate == HIGH && Button04.currentstate == LOW){
        Serial.print(n+1);
        Serial.println("番：ドラム");
        array[n++] = 3;
        array_frag[3] = false;
      }
      if(Button05.laststate == HIGH && Button05.currentstate == LOW){
        systemState = IDLE; // 順番を確定してSELECTを抜ける（この後2番ボタンで演奏開始）
        break;
      }
    Button01.laststate = Button01.currentstate;
    Button02.laststate = Button02.currentstate;
    Button03.laststate = Button03.currentstate;
    Button04.laststate = Button04.currentstate;
    Button05.laststate = Button05.currentstate;
    delay(10);
  }
  unsigned long current_time = millis();

  // ─── ① 同期・演奏開始ボタンの処理 (turn_table_A.inoのロジック) ───
  syncButton.currentstate = digitalRead(syncButton.pin);
  if (syncButton.laststate == HIGH && syncButton.currentstate == LOW) {
    if (systemState == SELECT || systemState == IDLE) {
      startSyncSequence(); // まだ同期前なら、一連の同期処理をスタート！
    }
    delay(50); // チャタリング防止
  }
  syncButton.laststate = syncButton.currentstate;

  // ─── ② 同期が完了し、演奏状態（PLAYING）になった後のリアルタイム処理 ───
  if (systemState == PLAYING) {
    
    // エフェクトボタンの処理
    int effectCurrentState = digitalRead(effectButton.pin);
    if (effectLastState == HIGH && effectCurrentState == LOW) {
      currentEffects = (currentEffects + 1) % 7; // 0→1→2→3→4→5→6→0 と切り替え
      Serial.print("Effect changed: "); Serial.println(currentEffects);
      delay(50);
    }
    effectLastState = effectCurrentState;

    // つまみの読み取り (turn_table_A.ino のクロストーク対策を適用)
    // A0: ターンテーブル
    analogRead(turnTablePin); // 1回目を空読み
    int turn_table_val = analogRead(turnTablePin);
    if (turn_table_val > 1020) turn_table_val = 1020;
    int angle = map(turn_table_val, 0, 1020, 0, 359);

    // A1: BPM
    analogRead(bpmPin); // 1回目を空読み
    int bpm_val = analogRead(bpmPin);
    int bpm = map(bpm_val, 0, 1023, 60, 180);

    // A2かA3: 音量
    analogRead(volumePin); // 1回目を空読み
    int volume_val = analogRead(volumePin);
    int volume = map(volume_val, 0, 1023, 0, 255);

    // turn_table.cpp のロジックで現在の小節(0〜7)と操作中(スクラッチ中)フラグを決定
    // ※isOperating はグローバル変数を渡す（300msの離し判定を跨いで保持するため、ローカルで隠さない）

    int shousetsu = update_conductor_shousetsu(angle, current_time, isOperating);

    // ─── LEDテープ(カエルの歌・輪唱)の駆動 ───
    // BPM(A1)=再生スピード / 音量(A2)=明るさ / ターンテーブル(A0)=再生位置(スクラッチ)
    if (!ledStarted) {
      startLEDPlayback(bpm, volume, array); // 演奏開始時に、順番ボタンで決めた登場順をLEDへ渡す
      ledStarted = true;
    }
    // スクラッチ中(isOperating)はターンテーブルの小節へ再生位置をジャンプ。離すと自動進行を再開
    updateLED(bpm, volume, shousetsu, isOperating);

      // LEDマトリクスの更新（約16msごとに更新）
    if (current_time - last_display_time >= LED_hz) {
      updateDisplay(bpm);
      last_display_time += LED_hz;
      matrixCount++; // この更新を1回ぶんカウント
    }
    // LEDマトリクスの更新頻度(Hz)を1秒ごとにシリアルへ表示
    if (current_time - lastMatrixHzTime >= 1000) {
      Serial.print("MatrixFPS: "); Serial.print(matrixCount); Serial.println(" Hz");
      matrixCount = 0;
      lastMatrixHzTime = current_time;
    }
    // ─── ③ 奏者(hack_client02)が待っている4バイトの形にデータをパッキング ───
    MusicData sendData;
    sendData.currentBPM = (uint8_t)bpm;
    sendData.currentmeasure = (uint8_t)GetCurrentMeasure();
    sendData.effects = (uint8_t)currentEffects;
    sendData.volume = (uint8_t)volume;

    // 100msごとに全員へ一斉ブロードキャスト送信
    if (current_time - lastSendTime >= sendInterval) {
#ifndef TEST_LED_NO_WIFI
      Udp.beginPacket(broadcastIp, localPort);
      Udp.write((uint8_t*)&sendData, sizeof(sendData));
      Udp.endPacket();
#endif
      lastSendTime = current_time;

      // 確認用に指揮者のシリアルモニタにも状態を表示
      Serial.print("BPM: "); Serial.print(sendData.currentBPM);
      Serial.print(" | Measure: "); Serial.print(sendData.currentmeasure);
      Serial.print(" | Effect: "); Serial.print(sendData.effects);
      Serial.print(" | Vol: "); Serial.println(sendData.volume);
    }
  }
}