// This example outputs the position and touch information to the serial montior
// for the 2.1" round display connected to the Adafruit Qualia ESP32-S3

#include <Adafruit_CST8XX.h>

// The CST8xx based CTP overlays uses hardware I2C (SCL/SDA)
#define I2C_TOUCH_ADDR 0x15  // often but not always 0x15!
Adafruit_CST8XX ctp = Adafruit_CST8XX(); // this library supports CST826!
bool touchOK = false;        // we will check if the touchscreen exists
enum Events lastevent = NONE;

void setup(void)
{
  Serial.begin(115200);
  //while (!Serial) delay(100);

  Serial.println("Beginning");

  if (!ctp.begin(&Wire, I2C_TOUCH_ADDR)) {
    Serial.println("No touchscreen found");
    touchOK = false;
  } else {
    Serial.println("Touchscreen found");
    touchOK = true;
  }
}

void loop()
{
  if (touchOK && ctp.touched()) {
    TS_Point p = ctp.getPoint(0);
    if (p.event != lastevent || p.event == TOUCHING) {
      Serial.printf("Touch ID #%d (%d, %d) Event: %s", p.id, p.x, p.y, events_name[p.event]);
      Serial.println();
      lastevent = p.event;
    }
  }
}