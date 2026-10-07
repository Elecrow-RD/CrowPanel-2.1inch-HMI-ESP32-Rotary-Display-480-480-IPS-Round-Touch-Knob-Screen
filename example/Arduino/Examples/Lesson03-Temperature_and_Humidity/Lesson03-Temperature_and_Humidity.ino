#include "Board21.h"

/*---------------------------------------------------------------
 * Lesson state
 * The label displays the latest sensor result or a read error.
 *--------------------------------------------------------------*/
static lv_obj_t *label;

/**
 * @brief Initialize the display and show the initial sensor status.
 * @param None.
 * @return None.
 * @call Once after reset or power-up.
 */
void setup() {
  Serial.begin(115200);
  boardBegin(true);
  label = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);
  lv_obj_center(label);
  lv_label_set_text(label, "Reading DHT20...");
}

/**
 * @brief Periodically read DHT20 data and update the display.
 * @param None.
 * @return None.
 * @call Continuously by the Arduino runtime.
 */
void loop() {
  static uint32_t t = 0;

  if (millis() - t > 2000) {
    t = millis();
    Wire1.begin(38, 39);

    // Start a DHT20 measurement at its fixed I2C address.
    uint8_t c[] = {0xAC, 0x33, 0};
    Wire1.beginTransmission(0x38);
    Wire1.write(c, 3);
    bool ok = Wire1.endTransmission() == 0;
    delay(80);
    ok = ok && Wire1.requestFrom(0x38, 6) == 6;

    char s[80];
    if (ok) {
      uint8_t d[6];
      for (int i = 0; i < 6; i++) {
        d[i] = Wire1.read();
      }

      // Reassemble the sensor bit fields and convert them to engineering units.
      float h = (
        ((uint32_t)d[1] << 12) |
        ((uint32_t)d[2] << 4) |
        (d[3] >> 4)) * 100.0 / 1048576.0;
      float temp = (
        ((uint32_t)(d[3] & 15) << 16) |
        ((uint32_t)d[4] << 8) |
        d[5]) * 200.0 / 1048576.0 - 50;

      snprintf(s, sizeof(s),
        "Temperature: %.1f C\nHumidity: %.1f %%", temp, h);
    } else {
      snprintf(s, sizeof(s), "DHT20 read error");
    }

    lv_label_set_text(label, s);
    Serial.println(s);
  }

  lv_timer_handler();
  delay(5);
}
