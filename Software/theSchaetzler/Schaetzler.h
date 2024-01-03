#pragma once

// hardware defs
#define PIN_LED          47
#define PIN_CLOCK        2
#define PIN_DATA         4
#define PIN_SCREEN       15 
#define PIN_BUTTON       0

#define VCC_CALIPERS     21
#define VCC_DISPLAY      17

#define VOLTAGE_CALIPERS 10
#define VOLTAGE_BATTERY  9

#define SCREEN_WIDTH     128
#define SCREEN_HEIGHT    32
#define SCREEN_ADDRESS   0x3C

class Schaetzler {
private:
  char ssid[80];
  char password[80];

  static void handleNotFound();
  static void handleRoot();
  static void handleStatus();
  static void handleRead();
  static void showUpdateProgress(uint8_t progress);

  static void handleClientTask(void* parameter);
  static void readTask(void* parameter);
  static float decode();

  float readBatteryVoltage();
  float readCalipersVoltage();

public:
  Schaetzler(const char* ssid, const char* pwd);
  void init();

  void handleOta();

  void setupDisplay();
  void setupOta();
  void setupWLan();

  bool readButton();


  void displayLogo();
  void testscrolltext();
  IPAddress getIP();
  void showIP();
  void showValues();

	void setLED(uint8_t r, uint8_t g, uint8_t b);
  void scanWLan();

  float getMessurement();

protected:

};