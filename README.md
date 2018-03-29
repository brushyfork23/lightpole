# lightpole
Poles of LEDs with varying means of interaction

## components

pole
---
Uploaded to: Moteino RFM69HW - 868/915Mhz
Physical description: A pole of APA102 LEDs driven by the Moteino.  Double sided, 110 LEDs on either side.  Powered by an 120V AC/DC 5V (something)A power supply.
Purpose: Receives frames from the `translator_moteino` and displays them on the LEDs

translator_moteino
---
Uploaded to: Moetino RFM69HW - 868/915Mhz
Pysical Description: A Moteino connected to an ESP8266 via GPIO.  Powered by a 120V AC/DC 5V 1A power supply.
Purpose: Receives frames from the `translator_esp` and relays them to the `pole`

translator_esp
---
Uploaded to: Adafruit ESP8266 FeatherWing
Physical Description: An ESP8266 connected to a Moteino via GPIO.  Powered by a 120V AC/DC 5V 1A power supply.
Purpose: Receives frames from the `gamecon` and relays them to `translator_esp`

gamecon
---
Uploaded to: Adafruit ESP8266 FeatherWing
Physical Description: An Adafruit ESP8266 FeatherWing in a 3d printed box with a spring doorstop on top.  On top of the doorstop is an MPU-6050 accelerometer, which is wired to the ESP.  The ESP is powered by a 3.7V 6600mAh LiIon battery.
Purpose: Read tilt and acceleration from MPU-6050 and use that as game input.  Compute game mechanics and generate a frame to display.  Broadcast that frame to `translator_esp`.