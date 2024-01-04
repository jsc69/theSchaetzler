/**************************************************************************
 This is software for TheSchätzler

 compile with:
    ESP32S3 Dev Module

 **************************************************************************/
#include <credentials.h>

#include "Schaetzler.h"

unsigned long ipmillis;

// ssid and password are taken from <Arduino_Home>/libraries/credentials/credentials.h
// if not used as library, remove #include <credentials.h> above and set values here
const char* ssid = SSID;
const char* password = PASSWORD;

Schaetzler theSchaetzler(ssid, password);

void setup() {
  Serial.begin(115200);
  Serial.println("start");

  theSchaetzler.init(ACTIVATE_CALIPERS | ACTIVATE_DISPLAY | ACTIVATE_WLAN | ACTIVATE_OTA);
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
  theSchaetzler.handleOta();
}
