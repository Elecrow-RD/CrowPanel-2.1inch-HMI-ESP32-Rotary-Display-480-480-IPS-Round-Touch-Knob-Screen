#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_CST8XX.h>
#include <PCF8574.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

/*---------------------------------------------------------------
 * Board hardware configuration
 * Pin assignments and device addresses for the ESP32-S3 2.1-inch board.
 *--------------------------------------------------------------*/
static constexpr uint16_t SCREEN_WIDTH = 480;
static constexpr uint16_t SCREEN_HEIGHT = 480;
static constexpr int I2C_SDA_PIN = 38;
static constexpr int I2C_SCL_PIN = 39;
static constexpr int SCREEN_BACKLIGHT_PIN = 6;
static constexpr int ONBOARD_LED_PIN = 43;
static constexpr int ENCODER_A_PIN = 42;
static constexpr int ENCODER_B_PIN = 4;
static constexpr int I2C_TOUCH_ADDR = 0x15;

/*---------------------------------------------------------------
 * Display, touch, and shared board objects
 *--------------------------------------------------------------*/
static PCF8574 boardPcf(0x21);
static Adafruit_CST8XX boardTouch;
static Arduino_DataBus *boardBus = new Arduino_SWSPI(
  GFX_NOT_DEFINED, 16, 2, 1, GFX_NOT_DEFINED);
static Arduino_ESP32RGBPanel *boardRgb = new Arduino_ESP32RGBPanel(
  40, 7, 15, 41, 46, 3, 8, 18, 17, 14, 13, 12, 11, 10, 9,
  5, 45, 48, 47, 21, 1, 10, 4, 20, 1, 10, 4, 20, 0, 12000000,
  false, 0, 0, 480 * 20);
static Arduino_RGB_Display *boardGfx = new Arduino_RGB_Display(
  SCREEN_WIDTH, SCREEN_HEIGHT, boardRgb, 0, true, boardBus,
  GFX_NOT_DEFINED, st7701_type5_init_operations,
  sizeof(st7701_type5_init_operations));

// LVGL draw buffers and display handle shared by course examples.
static uint8_t *boardBuf1 = nullptr;
static uint8_t *boardBuf2 = nullptr;
static lv_display_t *boardDisplay = nullptr;

/**
 * @brief Enable board power rails, reset lines, and indicator outputs.
 * @param None.
 * @return None.
 * @call Once at the beginning of board initialization.
 */
inline void boardPowerOn() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Configure the PCF8574 pins used by the display power sequence.
  boardPcf.pinMode(P0, OUTPUT);
  boardPcf.pinMode(P2, OUTPUT);
  boardPcf.pinMode(P3, OUTPUT);
  boardPcf.pinMode(P4, OUTPUT);
  boardPcf.pinMode(P5, INPUT_PULLUP);
  boardPcf.begin();

  // Apply the board-specific power and reset timing.
  boardPcf.digitalWrite(P3, HIGH);
  delay(50);
  boardPcf.digitalWrite(P4, LOW);
  delay(100);
  boardPcf.digitalWrite(P4, HIGH);
  delay(100);
  boardPcf.digitalWrite(P0, LOW);
  delay(100);
  boardPcf.digitalWrite(P0, HIGH);
  delay(100);
  boardPcf.digitalWrite(P2, HIGH);

  // Start the backlight at 204/255 duty and turn the LED off.
  pinMode(SCREEN_BACKLIGHT_PIN, OUTPUT);
  ledcAttach(SCREEN_BACKLIGHT_PIN, 5000, 8);
  ledcWrite(SCREEN_BACKLIGHT_PIN, 204);
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, LOW);
}

/**
 * @brief Initialize the RGB display and CST8XX touch controller.
 * @param None.
 * @return None.
 * @call After boardPowerOn() and before creating display content.
 */
inline void boardDisplayBegin() {
  boardGfx->begin();

  // Set the panel orientation expected by the 480 x 480 board layout.
  boardBus->beginWrite();
  boardBus->writeCommand(0x36);
  boardBus->write(0x08);
  boardBus->endWrite();
  boardGfx->fillScreen(0x0000);
  boardTouch.begin(&Wire, I2C_TOUCH_ADDR);
}

/**
 * @brief Create the LVGL display and connect its flush callback.
 * @param None.
 * @return None.
 * @call Only when the course uses LVGL widgets or labels.
 */
inline void boardLvglBegin() {
  lv_init();
  lv_tick_set_cb(millis);

  const size_t bytes = sizeof(uint16_t) * SCREEN_WIDTH * 40;
  boardBuf1 = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  boardBuf2 = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);

  // Fall back to ordinary heap memory when PSRAM is unavailable.
  if (!boardBuf1) {
    boardBuf1 = (uint8_t *)malloc(bytes);
  }
  if (!boardBuf2) {
    boardBuf2 = (uint8_t *)malloc(bytes);
  }

  boardDisplay = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_color_format(boardDisplay, LV_COLOR_FORMAT_RGB565);
  lv_display_set_buffers(
    boardDisplay, boardBuf1, boardBuf2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(boardDisplay,
    [](lv_display_t *d, const lv_area_t *a, uint8_t *p) {
      // Copy the rendered area, then release LVGL's draw buffer.
      boardGfx->draw16bitRGBBitmap(
        a->x1, a->y1, (uint16_t *)p,
        a->x2 - a->x1 + 1, a->y2 - a->y1 + 1);
      lv_display_flush_ready(d);
    });
}

/**
 * @brief Read and clamp the current CST8XX touch point.
 * @param x Reference receiving the horizontal coordinate.
 * @param y Reference receiving the vertical coordinate.
 * @return true when a touch point is available; false otherwise.
 * @call From an input callback or the main loop.
 */
inline bool boardTouchRead(uint16_t &x, uint16_t &y) {
  if (!boardTouch.touched()) {
    return false;
  }

  CST_TS_Point p = boardTouch.getPoint(0);
  x = constrain(p.x, 0, SCREEN_WIDTH - 1);
  y = constrain(p.y, 0, SCREEN_HEIGHT - 1);
  return true;
}

/**
 * @brief Run common board initialization.
 * @param lvgl Set true to initialize the LVGL display bridge.
 * @return None.
 * @call Once from setup() before using board peripherals.
 */
inline void boardBegin(bool lvgl = false) {
  boardPowerOn();
  boardDisplayBegin();
  if (lvgl) {
    boardLvglBegin();
  }
}
