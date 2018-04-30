# lightpole
Poles of LEDs with varying means of interaction

## Confirgurations

### lightpole_twang_wifi
TWANG on a lightpole using a controller using an esp8266 which relays to the pole through an esp8266->moteino translator.

### moteino_stream_frames
TWANG on a lightpole.  A Moteino Mega embedded in the controller runs the game code and transmits the rendered frames to the lightpole.  A Moteino powering the lightpole receives the rendered frames then scales them to the # of pole leds and displays them.

### moteino_remote_control
TWANG on a lightpole.  A Moteino embedded in the controller streams accelerometer data to the lightpole and receives game states back.  It displays life indication LEDs and plays audio based on state.  A Moteino powering the lightpole accepts accelerometer data from the controller, runs the game code, renders the display, and returns game state to the controller.