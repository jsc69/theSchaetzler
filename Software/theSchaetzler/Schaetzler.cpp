/**************************************************************************
 Class for TheSchätzler board (https://github.com/theBrutzler/theSchaetzler)

 open points:
   - sleep + deactive WLAN, OTA, display and calipers
   - WIP: read value via interupts
   - handle server by timer to reduce power consumption?
   - wlan access point for config
   - html pages (make them nicer, OTA don't work, add config page)
   - remove global variables? (needed in static function to handle http requests and ota)
   - add buttons from calipers? (cm/inch + on/off)
   - handle inch?

 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include "Schaetzler.h"
#include "logo.h"
#include "statuspage.h"

#define NUMPIXELS  1
#define OLED_RESET -1
TwoWire I2C = TwoWire(0); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C, OLED_RESET);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_SCREEN, NEO_GRB + NEO_KHZ800);

WebServer server(80);
const wifi_power_t wifiPower = WIFI_POWER_13dBm;

float measurement;
float batteryVoltage;
float calipersVoltage;

bool calipersOn;
bool displayOn;
bool wlanOn;
bool otaOn;

TaskHandle_t WebServerTask;
TaskHandle_t ReadTask;

Schaetzler::Schaetzler(const char* ssid, const char* pwd) {
  strcpy(this->ssid, ssid);
  strcpy(this->password, pwd);
}

void Schaetzler::init(uint8_t mode) {
  pinMode(PIN_CLOCK, INPUT_PULLUP);
  pinMode(PIN_DATA, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  pinMode(VCC_DISPLAY,OUTPUT);
  pinMode(VCC_CALIPERS,OUTPUT);

  pinMode(VOLTAGE_BATTERY, INPUT);
  pinMode(VOLTAGE_CALIPERS, INPUT);

  pixels.begin();

  if(mode && ACTIVATE_DISPLAY) {
    setupDisplay();
    displayLogo();
  }
  if(mode && ACTIVATE_CALIPERS) {
    setupCalipers();
  }
  if(mode && ACTIVATE_WLAN) {
    setupWLan();
  }
  if(mode && ACTIVATE_OTA) {
    setupOta();
  }
}

void Schaetzler::handleClientTask(void* parameter) {
  while(true) {
    server.handleClient();
  }
}

void Schaetzler::readTask(void* parameter) {
  while(true) {
    uint32_t tempmicros=micros();
    while ((digitalRead(PIN_CLOCK)==HIGH)) {}
    //if clock is LOW wait until it turns to HIGH
    tempmicros=micros();
    while ((digitalRead(PIN_CLOCK)==LOW)) {} //wait for the end of the HIGH pulse
    if ((micros()-tempmicros)>500) { //if the HIGH pulse was longer than 500 micros we are at the start of a new bit sequence
      measurement = decode(); //decode the bit sequence
    }
  }
}

float Schaetzler::decode() {
  int sign=1;
  long value=0;
  for (int i=0;i<23;i++) {
    while (digitalRead(PIN_CLOCK)==HIGH) {} //wait until clock returns to HIGH- the first bit is not needed
    while (digitalRead(PIN_CLOCK)==LOW) {} //wait until clock returns to LOW
    if (digitalRead(PIN_DATA)==LOW) {
      if (i<20) {
        value|= 1<<i;
      }
      if (i==20) {
        sign=-1;
      }
    }
  }
  return (value*sign)/100.00;    
}

long lastClock=micros();
uint8_t dataBit=0;
uint32_t readedBits=0;
bool inBitSequence=false;

// interrupt handle when clockPin changed
// see: https://sites.google.com/site/marthalprojects/home/arduino/arduino-reads-digital-caliper
// -> Each complete dataset consists of a series of 24 bits
// -> Between each series of 24 bits there is a longer period during which CLK remains HIGH (>500 micros)
// -> The first bit is always high and does not have any proper meaning
// -> Bit 21 is the sign bit: if bit 21 is HIGH, the value is negative 
// -> what's the meaning of bits 22,23,24?
void IRAM_ATTR Ext_INT1_ISR(){
  long timeGap=micros()-lastClock;
  lastClock=micros();

  // only changes to low are interesting
  if (digitalRead(PIN_CLOCK)) return;

  if(inBitSequence) {
    if(dataBit==23) { // or 21? 
      inBitSequence=false;
      int sign=1;
      long value=0;
      for(int i=0; i<23; i++) { // why 23, we only need 21
        if(readedBits && 1<<i) {
          if(i<20) {
            value|= 1<<i;
          }
          if(i==20) {
            sign=-1;
          }
        }
      }
      measurement = (value*sign)/100;
      readedBits=0;
      dataBit=0;
    } else {
      if (digitalRead(PIN_DATA)==LOW) {
        readedBits |= 1<<dataBit;
        dataBit++;
      }
    }
  } else if(timeGap>500) {
    // clk goes to false and was longer then 500 micros high -> we are at the start of a new bit sequence
    readedBits=0;
    inBitSequence=true;
  }
}

void Schaetzler::setupCalipers() {
  setLED(64,64,64);
  float Voltage = 1.5;
  analogWrite(VCC_CALIPERS,(int)(Voltage*255/3.24));

  // Create and start task for the measurement on second cpu
  // see https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
  uint16_t stackSize = 10000;
  void* parameter = NULL;
  uint8_t cpu = 1; // 0 or 1
  uint8_t priority = 0; // 0->lowest
  xTaskCreatePinnedToCore(readTask, "ReadTask", stackSize, parameter, priority, &ReadTask, cpu);

  // use interrupts
  //attachInterrupt(PIN_CLOCK, Ext_INT1_ISR, CHANGE);

  calipersOn=true;
  delay(500);
}

void Schaetzler::setupDisplay() {
  setLED(64,64,0);
  digitalWrite(VCC_DISPLAY, HIGH);
  delay(100);
  I2C.begin(11, 12);
  delay(100);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    setLED(255,0,0);
    delay(2000);
    ESP.restart();
  }
  displayOn=true;
  delay(500);
}

void Schaetzler::setupOta() {
  setLED(0,64,0);
  // Port defaults to 3232
  ArduinoOTA.setHostname("theSchaetzler");
  ArduinoOTA.setPassword("admin");
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      int percent = progress / (total / 100);
      showUpdateProgress(percent);
      Serial.printf("Progress: %u%%\r", percent);
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin(); 
  otaOn=true;
  delay(500); 
}

void Schaetzler::handleOta() {
  if(!otaOn) return;
  ArduinoOTA.handle();
}


void Schaetzler::setupWLan() {
  setLED(0,0,64);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setTxPower(wifiPower);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    setLED(255,0,0);
    delay(2000);
    ESP.restart();
  }
  
  if (MDNS.begin("esp32")) {
    Serial.println("MDNS responder started");
  }

  server.onNotFound(handleNotFound);
  server.on("/", handleRoot);
  server.on("/Status", handleStatus);
  server.on("/OTA", []() {
    server.send(200, "text/html", "OTA Activ");
    //ota_on = true;
  });
  server.on("/read", handleRead);

  server.begin();
  Serial.println("HTTP server started");

    // Create and start task for the WebServer
  // see https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
  uint16_t stackSize = 10000;
  void* parameter = NULL;
  uint8_t cpu = 1; // 0 or 1
  uint8_t priority = 0; // 0->lowest
  xTaskCreatePinnedToCore(handleClientTask, "WebServerTask", stackSize, parameter, priority, &WebServerTask, cpu);
  wlanOn=true;
  delay(500);
}

void Schaetzler::handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void Schaetzler::handleRoot() {
  server.send(200, "text/html", "G&uumlltige Befehle:<br> <a href=\"/Status\">Status</></a> gibt Messwert und Akkuspannung zur&uumlck <br> <a href=\"/OTA\">OTA</></a> aktiviert OTA");
}

void Schaetzler::handleStatus() {
  String s = webpage;
  server.send(200, "text/html", s);
}

void Schaetzler::handleRead() {
  char buffer[40];
  sprintf(buffer, "%6.2f", measurement);
  server.send(200, "text/plane", buffer);
}

bool Schaetzler::readButton() {
  return digitalRead(PIN_BUTTON);
}

float Schaetzler::readBatteryVoltage() {
  batteryVoltage = analogRead(VOLTAGE_BATTERY)*2.0/4096.0*3.3;
  return batteryVoltage;
}

float Schaetzler::readCalipersVoltage() {
  calipersVoltage = analogRead(VOLTAGE_CALIPERS)/4096.0*3.3;
  return calipersVoltage;
}

void Schaetzler::displayLogo() {
  if(!displayOn) return;

  display.clearDisplay();
  display.drawBitmap(0,0,epd_bitmap_theSchaetzler_scale_bot,LOGO_WIDTH,LOGO_HEIGHT,1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void Schaetzler::showUpdateProgress(uint8_t progress) {
  if(!displayOn) return;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println("Progress:");
  display.setTextSize(2);
  display.print(progress);
  display.print(" %");
  display.display();
}

void Schaetzler::showValues() {
  if(!displayOn) return;
  
  char buffer[40];
  sprintf(buffer, "%6.2f", measurement);

  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("CLP:");
  display.print(readCalipersVoltage());
  display.print(" BAT:");
  display.println(readBatteryVoltage());
  display.setTextSize(2);
  display.print(buffer);
  display.println("mm");
  display.display();
}

IPAddress Schaetzler::getIP() {
  return WiFi.localIP();
}

void Schaetzler::showIP() {
  if(!displayOn) return;
  
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("IP:");
  display.setTextSize(1);
  display.print(getIP().toString());
  display.display();
}

void Schaetzler::setLED(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}



// not used, keep as example
void Schaetzler::scrollText(const char* text) {
  if(!displayOn) return;

  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F(text));
  display.display(); 
  delay(100);

  // Scroll in various directions, pausing in-between:
  display.startscrollright(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrollleft(0x00, 0x0F);
  delay(2000);
  display.stopscroll();
  delay(1000);
  display.startscrolldiagright(0x00, 0x07);
  delay(2000);
  display.startscrolldiagleft(0x00, 0x07);
  delay(2000);
  display.stopscroll();
  delay(1000);
}

// not used, keep as example
void Schaetzler::scanWLan() {
  if(!wlanOn) {
    Serial.println("WLAN not activated!");
    return;
  }

  Serial.println("Scan start");
  int n = WiFi.scanNetworks();
  Serial.println("Scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    Serial.println("Nr | SSID                             | RSSI | CH | Encryption");
    for (int i = 0; i < n; ++i) {
        Serial.printf("%2d",i + 1);
        Serial.print(" | ");
        Serial.printf("%-32.32s", WiFi.SSID(i).c_str());
        Serial.print(" | ");
        Serial.printf("%4d", WiFi.RSSI(i));
        Serial.print(" | ");
        Serial.printf("%2d", WiFi.channel(i));
        Serial.print(" | ");
        switch (WiFi.encryptionType(i))
        {
        case WIFI_AUTH_OPEN:
            Serial.print("open");
            break;
        case WIFI_AUTH_WEP:
            Serial.print("WEP");
            break;
        case WIFI_AUTH_WPA_PSK:
            Serial.print("WPA");
            break;
        case WIFI_AUTH_WPA2_PSK:
            Serial.print("WPA2");
            break;
        case WIFI_AUTH_WPA_WPA2_PSK:
            Serial.print("WPA+WPA2");
            break;
        case WIFI_AUTH_WPA2_ENTERPRISE:
            Serial.print("WPA2-EAP");
            break;
        case WIFI_AUTH_WPA3_PSK:
            Serial.print("WPA3");
            break;
        case WIFI_AUTH_WPA2_WPA3_PSK:
            Serial.print("WPA2+WPA3");
            break;
        case WIFI_AUTH_WAPI_PSK:
            Serial.print("WAPI");
            break;
        default:
            Serial.print("unknown");
        }
        Serial.println();
    }
  }
}
