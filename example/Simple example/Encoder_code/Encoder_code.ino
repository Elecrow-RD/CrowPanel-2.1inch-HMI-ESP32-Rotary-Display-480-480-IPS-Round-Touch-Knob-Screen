#define ENCODER_A_PIN 45
#define ENCODER_B_PIN 42
#define SWITCH_PIN 41

volatile unsigned long lastPressTime = 0;
volatile int clickCount = 0;
const unsigned long debounceTime = 20;       // ms
const unsigned long doubleClickTime = 300;   // ms

void IRAM_ATTR buttonISR() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > debounceTime) {
    if (!digitalRead(SWITCH_PIN)) {     // active low
      lastPressTime = interruptTime;
      clickCount++;
    }
  }
  lastInterruptTime = interruptTime;
}

int currentStateCLK = 0;
int lastStateCLK = 0;

void setup() {
  Serial.begin(115200);

  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), buttonISR, CHANGE);

  lastStateCLK = digitalRead(ENCODER_A_PIN);
  Serial.println("Encoder Minimal Demo (A=45, B=42, SW=41)");
}

void loop() {
  // Read encoder A
  currentStateCLK = digitalRead(ENCODER_A_PIN);

  // Detect rising edge on A
  if (currentStateCLK != lastStateCLK && currentStateCLK == HIGH) {
    bool ccw = (digitalRead(ENCODER_B_PIN) != currentStateCLK);
    if (ccw) {
      Serial.println("CCW");
    } else {
      Serial.println("CW");
    }
  }
  lastStateCLK = currentStateCLK;

  // Click / Double-click detection
  if (clickCount == 1 && millis() - lastPressTime > doubleClickTime) {
    Serial.println("CLICK");
    clickCount = 0;
  } else if (clickCount >= 2) {
    Serial.println("DOUBLE CLICK");
    clickCount = 0;
  } else if (clickCount > 2) {
    clickCount = 0;
  }

  delay(10);
}