/*!
 * @file Adafruit_CST8XX.cpp
 *
 * @mainpage Adafruit CST8XX Library
 *
 * @section intro_sec Introduction
 *
 * This is a library for Adafruit CST8xx-based Capacitive Touch Screens
 *
 * ----> http://www.adafruit.com/products/5792
 *
 * Check out the links above for our tutorials and wiring diagrams
 * This chipset uses I2C to communicate
 *
 * Adafruit invests time and resources providing this open source code,
 * please support Adafruit and open-source hardware by purchasing
 * products from Adafruit!
 *
 * @section author Author
 *
 * Written by Melissa LeBlanc-Williams for Adafruit Industries.
 *
 * @section license License

 * MIT license, all text above must be included in any redistribution
 */

#include <Adafruit_CST8XX.h>

// #define CST8XX_DEBUG

/**************************************************************************/
/*!
    @brief  Instantiates a new CST8XX class
*/
/**************************************************************************/
// I2C, no address adjustments or pins
Adafruit_CST8XX::Adafruit_CST8XX() { touches = 0; }

/**************************************************************************/
/*!
    @brief  Setups the I2C interface and hardware, identifies if chip is found
    @param theWire Which I2C bus to use, defaults to &Wire
    @param i2c_addr The I2C address we expect to find the touch panel at
    @returns True if an FT captouch is found, false on any failure
*/
/**************************************************************************/
bool Adafruit_CST8XX::begin(TwoWire *theWire, uint8_t i2c_addr) {
  if (i2c_dev)
    delete i2c_dev;
  i2c_dev = new Adafruit_I2CDevice(i2c_addr, theWire);
  if (!i2c_dev->begin())
    return false;

#ifdef CST8XX_DEBUG
  Serial.print("Chip ID: 0x");
  Serial.println(readRegister16(CST8XX_REG_CHIPTYPE), HEX);
  Serial.print("Firm V: ");
  Serial.println(readRegister16(CST8XX_REG_FIRMVERS));

  // dump all registers
  for (int16_t i = 0; i < 0x10; i++) {
    Serial.print("I2C $");
    Serial.print(i, HEX);
    Serial.print(" = 0x");
    Serial.println(readRegister8(i), HEX);
  }
#endif

  uint8_t id = readRegister8(CST8XX_REG_CHIPTYPE);
  if (id != CST826_CHIPID) {
    return false;
  }

  return true;
}

/**************************************************************************/
/*!
    @brief  Determines if there are any touches detected
    @returns Number of touches detected
*/
/**************************************************************************/
uint8_t Adafruit_CST8XX::touched(void) {
  uint8_t n = readRegister8(CST8XX_REG_NUMTOUCHES);

  if (n > MAX_TOUCHES) {
    n = 0;
  }

  return n;
}

/**************************************************************************/
/*!
    @brief  Queries the chip and retrieves a point data
    @param  n The # index (0 or 1) to the points we can detect. In theory we can
   detect 2 points but we've found that you should only use this for
   single-touch since the two points cant share the same half of the screen.
    @returns {@link CST_TS_Point} object that has the x and y coordinets set. If
   the z coordinate is 0 it means the point is not touched. If z is 1, it is
   currently touched.
*/
/**************************************************************************/
CST_TS_Point Adafruit_CST8XX::getPoint(uint8_t n) {
  readData();
  if ((touches == 0) || (n > 1)) {
    return CST_TS_Point(0, 0, 0);
  } else {
    return CST_TS_Point(touchX[n], touchY[n], 1, touchID[n], event[n]);
  }
}

/************ lower level i/o **************/

/**************************************************************************/
/*!
    @brief  Reads the bulk of data from captouch chip. Fill in {@link touches},
   {@link touchX}, {@link touchY} and {@link touchID} with results
*/
/**************************************************************************/
void Adafruit_CST8XX::readData(void) {

  touches = touched();
  uint8_t touchDataSize = touches * 6;

  uint8_t addr = CST8XX_REG_TOUCHDATA;
  uint8_t i2cdat[touchDataSize]; // 6 bytes of data per touch
  i2c_dev->write_then_read(&addr, 1, i2cdat, touchDataSize);

#ifdef CST8XX_DEBUG
  Serial.print("# Touches: ");
  Serial.println(touches);

  for (uint8_t i = 0; i < touchDataSize; i++) {
    Serial.print("0x");
    Serial.print(i2cdat[i], HEX);
    Serial.print(" ");
  }
#endif

  for (uint8_t i = 0; i < touches; i++) {
    touchX[i] = i2cdat[i * 6] & 0x0F;
    touchX[i] <<= 8;
    touchX[i] |= i2cdat[1 + i * 6];

    touchY[i] = i2cdat[2 + i * 6] & 0x0F;
    touchY[i] <<= 8;
    touchY[i] |= i2cdat[3 + i * 6];

    event[i] = (Events)(i2cdat[i * 6] >> 6);
    touchID[i] = i2cdat[2 + i * 6] >> 4;
  }

#ifdef CST8XX_DEBUG
  Serial.println();
  for (uint8_t i = 0; i < touches; i++) {
    Serial.print("ID #");
    Serial.print(touchID[i]);
    Serial.print("\t(");
    Serial.print(touchX[i]);
    Serial.print(", ");
    Serial.print(touchY[i]);
    Serial.print(") ");
    Serial.print("\t Event: ");
    Serial.print(events_name[event[i]]);
    Serial.println();
  }
#endif
}

uint8_t Adafruit_CST8XX::readRegister8(uint8_t reg) {
  uint8_t buffer[1] = {reg};
  i2c_dev->write_then_read(buffer, 1, buffer, 1);
  return buffer[0];
}

void Adafruit_CST8XX::writeRegister8(uint8_t reg, uint8_t val) {
  uint8_t buffer[2] = {reg, val};
  i2c_dev->write(buffer, 2);
}

uint16_t Adafruit_CST8XX::readRegister16(uint8_t reg) {
  uint8_t buffer[2] = {reg};
  i2c_dev->write_then_read(buffer, 2, buffer, 1);
  return buffer[1] << 8 | buffer[0];
}

/****************/

/**************************************************************************/
/*!
    @brief  Instantiates a new CST8XX class with x, y and z set to 0 by default
*/
/**************************************************************************/
CST_TS_Point::CST_TS_Point(void) { x = y = z = 0; }

/**************************************************************************/
/*!
    @brief  Instantiates a new CST8XX class with x, y and z set by params.
    @param  _x The X coordinate
    @param  _y The Y coordinate
    @param  _z The Z coordinate
    @param  _id The touch ID
    @param  _event The event type
*/
/**************************************************************************/

CST_TS_Point::CST_TS_Point(int16_t _x, int16_t _y, int16_t _z, uint8_t _id,
                           enum Events _event) {
  x = _x;
  y = _y;
  z = _z;
  id = _id;
  event = _event;
}

/**************************************************************************/
/*!
    @brief  Simple == comparator for two CST_TS_Point objects
    @returns True if x, y, z, and id are the same for both points, False
   otherwise.
*/
/**************************************************************************/
bool CST_TS_Point::operator==(CST_TS_Point p1) {
  return ((p1.x == x) && (p1.y == y) && (p1.z == z) && (p1.id == id));
}

/**************************************************************************/
/*!
    @brief  Simple != comparator for two CST_TS_Point objects
    @returns False if x, y, z, and id are the same for both points, True
   otherwise.
*/
/**************************************************************************/
bool CST_TS_Point::operator!=(CST_TS_Point p1) {
  return ((p1.x != x) || (p1.y != y) || (p1.z != z) || p1.id != id);
}
