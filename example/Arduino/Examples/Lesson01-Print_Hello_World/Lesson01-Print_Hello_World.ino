/*---------------------------------------------------------------
 * Serial output demonstration
 * Print one startup message, then repeat it at a visible interval.
 *--------------------------------------------------------------*/

/**
 * @brief Configure the serial port and print the startup message.
 * @param None.
 * @return None.
 * @call Once after reset or power-up.
 */
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Hello World");
}

/**
 * @brief Print a periodic serial message.
 * @param None.
 * @return None.
 * @call Continuously by the Arduino runtime.
 */
void loop() {
  delay(1000);
  Serial.println("Hello World");
}
