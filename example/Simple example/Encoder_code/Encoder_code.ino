#include <Arduino.h>
#include <Wire.h>
#include <PCF8574.h>

#define SDA 38
#define SCL 39

#define ENCODER_CLK_PIN 42
#define ENCODER_DT_PIN 4

volatile unsigned long lastPressTime = 0;
volatile int clickCount = 0;

const unsigned long doubleClickTime = 300;   // ms

int currentStateCLK = 0;
int lastStateCLK = 0;

PCF8574 pcf8574(I2C_IO_EXPANDER_ADDRESS);
bool lastButtonState = true; // released (pull-up = high)

void setup() {
  Serial.begin(115200);

  // Encoder-Switch
  Wire.begin(SDA, SCL);
  pcf8574.pinMode(ENCODER_BUTTON_PIN, INPUT_PULLUP);
  if (!pcf8574.begin()) {
    Serial.println("Can't init pcf8574");
  }

  pinMode(ENCODER_CLK_PIN, INPUT);
  pinMode(ENCODER_DT_PIN, INPUT);

  lastStateCLK = digitalRead(ENCODER_CLK_PIN);
  Serial.println("Encoder Minimal Demo (A=42, B=4, SW=PCF8574 P5)");
}

void loop() {
  // Read encoder clock
  currentStateCLK = digitalRead(ENCODER_CLK_PIN);

  // Detect rising edge on encoder clock
  if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH) {
    bool ccw = (digitalRead(ENCODER_DT_PIN) != currentStateCLK);
    Serial.println(ccw ? "CCW" : "CW");
  }
  lastStateCLK = currentStateCLK;

  // Poll button with PCF8574 w/o interrupt
  bool buttonState = pcf8574.digitalRead(ENCODER_BUTTON_PIN, true); // true = fresh read
  if (lastButtonState && !buttonState) { // changed: released -> pressed
    lastPressTime = millis();
    clickCount += 1;
  }
  lastButtonState = buttonState;

  // Click / Double-click detection
  if (clickCount == 1 && millis() - lastPressTime > doubleClickTime) {
    Serial.println("CLICK");
    clickCount = 0;
  } else if (clickCount >= 2) {
    Serial.println("DOUBLE CLICK");
    clickCount = 0;
  }

  delay(10);
}
