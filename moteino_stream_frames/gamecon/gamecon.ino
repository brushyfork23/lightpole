/**
 * Uploaded to: Moteino Mega RFM69HW - 868/915Mhz
 * Physical Description: A Moteino Mega in a 3d printed box with a doorstop on top.
 *                       On top of the doorstop is an MPU-6050, which is wired to the Moteino.
 *                       The Moteino is powered by a 3.7V 6600mAh LiIon battery.
 * Purpose: Read tilt and acceleration from MPU-6050 and use that as game input.
 * Compute game mechanics and generate a frame to display.  Broadcast that frame to `pole`.
 */

#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)

#define NODEID      180    // node ID used for this unit
#define POLEID    80    // node ID of the lightpole to broadcast to
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

#define NUM_LEDS       40
#define LEDS_PER_PAYLOAD 20
#define PAYLOADS 2 // NUM_LEDS / LEDS_PER_PAYLOAD
CRGB leds[NUM_LEDS];

typedef struct {
  uint8_t   index;
  CRGB    leds[LEDS_PER_PAYLOAD];
} Payload;
Payload payload;

#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring
#define FRAMES_PER_SECOND 120 // this number actually generates about 32 FPS.  No idea why.
Metro pushNextFrame;
Metro calcFrames;
int frameCount = 0;

uint8_t actor = 0;

void setup() {
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

  pushNextFrame.interval(1000UL/FRAMES_PER_SECOND);
  calcFrames.interval(1000UL);
}

void loop() {
  if (calcFrames.check()) {
    Serial.print("Frames sent in the last second: ");Serial.print(frameCount);
    Serial.println();
    frameCount = 0;
  }
  if( pushNextFrame.check() ) {
      // Update LEDs array

      fill_rainbow(leds, NUM_LEDS, actor);
      actor += 3;
//      leds[actor] = CRGB::Black;
//      actor++;
//      if (actor >= NUM_LEDS) {
//        actor = 0;
//      }
//      leds[actor] = CRGB::Blue;

      // Send LEDs in batches
      for (int p=0;p<PAYLOADS;p++) {
        // set index of this batch
        payload.index = p;

        // copy this batch's leds
		memmove8( &payload.leds[0], &leds[LEDS_PER_PAYLOAD*p], LEDS_PER_PAYLOAD * sizeof(CRGB));

        // send this batch
      Serial.print("Sending struct (");
      Serial.print(sizeof(payload));
      Serial.print(" bytes) ... ");
      radio.send(POLEID, (const void*)&payload, sizeof(payload), false);
//      if (radio.sendWithRetry(POLEID, (const void*)&payload, sizeof(payload)))
//        Serial.print(" ok!");
//      else Serial.print(" nothing...");
        Serial.println();
      }
      frameCount++;
     pushNextFrame.reset();  // setup for next frame push
  } 

    
}