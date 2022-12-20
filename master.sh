#!/bin/bash

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

# ============================= NOTE! ==================================
# == This script is currently brittly dependent on the assignment of  ==
# == the ttyUSB device names.  Please ensure the following before     ==
# == executing:                                                       ==
# ==  /dev/ttyUSB0 -> JTAG to FPGA                                    ==
# ==  /dev/ttyUSB1 -> Reset control                                   ==
# ==  /dev/ttyUSB2 -> UART to/from FPGA                               ==
# ==  /dev/ttyUSB3 -> UART to/from MMC                                ==
# ======================================================================

# Paths. Modify according to your filesystem
BEDROCK_PATH=~/repos/bedrock
MMC_PATH=~/repos/marble_mmc
BITFILE=~/hardwareBin/marble2.c0222ba4.bit
UDPRTX=~/bin/udprtx

# =========== Nothing below here should need to be modified ===========

# Handy Params
SERIAL_NUM=$1
IP=192.168.19.$SERIAL_NUM

if [ $# -lt 1 ]; then
  echo "Usage: master.sh \$SERIAL_NUMBER"
  exit -1
fi

# Test for exist
if [[ ! ( -e $BEDROCK_PATH ) ]]; then
  echo "$BEDROCK_PATH does not exist"
  exit 1
fi

if [[ ! ( -d $BEDROCK_PATH ) ]]; then
  echo "$BEDROCK_PATH is not a directory"
  exit 1
fi

if [[ ! ( -e $MMC_PATH ) ]]; then
  echo "$MMC_PATH does not exist"
  exit 1
fi

if [[ ! ( -d $MMC_PATH ) ]]; then
  echo "$MMC_PATH is not a directory"
  exit 1
fi

if [[ ! ( -e $BITFILE ) ]]; then
  echo "$BITFILE does not exist"
  exit 1
fi

if [[ ! ( -f $BITFILE ) ]]; then
  echo "$BITFILE exists but is not a file (is it a directory or symlink?)"
  exit 1
fi

#### Marble Bringup Steps ####

# 1. Program FTDI serial number if needed
echo "Checking FTDI Configuration..."
$MMC_PATH/ftdi/verifyid.sh $SERIAL_NUM
if [ $? != 0 ]; then
  echo "Programming FTDI..."
  $MMC_PATH/ftdi/prog_marble.sh $SERIAL_NUM && echo "Success" || echo "Failed"
  $MMC_PATH/ftdi/verifyid.sh $SERIAL_NUM
  if [ $? != 0 ]; then
    echo "Could not verify FTDI configuration. Aborting."
    exit 1
  fi
fi

# 2. Program MMC
echo "Programming MMC"
cd $MMC_PATH
make marble_download
if [ $? != 0 ]; then
  echo "Could not program marble_mmc. Is Segger J-Link attached? Is board powered?"
  exit 1
fi

# Sleep for a few seconds to give the MMC time to boot
sleep 5

# 3. Write IP and MAC addresses to marble_mmc based on serial number
# TODO - Hard-coded /dev/ttyUSB3 needs to be handled somehow
./config.sh $SERIAL_NUM

# 4. Load bitfile to FPGA
cd $BEDROCK_PATH/projects/test_marble_family
BITFILE=$BITFILE ./mutil usb
if [ $? != 0 ]; then
  echo "Could not write bitfile to FPGA. Is USB FTDI (J10) connected? Is board powered?"
  exit 1
fi

# Sleep for a few seconds to give the FPGA time to reconfigure with new IP/MAC
sleep 2

# 5. Ping IP 3 times
ping -c3 $IP
if [ $? != 0 ]; then
  echo "No ping response received from IP $IP"
  exit 1
else
  echo "Successfully pinged from IP $IP"
fi

# 6. UDP Stress test
# TODO - Time these out?
echo "Testing UDP with 100k packets"
if [ $UDPRTX $IP 100000 8 ]; then
  echo "UDP test failed"
  exit 1
fi
echo "Testing UDP with 1M packets"
if [ $UDPRTX $IP 100000 8 ]; then
  echo "UDP test failed"
  exit 1
fi

