#!/bin/sh
# Turn on exit on failure
set -e
lsusb -v -d 0403:6011 | grep -E "iProduct|iSerial"
