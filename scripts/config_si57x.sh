# Configure marble_mmc si57x parameters based on PCB revision
# config_si57x.sh [-d /dev/ttyUSB3]
# Set environment variable TTY_MMC to point to whatever /dev/ttyUSBx
# the marble_mmc enumerated as.  Defaults to /dev/ttyUSB3
# In case no argument is given, it will try to figure it out

dev=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        -d)
            dev="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

SCRIPT_DIR=$( dirname -- "$0"; )
# Auto-detect TTY if not set
if [ -z "$dev" ]; then
    if [ -n "$TTY_MMC" ]; then
        dev="$TTY_MMC"
    else
        # Try to find tty associated to mmc (first one)
        dev=$(ls -l /dev/serial/by-id/ 2>/dev/null | grep "LBNL_Marble.*if03" | sed 's/.*ttyUSB/\/dev\/ttyUSB/')
        dev=$(echo "$dev" | sed 's/ .*//')
        if [ -z "$dev" ]; then
            dev="/dev/ttyUSB3"
        fi
    fi
fi

# Query the board for PCB revision using load.py
PCB_REV=$(python3 "$SCRIPT_DIR"/load.py -d "$dev" 0 2>/dev/null | tr '\r' '\n' | grep -i 'pcb rev' | grep -oE '[0-9]+\.[0-9]+')
if [ -z "$PCB_REV" ]; then
    echo "Could not detect PCB revision via console."
    exit 1
fi
echo "Detected Marble PCB revision: $PCB_REV"

awk_result=$(awk "BEGIN {print ($PCB_REV >= 1.4) ? 1 : 0}")
# Solution to configure Si570 parameters based on marble version
# NOTE: I2C address is hex 8-bit, frequency is decimal and the configuration
# contains the 0x40 for validity check.
if [ "$awk_result" -eq 1 ]; then
    si570_configuration_string="s AA 270000000 40"
    echo "Marble PCB rev = $PCB_REV (>=1.4) - Si570 parameters = {0xAA (0x55 7-bit), 270 MHz, 0x0}"
else
    si570_configuration_string="s EE 125000000 42"
    echo "Marble PCB rev = $PCB_REV (<1.4) - Si570 parameters = {0xEE (0x77 7-bit), 125 MHz, 0x1}"
fi

python3 "$SCRIPT_DIR"/load.py -d "$dev" "$si570_configuration_string"

exit 0
