#include <lvgl.h>
#include <demos/lv_demos.h>
#include <Arduino_GFX_Library.h>
#include "WiFiMulti.h"
#include <WiFi.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include "BLEDevice.h"  
#include "BLEServer.h"  
#include "BLEUtils.h"  
// #include <CSE_CST328.h>
#include <Adafruit_CST8XX.h>
#include "ui.h"
#include "PCF8574.h" 

#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39
PCF8574 pcf8574(0x21);

String str_uart; 
String wifiId = "yanfa1";            //WiFi SSID to coonnect to
String wifiPwd = "1223334444yanfa";  //PASSWORD of the WiFi SSID to coonnect to

#define ENCODER_A_PIN 42    
#define ENCODER_B_PIN 4     
volatile uint8_t currentA = 0; 
volatile uint8_t lastA = 0;   
volatile uint8_t currentSW = 0; 
volatile uint8_t swPin = 0;      
bool pressedFlag = false;      
volatile int pressCount = 0;            
const unsigned long debounceTime = 50;    
const unsigned long doubleClickTime = 300;  
volatile unsigned long singleClickTimeout = 0; 
volatile int8_t position_tmp = 2;      

#define SCREEN_BACKLIGHT_PIN 6
const int pwmFreq = 5000;
const int pwmChannel = 0;
const int pwmResolution = 8;

#define BLE_SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define BLE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
BLEServer *pBLEserver = NULL;
BLEService *pBLEservice = NULL;
BLECharacteristic *pBLEcharacteristic = NULL;
BLEAdvertising *pBLEadvertising = NULL;
bool isBLEon = false;
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    Serial.println("BLE Client connected");
  };

  void onDisconnect(BLEServer *pServer) {
    Serial.println("BLE Client disconnected - restarting advertising");
    pServer->startAdvertising();
  }
};
MyServerCallbacks *pServerCallbacks = NULL;

#define OLED_RESET -1
#define SCREEN_WIDTH 128     // OLED display width, in pixels
#define SCREEN_HEIGHT 64     // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C  ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool isQT = false;
bool isFinal = false;
TaskHandle_t lcdTestTaskHandle = NULL;
TaskHandle_t encTaskHandle = NULL;
TaskHandle_t swTaskHandle = NULL;

#define I2C_TOUCH_ADDR 0x15  // often but not always 0x15!
Adafruit_CST8XX tsPanel = Adafruit_CST8XX();
enum Events lastevent = NONE;
bool isTestingTouch = false;

static const uint16_t screenWidth = 480;
static const uint16_t screenHeight = 480;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static uint16_t *buf3 = NULL;
static uint16_t *buf4 = NULL;

lv_obj_t *current_screen = NULL;
int screen1_index = 1;

Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  16 /* CS */, 2 /* SCK */, 1 /* SDA */,
  40 /* DE */, 7 /* VSYNC */, 15 /* HSYNC */, 41 /* PCLK */,
  46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
  14 /* G0/P22 */, 13 /* G1/P23 */, 12 /* G2/P24 */, 11 /* G3/P25 */, 10 /* G4/P26 */, 9 /* G5 */,
  5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */
);

Arduino_ST7701_RGBPanel *gfx = new Arduino_ST7701_RGBPanel(
  bus, GFX_NOT_DEFINED /* RST */, 0 /* rotation */,
  false /* IPS */, 480 /* width */, 480 /* height */,
  st7701_type5_init_operations, sizeof(st7701_type5_init_operations),
  true /* BGR */,
  10 /* hsync_front_porch(10) */, 4 /* hsync_pulse_width(8) */, 20 /* hsync_back_porch(50) */,
  10 /* vsync_front_porch(10) */, 4 /* vsync_pulse_width(8) */, 20 /* vsync_back_porch(20) */);

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
#if (LV_COLOR_16_SWAP != 0)
  gfx->draw16bitBeRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#else
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
#endif
  lv_disp_flush_ready(disp);
}

/*Read CST826 the touchpad*/
const int SWIPE_THRESHOLD = 100;  
const int TIME_THRESHOLD = 300;   
const int VERTICAL_LIMIT = 100;    
int startX = 0;
int startY = 0;
unsigned long startTime = 0;
bool trackingSwipe = false;
bool first_click = false;
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  // if (tsPanel.touched()) {
  //   data->state = LV_INDEV_STATE_PR;
  //   CST_TS_Point p = tsPanel.getPoint(0);
  //   data->point.x = p.x;
  //   data->point.y = p.y - 5;

  //   if (p.event != lastevent || p.event == TOUCHING) {
  //     Serial.printf("Touch ID #%d (%d, %d) Event: %s", p.id, p.x, p.y, events_name[p.event]);
  //     Serial.println();
  //     lastevent = p.event;
  //   }
  // } else {
  //   data->state = LV_INDEV_STATE_REL;
  //   lastevent = NONE;
  // }
    if (tsPanel.touched()) {
      CST_TS_Point p = tsPanel.getPoint(0);
      data->point.x = p.x;
      data->point.y = p.y - 20;
      data->state = LV_INDEV_STATE_PR;
      // if (p.event == PRESS) {
      lv_obj_t * current_screen = lv_scr_act();
      if (current_screen == ui_Screen1) {
          if (first_click == false) {
            startX = p.x;
            startY = p.y;
            startTime = millis();
            trackingSwipe = true;
            // Serial.printf("Touch START @ (%d, %d)\n", startX, startY);
            first_click = true;
          }
          if (trackingSwipe && p.event == TOUCHING) {
            int deltaX = p.x - startX;
            int deltaY = abs(p.y - startY);
            unsigned long elapsed = millis() - startTime;
            // first_click = false;
            if (elapsed < TIME_THRESHOLD && deltaY < VERTICAL_LIMIT) {
              if (deltaX > SWIPE_THRESHOLD) {
                // Serial.println(">> RIGHT SWIPE DETECTED <<");
                first_click = false;
                trackingSwipe = false;
                lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                screen1_index = screen1_index + 1;
                if(screen1_index > 2)
                {
                    screen1_index = 2;
                }
                switch (screen1_index) {
                    case 0:  // Volume
                    // volume
                    lv_obj_clear_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, 0);
                    lv_obj_set_x(ui_volumeTextBlue, 0);
                    lv_obj_set_x(ui_volumeWhite, 0);
                    lv_obj_set_x(ui_volumeTextWhite, 0);

                    // temp
                    lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 140);
                    lv_obj_set_x(ui_tempTextBlue, 140);
                    lv_obj_set_x(ui_tempWhite, 140);
                    lv_obj_set_x(ui_tempTextWhite, 140);

                    // light
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    break;

                    case 1:  // Temperature
                    // volume
                    lv_obj_clear_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, -140);
                    lv_obj_set_x(ui_volumeTextBlue, -140);
                    lv_obj_set_x(ui_volumeWhite, -140);
                    lv_obj_set_x(ui_volumeTextWhite, -140);

                    //temp
                    lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 0);
                    lv_obj_set_x(ui_tempTextBlue, 0);
                    lv_obj_set_x(ui_tempWhite, 0);
                    lv_obj_set_x(ui_tempTextWhite, 0);

                    //light
                    lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 140);
                    lv_obj_set_x(ui_lightTextBlue, 140);
                    lv_obj_set_x(ui_lightWhite, 140);
                    lv_obj_set_x(ui_lightTextWhite, 140);
                    break;

                    case 2:  // Light
                    // volume
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

                    // temp
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, -140);
                    lv_obj_set_x(ui_tempTextBlue, -140);
                    lv_obj_set_x(ui_tempWhite, -140);
                    lv_obj_set_x(ui_tempTextWhite, -140);

                    // light
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 0);
                    lv_obj_set_x(ui_lightTextBlue, 0);
                    lv_obj_set_x(ui_lightWhite, 0);
                    lv_obj_set_x(ui_lightTextWhite, 0);
                    break;
                }
              } 
              else if (deltaX < -SWIPE_THRESHOLD) {
                // Serial.println("<< LEFT SWIPE DETECTED >>");
                first_click = false;
                trackingSwipe = false;
                lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                screen1_index = screen1_index - 1;
                if(screen1_index < 0)
                {
                    screen1_index = 0;
                }
                switch (screen1_index) {
                    case 0:  // Volume
                    // volume
                    lv_obj_clear_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, 0);
                    lv_obj_set_x(ui_volumeTextBlue, 0);
                    lv_obj_set_x(ui_volumeWhite, 0);
                    lv_obj_set_x(ui_volumeTextWhite, 0);

                    // temp
                    lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 140);
                    lv_obj_set_x(ui_tempTextBlue, 140);
                    lv_obj_set_x(ui_tempWhite, 140);
                    lv_obj_set_x(ui_tempTextWhite, 140);

                    // light
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    break;

                    case 1:  // Temperature
                    // volume
                    lv_obj_clear_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_volumeBlue, -140);
                    lv_obj_set_x(ui_volumeTextBlue, -140);
                    lv_obj_set_x(ui_volumeWhite, -140);
                    lv_obj_set_x(ui_volumeTextWhite, -140);

                    //temp
                    lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, 0);
                    lv_obj_set_x(ui_tempTextBlue, 0);
                    lv_obj_set_x(ui_tempWhite, 0);
                    lv_obj_set_x(ui_tempTextWhite, 0);

                    //light
                    lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_lightBlue, 140);
                    lv_obj_set_x(ui_lightTextBlue, 140);
                    lv_obj_set_x(ui_lightWhite, 140);
                    lv_obj_set_x(ui_lightTextWhite, 140);
                    break;

                    case 2:  // Light
                    // volume
                    lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

                    // temp
                    lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_set_x(ui_tempBlue, -140);
                    lv_obj_set_x(ui_tempTextBlue, -140);
                    lv_obj_set_x(ui_tempWhite, -140);
                    lv_obj_set_x(ui_tempTextWhite, -140);

                    // light
                    lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
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
      if (p.event != lastevent || p.event == TOUCHING) {
        Serial.printf("Touch ID #%d (%d, %d) Event: %s\n", 
                    p.id, p.x, p.y, events_name[p.event]);
        lastevent = p.event;
      }
    }
    else
    {
      data->state = LV_INDEV_STATE_REL;
      first_click = false;
    }
}
void processStrFromUart() {
  if (Serial.available()) { 
    str_uart = Serial.readStringUntil('\n');
    str_uart.trim();
    // Serial.printf("Serial(uart0):%s\n", str_uart.c_str());
    if (str_uart == "Q") {
      if (isQT == false) {
        Serial.println("test on");
        gfx->setCursor(200, 220);
        gfx->fillScreen(BLACK);
        gfx->print("QT test");
        isQT = true;
      }
    }
    else if (str_uart == "r") {
      gfx->fillScreen(RED);
    } else if (str_uart == "5") {
      gfx->fillScreen(GREEN);
    } else if (str_uart == "6") {
      gfx->fillScreen(BLUE);
    } else if (str_uart == "7") {
      gfx->fillScreen(gfx->color565(255, 255, 255));
    } else if (str_uart == "8") {
      gfx->fillScreen(gfx->color565(128, 128, 128));
    } else if (str_uart == "9") {
      gfx->fillScreen(BLACK);
    } else if (str_uart == "dG") {
      gfx->setTextColor(BLACK);
      gfx->fillScreen(gfx->color565(255, 255, 255));
      gfx->setCursor(200, 220);
      gfx->print("Backlight");
    } else if (str_uart == "dg") {
      gfx->setTextColor(WHITE);
      gfx->fillScreen(BLACK);
    } else if (str_uart == "0") {
      ledcWrite(pwmChannel, 0);
    } else if (str_uart == "1") {
      ledcWrite(pwmChannel, 63);
    } else if (str_uart == "2") {
      ledcWrite(pwmChannel, 126);
    } else if (str_uart == "3") {
      ledcWrite(pwmChannel, 189);
    } else if (str_uart == "4") {
      ledcWrite(pwmChannel, 255);
    } else if (str_uart.startsWith("wifiId:")) {
      int colonIndex = str_uart.indexOf(':');
      wifiId = str_uart.substring(colonIndex + 1);
      // static const char* wifiId = str_uart.substring(colonIndex + 1);
      Serial.printf("confirm wifiId:%s\n", wifiId.c_str());
    } else if (str_uart.startsWith("wifiPwd:")) {
      int colonIndex = str_uart.indexOf(':');
      wifiPwd = str_uart.substring(colonIndex + 1);
      Serial.printf("confirm wifiPwd:%s\n", wifiPwd.c_str());
    } else if (str_uart == "dW") {
      gfx->setCursor(200, 220);
      gfx->print("WiFi");
    } else if (str_uart == "W") {
      wifiConnect_sta();
    } else if (str_uart == "w") {
      wifiDisconnect_sta();
    } else if (str_uart == "I") {
      wifiInfo();
    } else if (str_uart == "dB") {
      gfx->setCursor(200, 220);
      gfx->print("BLE");
      bleON();
    } else if (str_uart == "b") {
      bleOFF();
    } else if (str_uart == "dT") {
      gfx->setCursor(200, 220);
      gfx->print("Touch");
    } else if (str_uart == "T") {
      isTestingTouch = true;
    } else if (str_uart == "t") {
      isTestingTouch = false;
    } else if (str_uart == "dO") {
      gfx->setCursor(200, 220);
      gfx->fillScreen(BLACK);
      gfx->print("IIC");
    } else if (str_uart == "O") {
      oled_test();
    } else if (str_uart == "o") {
      oled_stop();
    } else if (str_uart == "F") {
      vTaskSuspend(encTaskHandle);
      vTaskSuspend(swTaskHandle);
      isFinal = true;
      if (lcdTestTaskHandle != NULL) {
        vTaskResume(lcdTestTaskHandle);
      }
      oled_test();
      bleON();
      wifiConnect_sta();
    } else if (str_uart == "f") {
      vTaskResume(encTaskHandle);
      vTaskResume(swTaskHandle);
      isFinal = false;
      oled_stop();
      bleOFF();
      wifiDisconnect_sta();
    } else if (str_uart == "R") {
      isQT = false;
      ledcWrite(pwmChannel, 0);
      delay(200);
      ESP.restart();
    }
  }
}

void wifiConnect_sta() {
  if (WiFi.isConnected()) {
    Serial.println("WiFi already connected");
  } else {
    uint8_t i = 0;
    WiFi.begin(wifiId.c_str(), wifiPwd.c_str());

    while ((i < 20) && (!WiFi.isConnected())) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      Serial.print(".");
      i++;
    }
    if (i >= 20) {
      Serial.println("failed to connect wifi within 20s");
    } else {
      WiFi.setAutoReconnect(true);
      Serial.println("wifi connected");
    }
  }
}
void wifiDisconnect_sta() {
  uint8_t i = 0;
  WiFi.disconnect(true);
  WiFi.setAutoReconnect(false);
  while ((i < 5) && (WiFi.isConnected())) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    Serial.print(".");
    i++;
  }
  if (i >= 5) {
    Serial.println("failed to disconnect wifi");
  } else {
    WiFi.setAutoReconnect(false);
    Serial.println("wifi disconnected");
  }
}
void wifiInfo() {
  Serial.println("----------WiFi Info----------");
  Serial.printf("wifiId:%s\nwifiPwd:%s\n", wifiId.c_str(), wifiPwd.c_str());
  if (WiFi.isConnected()) {
    Serial.println("wifi status:connected");
    Serial.print("ssid:");
    Serial.println(WiFi.SSID());
    Serial.print("rssi:");
    Serial.println(WiFi.RSSI());
    Serial.print("localIP:");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Address: ");
    Serial.println(WiFi.macAddress());
  } else {
    Serial.println("wifi status:disconnected");
  }
  Serial.println("----------WiFi Info----------");
}

void bleInit() {
  BLEDevice::init("ESP32S3_2.1_BLE_Server");
  pBLEserver = BLEDevice::createServer();
  if (!pServerCallbacks) {
    pServerCallbacks = new MyServerCallbacks();
  }
  pBLEserver->setCallbacks(pServerCallbacks);

  pBLEservice = pBLEserver->createService(BLE_SERVICE_UUID);
  pBLEcharacteristic = pBLEservice->createCharacteristic(
    BLE_CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pBLEcharacteristic->setValue("Hello from BLE");

  pBLEadvertising = BLEDevice::getAdvertising();
  pBLEadvertising->addServiceUUID(BLE_SERVICE_UUID);
}
void bleON() {
  if (!isBLEon) {
    if (!pBLEservice || !pBLEadvertising) { bleInit(); }

    pBLEservice->start();
    pBLEadvertising->start();
    isBLEon = true;
    Serial.println("BLE started");
  } else {
    Serial.println("BLE already started");
  }
}
void bleOFF() {
  if (isBLEon) {
    pBLEservice->stop();
    pBLEadvertising->stop();
    isBLEon = false;
    Serial.println("BLE stopped.");
  } else {
    Serial.println("BLE already stopped");
  }
}

void testTouch() {
  CST_TS_Point p;
  static unsigned long lastPrintTime = 0;

  if (tsPanel.touched()) {
    p = tsPanel.getPoint(0);
    if (p.event != lastevent || p.event == TOUCHING) {
      unsigned long now = millis();
      if (now - lastPrintTime > 100) {  // 100ms
        Serial.printf("Touch ID #%d (%d, %d) Event: %s\n", p.id, p.x, p.y, events_name[p.event]);
        lastPrintTime = now;
        lastevent = p.event;
      }
    }
  } else {
    lastevent = NONE;  
  }
}

void testdrawstyles(void) {
  display.clearDisplay();

  display.setTextSize(2);               // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.setCursor(25, 25);            // Start at top-left corner
  display.println(F("ELECROW"));
  display.display();
  //  delay(2000);
}
void oled_test() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
  } else {
    Serial.println("OLED begins");
  }
  testdrawstyles();
}

void oled_stop() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);  // Draw white text
  display.display();
  Serial.println("OLED stoped");
}

void initBacklight() {
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(SCREEN_BACKLIGHT_PIN, pwmChannel);
  ledcWrite(pwmChannel, 204);
}

void setup() {
  Serial.begin(115200); /* prepare for possible serial debug */
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
  gfx->fillScreen(BLACK);
  gfx->setTextSize(2);
  gfx->setCursor(80, 100);

  if (!tsPanel.begin(&Wire, I2C_TOUCH_ADDR)) {
    Serial.println("No touchscreen found");
  } else {
    Serial.println("Touchscreen found");
  }

  pinMode(ENCODER_A_PIN, INPUT);
  pinMode(ENCODER_B_PIN, INPUT);
  lastA = digitalRead(ENCODER_A_PIN);

  lv_init();
  size_t buffer_size = sizeof(lv_color_t) * screenWidth * screenHeight;
  buf1 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  buf2 = (lv_color_t *)heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
  if (!buf1)
    Serial.println("Failed to allocate for LVGL---1---");
  if (!buf2)
    Serial.println("Failed to allocate for LVGL---2---");
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, screenWidth * screenHeight);

  /*Initialize the display*/
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  /*Change the following line to your display resolution*/
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  disp_drv.direct_mode = 0;
  disp_drv.full_refresh = 0; 
  disp_drv.sw_rotate = 0;
  disp_drv.screen_transp = 0;
  lv_disp_drv_register(&disp_drv);

  /*Initialize the (dummy) input device driver*/
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  /*Or try out a demo. Don't forget to enable the demos in lv_conf.h. E.g. LV_USE_DEMOS_WIDGETS*/
  // lv_demo_widgets();
  ui_init();

  delay(200);
  initBacklight();
  pcf8574.digitalWrite(P3, LOW);

  xTaskCreatePinnedToCore(lcdTestTask, "LCD Test", 2048, NULL, 1, &lcdTestTaskHandle, 0);
  if (lcdTestTaskHandle != NULL) vTaskSuspend(lcdTestTaskHandle);
  xTaskCreatePinnedToCore(encTask, "ENC", 2048, NULL, 1, &encTaskHandle, 0);
  xTaskCreatePinnedToCore(swTask, "SWITCH", 2048, NULL, 1, &swTaskHandle, 0);

  Serial.println("Setup done");
}

void loop() {
  if(isQT == false)
  {
      lv_timer_handler();
  }
  processStrFromUart();
  if (isTestingTouch) {
    testTouch();
  }
  vTaskDelay(pdMS_TO_TICKS(5));
}

void lcdTestTask(void *pvParameters) {
  while (1) {
    if (isFinal) {
      gfx->fillScreen(gfx->color565(255, 0, 0));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (isFinal) {
      gfx->fillScreen(gfx->color565(0, 255, 0));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (isFinal) {
      gfx->fillScreen(gfx->color565(0, 0, 255));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (isFinal) {
      gfx->fillScreen(gfx->color565(255, 255, 255));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (isFinal) {
      gfx->fillScreen(gfx->color565(128, 128, 128));
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
    if (!isFinal) {
      vTaskSuspend(NULL);
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

int last_counter = 0;
int counter = 0;
int currentStateCLK;
int lastStateCLK;
String currentDir = "";
bool one_test = false;

void performClickAction() {
  current_screen = lv_scr_act();
  if (current_screen == ui_Screen1) {
    if (screen1_index == 0) {
      _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen2_screen_init);
    } else if (screen1_index == 1) {
      _ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen3_screen_init);
    } else if (screen1_index == 2) {
      _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen4_screen_init);
    }
  }
}

void performDoubleClickAction() {
  current_screen = lv_scr_act();
  if (current_screen == ui_Screen2 || current_screen == ui_Screen3 || current_screen == ui_Screen4) {
    _ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen1_screen_init);
  }
}

void processEncoder() {
  current_screen = lv_scr_act();
  if (current_screen == ui_Screen1) {
    if (position_tmp == 1) {  
      if (screen1_index < 2) {
        screen1_index++;
      }
      Serial.printf("cur_index: %d\n", screen1_index);
    } else if (position_tmp == 0) { 
      if (screen1_index > 0) {
        screen1_index--;
      }
      Serial.printf("cur_index: %d\n", screen1_index);
    }
    updateScreen(screen1_index);
    position_tmp = -1; 
  }
}


void encTask(void *pvParameters) {
  while (1) {
    // Read the current state of CLK
    currentStateCLK = digitalRead(ENCODER_A_PIN);
    // If last and current state of CLK are different, then pulse occurred
    // React to only 1 state change to avoid double count
    if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {
      current_screen = lv_scr_act();
      // If the DT state is different than the CLK state then
      // the encoder is rotating CCW so decrement
      if (digitalRead(ENCODER_B_PIN) != currentStateCLK) {
        if (abs(last_counter - counter) > 10) {
          continue;
        }

        if (current_screen == ui_Screen2) {
          int currentVol = lv_arc_get_value(ui_VolumeArc);
          Serial.printf(" -- currentVol = %d\n", currentVol);
          int newVol = (currentVol - 5) < 0 ? 0 : currentVol - 5;
          Serial.printf(" -- END currentVol = %d\n", newVol);
          lv_arc_set_value(ui_VolumeArc, newVol);
          char volText[8];
          if (newVol == 100) {
            snprintf(volText, sizeof(volText), "%d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          } else {
            snprintf(volText, sizeof(volText), " %d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          }
        } else if (current_screen == ui_Screen3) {
          int currentTemp = lv_arc_get_value(ui_TempArc);
          Serial.printf(" -- currentVol = %d\n", currentTemp);
          int newTemp = (currentTemp - 5) < 0 ? 0 : currentTemp - 5;
          Serial.printf(" -- END currentVol = %d\n", newTemp);
          lv_arc_set_value(ui_TempArc, newTemp);
          char TempText[8];
          if (newTemp >= 100 && newTemp <= 200) {
            snprintf(TempText, sizeof(TempText), "%d%°C", newTemp);
            lv_label_set_text(ui_TempNum, TempText);
          } else {
            snprintf(TempText, sizeof(TempText), " %d%°C", newTemp);
            lv_label_set_text(ui_TempNum, TempText);
          }
        } else if (current_screen == ui_Screen4) {
          int currentLight = lv_arc_get_value(ui_lightArc);
          Serial.printf(" -- currentLight = %d\n", currentLight);
          int newLight = (currentLight - 5) < 0 ? 0 : currentLight - 5;
          Serial.printf(" -- END currentLight = %d\n", newLight);
          lv_arc_set_value(ui_lightArc, newLight);
          char LightText[8];
          if (newLight == 100) {
            snprintf(LightText, sizeof(LightText), "%d%%", newLight);
            lv_label_set_text(ui_LightNum, LightText);
          } else {
            snprintf(LightText, sizeof(LightText), " %d%%", newLight);
            lv_label_set_text(ui_LightNum, LightText);
          }
          int pwm_value = (newLight * 255) / 100;
          ledcSetup(pwmChannel, pwmFreq, pwmResolution);
          ledcAttachPin(SCREEN_BACKLIGHT_PIN, pwmChannel);
          ledcWrite(pwmChannel, pwm_value);
        }
        position_tmp = 0;

        counter++;
        currentDir = "CCW";
      } else {
        if (one_test == false)  
        {
          one_test = true;
          continue;
        }
        if (current_screen == ui_Screen2) {
          int currentVol = lv_arc_get_value(ui_VolumeArc);
          Serial.printf(" ++ currentVol = %d\n", currentVol);
          int newVol = (currentVol + 5) > 100 ? 100 : currentVol + 5;
          Serial.printf(" ++ END currentVol = %d\n", newVol);
          lv_arc_set_value(ui_VolumeArc, newVol);
          char volText[8];
          if (newVol == 100) {
            snprintf(volText, sizeof(volText), "%d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          } else {
            snprintf(volText, sizeof(volText), " %d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          }
        } else if (current_screen == ui_Screen3) {
          int currentTemp = lv_arc_get_value(ui_TempArc);
          Serial.printf(" ++ currentVol = %d\n", currentTemp);
          int newTemp = (currentTemp + 5) > 200 ? 200 : currentTemp + 5;
          Serial.printf(" ++ END currentVol = %d\n", newTemp);
          lv_arc_set_value(ui_TempArc, newTemp);
          char TempText[8];
          if (newTemp >= 100 && newTemp <= 200) {
            snprintf(TempText, sizeof(TempText), "%d%°C", newTemp);
            lv_label_set_text(ui_TempNum, TempText);
          } else {
            snprintf(TempText, sizeof(TempText), " %d%°C", newTemp);
            lv_label_set_text(ui_TempNum, TempText);
          }
        } else if (current_screen == ui_Screen4) {
          int currentLight = lv_arc_get_value(ui_lightArc);
          Serial.printf(" ++ currentLight = %d\n", currentLight);
          int newLight = (currentLight + 5) > 100 ? 100 : currentLight + 5;
          Serial.printf(" ++ END currentLight = %d\n", newLight);
          lv_arc_set_value(ui_lightArc, newLight);
          char LightText[8];
          if (newLight == 100) {
            snprintf(LightText, sizeof(LightText), "%d%%", newLight);
            lv_label_set_text(ui_LightNum, LightText);
          } else {
            snprintf(LightText, sizeof(LightText), " %d%%", newLight);
            lv_label_set_text(ui_LightNum, LightText);
          }
          int pwm_value = (newLight * 255) / 100;
          ledcSetup(pwmChannel, pwmFreq, pwmResolution);
          ledcAttachPin(SCREEN_BACKLIGHT_PIN, pwmChannel);
          ledcWrite(pwmChannel, pwm_value);
        }
        position_tmp = 1;
        counter--;
        currentDir = "CW";
      }

      Serial.print("Direction: ");
      Serial.print(currentDir);
      Serial.print(" | Counter: ");
      Serial.println(counter);
      last_counter = counter;
      processEncoder();
    }

    if (pressedFlag)
    {
      if (pressCount == 1 && millis() >= singleClickTimeout) {
        Serial.println("Single Click Detected");
        performClickAction();
        pressCount = 0;
        pressedFlag = false;

      }
      else if (pressCount >= 2) {
        Serial.println("Double Click Detected");
        performDoubleClickAction();
        pressCount = 0;
        pressedFlag = false;
      }
    }
    // Remember last CLK state
    lastStateCLK = currentStateCLK;
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void updateScreen(int index) {
  if (index < 0) {
    index = 0;
  } else if (index > 2) {
    index = 2;
  }
  Serial.printf("cur_index: %d\n", screen1_index);

  lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);

  switch (index) {
    case 0:  // Volume
      // volume
      lv_obj_clear_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_volumeBlue, 0);
      lv_obj_set_x(ui_volumeTextBlue, 0);
      lv_obj_set_x(ui_volumeWhite, 0);
      lv_obj_set_x(ui_volumeTextWhite, 0);

      // temp
      lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, 140);
      lv_obj_set_x(ui_tempTextBlue, 140);
      lv_obj_set_x(ui_tempWhite, 140);
      lv_obj_set_x(ui_tempTextWhite, 140);

      // light
      lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      break;

    case 1:  // Temperature
      // volume
      lv_obj_clear_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_volumeBlue, -140);
      lv_obj_set_x(ui_volumeTextBlue, -140);
      lv_obj_set_x(ui_volumeWhite, -140);
      lv_obj_set_x(ui_volumeTextWhite, -140);

      //temp
      lv_obj_add_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, 0);
      lv_obj_set_x(ui_tempTextBlue, 0);
      lv_obj_set_x(ui_tempWhite, 0);
      lv_obj_set_x(ui_tempTextWhite, 0);

      //light
      lv_obj_add_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_lightBlue, 140);
      lv_obj_set_x(ui_lightTextBlue, 140);
      lv_obj_set_x(ui_lightWhite, 140);
      lv_obj_set_x(ui_lightTextWhite, 140);
      break;

    case 2:  // Light
      // volume
      lv_obj_add_flag(ui_volumeWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_volumeTextBlue, LV_OBJ_FLAG_HIDDEN);

      // temp
      lv_obj_add_flag(ui_tempBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_tempTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_tempWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_tempTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_tempBlue, -140);
      lv_obj_set_x(ui_tempTextBlue, -140);
      lv_obj_set_x(ui_tempWhite, -140);
      lv_obj_set_x(ui_tempTextWhite, -140);

      // light
      lv_obj_add_flag(ui_lightWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(ui_lightTextWhite, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_lightBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(ui_lightTextBlue, LV_OBJ_FLAG_HIDDEN);
      lv_obj_set_x(ui_lightBlue, 0);
      lv_obj_set_x(ui_lightTextBlue, 0);
      lv_obj_set_x(ui_lightWhite, 0);
      lv_obj_set_x(ui_lightTextWhite, 0);
      break;
  }
}

void swTask(void *pvParameters) {
  while (1) {
    currentSW = pcf8574.digitalRead(P5, true);
    if (currentSW == LOW) {
      // Serial.printf("currentSW == LOW\n");
      static unsigned long lastInterruptTime = 0;
      unsigned long currentTime = millis();

      if (currentTime - lastInterruptTime > debounceTime) {
        pressCount++;
        pressedFlag = true;
        if (pressCount == 1) {
          singleClickTimeout = currentTime + doubleClickTime;
        }
      }
      lastInterruptTime = currentTime;
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

