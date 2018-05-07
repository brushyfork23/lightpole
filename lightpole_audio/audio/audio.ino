/**
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical Description: A Moteino connected to an MSGEQ7 audio decoder.
 *                       5V in for optimal audio decoder performance.
 *                       
 * Purpose: Read audio, take a rolling media, and stream data to `pole`.
 * TODO: expose API 
 */

/***************
Metronome (timer)
***************/
#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring

/***************
Microphone
***************/
#include "Mic.h"
#include "RunningMedian.h"
#define NOISE 115
#define READINGS_PER_SECOND  63
RunningMedian VolSamples = RunningMedian(60);
uint8_t triggerBand =1; // audio band to monitor volume on
uint16_t vol = 0;
Metro takeReading;
#define SAMPLES 60
int minVolAvg=0,
maxVolAvg=0,
vols[SAMPLES],
volCount=0;

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
  uint16_t vol;
} MicPayload;
MicPayload micPayload;

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);


void setup() {
  // init Serial
  Serial.begin(115200);
  while(!Serial);

  // init microphone
  listenLine.begin(MIC_RESET_PIN, MIC_STROBE_PIN, MIC_OUT_PIN);

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
  takeReading.interval(1000UL/READINGS_PER_SECOND);
}

void loop() {
  if (takeReading.check()) {
      takeReading.reset();
    readVolume();
    sendVol();
  }
}


// ---------------------------------
// --------- Microphone ------------
// ---------------------------------
void readVolume() {
int minLvl, maxLvl, floatMax, floatMin;
  // read value from microphone
  listenLine.update();
  int n = listenLine.getVol(triggerBand);
Serial.print("n: ");
Serial.print(n);
  // adjust sample for noise
  if (n < NOISE) {
    n = NOISE;
  }
    
    // take median value of recent readings
  VolSamples.add(n);
    vol = VolSamples.getAverage(5);
Serial.print(" avg5: ");
Serial.print(vol);

floatMax = VolSamples.getHighest();
Serial.print(" max: ");
Serial.print(floatMax);

if (vol >= floatMax) {
  vol = floatMax-1;
}

floatMin = VolSamples.getLowest();

if (floatMax - floatMin < 20) {
  // not enough data to get a good reading, so go dark.
  vol = 0;
    Serial.print(" lowvol: ");
  Serial.print(vol);
  Serial.println();
} else {

    vol = map(vol, NOISE, floatMax, 0, 255 );
  Serial.print(" map: ");
  Serial.print(vol);
  Serial.println();
}

    Serial.println(vol);
}
// int lvl=0,
// minLvlAvg=0,
// maxLvlAvg=0,
// vols[SAMPLES],
// volCount=0;
// #define TOP 1000
// void readVolume() {
//   uint8_t  i,
//         squash = 100;
//   uint16_t minLvl, maxLvl;
  
//   listenLine.update();
//   int n = listenLine.getVol(triggerBand);
//   //Serial << F("line in: ") << n << endl;
//   n = (n <= squash) ? 0 : (n - squash);                         // Remove noise/hum
//   lvl = ((lvl * 7) + n) >> 3;                                 // "Dampened" reading (else looks twitchy)

//   // Calculate bar height based on dynamic min/max levels (fixed point):
//   vol = TOP * (lvl - minLvlAvg) / (long)(maxLvlAvg - minLvlAvg);
//   //amplitude = map(lvl, 0, TOP, 0, 255); 
//   //amplitude = map(lvl, minLvlAvg, maxLvlAvg, 0, 255); 

//   //Serial << F("n, l, amp: ") << n << ", " << lvl << ", " << amplitude << endl;
  
//   if (vol < 0L)       vol = 0;                          // Clip output
//   else if (vol > TOP) vol = TOP;
  
//   vols[volCount] = n;                                          // Save sample for dynamic leveling
//   if (++volCount >= SAMPLES) volCount = 0;                    // Advance/rollover sample counter
 
//   // Get volume range of prior frames
//   minLvl = maxLvl = vols[0];
//   for (i=1; i<SAMPLES; i++) {
//     if (vols[i] < minLvl)      minLvl = vols[i];
//     else if (vols[i] > maxLvl) maxLvl = vols[i];
//   }
//   // minLvl and maxLvl indicate the volume range over prior frames, used
//   // for vertically scaling the output graph (so it looks interesting
//   // regardless of volume level).  If they're too close together though
//   // (e.g. at very low volume levels) the graph becomes super coarse
//   // and 'jumpy'...so keep some minimum distance between them (this
//   // also lets the graph go to zero when no sound is playing):
//   if((maxLvl - minLvl) <= TOP) maxLvl = minLvl + TOP;
//   minLvlAvg = (minLvlAvg * 63 + minLvl) >> 6;                 // Dampen min/max levels
//   maxLvlAvg = (maxLvlAvg * 63 + maxLvl) >> 6;                 // (fake rolling average)
//   Serial.println(vol);
// }


// ---------------------------------
// ------------ Radio --------------
// ---------------------------------
void sendVol() {
//  if (vol > 150) {
    micPayload.vol = vol;
//  } else {
//    micPayload.vol = 0;
//  }
//    
//    Serial.print("Sending volume struct (");
//    Serial.print("vol: ");
//    Serial.print(micPayload.vol);
//    Serial.print(" byes: ");
//  Serial.print(sizeof(micPayload));
//  Serial.print(" ... ");
//    // radio.send(POLE_ID, (const void*)&micPayload, sizeof(micPayload), false);
    radio.sendWithRetry(POLE_ID, (const void*)&micPayload, sizeof(micPayload));
//   if (radio.sendWithRetry(POLE_ID, (const void*)&micPayload, sizeof(micPayload)))
//    Serial.print(" ok!");
//   else
//    Serial.print(" nothing...");
  // Serial.println();
}














