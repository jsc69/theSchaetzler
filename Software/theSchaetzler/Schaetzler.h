#include <sys/_stdint.h>
#pragma once

// hardware defs
#define PIN_LED          15
#define PIN_CLOCK        2
#define PIN_DATA         4
#define PIN_BUTTON       0
#define PIN_SDA          11
#define PIN_SCL          12

#define VCC_CALIPERS     21
#define VCC_DISPLAY      17

#define VOLTAGE_CALIPERS 10
#define VOLTAGE_BATTERY  9

#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    32
#define SCREEN_ADDRESS   0x3C

#define ACTIVATE_CALIPERS 1
#define ACTIVATE_DISPLAY  2
#define ACTIVATE_WLAN     4
#define ACTIVATE_OTA      8

class Schaetzler {
private:
  char ssid[80];
  char password[80];

  void setupCalipers();
  void setupOta();
  void setupWLan();

  static void handleNotFound();
  static void handleRoot();
  static void handleStatus();
  static void handleRead();
  static void showUpdateProgress(uint8_t progress);

  static void handleClientTask(void* parameter);
  static void pinInterruptToCore(void* parameter);

  float readBatteryVoltage();
  float readCalipersVoltage();

  void dumpMeasurement(uint32_t value);

public:
  Schaetzler(const char* ssid, const char* pwd);

  void decode(uint32_t data);

  void setupDisplay(); //should be pivate
  void init(uint8_t mode);
  void handleOta();
  bool readButton();
  void displayLogo();
  void scrollText(const char* text);
  IPAddress getIP();
  void showIP();
  void showValues();

	void setLED(uint8_t r, uint8_t g, uint8_t b);
  void scanWLan();

  void refresh();

  void setMeasurement(float data);
  float getMeasurement();
  float getCalipersVoltage();
  float getBatteryVoltage();

protected:

};