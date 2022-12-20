#!/bin/bash
# Be careful with this script; FTDI is good at bricking its own products
# Usage: ./prog_marble.sh $SERIALNUM
# Requires:
#   ftdi_eeprom
# Build from source: https://www.intra2net.com/en/developer/libftdi/repository.php
if [ $# -lt 1 ]; then
  echo "Usage: prog_marble.sh \$SERIALNUM"
  exit 1
fi

printf -v snum "%06d" $1
#echo $snum

echo "vendor_id=0x0403" > _temp.conf
echo "product_id=0x6011" >> _temp.conf
echo "max_power=100" >> _temp.conf
echo "manufacturer=\"LBNL\"" >> _temp.conf
echo "product=\"Marble\"" >> _temp.conf
echo "serial=\"$snum\"" >> _temp.conf
echo "self_powered=false" >> _temp.conf
echo "remote_wakeup=false" >> _temp.conf
echo "use_serial=true" >> _temp.conf
echo "eeprom_type=0x46" >> _temp.conf
ftdi_eeprom --device i:0x0403:0x6011 --flash-eeprom _temp.conf
rm _temp.conf
