#include <lvgl.h>
#include <demos/lv_demos.h>
#include <Arduino_GFX_Library.h>
#include "WiFiMulti.h"
#include <WiFi.h>
#include <WiFiUdp.h>

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

// #include <CSE_CST328.h>
#include <Adafruit_CST8XX.h>
#include "ui.h"
#include "PCF8574.h" 

#define I2C_SDA_PIN 38
#define I2C_SCL_PIN 39
PCF8574 pcf8574(0x21);

String wifiId = "elecrow888";            //WiFi SSID to coonnect to
String wifiPwd = "elecrow2014";  //PASSWORD of the WiFi SSID to coonnect to

#define ENCODER_A_PIN 42     // 蓝  逆时针，先上升
#define ENCODER_B_PIN 4      // 黄  顺时针，先上升   B   A

volatile uint8_t lastA = 0;   
volatile uint8_t currentSW = 0; 

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

TaskHandle_t encTaskHandle = NULL;
TaskHandle_t swTaskHandle = NULL;

#define I2C_TOUCH_ADDR 0x15  // often but not always 0x15!
Adafruit_CST8XX tsPanel = Adafruit_CST8XX();
enum Events lastevent = NONE;

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

// WiFi连接函数
void connectWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(wifiId.c_str(), wifiPwd.c_str());
  
  // 等待连接，最多10秒
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.print("SSID: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}


void initBacklight() {
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(SCREEN_BACKLIGHT_PIN, pwmChannel);
  ledcWrite(pwmChannel, 204);
}

// 旋钮屏板载LED（引脚43）用于呼吸灯
#define BREATH_LED_PIN 43
// 呼吸灯PWM通道（避免与背光通道冲突）
const int breathPwmChannel = 1;

// UDP通信相关变量
WiFiUDP udp;
const char* ADVANCE_IP = "192.168.50.160";  // Advance设备的IP地址，需要根据实际情况修改
const int ADVANCE_PORT = 8888;             // Advance设备的UDP端口
 
// 远程灯带（Advance端）配置（前移，供setup中初始化使用）
const int REMOTE_NUM_LEDS = 15;      // Advance 上灯带 LED 数量
int lastLedCountSent = -1;           // 记录上次发送到 Advance 的 LED 个数

// 提前声明：LVGL滑动事件回调
void volumeArcEventCb(lv_event_t *e);
// void volumeIconEventCb(lv_event_t *e);

void tempArcEventCb(lv_event_t *e);
// 仅通过PWM直接设置亮度，无动画

// 界面初始值（在进入界面时应用）
int volumeValue = 50;  // 0-100
int tempValue = 0;     // 0-200
int lightValue = 50;   // 0-100

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

  // 绑定音量滑条数值改变事件
  lv_obj_add_event_cb(ui_VolumeArc, volumeArcEventCb, LV_EVENT_VALUE_CHANGED, NULL);
  // 绑定温度滑条数值改变事件
  lv_obj_add_event_cb(ui_TempArc, tempArcEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  delay(200);
  initBacklight();
  pcf8574.digitalWrite(P3, LOW);

  // 初始化引脚43为LED PWM输出
  ledcSetup(breathPwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(BREATH_LED_PIN, breathPwmChannel);
  // 开机不进行呼吸，仅保持当前值对应亮度
  ledcWrite(breathPwmChannel, (volumeValue * 255) / 100);

  xTaskCreatePinnedToCore(encTask, "ENC", 2048, NULL, 1, &encTaskHandle, 0);
  xTaskCreatePinnedToCore(swTask, "SWITCH", 2048, NULL, 1, &swTaskHandle, 0);

  // 初始化远程灯带控制状态
  Serial.println("初始化远程灯带控制...");
  lastLedCountSent = -1;
  Serial.println("远程灯带控制初始化完成");

  Serial.println("Settings completed, ready to connect to WiFi...");
  connectWiFi();

  // 等待WiFi连接完成后，获取Advance设备的IP地址
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi连接成功，准备UDP通信...");
    Serial.print("本机IP地址: ");
    Serial.println(WiFi.localIP());
  }
}

void loop() {
  lv_timer_handler();

  delay(5);
}

// 发送灯带点亮数量命令到 Advance 设备
void sendLedStripCommand(int ledCount) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi未连接，无法发送灯带命令");
    return;
  }

  ledCount = constrain(ledCount, 0, REMOTE_NUM_LEDS);
  String command = "LEDCOUNT:" + String(ledCount);
  udp.beginPacket(ADVANCE_IP, ADVANCE_PORT);
  udp.write((uint8_t*)command.c_str(), command.length());
  bool sent = udp.endPacket();
  if (sent) {
    Serial.printf("灯带命令发送成功: %s\n", command.c_str());
  } else {
    Serial.println("灯带命令发送失败");
  }
}

// 根据温度值映射并控制 Advance 上的 WS2812 灯带
void updateLedStripByTemperature(int temp) {
  temp = constrain(temp, 0, 200);
  int ledCount = map(temp, 0, 200, 0, REMOTE_NUM_LEDS);
  if (ledCount == lastLedCountSent) return;
  lastLedCountSent = ledCount;
  sendLedStripCommand(ledCount);
  Serial.printf("温度: %d → 点亮LED数量: %d\n", temp, ledCount);
}

// 音量滑条事件：更新数值并通过UDP发送到Advance
void volumeArcEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *arc = lv_event_get_target(e);
  int value = lv_arc_get_value(arc); // 0-100

  // 更新UI文本，格式与旋钮逻辑保持一致
  char volText[8];
  if (value == 100) {
    snprintf(volText, sizeof(volText), "%d%%", value);
    lv_label_set_text(ui_VolNum, volText);
  } else {
    snprintf(volText, sizeof(volText), " %d%%", value);
    lv_label_set_text(ui_VolNum, volText);
  }
  // 直接设置LED亮度到对应占空比
  volumeValue = value;
  ledcWrite(breathPwmChannel, (volumeValue * 255) / 100);
}


// 温度滑条事件：更新温度文本并控制舵机
void tempArcEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  lv_obj_t *arc = lv_event_get_target(e);
  int value = lv_arc_get_value(arc); // 0-200

  // 更新温度文本（与现有格式一致）
  char tempText[8];
  if (value >= 100 && value <= 200) {
    snprintf(tempText, sizeof(tempText), " %d%°", value);
    lv_label_set_text(ui_TempNum, tempText);
  } else {
    snprintf(tempText, sizeof(tempText), "  %d%°", value);
    lv_label_set_text(ui_TempNum, tempText);
  }

  // 控制远端灯带（0-200 映射 0-15 个 LED）
  updateLedStripByTemperature(value);
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
        if (abs(last_counter - counter) > 200) {
          continue;
        }

        if (current_screen == ui_Screen2) {
          int currentVol = lv_arc_get_value(ui_VolumeArc);
          Serial.printf(" -- currentVol = %d\n", currentVol);
          int newVol = (currentVol - 5) < 0 ? 0 : currentVol - 5;
          Serial.printf(" -- END currentVol = %d\n", newVol);
          lv_arc_set_value(ui_VolumeArc, newVol);

          volumeValue = newVol; // 同步全局音量

          char volText[8];
          if (newVol == 100) {
            snprintf(volText, sizeof(volText), "%d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          } else {
            snprintf(volText, sizeof(volText), " %d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          }

          // 直接设置LED亮度
          ledcWrite(breathPwmChannel, (volumeValue * 255) / 100);

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

          // 更新远端灯带
          updateLedStripByTemperature(newTemp);

        } else if (current_screen == ui_Screen4) {
          int currentLight = lv_arc_get_value(ui_lightArc);
          Serial.printf(" -- currentLight = %d\n", currentLight);
          int newLight = (currentLight - 5) < 0 ? 0 : currentLight - 5;
          Serial.printf(" -- END currentLight = %d\n", newLight);
          lv_arc_set_value(ui_lightArc, newLight);

          lightValue = newLight; // 同步全局亮度

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
        currentDir = "CCW"; // 逆时针
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

          volumeValue = newVol; // 同步全局音量

          char volText[8];
          if (newVol == 100) {
            snprintf(volText, sizeof(volText), "%d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          } else {
            snprintf(volText, sizeof(volText), " %d%%", newVol);
            lv_label_set_text(ui_VolNum, volText);
          }

          // 直接设置LED亮度
          ledcWrite(breathPwmChannel, (volumeValue * 255) / 100);

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

          // 更新远端灯带
          updateLedStripByTemperature(newTemp);

        } else if (current_screen == ui_Screen4) {
          int currentLight = lv_arc_get_value(ui_lightArc);
          Serial.printf(" ++ currentLight = %d\n", currentLight);
          int newLight = (currentLight + 5) > 100 ? 100 : currentLight + 5;
          Serial.printf(" ++ END currentLight = %d\n", newLight);
          lv_arc_set_value(ui_lightArc, newLight);

          lightValue = newLight; // 同步全局亮度

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
    vTaskDelay(pdMS_TO_TICKS(2));
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

