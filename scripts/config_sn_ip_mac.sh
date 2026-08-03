#!/bin/sh
# Configure marble_mmc IP and mac addresses based on serial number
# Usage: config_sn_ip_mac.sh [-d /dev/ttyUSB3] serial_number IP_address
#
# serial_number must be exactly 4 hex digits (XXXX), e.g. 0123 or 4AB2

snum_raw=""
ip=""
dev=""

# --- parse args ---
while [ $# -gt 0 ]; do
  case "$1" in
    -d)
      shift
      dev="$1"
      ;;
    -h|--help)
      echo "Usage: $0 [-d /dev/ttyUSB3] serial_number IP_address"
      exit 0
      ;;
    *)
      if [ -z "$snum_raw" ]; then
        snum_raw="$1"
      elif [ -z "$ip" ]; then
        ip="$1"
      else
        echo "Error: unexpected extra argument: $1"
        exit 2
      fi
      ;;
  esac
  shift
done

if [ -z "$snum_raw" ] || [ -z "$ip" ]; then
  echo "Usage: $0 [-d /dev/ttyUSB3] serial_number IP_address"
  exit 2
fi

SCRIPT_DIR="$(dirname -- "$0")"

# --- select device if not provided ---
if [ -z "$dev" ]; then
  if [ -n "${TTY_MMC:-}" ]; then
    dev="$TTY_MMC"
  else
    case "${OSTYPE:-}" in
      darwin*)
        dev="$(ls /dev/cu.usbserial*3 2>/dev/null | head -n 1)"
        if [ -z "$dev" ]; then
          echo "Error: No matching USB serial device found."
          exit 1
        fi
        ;;
      *)
        dev="/dev/ttyUSB3"
        ;;
    esac
  fi
fi

echo "Using device: $dev"

# --- serial parsing: ONLY accept 4 hex digits ---
sn_hex="$snum_raw"
case "$sn_hex" in
  [0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f][0-9A-Fa-f]) : ;;
  *)
    echo "Invalid serial number '$snum_raw'. Expected exactly 4 hex digits, e.g. 0123 or 4AB2"
    exit 2
    ;;
esac

sn_hex_uc="$(printf "%s" "$sn_hex" | tr '[:lower:]' '[:upper:]')"

# MAC: 12:55:55:0:XX:YY where XX=SN[0..1], YY=SN[2..3]
b1="$(printf "%s" "$sn_hex" | cut -c1-2 | tr '[:lower:]' '[:upper:]')" # e.g. "01"
b2="$(printf "%s" "$sn_hex" | cut -c3-4 | tr '[:lower:]' '[:upper:]')" # e.g. "23"

# Normalize for comparison with readback (readback seems to use %x so it may drop leading zeros)
b1_norm="${b1#0}"; [ -n "$b1_norm" ] || b1_norm=0
b2_norm="${b2#0}"; [ -n "$b2_norm" ] || b2_norm=0
mac_expected_norm="12:55:55:0:${b1_norm}:${b2_norm}"

# Also print the “full” two-digit form for clarity
mac_expected_full="12:55:55:0:${b1}:${b2}"

echo "sn = $sn_hex_uc; ip = $ip; mac = $mac_expected_full"

# echo "Unlocking settings..."
# python3 "$SCRIPT_DIR/load.py" -d "$dev" "z unlock"
# sleep 1

python3 "$SCRIPT_DIR/load.py" -d "$dev" \
  "z unlock" \
  "m SN $sn_hex_uc" \
  "m IP $ip" \
  "m MAC $mac_expected_full"

READBACK="$(python3 "$SCRIPT_DIR/load.py" -d "$dev" "0" 2>&1)"

# Compare in lowercase to avoid case issues
READBACK_LC="$(printf "%s" "$READBACK" | tr '[:upper:]' '[:lower:]')"
sn_lc="$(printf "%s" "$sn_hex_uc" | tr '[:upper:]' '[:lower:]')"
ip_lc="$(printf "%s" "$ip" | tr '[:upper:]' '[:lower:]')"
mac_lc="$(printf "%s" "$mac_expected_norm" | tr '[:upper:]' '[:lower:]')"

sn_pat="sn: ${sn_lc}"
ip_pat="ip: ${ip_lc}"
mac_pat="mac: ${mac_lc}"

sn_ok=0; ip_ok=0; mac_ok=0
case "$READBACK_LC" in *"$sn_pat"*) sn_ok=1;; esac
case "$READBACK_LC" in *"$ip_pat"*) ip_ok=1;; esac
case "$READBACK_LC" in *"$mac_pat"*) mac_ok=1;; esac

if [ "$sn_ok" -eq 1 ] && [ "$ip_ok" -eq 1 ] && [ "$mac_ok" -eq 1 ]; then
  echo "Successfully wrote SN, IP and MAC to marble_mmc"
  exit 0
fi

echo "Failed to write all required settings."
[ "$sn_ok" -eq 0 ] && echo " - SN not set as expected ($sn_hex_uc)"
[ "$ip_ok" -eq 0 ] && echo " - IP not set as expected ($ip)"
[ "$mac_ok" -eq 0 ] && echo " - MAC not set as expected (readback expects ${mac_expected_norm}, full form ${mac_expected_full})"
echo "Readback: $READBACK"
exit 1
