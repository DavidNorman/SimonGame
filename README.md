This repo contains the code for the Simon Game

There is an arduino part for the Simon game unit itself, which is a board type Adafruit QT Py ESP32.

There is a python script for running on the Mac laptop.  It needs a python3 environment with the bleak
and pygame libraries installed.

The Mac component will attempt to find the Bluetooth LE endpoints for the Simon game, a vibrating device,
and a shocker device.  See a different repo for the Arduino code for the shocker controller.


