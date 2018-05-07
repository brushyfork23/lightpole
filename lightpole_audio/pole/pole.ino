/*
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical description: A pole of APA102 LEDs driven by the Moteino.
 *                       Double sided, 110 LEDs on either side.
 *                       Powered by an 120V AC/DC 5V 10A power supply.
 * Purpose: Accepts audio data from `audio` and renders the LEDs
 */

/***************
Metronome (timer)
***************/
#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring


/***************
LEDs
***************/
#include <FastLED.h>
#define PIN_DATA1      4
#define PIN_DATA2      7
#define PIN_CLK        6
#define LED_TYPE       APA102
#define LED_COLOR_ORDER      BGR//GBR
#define BRIGHTNESS           150
#define NUM_LEDS       110
#define FPS         60
CRGB leds[NUM_LEDS];
Metro drawNextFrame;
uint16_t vals[NUM_LEDS]; // value (brightness) of each led

#define HUES_PER_SECOND   3
Metro cycleHue;
uint8_t hue = 0;


/***************
Radio
***************/
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)
#define NODEID        80  // node ID used for this unit
#define CONTROLLER_ID   180 // ID of `gamecon`
#define GATEWAY_ID    8   // ID of the `gateway` programmer node
#define NETWORKID     180
#define ENCRYPTKEY "4k8hwLgy4tRtVdGq" //(16 bytes of your choice - keep the same on all encrypted nodes)
#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif
#define IDLE_TIMEOUT_MILLIS  250
Metro idleTimer;

typedef struct {
  uint8_t vol;
} MicPayload;
MicPayload micPayload;

uint8_t vol=0; // last received volume

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);

enum stages {
  STG_STREAM=0,
  STG_SCREENSAVER
};
stages stage;

void setup() {
  // init Serial
  Serial.begin(115200);
  while(!Serial);

  // init LEDs
  FastLED.addLeds<APA102, PIN_DATA1, PIN_CLK, LED_COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.addLeds<APA102, PIN_DATA2, PIN_CLK, LED_COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();

  // init radio
  radio.initialize(RF69_915MHZ,NODEID,NETWORKID);
  radio.encrypt(ENCRYPTKEY);
  radio.setHighPower();
  Serial.print("Start node...");
  Serial.print("Node ID = ");
  Serial.println(NODEID);
  if (flash.initialize())
    Serial.println("SPI Flash Init OK!");
  else
    Serial.println("SPI Flash Init FAIL!");

  // init timers
  drawNextFrame.interval(1000UL/FPS);
  cycleHue.interval(1000UL/HUES_PER_SECOND);
  idleTimer.interval(IDLE_TIMEOUT_MILLIS);
}

void loop() {
  radioUpdate();

    if (drawNextFrame.check()) {
        drawNextFrame.reset();

        switch (stage) {
          case STG_SCREENSAVER:
            for (int i=0; i<NUM_LEDS; i++) {
              leds[i].fadeToBlackBy(26);
            }
            break;
          case STG_STREAM:
            if (idleTimer.check()) {
              stage = STG_SCREENSAVER;
            }
            // cycle color
            if (cycleHue.check()) {
              cycleHue.reset();
              hue--;
            }

            // // draw from one end to the other
            // for (uint8_t i=NUM_LEDS; i>0; i--){
            //   vals[i] = vals[i-1];
            // }
            // vals[0] = constrain(vol, 0, 255);
            // for (uint8_t i=0; i<NUM_LEDS; i++) {
            //     leds[i] = CHSV(hue, 254, vals[i]);
            // }

            // radiate from center
            for (uint8_t i=NUM_LEDS; i>NUM_LEDS/2; i--){
              vals[i] = vals[i-1];
            }
            for (uint8_t i=0; i<NUM_LEDS/2; i++){
              vals[i] = vals[i+1];
            }
            vals[NUM_LEDS/2] = constrain(vol, 0, 255);
            for (uint8_t i=0; i<NUM_LEDS; i++) {
                leds[i] = CHSV(hue, 254, vals[i]);
            }
            break;
        }

        FastLED.show();
  }
}


// ---------------------------------
// ------------ Radio --------------
// ---------------------------------
void radioUpdate() {
  if (radio.receiveDone()) {
    if( radio.SENDERID == GATEWAY_ID && radio.TARGETID == NODEID ) {
      Serial.print("Received targeted message from gateway.  Reflash?: ");Serial.print(radio.DATALEN);
      Serial.println();
      CheckForWirelessHEX(radio, flash, false);
    }

    if (radio.DATALEN == sizeof(MicPayload)) {
      idleTimer.reset();
      if (stage == STG_SCREENSAVER) {
        stage = STG_STREAM;
        for(int i=0;i<NUM_LEDS;i++){
          vals[i]=0;
        }
      }
      micPayload = *(MicPayload*)radio.DATA;
      // Serial.print("Received mic struct (");
      // Serial.print("byes: ");
      // Serial.print(radio.DATALEN);
      // Serial.print(")");
      // Serial.print(" vol: ");
      // Serial.print(micPayload.vol);
      // Serial.println();
      vol = micPayload.vol;
      if (radio.ACKRequested()) {
        radio.sendACK();
      }
    } else {
      Serial.print("Unrecognized payload size: ");Serial.print(radio.DATALEN);
      Serial.println();
    }
  }
}




