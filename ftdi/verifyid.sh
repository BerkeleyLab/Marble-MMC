#!/bin/sh
# Verify ID and serial number for marble board via FTDI utilities
# Usage: verifyid.sh $SERIALNUM
# Requires:
#   ftdi_eeprom
# Build from source: https://www.intra2net.com/en/developer/libftdi/repository.php

# Turn on exit on failure
set -e

if [ $# -lt 1 ]; then
  echo "Usage: verifyid.sh \$SERIAL_NUMBER"
  exit 1
fi

# Attempt to read from FTDI device
temp=$(lsusb -v -d 0403:6011 | grep -E "iProduct|iSerial")
if [ -z "$temp" ]; then
  echo "Could not find FTDI device.  Is Marble plugged in?"
  exit 1
fi

# Isolate product ID and serial number from output of above
marble=$(echo "$temp" | sed -e 's/[ ][ ]*iProduct[ ][ ]*2[ ][ ]*//'          -e 's/[ ][ ]*iSerial[ ][ ]*3[ ][ ]*[0-9][0-9]*//')
sn=$(echo "$temp" | sed     -e 's/[ ][ ]*iProduct[ ][ ]*2[ ][ ]*[^ ][^ ]*//' -e 's/[ ][ ]*iSerial[ ][ ]*3[ ][ ]*[\n\r\v]*//')
# Yikes! Apparently you need this '-z' option to get sed to recognize the newline char
sn=$(echo "$sn" | sed -z -e 's/[^0-9][^0-9]*//')

snum=$(printf "%06d" "$1")

# Ensure it's called "Marble"
if [ "$marble" != "Marble" ]; then
  echo "Incorrect name: $marble"
  exit 1
else
  echo "Correct name: $marble"
fi

# Ensure it has the correct serial number
if [ "$sn" != "$snum" ]; then
  echo "Incorrect serial number: $sn"
  exit 1
else
  echo "Correct serial number: $sn"
fi

exit 0



