/*
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical description: A pole of APA102 LEDs driven by the Moteino.
 *                       Double sided, 110 LEDs on either side.
 *                       Powered by an 120V AC/DC 5V (something)A power supply.
 * Purpose: Receives frames from the `gamecon` and displays them on the LEDs.
 */

#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)

#include <Arduino.h>

#define NODEID      80       // node ID used for this unit
#define NETWORKID   180
#define ENCRYPTKEY "4k8hwLgy4tRtVdGq" //(16 bytes of your choice - keep the same on all encrypted nodes)

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos hsave LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);

#include <FastLED.h>

#define PIN_DATA1      4
#define PIN_DATA2      7
#define PIN_CLK        6
#define LED_TYPE       APA102
#define NUM_STRIPS     2
#define NUM_LEDS       120
#define LEDS_PER_PAYLOAD 20
#define PAYLOADS 2 // NUM_LEDS / LEDS_PER_PAYLOAD
CRGB leds[NUM_LEDS];
CRGB frameLeds[NUM_LEDS/2];

typedef struct {
  uint8_t   index;
  CRGB    leds[LEDS_PER_PAYLOAD];
} Payload;
Payload payload;

#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring
Metro calcFrames;
int frameCount = 0;

void setup() {
  FastLED.addLeds<APA102, PIN_DATA1, PIN_CLK>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);
  FastLED.addLeds<APA102, PIN_DATA2, PIN_CLK>(leds, NUM_LEDS).setCorrection(TypicalSMD5050);

  Serial.begin(115200);
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

  for(int i=0;i<NUM_LEDS;i++){
    leds[i] = CRGB::Black;
  }

  FastLED.show();

  calcFrames.interval(1000UL);
}

void loop() {

  if (calcFrames.check()) {
    Serial.print("Frames received in the last second: ");Serial.print(frameCount);
    Serial.println();
    frameCount = 0;
  }

  // receive a payload
  if (radio.receiveDone()) {
      CheckForWirelessHEX(radio, flash, false);

      // Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
      // Serial.print(" [RX_RSSI:");Serial.print(radio.readRSSI());Serial.print("]");
      // Serial.print(" [DATALEN:");Serial.print(radio.DATALEN);Serial.print("]");

      if (radio.DATALEN != sizeof(Payload)){
        Serial.print("Invalid payload received, not matching leds array!");
        Serial.println();
      } else {
      if (radio.ACKRequested()) {
        radio.sendACK();
        // Serial.print(" - ACK sent");
      }
      // Serial.println();

      // retrieve leds
      payload = *(Payload*)radio.DATA;
      memmove8( &frameLeds[LEDS_PER_PAYLOAD * payload.index], &payload.leds[0], LEDS_PER_PAYLOAD * sizeof(CRGB));

      // When a full frame has been read, update the display
      if (payload.index == PAYLOADS-1) {

        // scale frame LEDs to full strip length
        for (int i=0; i<LEDS_PER_PAYLOAD*PAYLOADS-2; i++) {
          // leds[i*2] = frameLeds[i];
          // leds[i*2+1] = blend(frameLeds[i], frameLeds[i+1], 125);
          leds[i*3] = frameLeds[i];
          leds[i*3+1] = blend(frameLeds[i], frameLeds[i+1], 85);
          leds[i*3+2] = blend(frameLeds[i], frameLeds[i+1], 170);
        }
        leds[NUM_LEDS-1] = frameLeds[LEDS_PER_PAYLOAD*PAYLOADS-1];

        frameCount++;
        FastLED.show();
      }
    }
  }
}


