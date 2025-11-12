#!/bin/sh
# Configure marble_mmc IP and mac addresses based on serial number
# config_ip_mac.sh [-d /dev/ttyUSB3] serial_number
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
    echo "Usage: config_ip_mac.sh [-d /dev/ttyUSB3] serial_number"
    exit 1
  fi
fi

if [ -z "$dev" ]; then
  if [ -n "$TTY_MMC" ]; then
    dev="$TTY_MMC"
  else
    case "$OSTYPE" in
      darwin*)
        # macOS: find device starting with 'usbserial' and ending with '3'
        dev=$(ls /dev/cu.usbserial*3 2>/dev/null | head -n 1)
        if [[ -z "$dev" ]]; then
          echo "Error: No matching USB serial device found for FMC."
          exit 1
        fi
        ;;
      *)
        # Linux
        dev=/dev/ttyUSB3
        ;;
      esac
  fi
fi
echo "Using device: $dev"

mac=$(printf "12:55:55:0:1:%x" "$snum")
ip=$(printf "192.168.19.%s" "$snum")

echo "ip = $ip; mac = $mac; dev = $dev"
python3 $SCRIPT_DIR/load.py -d "$dev" "m $ip" "n $mac"

# readback
READBACK=$(python3 $SCRIPT_DIR/load.py -d "$dev" "6")

case "$READBACK" in
  *"$mac"*)
    case "$READBACK" in
      *"$ip"*)
        echo "Successfully wrote IP and MAC to marble_mmc"
        exit 0
        ;;
      *)
        ;;
    esac
    ;;
  *)
    ;;
esac

echo "Failed to write IP and MAC to marble_mmc"
echo "Readback: $READBACK"
exit 1

exit 0
