

//============================================================================================//
/*
  Filename: Fast-Read-Touch-Polling.ino
  Description: Example Arduino sketch from the CSE_CST328 Arduino library.
  Reads the touch sensor through polling method and prints the data to the serial monitor.
  This reads only one touch point at a time and therefore is faster than the `Read-Touch-Polling.ino`
  example. This code was written for and tested with FireBeetle-ESP32E board.
  
  Framework: Arduino, PlatformIO
  Author: Vishnu Mohanan (@vishnumaiea, @vizmohanan)
  Maintainer: CIRCUITSTATE Electronics (@circuitstate)
  Version: 0.1
  License: MIT
  Source: https://github.com/CIRCUITSTATE/CSE_CST328
  Last Modified: +05:30 19:47:00 PM 17-02-2025, Monday
 */
//============================================================================================//

#include <Wire.h>
#include <CSE_CST328.h>

#define CST328_PIN_RST  4
#define CST328_PIN_INT  16
#define CST328_PIN_SDA  21
#define CST328_PIN_SCL  22

//===================================================================================//

// Create a new instance of the CST328 class.
// Parameters: Width, Height, &Wire, Reset pin, Interrupt pin
CSE_CST328 tsPanel = CSE_CST328 (240, 320, &Wire, CST328_PIN_RST, CST328_PIN_INT);

//===================================================================================//
/**
 * @brief Setup runs once.
 * 
 */
void setup() {
  Serial.begin (115200);
  delay (100);
  
  Serial.println();
  Serial.println ("CST328 Touch Controller Test");

  // Initialize the I2C interface (for ESP32).
  Wire.begin (CST328_PIN_SDA, CST328_PIN_SCL);

  // Initialize the touch panel.
  tsPanel.begin();
  delay (100);
}

//===================================================================================//
/**
 * @brief Inifinite loop.
 * 
 */
void loop() {
  readTouch();
  delay (100);
}

//===================================================================================//
/**
 * @brief Reads a single touch point from the panel and print their info to the serial monitor.
 * 
 */
void readTouch() {
  uint8_t point = 0;
  
  if (tsPanel.isTouched (point)) {
    Serial.print ("Touch ID: ");
    Serial.print (point);
    Serial.print (", X: ");
    Serial.print (tsPanel.getPoint (point).x);
    Serial.print (", Y: ");
    Serial.print (tsPanel.getPoint (point).y);
    Serial.print (", Z: ");
    Serial.print (tsPanel.getPoint (point).z);
    Serial.print (", State: ");
    Serial.println (tsPanel.getPoint (point).state);
  }
  else {
    Serial.println ("No touches detected");
  }
}

//===================================================================================//
