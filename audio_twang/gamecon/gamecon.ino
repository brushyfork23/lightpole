/**
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical Description: A Moteino in a 3d printed box with a doorstop on top.
 *                       On top of the doorstop is an MPU-6050, which is wired to the Moteino.
 *                       The Moteino is powered by a 3.7V 6600mAh LiIon battery.
 * Purpose: Read tilt and acceleration from MPU-6050 and stream data to `pole`.
 *      `pole` sends back lives and sfx requests.
 *      `gamecon` displays life indication LEDs and plays audio.
 */

/***************
Metronome (timer)
***************/
#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring


/***************
Helper libs (for math)
***************/
#include <FastLED.h>


/***************
Radio
***************/
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)
#define NODEID        180     // node ID used for this unit
#define POLE_ID     81    // ID of `pole`
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
  int joystickTilt;
  int joystickWobble;
} JoystickPayload;
JoystickPayload joyPayload;

typedef struct {
  uint8_t lives;
} LivesPayload;
LivesPayload livesPayload;

enum sfx_states {
  SFX_SILENCE=0,
  SFX_MOVING,
  SFX_ATTACKING,
  SFX_KILL,
  SFX_DEAD,
  SFX_FANFARE_SMALL,
  SFX_FANFARE_LARGE,
  SFX_STATES
};
typedef struct {
  sfx_states  state;
  long    millis;
  int modifier;
} SfxPayload;
SfxPayload sfxPayload;

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);


/***************
Accelerometer/Gyroscope
***************/
#include "I2Cdev.h"
#include "MPU6050.h"
#include "RunningMedian.h"
MPU6050 accelgyro;
int16_t ax, ay, az;
int16_t gx, gy, gz;
RunningMedian MPUAngleSamples = RunningMedian(5);
RunningMedian MPUWobbleSamples = RunningMedian(5);


/***************
SFX
***************/
#include "toneAC.h"


/***************
Life LEDs
***************/
int lifeLEDs[3] = {4, 7, 8};


/***************
Game
***************/
#define MAX_VOLUME           10

// JOYSTICK
#define READINGS_PER_SECOND  63
#define JOYSTICK_ORIENTATION 1     // 0, 1 or 2 to set the angle of the joystick
#define JOYSTICK_DIRECTION   0     // 0/1 to flip joystick direction
#define JOYSTICK_DEADZONE    5     // Angle to ignore
int joystickTilt = 0;              // Stores the angle of the joystick
int joystickWobble = 0;            // Stores the max amount of acceleration (wobble)
Metro takeReading;

// PLAYER
int lives = 3;


void setup() {
  // init Serial
  Serial.begin(115200);
  while(!Serial);

  // init accelerator/gyroscope
  Wire.begin();
  accelgyro.initialize();

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

    // Life LEDs
    for(int i = 0; i<3; i++){
        pinMode(lifeLEDs[i], OUTPUT);
        digitalWrite(lifeLEDs[i], HIGH);
    }
}

void loop() {
  radioUpdate();

  if (takeReading.check()) {
    takeReading.reset();

    // read accelerometer
    getInput();

    // send tilt and wobble to `pole`
    sendJoystick();

    // play SFX
    switch(sfxPayload.state) {
      case SFX_MOVING:
        SFXtilt();
        break;
      case SFX_ATTACKING:
        SFXattacking();
        break;
      case SFX_KILL:
        SFXkill();
        break;
      case SFX_DEAD:
        SFXdead();
      case SFX_FANFARE_SMALL:
        SFXfanfareSmall();
        break;
      case SFX_FANFARE_LARGE:
        SFXfanfareLarge();
        break;
      default:
        noToneAC();
        break;
    }
  }
}


// ---------------------------------
// ----------- JOYSTICK ------------
// ---------------------------------
void getInput() {
      // This is responsible for the player movement speed and attacking. 
    // You can replace it with anything you want that passes a -90>+90 value to joystickTilt
    // and any value to joystickWobble that is greater than ATTACK_THRESHOLD (defined at start)
    // For example you could use 3 momentery buttons:
    // if(digitalRead(leftButtonPinNumber) == HIGH) joystickTilt = -90;
    // if(digitalRead(rightButtonPinNumber) == HIGH) joystickTilt = 90;
    // if(digitalRead(attackButtonPinNumber) == HIGH) joystickWobble = ATTACK_THRESHOLD;
    
    accelgyro.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    int a = (JOYSTICK_ORIENTATION == 0?ax:(JOYSTICK_ORIENTATION == 1?ay:az))/166;
    int g = (JOYSTICK_ORIENTATION == 0?gx:(JOYSTICK_ORIENTATION == 1?gy:gz));
    if(abs(a) < JOYSTICK_DEADZONE) a = 0;
    if(a > 0) a -= JOYSTICK_DEADZONE;
    if(a < 0) a += JOYSTICK_DEADZONE;
    MPUAngleSamples.add(a);
    MPUWobbleSamples.add(g);
    
    joystickTilt = MPUAngleSamples.getMedian();
    if(JOYSTICK_DIRECTION == 1) {
        joystickTilt = 0-joystickTilt;
    }
    joystickWobble = abs(MPUWobbleSamples.getHighest());
}


// ---------------------------------
// ------------- Lives -------------
// ---------------------------------
void updateLives(){
    // Updates the life LEDs to show how many lives the player has left
    for(int i = 0; i<3; i++){
       digitalWrite(lifeLEDs[i], lives>i?HIGH:LOW);
    }
}


// ---------------------------------
// -------------- SFX --------------
// ---------------------------------
void SFXtilt(){ 
    int f = map(abs(joystickTilt), 0, 90, 80, 900)+random8(100);
    if(sfxPayload.modifier < 0) f -= 500;
    if(sfxPayload.modifier > 0) f += 200;
    toneAC(f, min(min(abs(joystickTilt)/9, 5), MAX_VOLUME));
    
}
void SFXattacking(){
    int freq = map(sin(millis()/2.0)*1000.0, -1000, 1000, 500, 600);
    if(random8(5)== 0){
      freq *= 3;
    }
    toneAC(freq, MAX_VOLUME);
}
void SFXdead(){
    int freq = max(1000 - (millis()-sfxPayload.millis), 10);
    freq += random8(200);
    int vol = max(10 - (millis()-sfxPayload.millis)/200, 0);
    toneAC(freq, MAX_VOLUME);
}
void SFXkill(){
    toneAC(2000, MAX_VOLUME, 1000, true);
}
void SFXfanfareSmall(){
  if (sfxPayload.millis+1200 < millis()){
      int freq = (millis()-sfxPayload.millis)/3.0;
      freq += map(sin(millis()/20.0)*1000.0, -1000, 1000, 0, 20);
      int vol = 10;//max(10 - (millis()-sfxPayload.millis)/200, 0);
      toneAC(freq, MAX_VOLUME);
  }
  noToneAC();
}
void SFXfanfareLarge(){
  noToneAC();
}

void SFXcomplete(){
    noToneAC();
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

    switch(radio.DATALEN) {
        case sizeof(LivesPayload):
      livesPayload = *(LivesPayload*)radio.DATA;
      lives = livesPayload.lives;
      if (radio.ACKRequested()) {
        radio.sendACK();
      }
      updateLives();
      break;
        case sizeof(SfxPayload):
      sfxPayload = *(SfxPayload*)radio.DATA;
      Serial.print("Received SFX.  state: ");Serial.print(sfxPayload.state);
      Serial.print(" millis: ");Serial.print(sfxPayload.millis);
      Serial.print(" mod: ");Serial.print(sfxPayload.modifier);
      Serial.println();
      if (radio.ACKRequested()) {
        radio.sendACK();
      }
      break;
        default:
          Serial.print("Unrecognized payload size: ");Serial.print(radio.DATALEN);
            Serial.println();
            break;
      }
    }
}

void sendJoystick() {
  joyPayload.joystickTilt = joystickTilt;
    joyPayload.joystickWobble = joystickWobble;
    
//   Serial.print("Sending joystick struct (");
//   Serial.print(" tilt: ");
//   Serial.print(joyPayload.joystickTilt);
//   Serial.print(" wob: ");
//   Serial.print(joyPayload.joystickWobble);
//   Serial.print(" ) byes: ");
// Serial.print(sizeof(joyPayload));
// Serial.print(" ... ");
     radio.send(POLE_ID, (const void*)&joyPayload, sizeof(joyPayload), false);
//  if (radio.sendWithRetry(POLE_ID, (const void*)&joyPayload, sizeof(joyPayload)))
//   Serial.print(" ok!");
//  else
//   Serial.print(" nothing...");
// Serial.println();
}










