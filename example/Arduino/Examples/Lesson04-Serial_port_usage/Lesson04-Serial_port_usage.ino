#include "Board21.h"

/*---------------------------------------------------------------
 * Wi-Fi UART configuration
 * UART1 communicates with the external AT-command Wi-Fi module.
 * Replace the classroom credentials before uploading the sketch.
 *--------------------------------------------------------------*/
static constexpr int WIFI_UART_TX_PIN = 43;
static constexpr int WIFI_UART_RX_PIN = 44;
static constexpr size_t AT_RESPONSE_SIZE = 512;
static const char WIFI_SSID[] = "yanfa1";
static const char WIFI_PASSWORD[] = "1223334444yanfa";

HardwareSerial wifiSerial(1);
static lv_obj_t *status;

/**
 * @brief Update the single status label and let LVGL refresh the display.
 * @param message Text shown on the LCD.
 * @return None.
 * @call During Wi-Fi module setup and after each AT response.
 */
static void updateStatus(const char *message) {
  lv_label_set_text(status, message);
  lv_timer_handler();
}

/**
 * @brief Remove stale bytes from the Wi-Fi UART receive buffer.
 * @param None.
 * @return None.
 * @call Immediately before sending each AT command.
 */
static void clearWifiUart() {
  while (wifiSerial.available()) {
    wifiSerial.read();
  }
}

/**
 * @brief Read an AT response until timeout or buffer capacity is reached.
 * @param response Destination buffer for a null-terminated response.
 * @param capacity Size of the destination buffer in bytes.
 * @param timeoutMs Maximum wait time in milliseconds.
 * @return Number of response bytes stored, excluding the terminator.
 * @call By sendAtCommand() after a command is transmitted.
 */
static size_t readAtResponse(char *response, size_t capacity, uint32_t timeoutMs) {
  size_t length = 0;
  const uint32_t startTime = millis();

  while (millis() - startTime < timeoutMs && length < capacity - 1) {
    while (wifiSerial.available() && length < capacity - 1) {
      response[length++] = static_cast<char>(wifiSerial.read());
    }
    lv_timer_handler();
    delay(5);
  }

  response[length] = '\0';
  return length;
}

/**
 * @brief Send one AT command and collect its complete response.
 * @param command Command text without CR/LF.
 * @param timeoutMs Maximum response wait time.
 * @param response Destination buffer.
 * @param responseCapacity Destination buffer capacity.
 * @return true when the response contains "OK"; false otherwise.
 * @call During connectWifiModule().
 */
static bool sendAtCommand(const char *command, uint32_t timeoutMs,
                          char *response, size_t responseCapacity) {
  clearWifiUart();
  Serial.printf("[WIFI] >> %s\n", command);
  wifiSerial.print(command);
  wifiSerial.print("\r\n");
  wifiSerial.flush();

  readAtResponse(response, responseCapacity, timeoutMs);
  Serial.printf("[WIFI] << %s\n", response);
  updateStatus(response[0] ? response : "No response");
  return strstr(response, "OK") != nullptr;
}

/**
 * @brief Extract the station IP address from an AT+CIFSR response.
 * @param response Text returned by the Wi-Fi module.
 * @return IP address, or a fallback message when it is not present.
 * @call After a successful AT+CIFSR command.
 */
static String extractIpAddress(const char *response) {
  const char *start = strstr(response, "STAIP,\"");
  if (start == nullptr) {
    return "IP assigned";
  }

  start += strlen("STAIP,\"");
  const char *end = strchr(start, '\"');
  if (end == nullptr || end <= start) {
    return "IP assigned";
  }
  return String(start).substring(0, end - start);
}

/**
 * @brief Configure station mode, join the network, and query the IP address.
 * @param None.
 * @return None.
 * @call Once from setup() after UART initialization.
 */
static void connectWifiModule() {
  char response[AT_RESPONSE_SIZE];

  updateStatus("Checking Wi-Fi module...");
  if (!sendAtCommand("AT", 1000, response, sizeof(response))) {
    updateStatus("Module not found\nCheck TX43 / RX44");
    return;
  }

  updateStatus("Setting station mode...");
  if (!sendAtCommand("AT+CWMODE=1", 1200, response, sizeof(response))) {
    updateStatus("Station mode failed");
    return;
  }

  char joinCommand[160];
  snprintf(joinCommand, sizeof(joinCommand), "AT+CWJAP=\"%s\",\"%s\"",
    WIFI_SSID, WIFI_PASSWORD);
  updateStatus("Connecting to Wi-Fi...");
  if (!sendAtCommand(joinCommand, 15000, response, sizeof(response))) {
    updateStatus("Wi-Fi connect failed\nCheck SSID/password");
    return;
  }

  updateStatus("Wi-Fi connected\nReading IP...");
  if (sendAtCommand("AT+CIFSR", 1500, response, sizeof(response))) {
    String ipAddress = extractIpAddress(response);
    updateStatus(ipAddress.c_str());
  } else {
    updateStatus("Wi-Fi connected\nIP query failed");
  }
}

/**
 * @brief Initialize the display, UART1, and Wi-Fi module configuration.
 * @param None.
 * @return None.
 * @call Once after reset or power-up.
 */
void setup() {
  Serial.begin(115200);
  delay(500);
  boardBegin(true);

  status = lv_label_create(lv_screen_active());
  lv_obj_set_style_text_font(status, &lv_font_montserrat_32, 0);
  lv_obj_set_width(status, SCREEN_WIDTH - 20);
  lv_obj_set_style_text_align(status, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(status);
  lv_label_set_text(status, "Starting Wi-Fi UART...");

  // RX is GPIO44 and TX is GPIO43 for the external UART module.
  wifiSerial.begin(115200, SERIAL_8N1, WIFI_UART_RX_PIN, WIFI_UART_TX_PIN);
  connectWifiModule();
}

/**
 * @brief Keep the LVGL display responsive after Wi-Fi setup.
 * @param None.
 * @return None.
 * @call Continuously by the Arduino runtime.
 */
void loop() {
  lv_timer_handler();
  delay(5);
}
