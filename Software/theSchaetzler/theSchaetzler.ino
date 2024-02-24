/****************************************************************************************
 This is software for TheSchätzler (https://kurzschluss-blog.de/shop/)
 forked from https://github.com/theBrutzler/theSchaetzler

 compile with:
    ESP32S3 Dev Module

 ***************************************************************************************/
#include <credentials.h> // -> <Arduino_Home>/libraries/credentials/credentials.h

#include "Schaetzler.h"

unsigned long ticks;
unsigned long ipmillis;
unsigned long serialmillis;

// ssid and password are taken from <Arduino_Home>/libraries/credentials/credentials.h
// if not used as library, remove #include <credentials.h> above and set values here
const char* ssid = SSID;
const char* password = PASSWORD;

Schaetzler theSchaetzler(ssid, password);

//---------------------------------------------------------------------
volatile long lastClock=micros();
volatile uint8_t dataBit=0;
volatile uint32_t readedBits=0;
volatile bool inBitSequence=false;

void IRAM_ATTR Ext_ChangeCLK_ISR(){
  if (digitalRead(PIN_CLOCK) == LOW) {
    lastClock=micros();
    return;
  }
  if(!inBitSequence && micros()-lastClock>500) {
    // clk goes to false and last falling edge was longer ago then 500 micros -> so next call starts a new sequence
    inBitSequence=true;
    return;
  }
  
  if(inBitSequence) {
    if (digitalRead(PIN_DATA) == LOW) {
      readedBits |= 1<<dataBit;
      if (dataBit>=23) {
        inBitSequence=false;
        theSchaetzler.decode(readedBits);
        readedBits=0;
        dataBit=0;
      }
    }
    dataBit++;
  }
}
//------------------------------------------------------------------


void setup() {
  Serial.begin(115200);
  Serial.println("start");

  theSchaetzler.init( 0 
    | ACTIVATE_CALIPERS   // does not work on theSchätzler1 :(, get around 0.2V - 0.3V
    | ACTIVATE_DISPLAY    // kills theSchätzler2 :(
//    | ACTIVATE_WLAN 
//    | ACTIVATE_OTA
    );

  theSchaetzler.setLED(0,0,0);

  //attachInterrupt(PIN_CLOCK, Ext_ChangeCLK_ISR, CHANGE);
}


void printMeasurements() {
  Serial.printf("C:%f\tB:%f\tM:%f\n"
    ,theSchaetzler.getCalipersVoltage()
    ,theSchaetzler.getBatteryVoltage()
    ,theSchaetzler.getMeasurement());
}

void takeMeasurement() {
  unsigned long tempmicros=0;
  bool sampled=false;
  while (!sampled) {
    while ((digitalRead(PIN_CLOCK) == HIGH)) {}
    tempmicros = micros();
    while ((digitalRead(PIN_CLOCK) == LOW)) {}  //wait for the end of the HIGH pulse
    if ((micros() - tempmicros) > 500) {       //if the HIGH pulse was longer than 500 micros we are at the start of a new bit sequence
      uint32_t value = 0;
      for (int i = 0; i < 24; i++) {
        while (digitalRead(PIN_CLOCK) == HIGH) {}  //wait until clock returns to HIGH- the first bit is not needed
        while (digitalRead(PIN_CLOCK) == LOW) {}   //wait until clock returns to LOW
        if (digitalRead(PIN_DATA) == LOW) {
          value |= 1 << i;
        }
      }
      theSchaetzler.decode(value);
      sampled=true;
    }
  }
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
    //printMeasurements();
  }
  theSchaetzler.handleOta();

  takeMeasurement();
  delay(20);
}

