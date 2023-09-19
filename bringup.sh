#!/bin/sh

# TODO:
#   1. Should I support a LOGFILE environment variable or just make
#      the user pipe to a log file themselves? I.e.
#       ./bringup.sh | tee -a myLogFile

# Master script automating as much marble board bringup as possible
# Requires:
#   ftdi_eeprom
#   openocd
#   arm-none-eabi-gcc
#   udprtx (bedrock/badger/tests)
#
# Setup:
#   Perform electrical validation of marble
#   Supply +12V power (>=2A)
#   Connect USB micro-B cable to J10
#   Connect Segger J-Link Programmer to J14
#   Connect Ethernet (J4) and configure adapter
#     sudo ifconfig $ADAPTERNAME 192.168.19.10 netmask 255.255.224.0
#   Obtain or generate a bitfile from:
#     Generate:
#       cd bedrock/projects/test_marble_family
#       make marble2.bit
#     Or just get from build artifacts
#       Go to: https://gitlab.lbl.gov/hdl-libraries/bedrock
#       Navigate to CI pipeline of latest commit on master branch
#       synthesis -> marble_v2_synth -> Browse job artifacts
#       Download projects/test_marble_family/marble2.xxxxxxxx.bit
#       where 'xxxxxxxx' is the commit ID.
#     This is the file you'll reference with 'BITFILE' environment
#     variable below.
#
# Environment Config:
#   Mandatory Environment Variables:
#     BEDROCK_PATH=/path/to/bedrock
#     MMC_PATH=/path/to/marble_mmc
#     BITFILE=/path/to/bitfile.bit
#   Optional Environment Variables:
#     TTY_MMC=/dev/ttyUSB3
#     TTY_FPGA=/dev/ttyUSB2

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

if [ $# -lt 1 ]; then
  echo "Usage: bringup.sh \$SERIAL_NUMBER"
  exit 2
fi

# Mandatory Paths Check.
paths_complete=1
if [ -z $BEDROCK_PATH ]; then
  echo "Define BEDROCK_PATH environment variable"
  paths_complete=0
fi
if [ -z $MMC_PATH ]; then
  echo "Define MMC_PATH environment variable"
  paths_complete=0
fi
if [ -z $BITFILE ]; then
  echo "Define BITFILE environment variable"
  paths_complete=0
fi

if [ $paths_complete -eq 0 ]; then
  exit 1
fi

# Optional Environment Variables Check.
if [ -z $TTY_MMC ]; then
  TTY_MMC=/dev/ttyUSB3
fi
if [ -z $TTY_FPGA ]; then
  TTY_FPGA=/dev/ttyUSB2
fi

# Handy Params
UDPRTX=udprtx
SERIAL_NUM=$1
IP=192.168.19.$SERIAL_NUM
SCRIPTS_PATH=$MMC_PATH/scripts
FTDI_PATH=$MMC_PATH/ftdi

# Test for exist
if [ ! -e $BEDROCK_PATH ]; then
  echo "$BEDROCK_PATH does not exist"
  exit 1
fi

if [ ! -d $BEDROCK_PATH ]; then
  echo "$BEDROCK_PATH is not a directory"
  exit 1
fi

if [ ! -e $MMC_PATH ]; then
  echo "$MMC_PATH does not exist"
  exit 1
fi

if [ ! -d $MMC_PATH ]; then
  echo "$MMC_PATH is not a directory"
  exit 1
fi

if [ ! -r $BITFILE ]; then
  echo "$BITFILE does not exist or is not readable"
  exit 1
fi

if [ -d $BITFILE ]; then
  echo "$BITFILE appears to be a directory"
  exit 1
fi

if ! command -v $UDPRTX; then
  echo "$UDPRTX cannot be found.  Build with:"
  echo "  $ cd bedrock/badger/tests"
  echo "  $ make $UDPRTX"
  exit 1
fi

#### Marble Bringup Steps ####

# 1. Program FTDI serial number if needed
echo "Checking FTDI Configuration..."
if ! $FTDI_PATH/verifyid.sh "$SERIAL_NUM"; then
  echo "Programming FTDI..."
  $FTDI_PATH/prog_marble.sh "$SERIAL_NUM" && echo "Success" || echo "Failed"
  if ! $FTDI_PATH/verifyid.sh "$SERIAL_NUM"; then
    echo "Could not verify FTDI configuration. Aborting."
    exit 1
  fi
fi

# 2. Program MMC
echo "Programming MMC"
cd $MMC_PATH
if ! make marble_download; then
  echo "Could not program marble_mmc. Is Segger J-Link attached? Is board powered?"
  exit 1
fi

# Sleep for a few seconds to give the MMC time to boot
sleep 5

# 3. Write IP and MAC addresses to marble_mmc based on serial number
$SCRIPTS_PATH/config.sh -d "$TTY_MMC" "$SERIAL_NUM"

# 4. Load bitfile to FPGA
cd $BEDROCK_PATH/projects/test_marble_family
if ! BITFILE=$BITFILE ./mutil usb; then
  echo "Could not write bitfile to FPGA. Is USB FTDI (J10) connected? Is board powered?"
  exit 1
fi

# Sleep for a few seconds to give the FPGA time to reconfigure with new IP/MAC
sleep 3

# Read 4 lines from FPGA frequency counter output
echo "Reading 4 lines from FPGA frequency counter..."
python3 $SCRIPTS_PATH/readfromtty.py -d $TTY_FPGA -b 9600 4 -m 24 | tee -a $LOGFILE

# Cross check that the test packets can get _out_ of this workstation
if ! ip route get "$IP" | grep -E "eth|enp|enx"; then
  echo "No wired route to $IP?"
  exit 1
fi

# 5. Ping IP 3 times
if ! ping -c3 "$IP"; then
  echo "No ping response received from IP $IP"
  exit 1
else
  echo "Successfully pinged from IP $IP"
fi

# 6. UDP Stress test
echo "Testing UDP with 100k packets"
if ! time $UDPRTX "$IP" 100000 8; then
  echo "UDP test failed"
  exit 1
fi

echo "Testing UDP with 1M packets"
if ! time $UDPRTX "$IP" 1000000 8; then
  echo "UDP test failed"
  exit 1
fi

# 7. Record various device readouts and save it to a file
# three INA219 Voltage + current, SI570 output frequency
echo "Record various peripheral devices readouts"
cd $BEDROCK_PATH/projects/test_marble_family
sh first_readout.sh "$IP" 2>&1 | tee first_readout_"$IP";
if ! first_readout_"$IP"; then
  echo "Could not record various peripheral device readouts. Aborting."
  exit 1
fi

exit 0
