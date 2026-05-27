#ifndef LEDMatrix_h
#define LEDMatrix_h
#include "Arduino_LED_Matrix.h"
void initLEDMatrix();
void displayDigit(int d, int s_r, int s_c);
void updateDisplay(int v);
#endif