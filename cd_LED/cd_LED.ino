#include "LEDMatrix.h"
void setup() {
  Serial.begin(115200);
  initLEDMatrix();
}

// void loop() {
//   //動作テスト
//     for (int bpm = 120; bpm <= 130; bpm++) {
//         updateDisplay(bpm);
//         delay(100);
//     }
// }

void loop() {
  int val = analogRead(A1);
  int bpm = map(val, 0, 1023, -7, 180);
  updateDisplay(bpm);
  Serial.print(val);
  Serial.print(",");
  Serial.println(bpm);
  delay(100);
}
