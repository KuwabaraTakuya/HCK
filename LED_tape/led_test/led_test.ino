# include <Adafruit_NeoPixel.h>
# define SIG_PIN (6)
# define NUM_LEDS (60)
# define BRIGHTNESS (32) // 0-255

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, SIG_PIN);

unsigned int ptrR=0;
unsigned int ptrG=0;
unsigned int ptrB=0;

void setup() {
  Serial.begin(115200);
  pinMode(SIG_PIN, OUTPUT);  
  strip.setBrightness(BRIGHTNESS);
  strip.begin();
  strip.clear();
}

void loop() {
  ptrR = (ptrR+1)&0xff;
  ptrG = (ptrG+2)&0xff;
  ptrB = (ptrB+3)&0xff;
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    unsigned int valR = strip.sine8((ptrR+i)&0xff);
    unsigned int valG = strip.sine8((ptrG+i)&0xff);
    unsigned int valB = strip.sine8((ptrB+i)&0xff);
    uint32_t c = strip.Color(valR,valG,valB);
    strip.setPixelColor(i, c);
  }
  strip.show();
  delay(10);
}