#include "Board21.h"

/*---------------------------------------------------------------
 * Lesson state
 * The labels show the current backlight level and button state.
 * Remaining values track the quadrature encoder between samples.
 *--------------------------------------------------------------*/
static lv_obj_t *brightnessLabel;
static lv_obj_t *buttonLabel;
static int brightness = 50;
static uint8_t lastState;
static int8_t steps;
static uint8_t rawButtonState = HIGH;
static uint8_t stableButtonState = HIGH;
static uint32_t buttonChangedAt = 0;

/* Quadrature transitions: each index combines old state and new state. */
const int8_t trans[16] = {
  0, -1, 1, 0,
  1, 0, 0, -1,
  -1, 0, 0, 1,
  0, 1, -1, 0
};

/**
 * @brief Refresh the labels and apply the current backlight level.
 * @param None.
 * @return None.
 * @call After a brightness change and once during setup.
 */
void labels() {
  char s[32];
  snprintf(s, sizeof(s), "Brightness: %d%%", brightness);
  lv_label_set_text(brightnessLabel, s);
  ledcWrite(SCREEN_BACKLIGHT_PIN, brightness * 255 / 100);
}

/**
 * @brief Initialize the board, encoder inputs, and status labels.
 * @param None.
 * @return None.
 * @call Once after reset or power-up.
 */
void setup() {
  Serial.begin(115200);
  boardBegin(true);

  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  lastState = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);

  brightnessLabel = lv_label_create(lv_screen_active());
  lv_obj_align(brightnessLabel, LV_ALIGN_CENTER, 0, -40);
  lv_obj_set_style_text_font(brightnessLabel, &lv_font_montserrat_32, 0);
  buttonLabel = lv_label_create(lv_screen_active());
  lv_obj_align(buttonLabel, LV_ALIGN_CENTER, 0, 30);
  lv_obj_set_style_text_font(buttonLabel, &lv_font_montserrat_32, 0);

  // Read the expander input with latch refresh enabled, as required by this PCF8574 library.
  rawButtonState = boardPcf.digitalRead(P5, true);
  stableButtonState = rawButtonState;
  buttonChangedAt = millis();
  lv_label_set_text(buttonLabel, "Button released");
  labels();
}

/**
 * @brief Decode the encoder, debounce the button, and service LVGL.
 * @param None.
 * @return None.
 * @call Continuously by the Arduino runtime.
 */
void loop() {
  uint8_t s = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);
  steps += trans[(lastState << 2) | s];
  lastState = s;

  // Four valid transitions represent one encoder detent.
  if (steps >= 4) {
    brightness = constrain(brightness + 5, 0, 100);
    steps = 0;
    labels();
  }
  if (steps <= -4) {
    brightness = constrain(brightness - 5, 0, 100);
    steps = 0;
    labels();
  }

  // P5 is active-low. The second argument refreshes the PCF8574 input latch.
  uint8_t sampledState = boardPcf.digitalRead(P5, true);
  if (sampledState != rawButtonState) {
    buttonChangedAt = millis();
    rawButtonState = sampledState;
  }
  if (rawButtonState != stableButtonState && millis() - buttonChangedAt >= 50) {
    stableButtonState = rawButtonState;
    lv_label_set_text(buttonLabel,
      stableButtonState == LOW ? "Button pressed" : "Button released");
  }

  lv_timer_handler();
  delay(2);
}
