#!/bin/sh

# Environment Config:
#   Mandatory Environment Variables:
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

fail() {
  echo "Error: $1" >&2
  exit 1
}

# Find directory containing this script (regardless of PWD)
MMC_PATH="$(dirname "$(readlink -f "$0")")"

if [ $# -lt 0 ]; then
  echo "Usage: bringup_mmc.sh"
  exit 2
fi

# Mandatory Paths Check.
if [ -z "$LTM_SCRIPT" ]; then
  fail "Must set LTM_SCRIPT= to define register list for LTM4673"
fi

# Optional Environment Variables Check.
if [ -z "$TTY_MMC" ]; then
  TTY_MMC=/dev/ttyUSB3
fi

# Handy Params
SCRIPTS_PATH=$MMC_PATH/scripts

# Test for exist
if [ ! -e "$MMC_PATH" ]; then
  fail "MMC_PATH=$MMC_PATH does not exist"
fi

if [ ! -d "$MMC_PATH" ]; then
  fail "MMC_PATH=$MMC_PATH is not a directory"
fi

#### Marble Bringup Steps ####

# 1. Program MMC
echo "Programming MMC...."
cd "$MMC_PATH"
if ! make marble_download; then
  fail "Could not program marble_mmc. Is Segger J-Link attached? Is board powered?"
else
  echo "Successfully programmed MMC!"
fi
echo "##################################"

# Sleep for a few seconds to give the MMC time to boot
echo "napping for 5 seconds.."
sleep 5

# 2. Program LTM4673 power management chip
echo "Programming LTM4673 power management chip...."
if ! python3 "$SCRIPTS_PATH"/ltm4673.py -d "$TTY_MMC" write -f "$LTM_SCRIPT"; then
  fail "Could not program LTM4673."
else
  echo "##################################"
  python3 "$SCRIPTS_PATH"/ltm4673.py -d "$TTY_MMC" store
  echo "napping for 5 seconds.."
  sleep 5
  python3 "$SCRIPTS_PATH"/load.py -d "$TTY_MMC" "4b"
  echo "napping for 5 seconds.."
  sleep 5
  python3 "$SCRIPTS_PATH"/load.py -d "$TTY_MMC" "4B"
  echo "Successfully programmed LTM4673!"
fi

echo "bringup_mmc DONE"
exit 0
