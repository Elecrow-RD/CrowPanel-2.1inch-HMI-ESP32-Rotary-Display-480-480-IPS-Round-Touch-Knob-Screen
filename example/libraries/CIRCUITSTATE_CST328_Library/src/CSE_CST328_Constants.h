
//============================================================================================//
/*
  Filename: CSE_CST328_Constants.cpp
  Description: Macros and constands for the CSE_CST328 Arduino library.
  This is a modified version of `CSTMutualConstants.h` from TouchLib by @mmMicky.
  https://github.com/mmMicky/TouchLib/
  
  Framework: Arduino, PlatformIO
  Author: Vishnu Mohanan (@vishnumaiea, @vizmohanan)
  Maintainer: CIRCUITSTATE Electronics (@circuitstate)
  Version: 0.1
  License: MIT
  Source: https://github.com/CIRCUITSTATE/CSE_CST328
  Last Modified: +05:30 20:57:04 PM 18-02-2025, Tuesday
 */
//============================================================================================//

#ifndef CSE_CST328_CONSTANTS_H
#define CSE_CST328_CONSTANTS_H

// Even though the datasheet of CST328 says default addresses are 0x34/0x35, it is not correct.
#define CTS328_I2C_ADDRESS    (0x1A)

// Version Information Registers
// The operating mode should be set to MODE_DEBUG_INFO to read these registers.

/*
 BYTE 3: KEY_NUM
 BYTE 2: TP_NRX
 BYTE 1: NC
 BYTE 0: TP_NTX
*/
#define REG_CST328_INFO_1    uint16_t((0XD1F4))

/*
 BYTE 3 ~ BYTE 2: TP_RESY
 BYTE 1 ~ BYTE 0: TP_RESX
 */
#define REG_CST328_INFO_2    uint16_t((0XD1F8))

/*
 BYTE 3 ~ BYTE 2: 0XCACA (fixed value)
 BYTE 1 ~ BYTE 0: BOOT_TIMER
 */
#define REG_CST328_INFO_3    uint16_t((0XD1FC))

/*
 BYTE 3 ~ BYTE 2: IC_TYPE
 BYTE 1 ~ BYTE 0: PROJECT_ID
 */
#define REG_CST328_INFO_4    uint16_t((0XD204))

/*
 BYTE 3: FW_MAJOR
 BYTE 2: FW_MINOR
 BYTE 1 ~ BYTE 0: FW_BUILD
*/
#define REG_CST328_INFO_5    uint16_t((0XD208))

/*
 BYTE 3:CHECKSNM_H
 BYTE 2:CHECKSNM_H
 BYTE 1:CHECKSNM_L
 BYTE 0:CHECKSNM_L
*/
#define REG_CST328_INFO_6     uint16_t((0XD20C))

// Operating Mode Registers
#define REG_MODE_DEBUG_INFO         (uint16_t(0xD101))
#define REG_CHIP_SYSTEM_RESET       (uint16_t(0xD102))
#define REG_REDO_CALIBRATION        (uint16_t(0xD104))
#define REG_CHIP_DEEP_SLEEP         (uint16_t(0xD105))
#define REG_MODE_DEBUG_POINT        (uint16_t(0xD108))
#define REG_MODE_NORMAL             (uint16_t(0xD109))
#define REG_MODE_DEBUG_RAWDATA      (uint16_t(0xD10A))
#define REG_MODE_DEBUG_WRITE        (uint16_t(0xD10B))
#define REG_MODE_DEBUG_CALIBRATION  (uint16_t(0xD10C))
#define REG_MODE_DEBUG_DIFF         (uint16_t(0xD10D))
#define REG_MODE_FACTORY            (uint16_t(0xD119))

// Touch information registers.

#define REG_TOUCH_INFO              (uint8_t(0xD0))

/*
  BIT 7 ~ BIT 4: 1st finger ID
  BIT 3 ~ BIT 0: 1st finger state: pressed (0x06) or lifted
 */
#define REG_FINGER_1_ID             (uint8_t(0x00))

/*
  BIT 7 ~ BIT 0: The X coordinate value of the 1st finger is eight high digits: X_Position>>4
 */
#define REG_FINGER_1_XH8            (uint8_t(0x01))

/*
  BIT 7 ~ BIT 0: The Y coordinate value of the 1st finger is eight high digits: Y_Position>>4
 */
#define REG_FINGER_1_YH8            (uint8_t(0x02))

/*
  BIT 7 ~ BIT 4: The X coordinate value of the 1st finger, X_Position & 0x0F
  BIT 3 ~ BIT 0: The Y coordinate value of the 1st finger, Y_Position & 0x0F
 */
#define REG_FINGER_1_XL4YL4         (uint8_t(0x03))

/*
  BIT 7 ~ BIT 0: 1st finger pressure value
 */
#define REG_FINGER_1_Z              (uint8_t(0x04))

/*
  BIT 7 ~ BIT 4: Report button flag (0x80)
  BIT 3 ~ BIT 0: Report the number of fingers
 */
#define REG_KEY_REPORT              (uint8_t(0x05))

/*
  BIT 7 ~ BIT 0: Fixed 0xAB
 */
#define REG_FIXED_0XAB              (uint8_t(0x06))

/*
  BIT 7 ~ BIT 4: 2nd finger ID
  BIT 3 ~ BIT 0: 2nd finger state: pressed (0x06) or lifted
 */
#define REG_FINGER_2_ID             (uint8_t(0x07))

/*
  BIT 7 ~ BIT 0: The X coordinate value of the 2nd finger is eight high digits: X_Position >> 4
 */
#define REG_FINGER_2_XH8            (uint8_t(0x08))

/*
  BIT 7 ~ BIT 0: The Y coordinate value of the 2nd finger is eight high digits: Y_Position >> 4
 */
#define REG_FINGER_2_YH8            (uint8_t(0x09))

/*
  BIT 7 ~ BIT 4: The X coordinate value of the 2nd finger, X_Position & 0x0F
  BIT 3 ~ BIT 0: The Y coordinate value of the 2nd finger, Y_Position & 0x0F
 */
#define REG_FINGER_2_XL4YL4         (uint8_t(0x0A))

/*
  BIT 7 ~ BIT 0: 2nd finger pressure value
 */
#define REG_FINGER_2_Z              (uint8_t(0x0B))

/*
  BIT 7 ~ BIT 4: 3rd finger ID
  BIT 3 ~ BIT 0: 3rd finger state: pressed (0x06) or lifted
 */
#define REG_FINGER_3_ID             (uint8_t(0x0C))

/*
  BIT 7 ~ BIT 0: The X coordinate value of the 3rd finger is eight high digits: X_Position >> 4
 */
#define REG_FINGER_3_XH8            (uint8_t(0x0D))

/*
  BIT 7 ~ BIT 0: The Y coordinate value of the 3rd finger is eight high digits: Y_Position >> 4
 */
#define REG_FINGER_3_YH8            (uint8_t(0x0E))

/*
  BIT 7 ~ BIT 4: The X coordinate value of the 3rd finger, X_Position & 0x0F
  BIT 3 ~ BIT 0: The Y coordinate value of the 3rd finger, Y_Position & 0x0F
 */
#define REG_FINGER_3_XL4YL4         (uint8_t(0x0F))

/*
  BIT 7 ~ BIT 0: 3rd finger pressure value
 */
#define REG_FINGER_3_Z              (uint8_t(0x10))

/*
  BIT 7 ~ BIT 4: 4th finger ID
  BIT 3 ~ BIT 0: 4th finger state: pressed (0x06) or lifted
 */
#define REG_FINGER_4_ID             (uint8_t(0x11))

/*
  BIT 7 ~ BIT 0: The X coordinate value of the 4th finger is eight high digits: X_Position >> 4
 */
#define REG_FINGER_4_XH8            (uint8_t(0x12))

/*
  BIT 7 ~ BIT 0: The Y coordinate value of the 4th finger is eight high digits: Y_Position >> 4
 */
#define REG_FINGER_4_YH8            (uint8_t(0x13))

/*
  BIT 7 ~ BIT 4: The X coordinate value of the 4th finger, X_Position & 0x0F
  BIT 3 ~ BIT 0: The Y coordinate value of the 4th finger, Y_Position & 0x0F
 */
#define REG_FINGER_4_XL4YL4         (uint8_t(0x14))

/*
  BIT 7 ~ BIT 0: 4th finger pressure value
 */
#define REG_FINGER_4_Z              (uint8_t(0x15))

/*
  BIT 7 ~ BIT 4: 5th finger ID
  BIT 3 ~ BIT 0: 5th finger state: pressed (0x06) or lifted
 */
#define REG_FINGER_5_ID             (uint8_t(0x16))

/*
  BIT 7 ~ BIT 0: The X coordinate value of the 5th finger is eight high digits: X_Position >> 4
 */
#define REG_FINGER_5_XH8            (uint8_t(0x17))

/*
  BIT 7 ~ BIT 0: The Y coordinate value of the 5th finger is eight high digits: Y_Position >> 4
 */
#define REG_FINGER_5_YH8            (uint8_t(0x18))

/*
  BIT 7 ~ BIT 4: The X coordinate value of the 5th finger, X_Position & 0x0F
  BIT 3 ~ BIT 0: The Y coordinate value of the 5th finger, Y_Position & 0x0F
 */
#define REG_FINGER_5_XL4YL4         (uint8_t(0x19))

/*
  BIT 7 ~ BIT 0: 5th finger pressure value
 */
#define REG_FINGER_5_Z              (uint8_t(0x1A))

#endif