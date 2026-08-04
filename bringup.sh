#!/bin/sh

# Automatically saves a logfile called bringup_logfile_{serial_number} to the current directory

# Master script automating as much marble board bringup as possible
# Requires:
#   ftdi_eeprom
#   openocd
#   arm-none-eabi-gcc
#   udprtx (bedrock/badger/tests)
#		Build with:
#  			bedrock/badger/tests
# 	 		make udprtx
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
SERIAL_NUM="${1:-}"
IP="${2:-}"
TEST="${3:-}"

# Default test mode
TEST="BASIC"

# Optional 3rd arg: test=BASIC or test=FULL
if [ $# -ge 3 ] && [ -n "$3" ]; then
  case "$3" in
    test=FULL|test=full|FULL|full)   TEST="FULL" ;;
    test=BASIC|test=basic|BASIC|basic) TEST="BASIC" ;;
    *)
      echo "ERROR: Invalid test argument '$3' (expected test=BASIC or test=FULL)" >&2
      echo "Usage: $0 <SN> <IP> [test=BASIC|test=FULL]" >&2
      exit 1
      ;;
  esac
fi

ts="$(date '+%Y%m%d_%H%M%S')"
log="bringup_logfile_${SERIAL_NUM}_${ts}.log"

fifo="bringup_fifo_${SERIAL_NUM}_$$"

cleanup() {
  rm -f "$fifo" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

mkfifo "$fifo"

# Background: sanitize output -> logfile (remove carriage returns)
(tr -d '\r' < "$fifo" > "$log" ) &
filter_pid=$!

# Foreground: show live output to terminal AND feed it to FIFO
{
# Turn on exit on failure
set -e

# --- Validate script inputs: SERIAL_NUM and IP ---
# SERIAL_NUM must be exactly 4 hex digits (XXXX)
if [ -z "$SERIAL_NUM" ]; then
  echo "ERROR: SERIAL_NUM is empty" >&2
  echo "Usage: $0 <SN> <IP> [test=BASIC|test=FULL]" >&2
  exit 1
fi

case "$SERIAL_NUM" in
  [0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]) : ;;
  *)
    echo "ERROR: SERIAL_NUM must be exactly 4 hex digits (XXXX). Got: '$SERIAL_NUM'" >&2
    exit 1
    ;;
esac

# Normalize SN to uppercase for downstream tools/messages
SERIAL_NUM="$(printf "%s" "$SERIAL_NUM" | tr '[:lower:]' '[:upper:]')"

# IP must be valid IPv4 dotted-quad A.B.C.D (each 0..255)
if [ -z "$IP" ]; then
  echo "ERROR: IP is empty" >&2
  echo "Usage: $0 <SN> <IP> [test=BASIC|test=FULL]" >&2
  exit 1
fi

old_IFS=$IFS
IFS=.
set -- $IP
IFS=$old_IFS

if [ $# -ne 4 ]; then
  echo "ERROR: IP must be in IPv4 format A.B.C.D. Got: '$IP'" >&2
  exit 1
fi

for oct in "$1" "$2" "$3" "$4"; do
  case "$oct" in
    ''|*[!0-9]*)
      echo "ERROR: IP octets must be numeric 0..255. Got: '$IP'" >&2
      exit 1
      ;;
  esac
  n=$(printf '%d' "$oct")   # force base-10, works with leading zeros
  if [ "$n" -lt 0 ] || [ "$n" -gt 255 ]; then
    echo "ERROR: IP octet out of range (0..255). Got: '$IP'" >&2
    exit 1
  fi
done

# Mandatory Paths Check.
paths_complete=1
if [ -z "$BEDROCK_PATH" ]; then
  echo "Define BEDROCK_PATH environment variable"
  paths_complete=0
fi
if [ -z "$MMC_PATH" ]; then
  echo "Define MMC_PATH environment variable"
  paths_complete=0
fi
if [ -z "$BITFILE" ]; then
  echo "Define BITFILE environment variable"
  paths_complete=0
fi

if [ "$paths_complete" -eq 0 ]; then
  exit 1
fi

# Handy Params
UDPRTX=udprtx
#IP=192.168.19.$SERIAL_NUM
SCRIPTS_PATH=$MMC_PATH/scripts
FTDI_PATH=$MMC_PATH/ftdi

# Test for exist
if [ ! -e "$BEDROCK_PATH" ]; then
  echo "$BEDROCK_PATH does not exist"
  exit 1
fi

if [ ! -d "$BEDROCK_PATH" ]; then
  echo "$BEDROCK_PATH is not a directory"
  exit 1
fi

if [ ! -e "$MMC_PATH" ]; then
  echo "$MMC_PATH does not exist"
  exit 1
fi

if [ ! -d "$MMC_PATH" ]; then
  echo "$MMC_PATH is not a directory"
  exit 1
fi

if [ ! -r "$BITFILE" ]; then
  echo "$BITFILE does not exist or is not readable"
  exit 1
fi

if [ -d "$BITFILE" ]; then
  echo "$BITFILE appears to be a directory"
  exit 1
fi

if ! command -v "$UDPRTX"; then
  echo "$UDPRTX cannot be found.  Build with:"
  echo "  $ cd bedrock/badger/tests"
  echo "  $ make $UDPRTX"
  exit 1
fi

#### Marble Bringup Steps ####

# 1. Bring up MMC
echo "##################################"
echo "Starting MMC bringup script..."
if ! "$MMC_PATH/bringup_mmc.sh"; then
  echo "Could not bring up MMC. Aborting."
  exit 1
fi
echo "\033[1;32mSuccess(Task 1 of 12) – MMC Bringup\033[0m"
echo "##################################"

# 1. Program FTDI serial number if needed
echo "Checking FTDI Configuration..."
if ! "$FTDI_PATH/verifyid.sh" "$SERIAL_NUM"; then
  echo "Programming FTDI..."
  "$FTDI_PATH/prog_marble.sh" "$SERIAL_NUM" && echo "Success" || echo "Failed"
  if ! "$FTDI_PATH/verifyid.sh" "$SERIAL_NUM"; then
    echo "Could not verify FTDI configuration. Aborting."
    exit 1
  fi
fi
echo "\033[1;32mSuccess(Task 2 of 12) – FTDI Configuration\033[0m"
echo "##################################"

if [ -z "$TTY_MMC" ]; then
    case "$OSTYPE" in
      darwin*)
          # macOS: find device starting with 'usbserial' and ending with '3'
          TTY_MMC=$(ls /dev/cu.usbserial*3 2>/dev/null | head -n 1)
          if [[ -z "$TTY_MMC" ]]; then
              echo "Error: No matching USB serial device found for FMC."
              exit 1
          fi
          ;;
      *)
          TTY_MMC="/dev/ttyUSB3"
          ;;
    esac
fi
echo "Using TTY_MMC: $TTY_MMC"  #TTY_MMC=/dev/ttyUSB3

if [ -z "$TTY_FPGA" ]; then
    case "$OSTYPE" in
      darwin*)
          # macOS: find device starting with 'usbserial' and ending with '2'
          TTY_FPGA=$(ls /dev/cu.usbserial*2 2>/dev/null | head -n 1)
          if [[ -z "$TTY_FPGA" ]]; then
              echo "Error: No matching USB serial device found for FPGA."
              exit 1
        fi
        ;;
      *)
        TTY_FPGA="/dev/ttyUSB2"
        ;;
    esac
fi
echo "Using TTY_FPGA: $TTY_FPGA"  #TTY_FPGA=/dev/ttyUSB2

echo "##################################"
echo "Write SN, IP and MAC addresses to marble_mmc..."
if ! "$SCRIPTS_PATH/config_sn_ip_mac.sh" -d "$TTY_MMC" "$SERIAL_NUM" "$IP"; then
  echo "ERROR: SN/IP/MAC configuration failed (Task 3 of 12)" >&2
  exit 1
fi
echo "\033[1;32mSuccess(Task 3 of 12) – SN/IP/MAC Configuration\033[0m"
echo "##################################"
# 4. Write Si570 parameters to marble_mmc based on PCB version
echo "Write Si570 parameters to marble_mmc based on PCB version"
"$SCRIPTS_PATH/config_si57x.sh" -d "$TTY_MMC"
echo "\033[1;32mSuccess(Task 4 of 12) – Si570 Configuration\033[0m"
echo "##################################"
# 4. Load bitfile to FPGA
echo "Load bitfile to FPGA..."
cd "$BEDROCK_PATH/projects/test_marble_family"
if ! BITFILE="$BITFILE" ./mutil usb; then
  echo "Could not write bitfile to FPGA. Is USB FTDI (J10) connected? Is board powered?"
  exit 1
fi

# Sleep for a few seconds to give the FPGA time to reconfigure with new IP/MAC
echo "napping for 6 seconds.."
sleep 6

echo "##################################"
# Read 4 lines from FPGA frequency counter output
echo "Reading 4 lines from FPGA frequency counter..."
python3 "$SCRIPTS_PATH/readfromtty.py" -d "$TTY_FPGA" -b 9600 4 -m 24

echo "##################################"
# Cross check that the test packets can get _out_ of this workstation
case "$OSTYPE" in
  darwin*)
    connected=$(route get "$IP" | awk '/interface:/ {print $2}')
    ;;
  *)
    connected=$(ip route get "$IP" | grep -E "eth|enp|enx")
    ;;
esac
echo $connected

if [ -z "${connected:-}" ]; then
  echo "No wired route to $IP?"
  exit 1
fi
echo "\033[1;32mSuccess(Task 5 of 12) – FPGA Bitfile Load\033[0m"
echo "##################################"
# 5. Ping IP 3 times
if ! ping -c3 "$IP"; then
  echo "No ping response received from IP $IP"
  exit 1
else
  echo "Successfully pinged from IP $IP"
fi
echo "\033[1;32mSuccess(Task 6 of 12) – Ping Test\033[0m"
echo "##################################"
# 6. UDP Stress test
cd "$BEDROCK_PATH/badger/tests"
echo "Testing UDP with 100k packets"
if ! $UDPRTX "$IP" 100000 8; then
  echo "UDP test failed"
  exit 1
fi

echo "##################################"
echo "Testing UDP with 1M packets"
if ! $UDPRTX "$IP" 1000000 8; then
  echo "UDP test failed"
  exit 1
fi
echo "\033[1;32mSuccess(Task 7 of 12) – UDP Stress Test\033[0m"

# 8. Record various device readouts and save it to a file
# three INA219 Voltage + current, SI570 output frequency
# MAX6639 temperature in C and speed in rpm, Device DNA and XADC)
echo "##################################"
echo "Record various peripheral devices readouts using first_readout.sh"
cd "$BEDROCK_PATH/projects/test_marble_family"
sh first_readout.sh "$IP"

# Odd that the first one of these in first_readout.sh doesn't work,
# but a second one here does.  Is there a bug in spi_test?
tt=$(mktemp quick_XXXXXX)
python3 "$BEDROCK_PATH/badger/tests/spi_test.py" --ip "$IP" --udp 804 --otp --pages=1 --dump "$tt"
hexdump "$tt" | head -n 2
rm "$tt"
echo "Success (Task 7 of 7) – Peripheral Device Readouts"


echo "\033[1;32mSuccess(Task 8 of 12) – Peripheral Device Readouts\033[0m"

# 9. Set FPGA boot flash OTP bits
echo "##################################"
echo "Setting FPGA boot flash OTP bits"

cd "$BEDROCK_PATH/badger/tests/"

check_OTP_bits() {
  out="$1"
  printf '%s' "$out" | grep -q 'TBPROT 1' || return 1
  printf '%s' "$out" | grep -q 'TBPARM 1' || return 1
  return 0
}

out=$(python3 -m spi_test --ip "$IP" --id 2>&1)
echo "$out"

if check_OTP_bits "$out"; then
  echo "OTP bits have already been set. Proceeding to bit file burning..."
else
  echo "Missing TBPROT 1 and/or TBPARM 1. Running config_init..." >&2
  python3 -m spi_test --ip "$IP" --config_init

  out=$(python3 -m spi_test --ip "$IP" --id 2>&1)
  echo "$out"

  if check_OTP_bits "$out"; then
    echo "OTP bits have been set successfully. Proceeding to bit file burning..."
  else
    echo "ERROR: TBPROT 1 and TBPARM 1 not found after --config_init" >&2
    exit 1
  fi
fi
echo "\033[1;32mSuccess(Task 9 of 12) – OTP bits set\033[0m"

# 10. Burn bit file
echo "##################################"
echo "Burning projects/test_marble_family bitfile into address 0"
echo "make sure write protect switch (SW1) is off!"
python3 -m spi_test --ip $IP --program $BITFILE --force_write_enable

check_bitfile() {
  out="$1"
  printf '%s' "$out" | grep -q 'result is GOOD' || return 1
  return 0
}
out=$(python3 -m spi_test --ip $IP --program $BITFILE --verify)
echo "$out"
if check_bitfile "$out"; then
  echo "Bitfile verification PASSED"
else
  echo "bitfile verification FAILED"
  exit 1
fi

echo "\033[1;32mSuccess(Task 10 of 12) – Bit file burning\033[0m"


# 11. Ping test
echo "##################################"
echo "Ping test"
check_ping() {
  out="$1"
  printf '%s' "$out" |
    tr -d '\r' |
    grep -qiE '4 packets transmitted, 4 (packets )?received, 0(\.0)?% packet loss,'
}
#out=$(ping -c4 $IP)
out="$(ping -c4 "$IP" 2>&1)"
echo "$out"
if check_ping "$out"; then
  echo "Ping test PASSED"
else
  echo "Ping test FAILED"
  exit 1
fi
echo "\033[1;32mSuccess(Task 11 of 12) – Ping test\033[0m"

# 12. FMC I/O test
if [ "$TEST" = "FULL" ]; then
echo "##################################"
echo "FMC I/O test"
echo "Connect IAM FMC modules and press Enter to proceed..."
read -r _
cd "$BEDROCK_PATH/projects/test_marble_family"
check_FMC_IO() {
  out="$1"
  printf '%s' "$out" | grep -q 'P1L ........................................................................' || return 1
  printf '%s' "$out" | grep -q 'P2L ........................................................................' || return 1
  printf '%s' "$out" | grep -q 'P2H ................................................' || return 1
  printf '%s' "$out" | grep -q 'PASS' || return 1
  return 0
}
out="$(python3 -m fmc_test_iam -a "$IP" --plugged=12 2>&1 || true)"
echo "$out"
if check_FMC_IO "$out"; then
  echo "FMC I/O test PASSED"
else
  echo "FMC I/O test FAILED"
  exit 1
fi
 echo "\033[1;32mSuccess(Task 12 of 12) – FMC I/O test\033[0m"
else
echo "\033[1;33mSkipped(Task 12 of 12) – FMC I/O test\033[0m"
fi

# end of bringup
echo "\033[1;32mMarble bringup successful! Log saved to bringup_logfile_${SERIAL_NUM}_${ts}.log\033[0m"

exit 0
} 2>&1 | tee "$fifo"

# Wait for sanitizer to finish
wait "$filter_pid" 2>/dev/null || true
