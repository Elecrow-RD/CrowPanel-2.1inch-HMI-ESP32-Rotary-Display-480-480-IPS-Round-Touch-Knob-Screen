#include <Adafruit_NeoPixel.h>

#define LED_PIN 48
#define LED_NUM 5
#define DEFAULT_BRIGHTNESS 25

Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_NUM, LED_PIN, NEO_GRB + NEO_KHZ800);

void setAll(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < LED_NUM; i++) {
    strip.setPixelColor(i, strip.Color(r, g, b));
  }
  strip.show();
}

void clearAll() {
  strip.clear();
  strip.show();
}

void colorChase(uint32_t color, uint16_t stepDelayMs, uint8_t rounds) {
  for (uint8_t round = 0; round < rounds; round++) {
    for (int i = 0; i < LED_NUM; i++) {
      strip.clear();
      strip.setPixelColor(i, color);
      strip.show();
      delay(stepDelayMs);
    }
  }
}

void breathe(uint32_t color, uint8_t maxBrightness /*0-255*/, uint8_t cycles, uint16_t stepDelayMs) {
  // Save current brightness
  uint8_t original = strip.getBrightness();
  for (uint8_t c = 0; c < cycles; c++) {
    for (uint8_t b = 0; b <= maxBrightness; b++) {
      strip.setBrightness(b);
      for (int i = 0; i < LED_NUM; i++) strip.setPixelColor(i, color);
      strip.show();
      delay(stepDelayMs);
    }
    for (int b = maxBrightness; b >= 0; b--) {
      strip.setBrightness((uint8_t)b);
      for (int i = 0; i < LED_NUM; i++) strip.setPixelColor(i, color);
      strip.show();
      delay(stepDelayMs);
    }
  }
  strip.setBrightness(original);
  strip.show();
}

void setup() {
  strip.begin();
  strip.setBrightness(DEFAULT_BRIGHTNESS);
  strip.clear();
  strip.show();
}

void loop() {
  // 1) Solid colors
  setAll(255, 0, 0);   // Red
  delay(1000);
  setAll(0, 255, 0);   // Green
  delay(1000);
  setAll(0, 0, 255);   // Blue
  delay(1000);

  // 2) White chase (流水)
  colorChase(strip.Color(255, 255, 255), 150, 3);

  // 3) Breathing purple (呼吸灯)
  breathe(strip.Color(130, 0, 255), 50, 3, 20);

  // Pause then repeat
  clearAll();
  delay(500);
}