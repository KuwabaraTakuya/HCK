#include <WiFiS3.h>
#include <WiFiUdp.h>

const char ssid[] = "YOUR_SSID";
const char pass[] = "YOUR_PASS";
const IPAddress leaderIp(192, 168, 1, XX); // 指揮者の固定IP
unsigned int localPort = 10001;

WiFiUDP Udp;
long timeOffset = 0;
unsigned long nextSyncTime = 0;
unsigned long scheduledStartTime = 0;
int currentBPM = 0;
bool isPlaying = false;

// 簡易メロディデータ (MIDI番号, 4分音符を1.0とした長さ)
float melody[][2] = {{60, 1.0}, {62, 1.0}, {64, 1.0}, {65, 1.0}};
int noteIndex = 0;
unsigned long nextNoteTime = 0;

void setup() {
  Serial.begin(115200); // Processing通信用
  while (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(ssid, pass);
    delay(500);
  }
  Udp.begin(localPort);
  syncWithLeader(); // 初回同期
}

void loop() {
  unsigned long now = millis();

  // --- 1. 定期的な再同期 (10秒おき) ---
  if (now > nextSyncTime) {
    syncWithLeader();
    nextSyncTime = now + 10000;
  }

  // --- 2. 指揮者からのブロードキャスト受信 ---
  int packetSize = Udp.parsePacket();
  if (packetSize > 0) {
    char type = Udp.read();
    if (type == 'S') {
      Udp.read((byte)&scheduledStartTime, sizeof(scheduledStartTime));
      Udp.read((byte)&currentBPM, sizeof(currentBPM));
      isPlaying = true;
      noteIndex = 0;
      nextNoteTime = scheduledStartTime;
      Serial.println("STATUS:ReadyToStart");
    }
  }

  // --- 3. 演奏処理 ---
  if (isPlaying) {
    unsigned long globalTime = millis() + timeOffset;
    if (globalTime >= nextNoteTime) {
      // Processingへ音符情報を送信 (CSV形式: NOTE,音高,長さ)
      Serial.print("NOTE,");
      Serial.print(melody[noteIndex][0]);
      Serial.print(",");
      Serial.println(melody[noteIndex][1]);

      // 次の音の時間を計算 (ms = 60000 / BPM * 長さ)
      unsigned long duration = (60000 / currentBPM) * melody[noteIndex][1];
      nextNoteTime += duration;
      noteIndex++;

      if (noteIndex >= 4) isPlaying = false; // 曲終
    }
  }
}
void syncWithLeader() {
  unsigned long t1 = millis();
  Udp.beginPacket(leaderIp, 10000);
  Udp.write('T'); // Time request
  Udp.endPacket();

  // 応答待ち (簡易的に100ms待機)
  delay(100);
  if (Udp.parsePacket()) {
    unsigned long masterTime;
    Udp.read((byte*)&masterTime, sizeof(masterTime));
    unsigned long t2 = millis();
    long rtt = t2 - t1;
    timeOffset = (long)masterTime + (rtt / 2) - (long)t2;
    Serial.println("SyncOK: Offset=" + String(timeOffset));
  }
} // 奏者側コード 
