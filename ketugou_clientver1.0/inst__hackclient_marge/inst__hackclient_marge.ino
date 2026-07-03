// 奏者側（ネットワーク同期 ＋ LEDマトリクス表示）
#include <WiFiS3.h>
#include <WiFiUdp.h>
#include "inst_LEDMatrix.h"
#include "notedata.h"

// ─── 切り分け用デバッグ出力スイッチ（1=有効 / 0=無効）───
// 有効中は Serial に "#DBG ..." / "#START ..." / "#JUMP ..." 行が出る。
// Processing連携で使うときは、CSVに混ざるので 0 に戻すこと。
#define DEBUG_JUMP 1

// ─── ネットワーク設定 ───
unsigned int localport = 3000;
char ssid[] = "TP-Link_E356";      // 使用するWi-FiのSSIDに合わせて変更
char pass[] = "99692025"; // Wi-Fiのパスワード
IPAddress leaderIp(192, 168, 1, 10);
//IPAddress localIp(192, 168, 1, 11);
IPAddress localIp(192, 168, 1, 12);
WiFiUDP Udp;

struct MusicData {
  uint8_t currentBPM = 120;   // BPM
  uint8_t currentmeasure = 0; // 小節番号
  uint8_t effects = 0;        // エフェクト番号
  uint8_t volume = 100;       // 音量
};

long timeOffset = 0;
int last_received_measure = -1; 
int status = WL_IDLE_STATUS;
bool play_frag = false;

// ─── 楽譜データ ───
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
int myOffset = 0;  // 自分がどれくらい遅れて演奏するか設定（輪唱用）

// ─── 演奏・LED制御用の変数 ───
int idx = 0;                      // 楽譜のインデックス
bool isPlaying = false;           // 再生中フラグ
int current_bpm = 120;
int current_volume = 100;
int current_effects = 0;
int current_measure = 0;
int order;

float note_progress = 0.0;        // 現在の音符の描画進捗（0.25 = 1ピクセル分）
unsigned long last_shift_time = 0;// 最後にLEDをシフトした時刻

void setup() {
  Serial.begin(115200);
  initLEDMatrix(); // LEDマトリクスの初期化
  
  WiFi.config(localIp);
  while (!Serial) { ; } 

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }
  Serial.println("WiFi Connected");
  Udp.begin(localport);
}

// 指揮者と時刻を同期する処理
void syncWithLeader() {
  unsigned long t1 = millis();
  Udp.beginPacket(leaderIp, localport);
  MusicData time = {0, 0, 0, 0};
  Udp.write((const uint8_t*)&time, sizeof(time)); 
  Udp.endPacket();

  while (!Udp.parsePacket());
  uint32_t masterTime;
  Udp.read((uint8_t*)&masterTime, sizeof(masterTime));

  unsigned long t2 = millis();
  unsigned long delayTime = (t2 - t1) / 2;
  timeOffset = (masterTime + delayTime) - t2;
  play_frag = true;
}

void loop() {
  // ─── ① UDPパケット受信と同期・ジャンプ処理 ───
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    char packetBuffer[255];
    Udp.read(packetBuffer, 255);

    if (packetSize == 1) {
      order = packetBuffer[0];
      myOffset = order * 16; // 演奏順に応じて輪唱オフセットを設定（カエルの歌は2小節=16ステップずつ遅らせる）
      syncWithLeader(); 
    } 
    else if (packetSize == sizeof(MusicData)) {
      MusicData receivedData;
      memcpy(&receivedData, packetBuffer, sizeof(receivedData));
      
      current_bpm = receivedData.currentBPM;
      current_volume = receivedData.volume;
      current_effects = receivedData.effects;

      // ターンテーブルによるジャンプ機能
      // 【注意】自動進行によるダブりを防ぐため、差が「1小節以上」ある場合のみ強制ジャンプさせる
        int target_measure = receivedData.currentmeasure - order * 2; // 遅れも2小節刻み（1声部=2小節）

#if DEBUG_JUMP
      // 切り分け用：M(指揮者小節) / ord(順番) / tgt / cur / idx / myIdx / diff / JUMP判定
      Serial.print("#DBG M=");   Serial.print(receivedData.currentmeasure);
      Serial.print(" ord=");     Serial.print(order);
      Serial.print(" tgt=");     Serial.print(target_measure);
      Serial.print(" cur=");     Serial.print(current_measure);
      Serial.print(" idx=");     Serial.print(idx);
      Serial.print(" myIdx=");   Serial.print(idx - myOffset);
      Serial.print(" diff=");    Serial.print(target_measure - current_measure);
      Serial.print(" JUMP=");    Serial.println(abs(target_measure - current_measure) > 1 ? 1 : 0);
#endif

      if (abs(target_measure - current_measure) > 1) {
        play_frag = true;
#if DEBUG_JUMP
        Serial.print("#JUMP cur="); Serial.print(current_measure);
        Serial.print(" -> tgt=");   Serial.print(target_measure);
        Serial.print("  idx=");     Serial.print(idx);
        Serial.print(" -> ");       Serial.println(target_measure * 8 + myOffset);
#endif
        last_received_measure = target_measure;
        current_measure = target_measure;
        
        idx = current_measure * 8 + myOffset;               
        note_progress = 0.0;                     
        last_shift_time = millis() + timeOffset; 
      }

      // 演奏開始トリガー
      if (!isPlaying && play_frag) {
        isPlaying = true;
        idx = (receivedData.currentmeasure) * 8; // 指揮者の小節から開始
        note_progress = 0.0;
        last_shift_time = millis() + timeOffset;
#if DEBUG_JUMP
        Serial.print("#START M="); Serial.print(receivedData.currentmeasure);
        Serial.print(" ord=");     Serial.print(order);
        Serial.print(" idx=");     Serial.print(idx);
        Serial.print(" myIdx=");   Serial.println(idx - myOffset);
#endif
      }
    }
  }
  
  // ─── ② 演奏・音符出力・LED描画処理 ───
  if (!isPlaying) return;
  if (current_measure > 7) return;

  unsigned long current_time = millis() + timeOffset;
  unsigned long pixel = (60000.0 / current_bpm) / 2; 

  if (current_time - last_shift_time >= pixel) {
    
    if (idx < len + myOffset) {
      int myIdx = idx - myOffset;
      int myPitch = 0;   
      float duration = 0.5;

      //演奏する箇所だけ音を鳴らす
      if (myIdx >= 0 && myIdx < len) {
        myPitch = score[myIdx].pitch;
        duration = score[myIdx].duration;

        if (note_progress == 0.0) {
          unsigned long L_ms = (60000.0 / current_bpm) * duration;
          tx(myPitch, current_volume, L_ms, current_effects, current_measure);
        }
      }

      updateNoteDisplay(myPitch); 
      note_progress += 0.5;

      if (note_progress >= duration - 0.01) {
        idx++;
        note_progress = 0.0;
        if (myIdx >= 0){
          current_measure = (myIdx / 8);
        }else{
          current_measure = (idx - myOffset) / 8;
        }
      }
      
      last_shift_time += pixel; // 【修正】楽譜進行中のみ時間を進める
      
    } else {
      // 楽譜が終わったら最初からループ
      updateNoteDisplay(0);
      isPlaying = false; // 一度演奏を止め、次の指揮者からの指示（ジャンプ等）を待つ
      idx = 0;
      note_progress = 0.0;
      play_frag = false;
      //last_shift_time = current_time + 50; // 曲間の短いブレイクタイム
      // timeOffset = 0; // 時刻同期をリセット（次の指揮者からの指示で再同期）
    }
  }
}

// データをProcessingへシリアル出力する関数
void tx(int p, int v, unsigned long d, int e, int m) {
  Serial.print(p);      Serial.print(",");
  Serial.print(v);      Serial.print(",");
  Serial.print(d);      Serial.print(",");
  Serial.print(e);      Serial.print(",");
  Serial.println(m);
}