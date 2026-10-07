#include "USB.h"
#include "USBHIDMouse.h"
#include "Board21.h"

/*---------------------------------------------------------------
 * USB HID mouse state
 * lastX and lastY form the origin for relative mouse movement.
 * tracking distinguishes the first touch sample from a drag.
 *--------------------------------------------------------------*/
USBHIDMouse mouse;
uint16_t lastX;
uint16_t lastY;
bool tracking;
static uint32_t lastSerialReport;

/**
 * @brief Initialize the board and enumerate the USB HID mouse.
 * @param None.
 * @return None.
 * @call Once after reset or power-up.
 */
void setup() {
  Serial.begin(115200);
  boardBegin(false);
  mouse.begin();
  USB.begin();
  delay(1000);
  Serial.println("[USB HID] Mouse ready");
}

/**
 * @brief Convert touch movement into mouse reports and button state.
 * @param None.
 * @return None.
 * @call Continuously by the Arduino runtime.
 */
void loop() {
  uint16_t x;
  uint16_t y;

  if (boardTouchRead(x, y)) {
    // The first sample establishes an origin and prevents a cursor jump.
    if (!tracking) {
      lastX = x;
      lastY = y;
      tracking = true;
      Serial.printf("[TOUCH] start x=%u y=%u\n", x, y);
    }

    // HID mouse movement is relative and limited to a signed 8-bit step.
    int dx = constrain((int)x - lastX, -127, 127);
    int dy = constrain((int)y - lastY, -127, 127);
    if (dx || dy) {
      mouse.move(dx, dy);
      if (millis() - lastSerialReport >= 100) {
        Serial.printf("[MOUSE] touch=(%u,%u) move=(%d,%d)\n", x, y, dx, dy);
        lastSerialReport = millis();
      }
    }

    // Keep the left button pressed for the duration of the touch.
    if (!mouse.isPressed(MOUSE_LEFT)) {
      mouse.press(MOUSE_LEFT);
      Serial.println("[MOUSE] left button pressed");
    }
    lastX = x;
    lastY = y;
  } else {
    // Release the button and force the next touch to establish a new origin.
    if (mouse.isPressed(MOUSE_LEFT)) {
      mouse.release(MOUSE_LEFT);
      Serial.println("[MOUSE] left button released");
    }
    if (tracking) {
      Serial.println("[TOUCH] released");
    }
    tracking = false;
  }

  delay(8);
}
