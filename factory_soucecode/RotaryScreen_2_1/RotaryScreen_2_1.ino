#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <esp_system.h>
#include <Adafruit_CST8XX.h>
#include "ui.h"
#include "PCF8574.h" 

/*---------------------------------------------------------------
 * Hardware connections and shared devices
 * Define the fixed pins and device objects used by the display,
 * touch controller, encoder, and I/O expander.
 *--------------------------------------------------------------*/

#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39
// Provides reset, power, interrupt, and encoder-button I/O.
PCF8574 pcf8574(0x21);

#define ENCODER_A_PIN 42
#define ENCODER_B_PIN 4

// Counts completed button clicks until the single/double-click window closes.
volatile int pressCount = 0;  

/* Incremented by the GPIO sampling task for every rotary step. Button clicks
 * are accepted only if this value stays unchanged for the complete gesture
 * and the following double-click decision window. */
volatile uint32_t encoderRotationSequence = 0;

// Timing values used to debounce the encoder button and detect double clicks.
const unsigned long debounceTime = 50;    
const unsigned long doubleClickTime = 300;  

// Marks when a pending single click may be dispatched.
volatile unsigned long singleClickTimeout = 0; 
// Carries the last menu-navigation direction into processEncoder().
volatile int8_t position_tmp = 2;      

#define SCREEN_BACKLIGHT_PIN 6
// Shared PWM settings for the screen backlight and the onboard indicator LED.
const int pwmFreq = 5000;
const int pwmResolution = 8;

/* LVGL is single-threaded. The background task only samples GPIO and places
 * actions in a queue. All LVGL calls are performed by loop() on one core. */
enum EncoderActionType : int8_t {
  ENCODER_ROTATE_CW = 1,
  ENCODER_ROTATE_CCW = 2,
  ENCODER_CLICK = 3,
  ENCODER_DOUBLE_CLICK = 4
};
struct EncoderAction {
  EncoderActionType type;
  uint32_t rotationSequence;
};
// Transfers encoder actions safely from the sampling task to loop().
QueueHandle_t encoderActionQueue = NULL;
void encTaskSafe(void *pvParameters);
void handleEncoderAction(const EncoderAction &action);
void pollEncoderButton();
void updateScreen(int index);
void performClickAction();
void menuCardClickEventCb(lv_event_t *e);
void returnButtonClickEventCb(lv_event_t *e);
void loadScreenImmediately(lv_obj_t **target, void (*targetInit)(void));

#define I2C_TOUCH_ADDR 0x15
// Represents the CST8XX capacitive touch controller.
Adafruit_CST8XX tsPanel = Adafruit_CST8XX();
// Logical resolution shared by the RGB panel, LVGL, and touch coordinates.
static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;

// Full-size LVGL draw buffers allocated in external PSRAM during setup().
static uint8_t *buf1 = NULL;
static uint8_t *buf2 = NULL;

// Tracks the active LVGL screen and the selected item on the main menu.
lv_obj_t *current_screen = NULL;
int screen1_index = 1;

/*---------------------------------------------------------------
 * RGB display driver configuration
 * Configure the ST7701 command bus, RGB data pins, timing, and DMA
 * bounce buffers required by the 480 x 480 panel.
 *--------------------------------------------------------------*/

Arduino_DataBus *panelInitBus = new Arduino_SWSPI(
  GFX_NOT_DEFINED /* DC: ST7701 uses 9-bit SPI */, 16 /* CS */,
  2 /* SCK */, 1 /* SDA */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbPanel = new Arduino_ESP32RGBPanel(
  40 /* DE */, 7 /* VSYNC */, 15 /* HSYNC */, 41 /* PCLK */,
  46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
  14 /* G0/P22 */, 13 /* G1/P23 */, 12 /* G2/P24 */, 11 /* G3/P25 */, 10 /* G4/P26 */, 9 /* G5 */,
  5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
  1 /* hsync polarity */, 10 /* front porch */, 4 /* pulse width */, 20 /* back porch */,
  1 /* vsync polarity */, 10 /* front porch */, 4 /* pulse width */, 20 /* back porch */,
  0 /* pclk active neg */, 12000000 /* pixel clock */, false /* native endian */,
  0 /* de idle high */, 0 /* pclk idle high */,
  480 * 20 /* two internal-DMA bounce buffers, 20 lines each */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
  480 /* width */, 480 /* height */, rgbPanel, 0 /* rotation */, true /* auto flush */,
  panelInitBus, GFX_NOT_DEFINED /* RST */,
  st7701_type5_init_operations, sizeof(st7701_type5_init_operations));

/*---------------------------------------------------------------
 * LVGL display output
 * Convert LVGL's RGB565 output to the panel's channel order and
 * copy only the invalidated rectangle to the screen.
 *--------------------------------------------------------------*/

/**
 * @brief Flush an LVGL drawing area to the RGB display.
 *
 * Swaps the red and blue RGB565 fields required by this ST7701 wiring,
 * writes the resulting pixels, and then releases LVGL's draw buffer.
 *
 * @param display LVGL display that requested the flush operation.
 * @param area Inclusive rectangle that must be transferred.
 * @param px_map RGB565 pixel data produced by LVGL.
 * @return void
 * @note Called automatically by LVGL whenever an invalidated area is ready.
 */
void my_disp_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  /* ESP-IDF 5's RGB path on this ST7701 board presents the red and blue
     5-bit fields in the opposite order from the legacy working driver.
     Swap only R/B; swapping the two bytes corrupts all RGB565 bit fields. */
  uint16_t *pixels = (uint16_t *)px_map;
  const uint32_t pixelCount = w * h;
  for (uint32_t i = 0; i < pixelCount; ++i) {
    const uint16_t c = pixels[i];
    pixels[i] = (c & 0x07E0) | ((c & 0x001F) << 11) | ((c & 0xF800) >> 11);
  }
  gfx->draw16bitRGBBitmap(area->x1, area->y1, pixels, w, h);
  lv_display_flush_ready(display);
}

/*---------------------------------------------------------------
 * Touch input and main-menu swipe navigation
 * Report stable pointer coordinates to LVGL and recognize one
 * horizontal menu swipe per touch gesture.
 *--------------------------------------------------------------*/

// Gesture thresholds reject slow or strongly vertical movements.
const int SWIPE_THRESHOLD = 100;  
const int TIME_THRESHOLD = 300;   
const int VERTICAL_LIMIT = 100;    
int startX = 0;
int startY = 0;
unsigned long startTime = 0;
bool trackingSwipe = false;
bool first_click = false;
bool swipeHandled = false;

/**
 * @brief Read the touch controller and provide pointer data to LVGL.
 *
 * Filters small coordinate changes to reduce redraw noise. On the main
 * menu, it also detects horizontal swipes and updates the selected card.
 *
 * @param indev LVGL input device requesting a new sample.
 * @param data Output structure receiving pointer coordinates and state.
 * @return void
 * @note Called periodically by LVGL while lv_timer_handler() is running.
 */
void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (tsPanel.touched()) {
      CST_TS_Point p = tsPanel.getPoint(0);
      /* CST8XX coordinates can move by a few pixels while a finger is held
         still. Keep the last point for tiny moves so LVGL doesn't redraw a
         pressed object on every noisy sample. */
      static int16_t stableX = -1;
      static int16_t stableY = -1;
      const int16_t touchX = constrain(p.x, 0, screenWidth - 1);
      const int16_t touchY = constrain(p.y - 20, 0, screenHeight - 1);
      if (stableX < 0 || abs(touchX - stableX) >= 4 || abs(touchY - stableY) >= 4) {
        stableX = touchX;
        stableY = touchY;
      }
      data->point.x = stableX;
      data->point.y = stableY;
      data->state = LV_INDEV_STATE_PR;
      lv_obj_t * current_screen = lv_screen_active();
      if (current_screen == ui_Screen1) {
          if (first_click == false && !swipeHandled) {
            startX = p.x;
            startY = p.y;
            startTime = millis();
            trackingSwipe = true;
            first_click = true;
          }
          if (trackingSwipe && !swipeHandled && p.event == TOUCHING) {
            int deltaX = p.x - startX;
            int deltaY = abs(p.y - startY);
            unsigned long elapsed = millis() - startTime;
            if (elapsed < TIME_THRESHOLD && deltaY < VERTICAL_LIMIT) {
              if (deltaX > SWIPE_THRESHOLD) {
                trackingSwipe = false;
                swipeHandled = true;
                /* The cards are moved manually rather than by LVGL's scroll
                   engine, so LVGL would otherwise treat this gesture as a
                   click on the card that was pressed before the move.  Keep
                   the pointer ignored until the finger is physically lifted;
                   the release is then reported as PRESS_LOST, not CLICKED. */
                lv_indev_wait_release(indev);
                lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                // Right swipe reveals the item on the left.
                screen1_index = screen1_index - 1;
                if(screen1_index < 0)
                {
                    screen1_index = 0;
                }
                switch (screen1_index) {
                    case 0:  // Volume
                    // Place the volume card at the center and temperature to its right.
                    lv_obj_remove_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, 0);
                    lv_obj_set_x(ui_volumeTextBlue, 0);
                    lv_obj_set_x(ui_volumeWhite, 0);
                    lv_obj_set_x(ui_volumeTextWhite, 0);

                    lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 140);
                    lv_obj_set_x(ui_tempTextBlue, 140);
                    lv_obj_set_x(ui_tempWhite, 140);
                    lv_obj_set_x(ui_tempTextWhite, 140);

                    // The light card lies outside the visible three-card arrangement.
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    break;

                    case 1:  // Temperature
                    // Place temperature at the center with its adjacent cards on each side.
                    lv_obj_remove_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, -140);
                    lv_obj_set_x(ui_volumeTextBlue, -140);
                    lv_obj_set_x(ui_volumeWhite, -140);
                    lv_obj_set_x(ui_volumeTextWhite, -140);

                    lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 0);
                    lv_obj_set_x(ui_tempTextBlue, 0);
                    lv_obj_set_x(ui_tempWhite, 0);
                    lv_obj_set_x(ui_tempTextWhite, 0);

                    lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 140);
                    lv_obj_set_x(ui_lightTextBlue, 140);
                    lv_obj_set_x(ui_lightWhite, 140);
                    lv_obj_set_x(ui_lightTextWhite, 140);
                    break;

                    case 2:  // Light
                    // Place the light card at the center and temperature to its left.
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, -140);
                    lv_obj_set_x(ui_tempTextBlue, -140);
                    lv_obj_set_x(ui_tempWhite, -140);
                    lv_obj_set_x(ui_tempTextWhite, -140);

                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 0);
                    lv_obj_set_x(ui_lightTextBlue, 0);
                    lv_obj_set_x(ui_lightWhite, 0);
                    lv_obj_set_x(ui_lightTextWhite, 0);
                    break;
                }
              } 
              else if (deltaX < -SWIPE_THRESHOLD) {
                trackingSwipe = false;
                swipeHandled = true;
                // A completed swipe must never activate the pressed menu card.
                lv_indev_wait_release(indev);
                lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                // Left swipe reveals the item on the right.
                screen1_index = screen1_index + 1;
                if(screen1_index > 2)
                {
                    screen1_index = 2;
                }
                switch (screen1_index) {
                    case 0:  // Volume
                    // Place the volume card at the center and temperature to its right.
                    lv_obj_remove_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, 0);
                    lv_obj_set_x(ui_volumeTextBlue, 0);
                    lv_obj_set_x(ui_volumeWhite, 0);
                    lv_obj_set_x(ui_volumeTextWhite, 0);

                    lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 140);
                    lv_obj_set_x(ui_tempTextBlue, 140);
                    lv_obj_set_x(ui_tempWhite, 140);
                    lv_obj_set_x(ui_tempTextWhite, 140);

                    // The light card lies outside the visible three-card arrangement.
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    break;

                    case 1:  // Temperature
                    // Place temperature at the center with its adjacent cards on each side.
                    lv_obj_remove_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, -140);
                    lv_obj_set_x(ui_volumeTextBlue, -140);
                    lv_obj_set_x(ui_volumeWhite, -140);
                    lv_obj_set_x(ui_volumeTextWhite, -140);

                    lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 0);
                    lv_obj_set_x(ui_tempTextBlue, 0);
                    lv_obj_set_x(ui_tempWhite, 0);
                    lv_obj_set_x(ui_tempTextWhite, 0);

                    lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 140);
                    lv_obj_set_x(ui_lightTextBlue, 140);
                    lv_obj_set_x(ui_lightWhite, 140);
                    lv_obj_set_x(ui_lightTextWhite, 140);
                    break;

                    case 2:  // Light
                    // Place the light card at the center and temperature to its left.
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, -140);
                    lv_obj_set_x(ui_tempTextBlue, -140);
                    lv_obj_set_x(ui_tempWhite, -140);
                    lv_obj_set_x(ui_tempTextWhite, -140);

                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_remove_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 0);
                    lv_obj_set_x(ui_lightTextBlue, 0);
                    lv_obj_set_x(ui_lightWhite, 0);
                    lv_obj_set_x(ui_lightTextWhite, 0);
                    break;
                }
              }
            }
          }
      }
    }
    else
    {
      data->state = LV_INDEV_STATE_REL;
      first_click = false;
      trackingSwipe = false;
      swipeHandled = false;
    }
}

/**
 * @brief Configure the display backlight PWM output.
 *
 * Starts the LEDC output at approximately 80 percent brightness so the
 * interface is visible before the user adjusts the brightness arc.
 *
 * @param none.
 * @return void.
 * @note Called once from setup() after the generated UI is initialized.
 */
void initBacklight() {
  ledcAttach(SCREEN_BACKLIGHT_PIN, pwmFreq, pwmResolution);
  ledcWrite(SCREEN_BACKLIGHT_PIN, 204);
}

// The onboard LED follows the local volume setting as a simple status light.
#define BREATH_LED_PIN 43

/*---------------------------------------------------------------
 * UI event callbacks and application state
 * Bind each arc to the label or hardware output it controls.
 *--------------------------------------------------------------*/

void volumeArcEventCb(lv_event_t *e);
void tempArcEventCb(lv_event_t *e);
void lightArcEventCb(lv_event_t *e);

// Stores the current local volume value for the onboard indicator.
int volumeValue = 50;

/*---------------------------------------------------------------
 * System initialization
 * Initialize the I/O expander, display, touch input, LVGL, UI,
 * encoder queue, and local PWM outputs in dependency order.
 *--------------------------------------------------------------*/

/**
 * @brief Initialize hardware, LVGL, and the generated user interface.
 *
 * Resets the panel and touch controller, allocates PSRAM draw buffers,
 * registers input and arc callbacks, and starts encoder sampling.
 *
 * @param none.
 * @return void.
 * @note Called once by the Arduino runtime before loop().
 */
void setup() {
  Serial.begin(115200); /* prepare for possible serial debug */
  delay(100);
  Serial.printf("[BOOT] ESP32 Arduino %s, LVGL %d.%d.%d, reset_reason=%d\n",
                ESP_ARDUINO_VERSION_STR, LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
                LVGL_VERSION_PATCH, (int)esp_reset_reason());
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  pcf8574.pinMode(P0, OUTPUT);        //tp RST
  pcf8574.pinMode(P2, OUTPUT);        //tp INT
  pcf8574.pinMode(P3, OUTPUT);        //lcd power
  pcf8574.pinMode(P4, OUTPUT);        //lcd reset
  pcf8574.pinMode(P5, INPUT_PULLUP);  //encoder SW

  Serial.print("Init pcf8574...\n");
  if (pcf8574.begin()) {
    Serial.println("pcf8574 OK");
  } else {
    Serial.println("pcf8574 KO");
  }

  pcf8574.digitalWrite(P3, HIGH);
  delay(100);

  /*lcd reset*/
  pcf8574.digitalWrite(P4, HIGH);
  delay(100);
  pcf8574.digitalWrite(P4, LOW);
  delay(120);
  pcf8574.digitalWrite(P4, HIGH);
  delay(120);
  /*end*/

  /*tp RST*/
  pcf8574.digitalWrite(P0, HIGH);
  delay(100);
  pcf8574.digitalWrite(P0, LOW);
  delay(120);
  pcf8574.digitalWrite(P0, HIGH);
  delay(120);
  /*tp INT*/
  pcf8574.digitalWrite(P2, HIGH);
  delay(120);

  gfx->begin();
  /* The new SquareLine LVGL 9 assets are native RGB565. Keep MADCTL's BGR
     bit set for this ST7701 panel so blue (for example 0x0294) stays blue. */
  panelInitBus->beginWrite();
  panelInitBus->writeCommand(0x36);
  panelInitBus->write(0x08);
  panelInitBus->endWrite();
  gfx->fillScreen(0x0000);

  if (!tsPanel.begin(&Wire, I2C_TOUCH_ADDR)) {
    Serial.println("No touchscreen found");
  } else {
    Serial.println("Touchscreen found");
  }

  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);

  lv_init();
  lv_tick_set_cb(millis);
  size_t buffer_size = sizeof(uint16_t) * screenWidth * screenHeight;
  buf1 = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf2 = (uint8_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  if (!buf1)
    Serial.println("Failed to allocate for LVGL---1---");
  if (!buf2)
    Serial.println("Failed to allocate for LVGL---2---");
  if (!buf1 || !buf2) {
    Serial.println("[FATAL] LVGL framebuffer allocation failed");
    while (true) delay(1000);
  }

  lv_display_t *display = lv_display_create(screenWidth, screenHeight);
  lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
  lv_display_set_flush_cb(display, my_disp_flush);
  /* Only copy invalidated regions. Full rendering copied all 480x480 pixels
     for every touch-state change and caused visible tearing on the RGB panel. */
  lv_display_set_buffers(display, buf1, buf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, my_touchpad_read);
  lv_indev_set_display(touch_indev, display);

  ui_init();

  /* SquareLine originally generated direct navigation handlers for the three
     highlighted cards (and used RELEASED for the temperature card). Replace
     them with one CLICKED-only handler that also verifies that the tapped card
     is still the centered selection. */
  lv_obj_remove_event_cb(ui_volumeBlue, ui_event_volumeBlue);
  lv_obj_remove_event_cb(ui_tempBlue, ui_event_tempBlue);
  lv_obj_remove_event_cb(ui_lightBlue, ui_event_lightBlue);
  lv_obj_add_event_cb(ui_volumeBlue, menuCardClickEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_tempBlue, menuCardClickEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_lightBlue, menuCardClickEventCb, LV_EVENT_CLICKED, NULL);

  // Replace SquareLine's 200 ms fade callbacks on all three return buttons.
  lv_obj_remove_event_cb(ui_screen2ReturnBt, ui_event_screen2ReturnBt);
  lv_obj_remove_event_cb(ui_screen3ReturnBt, ui_event_screen3ReturnBt);
  lv_obj_remove_event_cb(ui_screen4ReturnBt, ui_event_screen4ReturnBt);
  lv_obj_add_event_cb(ui_screen2ReturnBt, returnButtonClickEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_screen3ReturnBt, returnButtonClickEventCb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_event_cb(ui_screen4ReturnBt, returnButtonClickEventCb, LV_EVENT_CLICKED, NULL);

  // Connect each generated arc to its local value-update callback.
  lv_obj_add_event_cb(ui_VolumeArc, volumeArcEventCb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_TempArc, tempArcEventCb, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_add_event_cb(ui_lightArc, lightArcEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  delay(200);
  initBacklight();
  pcf8574.digitalWrite(P3, LOW);

  // Keep the onboard indicator at the initial volume level; no animation is used.
  ledcAttach(BREATH_LED_PIN, pwmFreq, pwmResolution);
  ledcWrite(BREATH_LED_PIN, (volumeValue * 255) / 100);

  encoderActionQueue = xQueueCreate(16, sizeof(EncoderAction));
  if (!encoderActionQueue) {
    Serial.println("[FATAL] Failed to create encoder queue");
  }
  xTaskCreatePinnedToCore(encTaskSafe, "ENC", 2048, NULL, 1, NULL, 0);

  Serial.println("Settings completed.");
}

/**
 * @brief Run the cooperative application loop.
 *
 * Delivers queued encoder actions, polls the encoder button, and lets LVGL
 * process touch, drawing, animations, and widget events.
 *
 * @param none.
 * @return void.
 * @note Called continuously by the Arduino runtime.
 */
void loop() {
  pollEncoderButton();
  EncoderAction action;
  while (encoderActionQueue && xQueueReceive(encoderActionQueue, &action, 0) == pdTRUE) {
    handleEncoderAction(action);
  }
  lv_timer_handler();

  delay(5);
}

/**
 * @brief Update the local volume value and its status indicator.
 *
 * Reads the arc value, formats the percentage label, and adjusts the
 * onboard LED PWM. No network or external-device command is sent.
 *
 * @param e LVGL value-change event for ui_VolumeArc.
 * @return void.
 * @note Called by LVGL after touch or encoder changes the volume arc.
 */
void volumeArcEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);
  int value = lv_arc_get_value(arc); // 0-100

  char volText[8];
  if (value == 100) {
    snprintf(volText, sizeof(volText), "%d%%", value);
    lv_label_set_text(ui_VolNum, volText);
  } else {
    snprintf(volText, sizeof(volText), " %d%%", value);
    lv_label_set_text(ui_VolNum, volText);
  }
  volumeValue = value;
  ledcWrite(BREATH_LED_PIN, (volumeValue * 255) / 100);
}


/**
 * @brief Update the local temperature label.
 *
 * Formats the selected value with the Celsius unit. The temperature is
 * intentionally local and does not control another product.
 *
 * @param e LVGL value-change event for ui_TempArc.
 * @return void.
 * @note Called by LVGL after touch or encoder changes the temperature arc.
 */
void tempArcEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *arc = (lv_obj_t *)lv_event_get_target(e);
  int value = lv_arc_get_value(arc); // 0-200

  char tempText[8];
  if (value >= 100 && value <= 200) {
    snprintf(tempText, sizeof(tempText), " %d°C", value);
    lv_label_set_text(ui_TempNum, tempText);
  } else {
    snprintf(tempText, sizeof(tempText), "  %d°C", value);
    lv_label_set_text(ui_TempNum, tempText);
  }

}

/**
 * @brief Update the screen-brightness label and PWM output.
 *
 * Keeps the arc value, percentage label, and physical backlight synchronized.
 * This is the only arc that controls a hardware output beyond the UI.
 *
 * @param e LVGL value-change event for ui_lightArc.
 * @return void.
 * @note Called by LVGL after touch or encoder changes the brightness arc.
 */
void lightArcEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *arc = lv_event_get_target_obj(e);
  int value = constrain(lv_arc_get_value(arc), 0, 100);

  char lightText[8];
  snprintf(lightText, sizeof(lightText), value == 100 ? "%d%%" : " %d%%", value);
  lv_label_set_text(ui_LightNum, lightText);
  ledcWrite(SCREEN_BACKLIGHT_PIN, (value * 255) / 100);
}


/*---------------------------------------------------------------
 * Encoder navigation and value control
 * Convert rotation and button gestures into screen navigation or
 * arc value changes while keeping all LVGL work on the main task.
 *--------------------------------------------------------------*/

/**
 * @brief Open the sub-screen selected on the main menu.
 *
 * Maps the current menu index to the volume, temperature, or brightness
 * screen and starts the generated fade transition.
 *
 * @param none.
 * @return void.
 * @note Called only after a validated touch click on the centered card.
 */
void performClickAction() {
  current_screen = lv_screen_active();
  if (current_screen == ui_Screen1) {
    if (screen1_index == 0) {
      loadScreenImmediately(&ui_Screen2, &ui_Screen2_screen_init);
    } else if (screen1_index == 1) {
      loadScreenImmediately(&ui_Screen3, &ui_Screen3_screen_init);
    } else if (screen1_index == 2) {
      loadScreenImmediately(&ui_Screen4, &ui_Screen4_screen_init);
    }
  }
}

/** Load a generated screen immediately, without a fade or transition frame. */
void loadScreenImmediately(lv_obj_t **target, void (*targetInit)(void)) {
  if (*target == NULL) targetInit();
  lv_screen_load(*target);
}

/**
 * @brief Open a menu card only when it is the current centered selection.
 *
 * A swipe is cancelled at the input-device level by my_touchpad_read(), so
 * this callback is reached only for a real tap. The target/index check also
 * prevents a stale event from a card that was moved during rapid selection.
 */
void menuCardClickEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_CLICKED || lv_screen_active() != ui_Screen1) return;

  lv_obj_t *target = lv_event_get_target_obj(e);
  const bool targetIsCenteredSelection =
      (screen1_index == 0 && target == ui_volumeBlue) ||
      (screen1_index == 1 && target == ui_tempBlue) ||
      (screen1_index == 2 && target == ui_lightBlue);
  if (targetIsCenteredSelection) performClickAction();
}

/** Return buttons share the same immediate, animation-free navigation. */
void returnButtonClickEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
    loadScreenImmediately(&ui_Screen1, &ui_Screen1_screen_init);
  }
}

/**
 * @brief Return from any sub-screen to the main menu.
 *
 * @param none.
 * @return void.
 * @note Called after a confirmed double encoder click.
 */
void performDoubleClickAction() {
  current_screen = lv_screen_active();
  if (current_screen == ui_Screen2 || current_screen == ui_Screen3 || current_screen == ui_Screen4) {
    loadScreenImmediately(&ui_Screen1, &ui_Screen1_screen_init);
  }
}

/**
 * @brief Move the main-menu selection by one position.
 *
 * Applies the queued rotation direction, clamps the selection to the three
 * available cards, and refreshes their highlighted/visible state.
 *
 * @param none.
 * @return void.
 * @note Called by handleEncoderAction() for rotations on the main menu.
 */
void processEncoder() {
  current_screen = lv_screen_active();
  if (current_screen == ui_Screen1) {
    if (position_tmp == 1) {  
      if (screen1_index < 2) {
        screen1_index++;
      }
    } else if (position_tmp == 0) { 
      if (screen1_index > 0) {
        screen1_index--;
      }
    }
    updateScreen(screen1_index);
    position_tmp = -1; 
  }
}

/**
 * @brief Execute one queued encoder action on the LVGL task.
 *
 * Rotation moves the menu selection or adjusts an active sub-screen arc.
 * Encoder single-clicks never open a menu card; entry is touch-only.
 *
 * @param type Rotation, single-click, or double-click action to execute.
 * @return void.
 * @note Called from loop() after an action is removed from the queue.
 */
void handleEncoderAction(const EncoderAction &action) {
  const EncoderActionType type = action.type;
  if (type == ENCODER_CLICK) {
    Serial.println("[ENC] click ignored; menu entry is touch-only");
    return;
  }
  if (type == ENCODER_DOUBLE_CLICK) {
    if (action.rotationSequence != encoderRotationSequence) {
      Serial.println("[ENC] stale double click cancelled by rotation");
      return;
    }
    Serial.println("[ENC] double click");
    performDoubleClickAction();
    return;
  }

  const int delta = (type == ENCODER_ROTATE_CW) ? 5 : -5;
  current_screen = lv_screen_active();
  if (current_screen == ui_Screen2 && ui_VolumeArc) {
    const int value = constrain(lv_arc_get_value(ui_VolumeArc) + delta, 0, 100);
    lv_arc_set_value(ui_VolumeArc, value);
    lv_obj_send_event(ui_VolumeArc, LV_EVENT_VALUE_CHANGED, NULL);
  } else if (current_screen == ui_Screen3 && ui_TempArc) {
    const int value = constrain(lv_arc_get_value(ui_TempArc) + delta, 0, 200);
    lv_arc_set_value(ui_TempArc, value);
    lv_obj_send_event(ui_TempArc, LV_EVENT_VALUE_CHANGED, NULL);
  } else if (current_screen == ui_Screen4 && ui_lightArc) {
    const int value = constrain(lv_arc_get_value(ui_lightArc) + delta, 0, 100);
    lv_arc_set_value(ui_lightArc, value);
    lv_obj_send_event(ui_lightArc, LV_EVENT_VALUE_CHANGED, NULL);
  } else if (current_screen == ui_Screen1) {
    position_tmp = (type == ENCODER_ROTATE_CW) ? 1 : 0;
    processEncoder();
  }
}

/**
 * @brief Sample the encoder rotation pins in a background task.
 *
 * Detects rising edges, determines direction from the second phase, and
 * queues a compact action without calling LVGL from the background core.
 *
 * @param pvParameters Unused FreeRTOS task parameter.
 * @return void; this task runs indefinitely.
 * @note Started once from setup() and scheduled by FreeRTOS.
 */
void encTaskSafe(void *pvParameters) {
  int previousA = digitalRead(ENCODER_A_PIN);
  while (true) {
    const int currentAState = digitalRead(ENCODER_A_PIN);
    if (currentAState != previousA && currentAState == HIGH && encoderActionQueue) {
      encoderRotationSequence++;
      EncoderAction action;
      action.type = (digitalRead(ENCODER_B_PIN) != currentAState)
                      ? ENCODER_ROTATE_CCW : ENCODER_ROTATE_CW;
      action.rotationSequence = encoderRotationSequence;
      xQueueSend(encoderActionQueue, &action, 0);
    }
    previousA = currentAState;
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

/**
 * @brief Poll and debounce the encoder push button.
 *
 * Accepts only a debounced press followed by a debounced release. Any encoder
 * rotation during that gesture or its double-click decision window cancels
 * the pending click, preventing rotary contact noise from opening a screen.
 *
 * @param none.
 * @return void.
 * @note Called on every pass through loop().
 */
void pollEncoderButton() {
  static uint8_t rawState = HIGH;
  static uint8_t stableState = HIGH;
  static unsigned long rawChangedMs = 0;
  static unsigned long pressedMs = 0;
  static uint32_t pressRotationSequence = 0;
  static uint32_t pendingClickRotationSequence = 0;
  const uint8_t sampledState = pcf8574.digitalRead(P5, true);
  const unsigned long now = millis();

  if (sampledState != rawState) {
    rawState = sampledState;
    rawChangedMs = now;
  }

  if (rawState != stableState && now - rawChangedMs >= debounceTime) {
    stableState = rawState;
    if (stableState == LOW) {
      pressedMs = now;
      pressRotationSequence = encoderRotationSequence;
    } else {
      const bool completedWithoutRotation =
          pressedMs != 0 && encoderRotationSequence == pressRotationSequence;
      pressedMs = 0;
      if (completedWithoutRotation) {
        if (pressCount == 0) pendingClickRotationSequence = encoderRotationSequence;
        pressCount++;
        singleClickTimeout = now + doubleClickTime;
      } else {
        pressCount = 0;
      }
    }
  }

  // Rotation is selection only; it invalidates every pending confirmation.
  if (pressCount > 0 && encoderRotationSequence != pendingClickRotationSequence) {
    pressCount = 0;
  }

  EncoderAction action;
  bool ready = false;
  if (pressCount >= 2) {
    pressCount = 0;
    action.type = ENCODER_DOUBLE_CLICK;
    action.rotationSequence = pendingClickRotationSequence;
    ready = true;
  } else if (pressCount == 1 && (long)(now - singleClickTimeout) >= 0) {
    pressCount = 0;
    action.type = ENCODER_CLICK;
    action.rotationSequence = pendingClickRotationSequence;
    ready = true;
  }
  if (ready && encoderActionQueue) xQueueSend(encoderActionQueue, &action, 0);
}


/*---------------------------------------------------------------
 * Main-menu card layout
 * Position and highlight the three generated menu cards according
 * to the selected index.
 *--------------------------------------------------------------*/

/**
 * @brief Refresh the visible main-menu card arrangement.
 *
 * Clamps the requested index, hides stale variants, and positions the
 * selected card at the center with its available neighbors beside it.
 *
 * @param index Selected card: 0 volume, 1 temperature, or 2 brightness.
 * @return void.
 * @note Called after encoder-based main-menu navigation.
 */
void updateScreen(int index) {
  if (index < 0) {
    index = 0;
  } else if (index > 2) {
    index = 2;
  }
  lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);

  switch (index) {
    case 0:  // Volume
      // Center volume, place temperature to the right, and hide brightness.
      lv_obj_remove_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_volumeBlue, 0);
      lv_obj_set_x(ui_volumeTextBlue, 0);
      lv_obj_set_x(ui_volumeWhite, 0);
      lv_obj_set_x(ui_volumeTextWhite, 0);

      lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, 140);
      lv_obj_set_x(ui_tempTextBlue, 140);
      lv_obj_set_x(ui_tempWhite, 140);
      lv_obj_set_x(ui_tempTextWhite, 140);

      lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      break;

    case 1:  // Temperature
      // Center temperature and place volume/light on the left/right.
      lv_obj_remove_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_volumeBlue, -140);
      lv_obj_set_x(ui_volumeTextBlue, -140);
      lv_obj_set_x(ui_volumeWhite, -140);
      lv_obj_set_x(ui_volumeTextWhite, -140);

      lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, 0);
      lv_obj_set_x(ui_tempTextBlue, 0);
      lv_obj_set_x(ui_tempWhite, 0);
      lv_obj_set_x(ui_tempTextWhite, 0);

      lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_lightBlue, 140);
      lv_obj_set_x(ui_lightTextBlue, 140);
      lv_obj_set_x(ui_lightWhite, 140);
      lv_obj_set_x(ui_lightTextWhite, 140);
      break;

    case 2:  // Light
      // Center brightness, place temperature to the left, and hide volume.
      lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

      lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, -140);
      lv_obj_set_x(ui_tempTextBlue, -140);
      lv_obj_set_x(ui_tempWhite, -140);
      lv_obj_set_x(ui_tempTextWhite, -140);

      lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_remove_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_lightBlue, 0);
      lv_obj_set_x(ui_lightTextBlue, 0);
      lv_obj_set_x(ui_lightWhite, 0);
      lv_obj_set_x(ui_lightTextWhite, 0);
      break;
  }
}
