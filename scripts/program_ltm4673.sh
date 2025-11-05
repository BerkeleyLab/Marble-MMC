#!/bin/sh

# USAGE: program_ltm4673.sh [-f program_file.txt] [/dev/ttyUSB3] [-s] [-h|--help]

# Set environment variable TTY_MMC to point to whatever /dev/ttyUSBx
# the marble_mmc enumerated as.

filenext=0
doStore=0
doHelp=0
filename=
dev=
for arg in "$@"; do
  if [ $filenext != 0 ]; then
    filename=$arg
    filenext=0
  elif [ "$arg" = "-f" ]; then
    filenext=1
  elif [ "$arg" = "-s" ]; then
    doStore=1
  elif [ "$arg" = "-h" ]; then
    doHelp=1
  elif [ "$arg" = "--help" ]; then
    doHelp=1
  else
    dev=$arg
  fi
done

SCRIPT_DIR=$( dirname -- "$0"; )

if [ -z "$dev" ]; then
  if [ -n "$TTY_MMC" ]; then
    dev=$TTY_MMC
  fi
fi

if [ -z "$dev" ]; then
  doHelp=1
fi

if [ -z "$filename" ] && [ $doStore = 0 ]; then
  doHelp=1
fi

if [ $doHelp != 0 ]; then
  echo "USAGE: program_ltm4673.sh [device] [-f program_file.txt] [-s] [-h|--help]"
  echo "  -f        : Register programming list (format from LTC PMBus)"
  echo "  -s        : Store register programming to flash."
  echo "  -h|--help : Print this help and exit."
  echo "  device    : Character device file (e.g. /dev/ttyUSB3) of the MMC channel on the FTDI USB-to-UART."
  echo "            : If 'device' is not supplied, tries to use the value of the environment variable 'TTY_MMC'."
  exit 1
fi

if [ -n "$filename" ]; then
  python3 "$SCRIPT_DIR/ltm4673.py" -d "$dev" write -f "$filename"
fi

if [ $doStore != 0 ]; then
  python3 "$SCRIPT_DIR/ltm4673.py" -d "$dev" store
fi

exit 0
