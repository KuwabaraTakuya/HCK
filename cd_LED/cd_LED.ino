#include "LEDMatrix.h"
void setup() {
  Serial.begin(115200);
  initLEDMatrix();
}

void loop() {
  int val = analogRead(A1);
  int bpm = map(val, 0, 1023, 60, 180);
  updateDisplay(bpm);
  // Serial.print(val);
  // Serial.print(",");
  // Serial.println(bpm);
}
