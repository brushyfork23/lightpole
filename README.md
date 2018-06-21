# lightpole
Poles of LEDs with varying means of interaction

## audio_twang
TWANG (https://github.com/Critters/TWANG) with an integrated audio visualizer.

The lightpole is 220 APA102 LEDs controlled by a Moteino (https://lowpowerlab.com/shop/product/99) and mounted inside a polycarbonate tube.

Another Moteino embedded in the controller reads accelerometer data from an MPU-6050 (https://www.sparkfun.com/products/10937) and streams it to the lightpole to control the green character's movement and attack.

When the game is not being played, an audio visualizer is displayed instead.  A third Moteino decodes audio data using an MSGEQ7 (https://www.sparkfun.com/products/10468) and streams it to the lightpole for display.

## reprogramming
twang can be wirelessly reprogrammed using: python OTA.py -s /dev/tty.usbserial-A504WLK2 -f /var/folders/rc/nqhm069x29vg_n_zg4q6k0xh0000gp/T/arduino_build_326498/pole.ino.hex -t 80