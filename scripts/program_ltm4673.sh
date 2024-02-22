#!/bin/sh
# program_ltm4673.sh [-f program_file.txt] [/dev/ttyUSB3]
# Set environment variable TTY_MMC to point to whatever /dev/ttyUSBx
# the marble_mmc enumerated as.

filenext=0
doStore=0
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
  else
    dev=$arg
  fi
done

SCRIPT_DIR=$( dirname -- "$0"; )

if [ -z "$dev" ]; then
  if [ -n "$TTY_MMC" ]; then
    dev=$TTY_MMC
  else
    dev=/dev/ttyUSB3
  fi
fi

if [ -z "$filename" ]; then
  python3 "$SCRIPT_DIR/ltm4673.py" -d "$dev" write
else
  python3 "$SCRIPT_DIR/ltm4673.py" -d "$dev" write -f "$filename"
fi
python3 "$SCRIPT_DIR/ltm4673.py" -d "$dev" store

exit 0
