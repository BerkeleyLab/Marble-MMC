#!/bin/bash
if [ $# -lt 1 ]; then
  echo "Usage: verifyid.sh \$SERIAL_NUMBER"
  exit -1
fi

# Attempt to read from FTDI device
temp=`lsusb -v -d 0403:6011 | grep -E "iProduct|iSerial"`
if [ -z "$temp" ]; then
  echo "Could not find FTDI device.  Is Marble plugged in?"
  exit -1
fi

# Isolate product ID and serial number from output of above
marble=`echo $temp | sed -e 's/iProduct 2 //' -e 's/ iSerial 3 [0-9][0-9]*//'`
sn=`echo $temp | sed -e 's/iProduct 2 [^ ][^ ]*//' -e 's/ iSerial 3 //'`
printf -v snum "%06d" $1

# Ensure it's called "Marble"
if [[ $marble != "Marble" ]]; then
  echo "Incorrect name"
  exit 1
else
  echo "Correct name"
fi

# Ensure it has the correct serial number
if [[ $sn != $snum ]]; then
  echo "Incorrect serial number"
  exit 1
else
  echo "Correct serial number"
fi

exit 0



