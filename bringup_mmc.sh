#!/bin/sh

# Environment Config:
#   Mandatory Environment Variables:
#     MMC_PATH=/path/to/marble_mmc
#     LTM_SCRIPT=/path/to/LTM4673_reglist.txt
#   Optional Environment Variables:
#     TTY_MMC=/dev/ttyUSB3

# ============================= NOTE! ==================================
# == This script is currently brittly dependent on the assignment of  ==
# == the ttyUSB device names.  Please ensure the following before     ==
# == executing:                                                       ==
# ==  /dev/ttyUSB0 -> JTAG to FPGA                                    ==
# ==  /dev/ttyUSB1 -> Reset control                                   ==
# ==  /dev/ttyUSB2 -> UART to/from FPGA                               ==
# ==  /dev/ttyUSB3 -> UART to/from MMC                                ==
# ======================================================================

# Turn on exit on failure
set -e

if [ $# -lt 0 ]; then
  echo "Usage: bringup_mmc.sh"
  exit 2
fi

# Mandatory Paths Check.
if [ -z "$MMC_PATH" ]; then
  echo "Define MMC_PATH environment variable"
  paths_complete=0
fi

if [ -z "$LTM_SCRIPT" ]; then
  echo "Define register list for LTM4673"
  paths_complete=0
fi

if [ $paths_complete -eq 0 ]; then
  exit 1
fi

# Optional Environment Variables Check.
if [ -z "$TTY_MMC" ]; then
  TTY_MMC=/dev/ttyUSB3
fi

# Handy Params
SCRIPTS_PATH=$MMC_PATH/scripts

# Test for exist
if [ ! -e "$MMC_PATH" ]; then
  echo "$MMC_PATH does not exist"
  exit 1
fi

if [ ! -d "$MMC_PATH" ]; then
  echo "$MMC_PATH is not a directory"
  exit 1
fi

#### Marble Bringup Steps ####

# 1. Program MMC
echo "Programming MMC...."
cd "$MMC_PATH"
if ! make marble_download; then
  echo "Could not program marble_mmc. Is Segger J-Link attached? Is board powered?"
  exit 1
else
  echo "Successfully programmed MMC!"
fi
echo "##################################"

# Sleep for a few seconds to give the MMC time to boot
sleep 5

# 2. Program LTM4673 power management chip
echo "Programming LTM4673 power management chip...."
if ! python3 "$SCRIPTS_PATH"/ltm4673.py -d "$TTY_MMC" write -f "$LTM_SCRIPT"; then
  echo "Could not program LTM4673."
  exit 1
else
  echo "##################################"
  python3 "$SCRIPTS_PATH"/ltm4673.py -d "$TTY_MMC" store
  sleep 5
  python3 "$SCRIPTS_PATH"/load.py -d "$TTY_MMC" "4b"
  sleep 5
  python3 "$SCRIPTS_PATH"/load.py -d "$TTY_MMC" "4B"
  echo "Successfully programmed LTM4673!"
fi

exit 0
