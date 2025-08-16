#!/bin/bash

set -e  # Exit on any error

LOGFILE="marble_bringup_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "$LOGFILE") 2>&1

# MMC + LTM4673 bring-up
echo "Programming MMC and LTM4673..."
./bringup_mmc.sh

# Check & set OTP bits
echo "Setting FPGA boot flash OTP bits..."
python3 -m spi_test --ip "$IP" --id

# Power cycle prompt and ping
echo "Please cycle power manually and wait ~2 sec"
read -p "Press Enter once DONE LED is on..."
ping -c4 "$IP"

# FMC I/O test
echo "FMC I/O test"
python3 -m fmc_test_iam -a "$IP" --plugged=12
