/*
 * Uploaded to: Moteino RFM69HW - 868/915Mhz
 * Physical description: A pole of APA102 LEDs driven by the Moteino.
 *                       Double sided, 110 LEDs on either side.
 *                       Powered by an 120V AC/DC 5V 10A power supply.
 * Purpose: Accepts accelerometer data from `gamecon`, runs the game code, renders the LEDs, and returns lives to `gamecon`.
 *          Simultaniously accepts audio data from `audio` and uses it to render a visualizer in the background.
 */

#define NODEID        81  // node ID used for this unit
#define CONTROLLER_ID 181 // ID of `gamecon`
#define GATEWAY_ID    254 // ID of the `gateway` programmer node
#define NETWORKID     180
#define ENCRYPTKEY "sampleEncryptKey" //(16 bytes of your choice - keep the same on all encrypted nodes)

/***************
Metronome (timer)
***************/
#include <Metro.h>    // get it here: https://github.com/thomasfredericks/Metro-Arduino-Wiring


/***************
LEDs
***************/
#include <FastLED.h>
#define PIN_DATA1         4
#define PIN_DATA2         7
#define PIN_CLK           6
#define LED_TYPE          APA102
#define LED_COLOR_ORDER   BGR//GBR
#define BRIGHTNESS        200
#define DIRECTION         1     // 0 = right to left, 1 = left to right
#define NUM_LEDS          110
#define FRAMES_PER_SECOND 63
CRGB leds[NUM_LEDS];


/***************
Audio
***************/
uint16_t hue = 0;
uint8_t targetHue = 0;
uint8_t hueFrameCounter = 0;
#define FRAMES_PER_HUE 4
#define VISUALIZER_SAT 254
uint8_t lastVol = 0; // last received volume
uint8_t noop = 0;
uint8_t vols[NUM_LEDS]; // volume readings (one for each LED)
#define AUDIO_IDLE_TIMEOUT_MILLIS  90000
long lastAudioVolMillis = 0;


/***************
Radio
***************/
#include <RFM69.h>         //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_ATC.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <RFM69_OTA.h>     //get it here: https://github.com/lowpowerlab/RFM69
#include <SPIFlash.h>      //get it here: https://github.com/lowpowerlab/spiflash
#include <SPI.h>           //included with Arduino IDE install (www.arduino.cc)
#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif
#define AUDIO_TRANSMISSION_TIMEOUT_MILLIS 250
enum audioState {
  AS_RECEIVING=0,
  AS_IDLE
};
audioState audioState;
#define GAMECON_TRANSMISSION_TIMEOUT_MILLIS 151
long lastAudioTransmissionMillis = 0;
long lastGameconTransmissionMillis = 0;
enum gameconState {
  GCS_IDLE=0,
  GCS_MOVING,
  GCS_ATTACKING
};
gameconState gameconState;

typedef struct {
  int joystickTilt;
  int joystickWobble;
} JoystickPayload;
JoystickPayload joyPayload;

typedef struct {
  uint8_t lives;
} LivesPayload;
LivesPayload livesPayload;

typedef struct {
  uint8_t vol;
} MicPayload;
MicPayload micPayload;

RFM69 radio;
SPIFlash flash(FLASH_SS, 0xEF30);


/***************
Game
***************/
#include "Enemy.h"
#include "Particle.h"
#include "Spawner.h"
#include "Lava.h"
#include "Boss.h"
#include "Conveyor.h"
long lastFrameDrawMillis = 0;
long lastJoystickMoveMillis = 0;
int levelNumber = 0;
#define JOYSTICK_IDLE_TIMEOUT_MILLIS  30000
#define LEVEL_COUNT          11

// JOYSTICK
#define ATTACK_THRESHOLD     30000 // The threshold that triggers an attack
#define JOYSTICK_DEADZONE    5     // Angle to ignore
int joystickTilt = 0;              // Stores the angle of the joystick
int joystickWobble = 0;            // Stores the max amount of acceleration (wobble)

// WOBBLE ATTACK
#define ATTACK_WIDTH        70     // Width of the wobble attack, world is 1000 wide
#define ATTACK_DURATION     500    // Duration of a wobble attack (ms)
long attackMillis = 0;             // Time the attack started
bool attacking = 0;                // Is the attack in progress?
#define BOSS_WIDTH          40

// PLAYER
#define MAX_PLAYER_SPEED    10     // Max move speed of the player
enum gameState {
  GS_PLAY=0,
  GS_DEAD,
  GS_WIN,
  GS_COMPLETE,
  GS_GAMEOVER,
  GS_SCREENSAVER
};
gameState stage;
long stageStartTime;               // Stores the time the stage changed for stages that are time based
int playerPosition;                // Stores the player position
int playerPositionModifier;        // +/- adjustment to player position
bool playerAlive;
long killTime;
#define LIVES 6
int lives = LIVES;

// POOLS
Enemy enemyPool[7] = {
    Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy(), Enemy()
};
int const enemyCount = 7;
Particle particlePool[40] = {
    Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle(), Particle()
};
int const particleCount = 40;
Spawner spawnPool[2] = {
    Spawner(), Spawner()
};
int const spawnCount = 2;
Lava lavaPool[4] = {
    Lava(), Lava(), Lava(), Lava()
};
int const lavaCount = 4;
Conveyor conveyorPool[2] = {
    Conveyor(), Conveyor()
};
int const conveyorCount = 2;
Boss boss = Boss();


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
  radio.promiscuous(true);
  radio.setHighPower();
  Serial.print(F("Start node..."));
  Serial.print("Node ID = ");
  Serial.println(NODEID);
  if (flash.initialize())
    Serial.println(F("SPI Flash Init OK!"));
  else
    Serial.println(F("SPI Flash Init FAIL!"));


  // init game
  loadLevel();

  // init timers
  audioState = AS_IDLE;
}

void loop() {
  radioUpdate();

  int brightness = 0;

  long mm = millis();
  if (mm - lastFrameDrawMillis > 1000UL/FRAMES_PER_SECOND) {
    lastFrameDrawMillis = mm;

    // check joystick; potentially toggle game state
    if(abs(joystickTilt) > JOYSTICK_DEADZONE){
      lastJoystickMoveMillis = mm;
      if(stage == GS_SCREENSAVER){
          levelNumber = -1;
          stageStartTime = mm;
          stage = GS_WIN;
          lives=3;
      }
    }else{
      if (stage != GS_SCREENSAVER && (mm - lastJoystickMoveMillis > JOYSTICK_IDLE_TIMEOUT_MILLIS)) {
        stage = GS_SCREENSAVER;
        lives=0;
        updateLives();
      }
    }

    // check audio level, potentially toggle audio state
    if (audioState == AS_RECEIVING && 
      (
        (mm - lastAudioVolMillis > AUDIO_IDLE_TIMEOUT_MILLIS)
        || (mm - lastAudioTransmissionMillis > AUDIO_TRANSMISSION_TIMEOUT_MILLIS)
      )) {
      audioState = AS_IDLE; 
    }

    // the non-audio screensaver requires the existing pixels not be cleared away
    if (stage != GS_SCREENSAVER || audioState == AS_RECEIVING) {
      FastLED.clear();
    }

    if (stage == GS_SCREENSAVER) {
      if (audioState == AS_RECEIVING) {
        //animAudioSideToSideTick();
        animAudioCenterRadiateTick();
      } else {
        //animScreenSaverTick();
        FastLED.clear(); // remove this line when using the screensaver
      }
    }else if(stage == GS_PLAY){
      // PLAYING
      if(attacking && attackMillis+ATTACK_DURATION < mm) {
        attacking = 0; 
      } 
      
      // If not attacking, check if they should be
      if(!attacking && joystickWobble > ATTACK_THRESHOLD){
          attackMillis = mm;
          attacking = 1;
      }
      
      // If still not attacking, move!
      playerPosition += playerPositionModifier;
      if(!attacking){
          int moveAmount = (joystickTilt/6.0);
          if(DIRECTION) moveAmount = -moveAmount;
          moveAmount = constrain(moveAmount, -MAX_PLAYER_SPEED, MAX_PLAYER_SPEED);
          playerPosition -= moveAmount;
          if(playerPosition < 0) playerPosition = 0;
          if(playerPosition >= 1000 && !boss.Alive()) {
              // Reached exit!
              levelComplete();
              return;
          }
      }
      
      if(inLava(playerPosition)){
          die();
      }
      
      // Ticks and draw calls
      tickConveyors();
      tickSpawners();
      tickBoss();
      tickLava();
      tickEnemies();
      drawPlayer();
      drawAttack();
      drawExit();
    }else if(stage == GS_DEAD){
        // DEAD
        // FastLED.clear();
        if(!tickParticles()){
            loadLevel();
        }
    }else if(stage == GS_WIN){
        // LEVEL COMPLETE
        // FastLED.clear();
        if(stageStartTime+500 > mm){
            int n = max(map(((mm-stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
            for(int i = NUM_LEDS; i>= n; i--){
                brightness = 255;
                leds[i] = CRGB(0, brightness, 0);
            }
        }else if(stageStartTime+1000 > mm){
            int n = max(map(((mm-stageStartTime)), 500, 1000, NUM_LEDS, 0), 0);
            for(int i = 0; i< n; i++){
                brightness = 255;
                leds[i] = CRGB(0, brightness, 0);
            }
        }else if(stageStartTime+1200 > mm){
            leds[0] = CRGB(0, 255, 0);
        }else{
            nextLevel();
        }
    }else if(stage == GS_COMPLETE){
      // GAME COMPLETE
        // FastLED.clear();
        if(stageStartTime+500 > mm){
            int n = max(map(((mm-stageStartTime)), 0, 500, NUM_LEDS, 0), 0);
            for(int i = NUM_LEDS; i>= n; i--){
                brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                leds[i].setHSV(brightness, 255, 50);
            }
        }else if(stageStartTime+5000 > mm){
            for(int i = NUM_LEDS; i>= 0; i--){
                brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                leds[i].setHSV(brightness, 255, 50);
            }
        }else if(stageStartTime+5500 > mm){
            int n = max(map(((mm-stageStartTime)), 5000, 5500, NUM_LEDS, 0), 0);
            for(int i = 0; i< n; i++){
                brightness = (sin(((i*10)+mm)/500.0)+1)*255;
                leds[i].setHSV(brightness, 255, 50);
            }
        }else{
            nextLevel();
        }
    }else if(stage == GS_GAMEOVER){
        // GAME OVER!
        // FastLED.clear();
        stageStartTime = 0;
    } 
    
//        Serial.print(millis()-mm);
//        Serial.print(" - ");
        FastLED.show();
//        Serial.println(millis()-mm);
  }
}


// ---------------------------------
// ------------ LEVELS -------------
// ---------------------------------
void loadLevel(){
    updateLives();
    cleanupLevel();
    playerPosition = 0;
    playerAlive = 1;
    switch(levelNumber){
        case 0:
            // Left or right?
            playerPosition = 200;
            spawnEnemy(1, 0, 0, 0);
            break;
        case 1:
            // Slow moving enemy
            spawnEnemy(900, 0, 1, 0);
            break;
        case 2:
            // Spawning enemies at exit every 2 seconds
            spawnPool[0].Spawn(1000, 3000, 2, 0, 0);
            break;
        case 3:
            // Lava intro
            spawnLava(400, 490, 2000, 2000, 0, "OFF");
            spawnPool[0].Spawn(1000, 5500, 3, 0, 0);
            break;
        case 4:
            // Offset Lava Run
            spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
            spawnLava(350, 440, 2000, 2000, 0, "OFF");
            spawnLava(560, 650, 2000, 2000, 0, "ON");
            break;
        case 5:
            // Sin enemy
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            break;
        case 6:
            // Conveyor
            spawnConveyor(100, 600, -1);
            spawnEnemy(800, 0, 0, 0);
            break;
        case 7:
            // Conveyor of enemies
            spawnConveyor(50, 1000, 1);
            spawnEnemy(300, 0, 0, 0);
            spawnEnemy(400, 0, 0, 0);
            spawnEnemy(500, 0, 0, 0);
            spawnEnemy(600, 0, 0, 0);
            spawnEnemy(700, 0, 0, 0);
            spawnEnemy(800, 0, 0, 0);
            spawnEnemy(900, 0, 0, 0);
            break;
        case 8:
            // Alternating All Lava Run
            spawnLava(200, 280, 1250, 1250, 0, "OFF");
            spawnLava(370, 430, 1250, 1250, 0, "ON");
            spawnLava(520, 580, 1250, 1250, 0, "OFF");
            spawnLava(670, 730, 1250, 1250, 0, "ON");
            spawnLava(820, 880, 1250, 1250, 0, "OFF");
            spawnPool[1].Spawn(0, 4500, 5, 1, 10000);
            break;
        case 9:
            // Lava run
            spawnLava(195, 300, 2000, 2000, 0, "OFF");
            spawnLava(350, 455, 2000, 2000, 0, "OFF");
            spawnLava(510, 610, 2000, 2000, 0, "OFF");
            spawnLava(660, 760, 2000, 2000, 0, "OFF");
            spawnPool[0].Spawn(1000, 3800, 4, 0, 0);
            break;
        case 10:
            // Sin enemy #2
            spawnEnemy(700, 1, 7, 275);
            spawnEnemy(500, 1, 5, 250);
            spawnPool[0].Spawn(1000, 5500, 4, 0, 3000);
            spawnPool[1].Spawn(0, 5500, 5, 1, 10000);
            spawnConveyor(100, 900, -1);
            break;
        case 11:
            // Boss
            spawnBoss();
            break;
    }
    stageStartTime = millis();
    stage = GS_PLAY;
}

void spawnBoss(){
    boss.Spawn();
    moveBoss();
}

void moveBoss(){
    int spawnSpeed = 2500;
    if(boss._lives == 2) spawnSpeed = 2000;
    if(boss._lives == 1) spawnSpeed = 1500;
    spawnPool[0].Spawn(boss._pos, spawnSpeed, 3, 0, 0);
    spawnPool[1].Spawn(boss._pos, spawnSpeed, 3, 1, 0);
}

void spawnEnemy(int pos, int dir, int sp, int wobble){
    for(int e = 0; e<enemyCount; e++){
        if(!enemyPool[e].Alive()){
            enemyPool[e].Spawn(pos, dir, sp, wobble);
            enemyPool[e].playerSide = pos > playerPosition?1:-1;
            return;
        }
    }
}

void spawnLava(int left, int right, int ontime, int offtime, int offset, char* state){
    for(int i = 0; i<lavaCount; i++){
        if(!lavaPool[i].Alive()){
            lavaPool[i].Spawn(left, right, ontime, offtime, offset, state);
            return;
        }
    }
}

void spawnConveyor(int startPoint, int endPoint, int dir){
    for(int i = 0; i<conveyorCount; i++){
        if(!conveyorPool[i]._alive){
            conveyorPool[i].Spawn(startPoint, endPoint, dir);
            return;
        }
    }
}

void cleanupLevel(){
    for(int i = 0; i<enemyCount; i++){
        enemyPool[i].Kill();
    }
    for(int i = 0; i<particleCount; i++){
        particlePool[i].Kill();
    }
    for(int i = 0; i<spawnCount; i++){
        spawnPool[i].Kill();
    }
    for(int i = 0; i<lavaCount; i++){
        lavaPool[i].Kill();
    }
    for(int i = 0; i<conveyorCount; i++){
        conveyorPool[i].Kill();
    }
    boss.Kill();
}

void levelComplete(){
    stageStartTime = millis();
    stage = GS_WIN;
    if(levelNumber == LEVEL_COUNT) {
      stage = GS_COMPLETE;
    }
    lives = LIVES;
}

void nextLevel(){
    levelNumber ++;
    if(levelNumber > LEVEL_COUNT) levelNumber = 0;
    loadLevel();
}

void gameOver(){
    levelNumber = 0;
    loadLevel();
}

void die(){
    playerAlive = 0;
    // if(levelNumber > 0) lives --;
    updateLives();
    if(lives == 0){
        levelNumber = 0;
        lives = LIVES;
    }
    for(int p = 0; p < particleCount; p++){
        particlePool[p].Spawn(playerPosition);
    }
    stageStartTime = millis();
    stage = GS_DEAD;
    killTime = millis();
}


// ----------------------------------
// -------- TICKS & RENDERS ---------
// ----------------------------------
void tickEnemies(){
    for(int i = 0; i<enemyCount; i++){
        if(enemyPool[i].Alive()){
            enemyPool[i].Tick();
            // Hit attack?
            if(attacking){
                if(enemyPool[i]._pos > playerPosition-(ATTACK_WIDTH/2) && enemyPool[i]._pos < playerPosition+(ATTACK_WIDTH/2)){
                   enemyPool[i].Kill();
                }
            }
            if(inLava(enemyPool[i]._pos)){
                enemyPool[i].Kill();
            }
            // Draw (if still alive)
            if(enemyPool[i].Alive()) {
                leds[getLED(enemyPool[i]._pos)] = CRGB(255, 0, 0);
            }
            // Hit player?
            if(
                (enemyPool[i].playerSide == 1 && enemyPool[i]._pos <= playerPosition) ||
                (enemyPool[i].playerSide == -1 && enemyPool[i]._pos >= playerPosition)
            ){
                die();
                return;
            }
        }
    }
}

void tickBoss(){
    // DRAW
    if(boss.Alive()){
        boss._ticks ++;
        for(int i = getLED(boss._pos-BOSS_WIDTH/2); i<=getLED(boss._pos+BOSS_WIDTH/2); i++){
            leds[i] = CRGB::DarkRed;
            leds[i] %= 100;
        }
        // CHECK COLLISION
        if(getLED(playerPosition) > getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition) < getLED(boss._pos + BOSS_WIDTH)){
            die();
            return; 
        }
        // CHECK FOR ATTACK
        if(attacking){
            if(
              (getLED(playerPosition+(ATTACK_WIDTH/2)) >= getLED(boss._pos - BOSS_WIDTH/2) && getLED(playerPosition+(ATTACK_WIDTH/2)) <= getLED(boss._pos + BOSS_WIDTH/2)) ||
              (getLED(playerPosition-(ATTACK_WIDTH/2)) <= getLED(boss._pos + BOSS_WIDTH/2) && getLED(playerPosition-(ATTACK_WIDTH/2)) >= getLED(boss._pos - BOSS_WIDTH/2))
            ){
               boss.Hit();
               if(boss.Alive()){
                   moveBoss();
               }else{
                   spawnPool[0].Kill();
                   spawnPool[1].Kill();
               }
            }
        }
    }
}

void drawPlayer(){
    leds[getLED(playerPosition)] = CRGB(0, 255, 0);
}

void drawExit(){
    if(!boss.Alive()){
        leds[NUM_LEDS-1] = CRGB(0, 0, 255);
    }
}

void tickSpawners(){
    long mm = millis();
    for(int s = 0; s<spawnCount; s++){
        if(spawnPool[s].Alive() && spawnPool[s]._activate < mm){
            if(spawnPool[s]._lastSpawned + spawnPool[s]._rate < mm || spawnPool[s]._lastSpawned == 0){
                spawnEnemy(spawnPool[s]._pos, spawnPool[s]._dir, spawnPool[s]._sp, 0);
                spawnPool[s]._lastSpawned = mm;
            }
        }
    }
}

void tickLava(){
    int A, B, p, i, brightness, flicker;
    long mm = millis();
    Lava LP;
    for(i = 0; i<lavaCount; i++){
        flicker = random8(5);
        LP = lavaPool[i];
        if(LP.Alive()){
            A = getLED(LP._left);
            B = getLED(LP._right);
            if(LP._state == "OFF"){
                if(LP._lastOn + LP._offtime < mm){
                    LP._state = "ON";
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++){
                    leds[p] = CRGB(3+flicker, (3+flicker)/1.5, 0);
                }
            }else if(LP._state == "ON"){
                if(LP._lastOn + LP._ontime < mm){
                    LP._state = "OFF";
                    LP._lastOn = mm;
                }
                for(p = A; p<= B; p++){
                    leds[p] = CRGB(150+flicker, 100+flicker, 0);
                }
            }
        }
        lavaPool[i] = LP;
    }
}

bool tickParticles(){
    bool stillActive = false;
    for(int p = 0; p < particleCount; p++){
        if(particlePool[p].Alive()){
            particlePool[p].Tick(0);
            leds[getLED(particlePool[p]._pos)] += CRGB(particlePool[p]._power, 0, 0);
            stillActive = true;
        }
    }
    return stillActive;
}

void tickConveyors(){
    int b, dir, n, i, ss, ee, led;
    long m = 10000+millis();
    playerPositionModifier = 0;
    
    for(i = 0; i<conveyorCount; i++){
        if(conveyorPool[i]._alive){
            dir = conveyorPool[i]._dir;
            ss = getLED(conveyorPool[i]._startPoint);
            ee = getLED(conveyorPool[i]._endPoint);
            for(led = ss; led<ee; led++){
                b = 5;
                n = (-led + (m/100)) % 5;
                if(dir == -1) n = (led + (m/100)) % 5;
                b = (5-n)/2.0;
                if(b > 0) leds[led] = CRGB(0, 0, b);
            }
            
            if(playerPosition > conveyorPool[i]._startPoint && playerPosition < conveyorPool[i]._endPoint){
                if(dir == -1){
                    playerPositionModifier = -(MAX_PLAYER_SPEED-4);
                }else{
                    playerPositionModifier = (MAX_PLAYER_SPEED-4);
                }
            }
        }
    }
}

void drawAttack(){
    if(!attacking) return;
    int n = map(millis() - attackMillis, 0, ATTACK_DURATION, 100, 5);
    for(int i = getLED(playerPosition-(ATTACK_WIDTH/2))+1; i<=getLED(playerPosition+(ATTACK_WIDTH/2))-1; i++){
        leds[i] = CRGB(0, 0, n);
    }
    if(n > 90) {
        n = 255;
        leds[getLED(playerPosition)] = CRGB(255, 255, 255);
    }else{
        n = 0;
        leds[getLED(playerPosition)] = CRGB(0, 255, 0);
    }
    leds[getLED(playerPosition-(ATTACK_WIDTH/2))] = CRGB(n, n, 255);
    leds[getLED(playerPosition+(ATTACK_WIDTH/2))] = CRGB(n, n, 255);
}

int getLED(int pos){
    // The world is 1000 pixels wide, this converts world units into an LED number
    return constrain((int)map(pos, 0, 1000, 0, NUM_LEDS-1), 0, NUM_LEDS-1);
}

bool inLava(int pos){
    // Returns if the player is in active lava
    int i;
    Lava LP;
    for(i = 0; i<lavaCount; i++){
        LP = lavaPool[i];
        if(LP.Alive() && LP._state == "ON"){
            if(LP._left < pos && LP._right > pos) return true;
        }
    }
    return false;
}

void updateLives(){
    // Updates the life LEDs to show how many lives the player has left
  livesPayload.lives = lives;
  // Serial.print("Sending lives struct (lives: ");
  // Serial.print(lives);
  // Serial.print(" bytes: ");
  // Serial.print(sizeof(livesPayload));
  // Serial.print(" ... ");
  radio.sendWithRetry(CONTROLLER_ID, (const void*)&livesPayload, sizeof(livesPayload));
  // if (radio.sendWithRetry(CONTROLLER_ID, (const void*)&livesPayload, sizeof(livesPayload)))
  //   Serial.print(" ok!");
  // else
  //   Serial.print(" nothing...");
  //   Serial.println();
}

// ---------------------------------
// --------- ANIMATIONS ------------
// ---------------------------------
void animScreenSaverTick(){
    int n, b, c, i;
    long mm = millis();
    int mode = (mm/20000)%2;
    
    for(i = 0; i<NUM_LEDS; i++){
        leds[i].nscale8(250);
    }
    if(mode == 0){
        // Marching green <> orange
        n = (mm/250)%10;
        b = 10+((sin(mm/500.00)+1)*20.00);
        c = 20+((sin(mm/5000.00)+1)*33);
        for(i = 0; i<NUM_LEDS; i++){
            if(i%10 == n){
                leds[i] = CHSV( c, 255, 150);
            }
        }
    }else if(mode == 1){
        // Random flashes
        randomSeed(mm);
        for(i = 0; i<NUM_LEDS; i++){
            if(random8(200) == 0){
                leds[i] = CHSV( 25, 255, 100);
            }
        }
    }
}

// scroll pixels across the display
void animAudioSideToSideTick(){
  // cycle color
  if (hue >= 255) {
    hue=0;
  }
  if (++hueFrameCounter > FRAMES_PER_HUE) {
    hue++;
  }
  
  for (uint8_t i=NUM_LEDS; i>0; i--){
    vols[i] = vols[i-1];
  }
  vols[0] = lastVol;
  for (uint8_t i=0; i<NUM_LEDS; i++) {
    leds[i] = CHSV(hue, VISUALIZER_SAT, vols[i]);
  }
  
}

// radiate pixels from center
void animAudioCenterRadiateTick(){
  // move toward target hue
  if (hue == targetHue) {
    targetHue = random8();
  } else if (255 - hue + targetHue > 255-targetHue+hue) {
    if (++hueFrameCounter > FRAMES_PER_HUE) {
      hueFrameCounter = 0;
      hue++;
    }
  } else {
    if (++hueFrameCounter > FRAMES_PER_HUE) {
      hueFrameCounter = 0;
      hue--;
    }
  }
  
  // if (++hueFrameCounter > FRAMES_PER_HUE) {
  //   hueFrameCounter = 0;
  //   hue++;
  // }

  for (uint8_t i=NUM_LEDS; i>NUM_LEDS/2; i--){
    // diminish as we go outward
    vols[i] = scale8(vols[i-1], 245);
  }
  for (uint8_t i=0; i<NUM_LEDS/2; i++){
    // diminish as we go outward
    vols[i] = scale8(vols[i+1], 245);
  }
  vols[NUM_LEDS/2] = lastVol;
  for (uint8_t i=0; i<NUM_LEDS; i++) {
      leds[i] = CHSV(hue, VISUALIZER_SAT, vols[i]);
  }
}


// void animMeetAndMergeTick() {
//   for (uint8_t i=NUM_LEDS/3+1; i<NUM_LEDS; i++){
//     // diminish as we go down
//     vols[i] = scale8(vols[i+1], 245);
//   }

//   for (uint8_t i=NUM_LEDS/3+1; i<NUM_LEDS; i++){
//     // diminish as we go up
//     vols[i] = scale8(vols[i+1], 245);
//   }
// }


// ---------------------------------
// ------------ Radio --------------
// ---------------------------------
void radioUpdate() {
  long mm = millis();

  // clear gamecon readings if it's gone idle
  if (gameconState != GCS_IDLE && mm - lastGameconTransmissionMillis > GAMECON_TRANSMISSION_TIMEOUT_MILLIS) {
    gameconState = GCS_IDLE;
    joystickTilt = 0;
    joystickWobble = 0;
  }

  if (radio.receiveDone()) {
    if( radio.SENDERID == GATEWAY_ID && radio.TARGETID == NODEID ) {
      Serial.print("Received targeted message from gateway.  Reflash?: ");Serial.print(radio.DATALEN);
      Serial.println();
      CheckForWirelessHEX(radio, flash, false);
    }

    if (radio.DATALEN == sizeof(MicPayload)) {
      lastAudioTransmissionMillis = mm;
      // get volume
      micPayload = *(MicPayload*)radio.DATA;
      lastVol = micPayload.vol;
      noop = lastVol;
      // update state
      if (micPayload.vol > 45) {
        lastAudioVolMillis = mm;
        if (audioState == AS_IDLE) {
          audioState = AS_RECEIVING;
          // clear previous volume readings
          for(int i=0;i<NUM_LEDS;i++){
            vols[i]=0;
          }
        }
      }
      // if (radio.ACKRequested()) {
      //   radio.sendACK();
      // }
    } else if (radio.DATALEN == sizeof(JoystickPayload)) {
      if (radio.TARGETID == NODEID) {
        lastGameconTransmissionMillis = mm;
        // get tilt and wobble
        joyPayload = *(JoystickPayload*)radio.DATA;
        joystickTilt = joyPayload.joystickTilt;
        joystickWobble = joyPayload.joystickWobble;
        // update state
        if (abs(joystickTilt) > JOYSTICK_DEADZONE) {
          if (gameconState != GCS_ATTACKING && joystickWobble > ATTACK_THRESHOLD) {
            gameconState = GCS_ATTACKING;
          } else if (gameconState != GCS_MOVING) {
            gameconState = GCS_MOVING;
          }
        } else if (gameconState != GCS_IDLE) {
          gameconState = GCS_IDLE;
        }
        //    if (radio.ACKRequested()) {
        //      radio.sendACK();
        //    }
      }
    } else {
        Serial.print("Unrecognized payload size: ");Serial.print(radio.DATALEN);
        Serial.println();
    }
  }
}





