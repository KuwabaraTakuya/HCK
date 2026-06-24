#ifndef INST_LEDMATRIX_H
#define INST_LEDMATRIX_H
#include "Arduino_LED_Matrix.h"

void initLEDMatrix();
void updateNoteDisplay(int pitch);
int ledheight(int pitch);
void clearDisplay();

#endif