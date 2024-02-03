/****************************************************************************************
 This is software for TheSchätzler (https://kurzschluss-blog.de/shop/)
 forked from https://github.com/theBrutzler/theSchaetzler

 compile with:
    ESP32S3 Dev Module

 ***************************************************************************************/
#include <credentials.h> // -> <Arduino_Home>/libraries/credentials/credentials.h

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

  theSchaetzler.init( 0 
    | ACTIVATE_CALIPERS   // does not work on theSchätzler1 :(, get around 0.2V - 0.3V
    | ACTIVATE_DISPLAY    // kills theSchätzler2 :(
    | ACTIVATE_WLAN 
    | ACTIVATE_OTA
    );

  theSchaetzler.setLED(0,0,0);
}


void printMeasurements() {
  Serial.printf("C:%f\tB:%f\tM:%f\n"
    ,theSchaetzler.getCalipersVoltage()
    ,theSchaetzler.getBatteryVoltage()
    ,theSchaetzler.getMeasurement());
}


void loop() {
  ticks = millis();

  theSchaetzler.refresh();
  if(!theSchaetzler.readButton()){
    if((ticks-ipmillis)>4000){
      Serial.println(theSchaetzler.getIP());
      theSchaetzler.setupDisplay();
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
