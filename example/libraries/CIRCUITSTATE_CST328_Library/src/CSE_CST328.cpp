
//============================================================================================//
/*
  Filename: CSE_CST328.cpp
  Description: Main source file for the CSE_CST328 Arduino library.
  Framework: Arduino, PlatformIO
  Author: Vishnu Mohanan (@vishnumaiea, @vizmohanan)
  Maintainer: CIRCUITSTATE Electronics (@circuitstate)
  Version: 0.1
  License: MIT
  Source: https://github.com/CIRCUITSTATE/CSE_CST328
  Last Modified: +05:30 21:04:21 PM 18-02-2025, Tuesday
 */
//============================================================================================//

#include "CSE_CST328.h"

//============================================================================================//
/**
 * @brief Constructor for a point with no arguments.
 * 
 * @return `TS_Point::` The object.
 */
TS_Point:: TS_Point (void) {
  x = 0;
  y = 0;
  z = 0;
  touchId = 0;
  state = 0;
}

//============================================================================================//
/**
 * @brief Constructor for a point with arguments for x, y, and z.
 * 
 * @param x X coordinate.
 * @param y Y coordinate.
 * @param z Z coordinate (often used for pressure).
 * @param id Touch ID.
 * @return `TS_Point::` The object.
 */
TS_Point:: TS_Point (int16_t x, int16_t y, int16_t z, uint8_t id) {
  this->x = x;
  this->y = y;
  this->z = z;
  this->touchId = id;
  state = 0;
}

//============================================================================================//
/**
 * @brief Compare two points for equality. The z coordinate is ignored.
 * 
 * @param p Point to compare.
 * @return `true` If equal.
 * @return `false` If not equal.
 */
bool TS_Point:: operator== (TS_Point p) {
  return ((p.x == x) && (p.y == y));
}

//============================================================================================//
/**
 * @brief Compare two points for inequality. The z coordinate is ignored.
 * 
 * @param p Point to compare.
 * @return `true` If not equal.
 * @return `false` If equal.
 */
bool TS_Point:: operator!= (TS_Point p) {
  return ((p.x != x) || (p.y != y));
}

//============================================================================================//
/**
 * @brief Constructor for CSE_CST328 touch controller.
 * 
 * @param width Width of the touch screen.
 * @param height Height of the touch screen.
 * @param i2c Pointer to I2C bus.
 * @param pinRst Reset pin (default -1 for no hardware reset).
 * @param pinIrq Interrupt pin (default -1 for no hardware interrupt).
 * @return `CSE_CST328::` The object.
 */
CSE_CST328:: CSE_CST328 (uint16_t width, uint16_t height, TwoWire *i2c, int8_t pinRst, int8_t pinIrq) {
  wireInstance = i2c;
  pinReset = pinRst;
  pinInterrupt = pinIrq;
  
  // Store the default width and height.
  // This will be later used when performing rotations.
  defWidth = width;
  defHeight = height;
  
  // Set the current width and height (will be adjusted for rotation).
  this->width = width;
  this->height = height;
  
  // Initialize the touch points.
  for (int i = 0; i < 5; i++) {
    touchPoints [i] = TS_Point (0, 0, 0, i);
  }
  
  // Initialize other variables.
  rotation = 0;
  inited = false;
}

//============================================================================================//
/**
 * @brief Initialize the CST328.
 * 
 * @return `true` If the CST328 was initialized successfully.
 * @return `false` If the CST328 failed to initialize.
 */
bool CSE_CST328:: begin() {
  if (inited) {
    return true;
  }
  
  // Initialize I2C if not already done.
  wireInstance->begin();
  
  // If reset pin is defined, toggle it to reset the controller.
  if (pinReset != -1) {
    pinMode (pinReset, OUTPUT);
    digitalWrite (pinReset, HIGH);
    delay (10);
    digitalWrite (pinReset, LOW);
    delay (10);
    digitalWrite (pinReset, HIGH);
    delay (100); // Wait for chip to initialize (TRON = 200ms from datasheet)
  }

  if (pinInterrupt != -1) {
    pinMode (pinInterrupt, INPUT_PULLUP);
  }

  // The chip can take a few tries to read the info.
  for (int i = 0; i < 3; i++) {
    // Enable debug mode to read the chip information.
    // In normal mode, reading the chip info will not work.
    write16 (REG_MODE_DEBUG_INFO);

    // Read the firmware checksum. Shoule be 0xCACAxxxx.
    uint32_t id = readRegister32 (REG_CST328_INFO_3);
    // The high bytes 3 and 2 should be 0xCACA. Extract the bytes.
    uint32_t fw_vc = (id >> 16) & 0xFFFF; // Firmware version code
    
    if (fw_vc == 0xCACA) {
      // DEBUG_SERIAL.println ("CST328 is found on the bus.");
      // DEBUG_SERIAL.println (id, HEX);
      break;
    }
    else {
      if (i == 2) {
        // DEBUG_SERIAL.println ("CST328 not found on the bus.");
        return false;
      }
    }
  }

  // Put the device in normal reporting mode.
  write16 (REG_MODE_NORMAL);
  
  return true;
}

//============================================================================================//
/**
 * @brief Read the touch data from the controller
 * 
 */
void CSE_CST328:: readData() {
  uint8_t data [27]; // Buffer for all touch registers (0xD000-0xD01A)
  
  // Read all data registers at once for efficiency.
  wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
  wireInstance->write (REG_TOUCH_INFO);
  wireInstance->write (REG_FINGER_1_ID);
  wireInstance->endTransmission (false);
  
  wireInstance->requestFrom (CTS328_I2C_ADDRESS, 27);
  
  uint8_t i = 0;

  for (i = 0; i < 27; i++) {
    if (wireInstance->available()) {
      data [i] = wireInstance->read();
    }
    else {
      break;
    }
  }

  if (i < 27) {
    // DEBUG_SERIAL.println ("readData [WARNING]: Not all registers were read.");
  }
  
  // touches = data [5] & 0x0F; // Get number of touches reported
  
  // Touch 1
  int dataIndex = 0;
  touchPoints [0].state = ((data [dataIndex] & 0x0F) == 6) ? 1 : 0;
  touchPoints [0].x = (data [dataIndex + 1] << 4) | ((data [dataIndex + 3] >> 4) & 0x0F); // Calculate X coordinate (combining high and low bits)
  touchPoints [0].y = (data [dataIndex + 2] << 4) | (data [dataIndex + 3] & 0x0F); // Calculate Y coordinate (combining high and low bits)
  touchPoints [0].z = data [dataIndex + 4]; // Touch weight/pressure

  // There is a two byte (0xD005 and 0xD006) gap between finger 1 and 2. The rest of the data is contiguous.
  // So we will use a loop for Touch 2~5.
  for (int i = 1, dataIndex = 7; dataIndex < 27; i++) { 
    touchPoints [i].state = ((data [dataIndex] & 0x0F) == 6) ? 1 : 0;
    touchPoints [i].x = (data [dataIndex + 1] << 4) | ((data [dataIndex + 3] >> 4) & 0x0F); // Calculate X coordinate (combining high and low bits)
    touchPoints [i].y = (data [dataIndex + 2] << 4) | (data [dataIndex + 3] & 0x0F); // Calculate Y coordinate (combining high and low bits)
    touchPoints [i].z = data [dataIndex + 4]; // Touch weight/pressure
    dataIndex += 5; // Increment by 5 to move to next finger.
  }
  
  // // Apply rotation if necessary
  for (uint8_t i = 0; i < 5; i++) {
    switch (rotation) {
      case 0: // Default orientation
        break;
      case 1: // 90 degrees clockwise
        {
          int16_t temp = touchPoints [i].y;
          touchPoints [i].y = width - touchPoints [i].x - 1;
          touchPoints [i].x = temp;
        }
        break;
      case 2: // 180 degrees
        touchPoints [i].x = width - touchPoints [i].x - 1;
        touchPoints [i].y = height - touchPoints [i].y - 1;
        break;
      case 3: // 270 degrees clockwise
        {
          int16_t temp = touchPoints [i].y;
          touchPoints [i].y = touchPoints [i].x;
          touchPoints [i].x = height - temp - 1;
        }
        break;
    }
  }
}

//============================================================================================//
/**
 * @brief Reads a single touch point data from the controller. This is faster than `readData()`.
 * 
 * @param id Touch or finger ID (0-4).
 */
void CSE_CST328:: fastReadData (uint8_t id) {
  uint8_t data [5]; // Buffer for data for one finger.
  
  switch (id) {
    case 0: // Finger 1 (id 0)
    default: // Default to finger 1
      wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
      wireInstance->write (REG_TOUCH_INFO); 
      wireInstance->write (REG_FINGER_1_ID); // Register address for first finger.
      wireInstance->endTransmission (false);

      wireInstance->requestFrom (CTS328_I2C_ADDRESS, 5);

      for (int i = 0; i < 5; i++) {
        if (wireInstance->available()) {
          data [i] = wireInstance->read();
        }
        else {
          break;
        }
      }
      break;

    case 1:
      wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
      wireInstance->write (REG_TOUCH_INFO); // Register address for second finger.
      wireInstance->write (REG_FINGER_2_ID);
      wireInstance->endTransmission (false);

      wireInstance->requestFrom (CTS328_I2C_ADDRESS, 5);

      for (int i = 0; i < 5; i++) {
        if (wireInstance->available()) {
          data [i] = wireInstance->read();
        }
        else {
          break;
        }
      }
      break;

    case 2:
      wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
      wireInstance->write (REG_TOUCH_INFO); // Register address for third finger.
      wireInstance->write (REG_FINGER_3_ID);
      wireInstance->endTransmission (false);

      wireInstance->requestFrom (CTS328_I2C_ADDRESS, 5);

      for (int i = 0; i < 5; i++) {
        if (wireInstance->available()) {
          data [i] = wireInstance->read();
        }
        else {
          break;
        }
      }
      break;

    case 3:
      wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
      wireInstance->write (REG_TOUCH_INFO); // Register address for fourth finger.
      wireInstance->write (REG_FINGER_4_ID);
      wireInstance->endTransmission (false);

      wireInstance->requestFrom (CTS328_I2C_ADDRESS, 5);

      for (int i = 0; i < 5; i++) {
        if (wireInstance->available()) {
          data [i] = wireInstance->read();
        }
        else {
          break;
        }
      }
      break;

    case 4:
      wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
      wireInstance->write (REG_TOUCH_INFO); // Register address for fifth finger.
      wireInstance->write (REG_FINGER_5_ID);
      wireInstance->endTransmission (false);

      wireInstance->requestFrom (CTS328_I2C_ADDRESS, 5);

      for (int i = 0; i < 5; i++) {
        if (wireInstance->available()) {
          data [i] = wireInstance->read();
        }
        else {
          break;
        }
      }
      break;
  }
  
  touchPoints [id].state = ((data [0] & 0x0F) == 6) ? 1 : 0;
  touchPoints [id].x = (data [1] << 4) | ((data [3] >> 4) & 0x0F); // Calculate X coordinate (combining high and low bits)
  touchPoints [id].y = (data [2] << 4) | (data [3] & 0x0F); // Calculate Y coordinate (combining high and low bits)
  touchPoints [id].z = data [4]; // Touch weight/pressure
  
  // Apply rotation if necessary.
  switch (rotation) {
    case 0: // Default orientation
      break;

    case 1: // 90 degrees clockwise
      {
        int16_t temp = touchPoints [id].y;
        touchPoints [id].y = width - touchPoints [id].x - 1;
        touchPoints [id].x = temp;
      }
      break;

    case 2: // 180 degrees
      touchPoints [id].x = width - touchPoints [id].x - 1;
      touchPoints [id].y = height - touchPoints [id].y - 1;
      break;
      
    case 3: // 270 degrees clockwise
      {
        int16_t temp = touchPoints [id].y;
        touchPoints [id].y = touchPoints [id].x;
        touchPoints [id].x = height - temp - 1;
      }
      break;
  }
}

//============================================================================================//
/**
 * @brief Get the number of touches detected. This will read all touch data from the controller.
 * Lifted touch data is not ignored.
 * 
 * @return `uint8_t` Number of active touches (0-5 for CST328).
 */
uint8_t CSE_CST328:: getTouches() {
  readData(); // Read all data.
  
  uint8_t touches = 0;

  for (uint8_t i = 0; i < 5; i++) { // Finc the number of active touches.
    if (touchPoints [i].state == 1) {
      touches++;
    }
  }

  return touches;
}

//============================================================================================//
/**
 * @brief Checks if the finger id is being touched right now.
 * 
 * @param id The id of the finger.
 * @return `bool` True if touched, false if not.
 */
bool CSE_CST328:: isTouched (uint8_t id) {
  fastReadData (id);  // Read the single touch point as fast as possible.
  
  if (touchPoints [id].state == 1) { // Check if the point is touched.
    return true;
  }

  return false;
}

//============================================================================================//
/**
 * @brief Check if the screen is being touched by checking all touch points.
 * @returns  True if touched, false if not.
 */
bool CSE_CST328:: isTouched() {
  return (getTouches() > 0);
}

//============================================================================================//
/**
 * @brief Get the coordinates of a touch point.
 * 
 * @param n Touch point to get (0 or 1, default 0).
 * @return `TS_Point` TS_Point object with x, y, and z coordinates.
 */
TS_Point CSE_CST328:: getPoint (uint8_t n) {
  return touchPoints [n];
}

//============================================================================================//
/**
 * @brief Set the rotation of the touch panel.
 * 
 * @param r  Rotation (0-3, where 0=0°, 1=90°, 2=180°, 3=270°).
 * @return uint8_t Current rotation setting.
 */
uint8_t CSE_CST328:: setRotation (uint8_t r) {
  rotation = r % 4; // Ensure rotation is 0-3
  
  // Update width and height based on rotation.
  switch (rotation) {
    case 0:
    case 2:
      width = defWidth;
      height = defHeight;
      break;
    case 1:
    case 3:
      width = defHeight;
      height = defWidth;
      break;
  }
  
  return rotation;
}

//============================================================================================//
/**
 * @brief Get the current rotation setting.
 * 
 * @return `uint8_t` Current rotation (0-3).
 */
uint8_t CSE_CST328:: getRotation() {
  return rotation;
}

//============================================================================================//
/**
 * @brief Get the current width considering rotation.
 * 
 * @return `uint16_t` Width in pixels.
 */
uint16_t CSE_CST328:: getWidth() {
  return width;
}

//============================================================================================//
/**
 * @brief Get the current height considering rotation.
 * 
 * @return `uint16_t` Height in pixels.
 */
uint16_t CSE_CST328:: getHeight() {
  return height;
}

//============================================================================================//
/**
 * @brief Write 8 bits to a register.
 * 
 * @param reg Register address.
 * @param val Value to write.
 */
void CSE_CST328:: writeRegister8 (uint16_t reg, uint8_t val) {
  wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
  wireInstance->write (byte (reg >> 8));
  wireInstance->write (byte (reg & 0xFF));
  wireInstance->write (byte (val));
  wireInstance->endTransmission();
}

//============================================================================================//
/**
 * @brief Writes a 16-bit value to the device. For some operations like mode setting, we just
 * need to write register address only, as per the datasheet. Therefore, this function does
 * not accept a separate value parameter.
 * 
 * @param reg The 16-bit register address.
 */
void CSE_CST328:: write16 (uint16_t reg) {
  wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
  wireInstance->write (byte (reg >> 8));
  wireInstance->write (byte (reg & 0xFF));
  wireInstance->endTransmission();
}

//============================================================================================//
/**
 * @brief Reads a single byte from the device. All register addresses on CST328 are 16-bits.
 * 
 * @param reg 16-bit register address.
 * @return `uint8_t` Value read from register.
 */
uint8_t CSE_CST328:: readRegister8 (uint16_t reg) {
  uint8_t value;

  wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
  // Write high byte first.
  wireInstance->write (byte (reg >> 8));
  wireInstance->write (byte (reg & 0xFF));
  wireInstance->endTransmission();

  wireInstance->requestFrom (byte (CTS328_I2C_ADDRESS), byte (1));

  if (wireInstance->available()) {
    value = wireInstance->read();
  }

  return value;
}

//============================================================================================//
/**
 * @brief Reads four bytes from the device. The register address is still 16-bits.
 * 
 * @param reg The 16-bit register address.
 * @return `uint32_t` The four bytes combined into a 32-bit value.
 */
uint32_t CSE_CST328:: readRegister32 (uint16_t reg) {
  uint32_t value;
  uint8_t buffer [4] = {0, 0, 0, 0};

  wireInstance->beginTransmission (CTS328_I2C_ADDRESS);
  wireInstance->write (byte (reg >> 8));  // Write high byte first.
  wireInstance->write (byte (reg & 0xFF));
  wireInstance->endTransmission();

  wireInstance->requestFrom (byte (CTS328_I2C_ADDRESS), byte (4));

  wireInstance->readBytes (buffer, 4);
  // Combine the bytes.
  value = (uint32_t) buffer [0] | (uint32_t) buffer [1] << 8 | (uint32_t) buffer [2] << 16 | (uint32_t) buffer [3] << 24;

  // if (wireInstance->available()) {
  //   value = wireInstance->read();
  //   value |= (uint32_t) wireInstance->read() << 8;
  //   value |= (uint32_t) wireInstance->read() << 16;
  //   value |= (uint32_t) wireInstance->read() << 24;
  // }

  return value;
}

//============================================================================================//