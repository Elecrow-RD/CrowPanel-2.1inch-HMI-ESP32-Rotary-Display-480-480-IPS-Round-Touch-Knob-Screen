

//============================================================================================//
/*
  Filename: Test.ino
  Description: Test sketch.
  Framework: Arduino, PlatformIO
  Author: Vishnu Mohanan (@vishnumaiea, @vizmohanan)
  Maintainer: CIRCUITSTATE Electronics (@circuitstate)
  Version: 0.1
  License: MIT
  Source: https://github.com/CIRCUITSTATE/CSE_CST328
  Last Modified: +05:30 19:37:15 PM 17-02-2025, Monday
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

void setup() {
  Serial.begin (115200);
  delay (100);

  Serial.println();
  Serial.println ("== CSE_CST328: Test ==");

  // Set the I2C pins if your board allows it.
  // For RP2040
  // Wire.setSDA (CST328_PIN_SDA);
  // Wire.setSCL (CST328_PIN_SCL);

  // For ESP32.
  Wire.begin (CST328_PIN_SDA, CST328_PIN_SCL);

  // findI2CDevices();

  tsPanel.begin();
  delay (500);
}

//===================================================================================//

void loop() {
  readTouch();
  delay (100);
}

//===================================================================================//

void readTouch() {
  if (tsPanel.isTouched (0)) {
    uint8_t i = 0;
    Serial.print ("Touch ID: ");
    Serial.print (i);
    Serial.print (", X: ");
    Serial.print (tsPanel.getPoint (i).x);
    Serial.print (", Y: ");
    Serial.print (tsPanel.getPoint (i).y);
    Serial.print (", Z: ");
    Serial.print (tsPanel.getPoint (i).z);
    Serial.print (", State: ");
    Serial.println (tsPanel.getPoint (i).state);
  }
  else {
    Serial.println ("No touches detected");
  }
}

//===================================================================================//

void findI2CDevices() {
  uint8_t error, address;
  int nDevices;
  nDevices = 0;
  
  Serial.println ("findI2CDevices [INFO]: Scanning..");

  for (address = 1; address < 127; address++ ) {
    Wire.beginTransmission (address);
    error = Wire.endTransmission();
    if (error == 0) {
      Serial.print ("findI2CDevices [OK]: I2C device found at address 0x");
      if (address < 16) {
        Serial.print ("0");
      }
      Serial.print(address,HEX);
      Serial.println("  !");
      nDevices++;
    }
    else if (error==4) {
      Serial.print("Unknow error at address 0x");
      if (address<16) {
        Serial.print("0");
      }
      Serial.println(address,HEX);
    }
  }
  if (nDevices == 0) {
    Serial.println("No I2C devices found\n");
  }
  else {
    Serial.println("done\n"); 
  }
}

//===================================================================================//