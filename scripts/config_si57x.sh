#!/bin/sh
# Configure marble_mmc si57x parameters based on serial number
# config_si57x.sh [-d /dev/ttyUSB3] serial_number
# Set environment variable TTY_MMC to point to whatever /dev/ttyUSBx
# the marble_mmc enumerated as.  Defaults to /dev/ttyUSB3

devnext=0
dev=
snum=
for arg in "$@"; do
  if [ $devnext != 0 ]; then
    dev=$arg
    devnext=0
  elif [ "$arg" = "-d" ]; then
    devnext=1
  else
    snum=$arg
  fi
done

SCRIPT_DIR=$( dirname -- "$0"; )

if [ -z "$snum" ]; then
  if [ -n "$IP" ]; then
    # TODO - This only extracts 2-digit numbers!!! Need to use sed to extract everything after last '.'
    snum=$(printf "%-.2s" "$IP")
    echo "Using serial number from IP: $snum"
  else
    echo "Usage: config_si57x.sh [-d /dev/ttyUSB3] serial_number"
    exit 1
  fi
fi

if [ -z "$dev" ]; then
  if [ -n "$TTY_MMC" ]; then
    dev=$TTY_MMC
  else
    dev=/dev/ttyUSB3
  fi
fi

### Temporary solution to configure Si570 parameters.
# NOTE: I2C address is hex 8-bit, frequency is decimal and the configuration
# contains the 0x40 for validity check.
if [ $snum > 0 ]; then
  if [ "$snum" -ge 53 ]; then # Marble 1.4 starts from #53
    si570_configuration_string="s AA 270000000 40"
    echo "Marble PCB rev = 1.4 - Si570 parameters = {0xAA (0x55 7-bit), 270 MHz, 0x0}"
  else # considering Marble 1.3
    si570_configuration_string="s EE 125000000 42"
    echo "Marble PCB rev = 1.3 - Si570 parameters = {0xEE (0x77 7-bit), 125 MHz, 0x1}"
  fi
fi

python3 $SCRIPT_DIR/load.py -d "$dev" "$si570_configuration_string"

exit 0
