/**************************************************************************
 This is software for TheSchätzler

 compile with:
    ESP32S3 Dev Module

 **************************************************************************/
#include <credentials.h>

#include <Adafruit_NeoPixel.h>
#include "Schaetzler.h"

unsigned long ticks;
unsigned long ipmillis;
unsigned long serialmillis;

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


void printMeasurements() {
  Serial.printf("%f\t%f\t%f\n",theSchaetzler.getCalipersVoltage(), theSchaetzler.getBatteryVoltage(), theSchaetzler.getMeasurement());
}


void loop() {
  ticks = millis();

  theSchaetzler.refresh();
  if(!theSchaetzler.readButton()){
    if((ticks-ipmillis)>4000){
      theSchaetzler.showIP();
    }
  } else {
    ipmillis=ticks;
    theSchaetzler.showValues();
  }
  if ((ticks-serialmillis)>1000) {
    serialmillis=ticks;
    printMeasurements();
  }

  theSchaetzler.handleOta();
}
