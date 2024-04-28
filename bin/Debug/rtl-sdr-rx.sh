#!/bin/bash
# rtl_fm -f 144414k -M usb -s 44100 -l 1 -p 75 -o 4 | play -r 44100 -t raw -e signed-integer -b 16 -c 1 -V1 -
# https://forums.raspberrypi.com/viewtopic.php?t=72545
rtl_fm -f 144414k -M usb -s 260k -r 48k -l 1 -p 75 -o 4 -g 50 | play -r 44100 -t raw -e signed-integer -b 16 -c 1 -V1 -
