/**
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical Description: A Moteino connected to an MSGEQ7 audio decoder.
 *                       5V in for optimal audio decoder performance.
 *                       
 * Purpose: Read audio and stream data to `pole`.
 * TODO: expose API 
 */

/***************
Metronome (timer)
***************/
#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring

/***************
Microphone
***************/
#include <MSGEQ7.h> // get it here: https://github.com/NicoHood/MSGEQ7
#define MIC_OUT_PIN A0
#define MIC_STROBE_PIN A1
#define MIC_RESET_PIN A2
#define SMOOTHING 191
CMSGEQ7<SMOOTHING, MIC_RESET_PIN, MIC_STROBE_PIN, MIC_OUT_PIN> MSGEQ7;
#define MSGEQ7_INTERVAL ReadsPerSecond(30)
uint8_t vol = 0;

/***************
Radio
***************/
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)
#define NODEID        170     // node ID used for this unit
#define POLE_ID     80    // ID of `pole`
#define GATEWAY_ID    8     // ID of the `gateway` programmer node
#define NETWORKID     180
#define ENCRYPTKEY "4k8hwLgy4tRtVdGq" //(16 bytes of your choice - keep the same on all encrypted nodes)
#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

typedef struct {
  uint8_t vol;
} MicPayload;
MicPayload micPayload;

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);


void setup() {
  // init Serial
  Serial.begin(115200);
  while(!Serial);

  // init microphone
  MSGEQ7.begin();

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
}

void loop() {
  if (vol = MSGEQ7.read(MSGEQ7_INTERVAL)) {
    vol = MSGEQ7.get(MSGEQ7_MID);
    vol = mapNoise(vol, 30);
    Serial.println(vol);
    sendVol();
  }
}


// ---------------------------------
// ------------ Radio --------------
// ---------------------------------
void sendVol() {
    micPayload.vol = vol;
//    Serial.print("Sending volume struct (");
//    Serial.print("vol: ");
//    Serial.print(micPayload.vol);
//    Serial.print(" byes: ");
//  Serial.print(sizeof(micPayload));
//  Serial.print(" ... ");
     radio.send(POLE_ID, (const void*)&micPayload, sizeof(micPayload), false);
//    radio.sendWithRetry(POLE_ID, (const void*)&micPayload, sizeof(micPayload));
//   if (radio.sendWithRetry(POLE_ID, (const void*)&micPayload, sizeof(micPayload)))
//    Serial.print(" ok!");
//   else
//    Serial.print(" nothing...");
  // Serial.println();
}



