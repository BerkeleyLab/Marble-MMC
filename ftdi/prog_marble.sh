#!/bin/sh
# Be careful with this script; FTDI is good at bricking its own products
# Usage: prog_marble.sh $SERIALNUM
# Requires:
#   ftdi_eeprom
# Build from source: https://www.intra2net.com/en/developer/libftdi/repository.php

# Turn on exit on failure
set -e

if [ $# -lt 1 ]; then
  echo "Usage: prog_marble.sh \$SERIALNUM"
  exit 1
fi

#printf -v snum "%06d" $1
snum=$(printf "%06d" "$1")
#echo $snum

# Redirect group
{
  echo "vendor_id=0x0403"
  echo "product_id=0x6011"
  echo "max_power=100"
  echo "manufacturer=\"LBNL\""
  echo "product=\"Marble\""
  echo "serial=\"$snum\""
  echo "self_powered=false"
  echo "remote_wakeup=false"
  echo "use_serial=true"
  echo "eeprom_type=0x46"
} > _temp.conf

ftdi_eeprom --device i:0x0403:0x6011 --flash-eeprom _temp.conf; rm _temp.conf
exit 0
