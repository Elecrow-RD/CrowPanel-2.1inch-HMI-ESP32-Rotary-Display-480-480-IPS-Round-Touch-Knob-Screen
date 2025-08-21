/*!
 * @file Adafruit_CST8XX.h
 */

#ifndef ADAFRUIT_CST8XX_LIBRARY
#define ADAFRUIT_CST8XX_LIBRARY

#include "Arduino.h"
#include <Adafruit_I2CDevice.h>

#define CST8XX_DEFAULT_ADDR 0x15   //!< I2C address
#define CST8XX_REG_NUMTOUCHES 0x02 //!< Number of touch points
#define CST8XX_REG_TOUCHDATA 0x03  //!< Number of touch points

#define CST8XX_NUM_X 0x33 //!< Touch X position
#define CST8XX_NUM_Y 0x34 //!< Touch Y position

#define CST8XX_REG_MODE 0x00      //!< Device mode, either WORKING or FACTORY
#define CST8XX_REG_CALIBRATE 0x02 //!< Calibrate mode

#define CST8XX_WORKMODE 0x00       //!< Normal mode
#define CST8XX_DEBUGMODE_IDAC 0x04 //!< Factory mode
#define CST8XX_DEBUGMODE_POS 0xE0  //!< Factory mode
#define CST8XX_DEBUGMODE_RAW 0x06  //!< Factory mode
#define CST8XX_DEBUGMODE_SIG 0x07  //!< Factory mode

#define CST8XX_REG_FIRMVERS 0xA6 //!< Firmware version
#define CST8XX_REG_SLEEP 0xA5    //!< Firmware version
#define CST8XX_REG_MODID 0xA8    //!< Module ID
#define CST8XX_REG_PROJID 0xA9   //!< Project ID
#define CST8XX_REG_CHIPTYPE 0xAA //!< Chip selecting

#define CST826_CHIPID 0x11 //!< Chip selecting

#define MAX_TOUCHES 5 //!< Maximum number of touches to detect

// Untested Chip IDs which may use different registers
// If future chips do use different registers, it may be best to
// subclass each one and override the register constants
#define CST816S_CHIPID 0xB4 //!< Chip selecting
#define CST816T_CHIPID 0xB5 //!< Chip selecting
#define CST816D_CHIPID 0xB6 //!< Chip selecting
#define CST820_CHIPID 0xB7  //!< Chip selecting

/// Events enumeration
enum Events {
  PRESS = 0, ///< Press Event Detected
  RELEASE,   ///< Release Event Detected
  TOUCHING,  ///< Touching Detected
  NONE       ///< Not Touching Detected
};

const char *const events_name[] = {"PRESS", "RELEASE", "TOUCHING",
                                   "NONE"}; //!< Event names to print

/**************************************************************************/
/*!
    @brief  Helper class that stores a TouchScreen Point with x, y, and z
   coordinates, for easy math/comparison
*/
/**************************************************************************/
class CST_TS_Point {
public:
  CST_TS_Point(void);
  CST_TS_Point(int16_t x, int16_t y, int16_t z, uint8_t id = 0,
               enum Events event = NONE);

  bool operator==(CST_TS_Point);
  bool operator!=(CST_TS_Point);

  int16_t x; /*!< X coordinate */
  int16_t y; /*!< Y coordinate */
  int16_t z; /*!< Z coordinate (often used for pressure) */

  uint8_t id;        /*!< Touch ID */
  enum Events event; /*!< Event type */
};

/**************************************************************************/
/*!
    @brief  Class that stores state and functions for interacting with CST8XX
   capacitive touch chips
*/
/**************************************************************************/
class Adafruit_CST8XX {
public:
  Adafruit_CST8XX(void);
  bool begin(TwoWire *theWire = &Wire, uint8_t i2c_addr = CST8XX_DEFAULT_ADDR);
  uint8_t touched(void);
  CST_TS_Point getPoint(uint8_t n = 0);

  // void autoCalibrate(void);

private:
  Adafruit_I2CDevice *i2c_dev = NULL; ///< Pointer to I2C bus interface
  void writeRegister8(uint8_t reg, uint8_t val);
  uint8_t readRegister8(uint8_t reg);
  uint16_t readRegister16(uint8_t reg);

  void readData(void);
  uint8_t touches;
  uint16_t touchX[MAX_TOUCHES], touchY[MAX_TOUCHES];
  uint8_t touchID[MAX_TOUCHES];
  enum Events event[MAX_TOUCHES];
};

#endif // ADAFRUIT_CST8XX_LIBRARY
