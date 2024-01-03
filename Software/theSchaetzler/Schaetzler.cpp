/**************************************************************************
 Class for TheSchätzler board

 open points:
   - sleep + deactive WLAN, OTA, display etc
   - read value via interupts
   - html pages (make them nicer, OTA don't work, use REST?)
   - remove global variables? (needed in static function to handle http requests)
   - multi core?

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

#include <credentials.h>

#include "logo.h"
#include "Schaetzler.h"

#define NUMPIXELS  1
#define OLED_RESET -1
TwoWire I2C = TwoWire(0); 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C, OLED_RESET);
Adafruit_NeoPixel pixels(NUMPIXELS, PIN_SCREEN, NEO_GRB + NEO_KHZ800);

WebServer server(80);
const wifi_power_t wifiPower = WIFI_POWER_13dBm;

// ssid and password are taken from <Arduino_Home>/libraries/credentials/credentials.h
// if not used as library, remove #include <credentials.h> above and set values here
const char* ssid = SSID;
const char* password = PASSWORD;

float messurement;
float batteryVoltage;
float calipersVoltage;

bool ota_setup = false;

Schaetzler::Schaetzler(uint8_t state) {
}

void Schaetzler::init() {
  pinMode(PIN_CLOCK, INPUT_PULLUP);
  pinMode(PIN_DATA, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  pinMode(VCC_DISPLAY,OUTPUT);
  pinMode(VCC_CALIPERS,OUTPUT);

  pinMode(VOLTAGE_BATTERY, INPUT);
  pinMode(VOLTAGE_CALIPERS, INPUT);

  float Voltage = 1.5;
  analogWrite(VCC_CALIPERS,(int)(Voltage*255/3.24));

  pixels.begin();
}

void Schaetzler::setupDisplay() {
  digitalWrite(VCC_DISPLAY, HIGH);
  delay(20);
  I2C.begin(11, 12);
  delay(20);

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    setLED(255,0,0);
    delay(5000);
    ESP.restart();
  }
}

void Schaetzler::setupOta() {
  // Port defaults to 3232
  ArduinoOTA.setHostname("Schaetzler");
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
}

void Schaetzler::setupWLan() {
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

  server.begin();
  Serial.println("HTTP server started");
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
    float RSSI = WiFi.RSSI();
    server.send(200, "text/html", 
      (String)"<meta http-equiv=\"refresh\" content=\"5\" />"
      +"Messwert:"+ String(messurement)+ "<br>Akku: "+ String(batteryVoltage)+"<br>RSSI: "+ String(RSSI));
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
  display.clearDisplay();
  display.drawBitmap(0,0,epd_bitmap_theSchaetzler_scale_bot,LOGO_WIDTH,LOGO_HEIGHT,1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void Schaetzler::showUpdateProgress(uint8_t progress) {
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
  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("CLP:");
  display.print(readCalipersVoltage());
  display.print(" BAT:");
  display.println(readBatteryVoltage());
  display.setTextSize(2);
  display.print("x.xx");//  display.print(read()); !!!!
  display.println("mm");
  display.display();
}

// not used, keep as example
void Schaetzler::testscrolltext() {
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 0);
  display.println(F("scroll"));
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

IPAddress Schaetzler::getIP() {
  return WiFi.localIP();
}

void Schaetzler::showIP() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("IP:");
  display.setTextSize(1);
  display.print(getIP().toString());
  display.display();
}

void Schaetzler::handleClient() {
  server.handleClient();
}

void Schaetzler::handleOta() {
  ArduinoOTA.handle();
}


void Schaetzler::setLED(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

// not used, keep as example
void Schaetzler::scanWLan() {
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


float Schaetzler::read() {
  while(true){
    uint16_t tempmicros=micros();
    while ((digitalRead(PIN_CLOCK)==HIGH)) {}
    //if clock is LOW wait until it turns to HIGH
    tempmicros=micros();
    while ((digitalRead(PIN_CLOCK)==LOW)) {} //wait for the end of the HIGH pulse
    if ((micros()-tempmicros)>500) { //if the HIGH pulse was longer than 500 micros we are at the start of a new bit sequence
      messurement = decode(); //decode the bit sequence
      return messurement;
    }
  }
}

float Schaetzler::decode() {
  int sign=1;
  long value=0;
  for (int i=0;i<23;i++) {
    //esp_sleep_enable_ext1_wakeup(GPIO_NUM_12,HIGH) 
    //immer nur Aufwachen wenn GPIO_NUM_12 High ist while 
    //(digitalRead(clockpin)==LOW) {esp_light_sleep_start() 
    //print "ESP GEHT SCHLAFEN"} while (digitalRead(clockpin)==HIGH) { 
    //Automatisch wach und hier Werte messen (durch "esp_sleep_enable_ext1_wakeup" }
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
