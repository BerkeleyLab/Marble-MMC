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
case "$OSTYPE" in
  darwin*)
    marble=$(system_profiler SPUSBDataType | awk '/Product ID:.*6011/ { print x[NR-2]}{ x[NR]=$0 }')
    if [ -z "$marble" ]; then
      echo "Could not find FTDI device.  Is Marble plugged in?"
      exit 1
    fi
    sn=$(system_profiler SPUSBDataType | awk '
        /Product ID:.*0x6011/ { inblock=1 }          # We are inside the block we care about
        /^$/ { inblock=0 }                            # Blank line ends the block
        inblock && /Serial Number:/ { print $0 }      # If inside the block and Serial Number line, print it
    ')
    sn=$(echo "$sn" | awk -F': ' '{printf "%06d", $2+0}')
    vendor=$(system_profiler SPUSBDataType | awk '
        /Product ID:.*0x6011/ { inblock=1 }          # We are inside the block we care about
        /^$/ { inblock=0 }                            # Blank line ends the block
        inblock && /Vendor ID:/ { print $0 }      # If inside the block and Serial Number line, print it
    ')
    ;;
  *)
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
    ;;
esac

# Expect input as 4 hex digits (XXXX), convert to decimal and pad to 6 digits
sn_hex="$1"
case "$sn_hex" in
  [0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]) : ;;
  *)
    echo "Error: FTDI SERIALNUM must be exactly 4 hex digits (XXXX). Got: '$sn_hex'" >&2
    exit 1
    ;;
esac

sn_dec=$(printf '%d' "0x$sn_hex")         # hex -> decimal
snum=$(printf "%06d" "$sn_dec")  # zero-pad to 6 digits for FTDI

# Ensure it's called "Marble"
case "$marble" in
  *Marble*)
    echo "Correct name: $marble"
    ;;
  *)
    echo "Incorrect name: $marble"
    exit 1
    ;;
esac

# Ensure it has the correct serial number
if [ "$sn" != "$snum" ]; then
  echo "Incorrect serial number: $sn"
  exit 1
else
  echo "Correct serial number: $sn"
fi

exit 0
