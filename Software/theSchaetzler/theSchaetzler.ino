/**************************************************************************
 This is software for TheSchätzler

 compile with:
    ESP32S3 Dev Module

 **************************************************************************/
#include <credentials.h>

#include "Schaetzler.h"

const bool wlan_on = true;
const bool ota_on = true;
const bool display_on = true;

unsigned long ipmillis;

// ssid and password are taken from <Arduino_Home>/libraries/credentials/credentials.h
// if not used as library, remove #include <credentials.h> above and set values here
const char* ssid = SSID;
const char* password = PASSWORD;

Schaetzler theSchaetzler(ssid, password);

void setup() {
  Serial.begin(115200);
  Serial.println("start");

  theSchaetzler.init();

  if(display_on) {
    theSchaetzler.setLED(64,64,0);
    theSchaetzler.setupDisplay();
    theSchaetzler.displayLogo();
    delay(500);
  }

  if(wlan_on){
    theSchaetzler.setLED(0,0,64);
    theSchaetzler.setupWLan();
    Serial.print("IP address: ");
    Serial.println(theSchaetzler.getIP());
    delay(500);
  }

  if(ota_on) {
    theSchaetzler.setLED(0,64,0);
    theSchaetzler.setupOta();
    Serial.println("OTA is active");
    delay(500);
  }
  theSchaetzler.setLED(0,0,0);
}

void loop() {    
  if(!theSchaetzler.readButton()){
    if((millis()-ipmillis)>4000){
      theSchaetzler.showIP();
    }
  } else {
    ipmillis=millis();
    theSchaetzler.showValues();
  }

  if(ota_on){
    theSchaetzler.handleOta();
  }
}
