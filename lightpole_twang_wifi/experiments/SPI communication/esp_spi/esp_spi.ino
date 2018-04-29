/*
 ESP8266 SPI communication to a Moteino.
 Right now, just blinks; no SPI yet.  Soooooon!
*/

#include <SPI.h>
// Moteino is 16 MHz, ESP8266 is 80 MHz

const int moteSSPin = 14;

SPISettings settings(16000000, MSBFIRST, SPI_MODE0); 

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  pinMode (moteSSPin, OUTPUT);
  SPI.begin();
}


void loop() {

	uint8_t msg;

	digitalWrite(LED_BUILTIN, LOW); //LED on

	// send char A
	SPI.beginTransaction(settings);
	digitalWrite (moteSSPin, LOW);
	SPI.transfer('B');
	digitalWrite (moteSSPin, HIGH); 
	SPI.endTransaction();

	delay(1000);

	digitalWrite(LED_BUILTIN, HIGH); // LED off

	// send char B
	SPI.beginTransaction(settings);
	digitalWrite (moteSSPin, LOW);
	SPI.transfer('A');
	digitalWrite (moteSSPin, HIGH); 
	SPI.endTransaction();

	delay(2000);
}
