#!/bin/bash
lsusb -v -d 0403:6011 | grep -E "iProduct|iSerial"
