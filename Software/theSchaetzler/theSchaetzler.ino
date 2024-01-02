/**************************************************************************
 This is software for TheSchätzler

 compile with:
    ESP32S3 Dev Module

 **************************************************************************/

#include "theSchaetzler.h"

const bool wlan_on = true;
const bool ota_on = true;
const bool display_on = true;

unsigned long ipmillis;

theSchaetzler mySchaetzler(0);

void setup() {
  Serial.begin(115200);
  Serial.println("start");

  mySchaetzler.init();

  if(display_on) {
    mySchaetzler.setLED(64,64,0);
    mySchaetzler.setupDisplay();
    mySchaetzler.displayLogo();
    delay(500);
  }

  if(wlan_on){
    mySchaetzler.setLED(0,0,64);
    mySchaetzler.setupWLan();
    Serial.print("IP address: ");
    Serial.println(mySchaetzler.getIP());
    delay(500);
  }

  if(ota_on) {
    mySchaetzler.setLED(0,64,0);
    mySchaetzler.setupOta();
    delay(500);
  }
  mySchaetzler.setLED(0,0,0);
}

void loop() {    
  if(!mySchaetzler.readButton()){
    if((millis()-ipmillis)>4000){
      mySchaetzler.showip();
    }
  } else {
    ipmillis=millis();
    mySchaetzler.showValues();
  }
  
  if(wlan_on){
    mySchaetzler.handleClient();
  }

  if(ota_on){
    mySchaetzler.handleOta();
  }
}
