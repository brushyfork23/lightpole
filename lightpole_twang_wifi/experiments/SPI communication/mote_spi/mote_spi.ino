/*
 Moteino SPI communication from an ESP8266.
 Right now, just blinks; no SPI yet.  Soooooon!
*/

#include <SPI.h>

int LED_PIN = 9;
const int espSSPin = 7;

SPISettings settings(16000000, MSBFIRST, SPI_MODE0); 

void setup() {
  pinMode(LED_PIN, OUTPUT);     // Initialize the LED_PIN pin as an output
  pinMode (espSSPin, INPUT);
  SPI.begin();

  // Let the world know we've booted
  digitalWrite(LED_PIN, HIGH); //LED on
  delay(2000);
  digitalWrite(LED_PIN, LOW); //LED off
}

void loop() {
  char msg;
  if (digitalRead(espSSPin) == LOW) {
  SPI.beginTransaction(settings);
    msg = SPI.transfer(0);
    SPI.endTransaction();

    if (msg == 'B') {
      digitalWrite(LED_PIN, HIGH); // LED on
    }
    // else {
    //  digitalWrite(LED_PIN, LOW); // LED off
    //}
  }
}

