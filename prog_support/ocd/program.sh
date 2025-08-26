# A portable programming script via openocd for programming/updating the MMC
# outside of the context of a development machine

TOP="$(dirname "$(realpath "$0")")"
if [ $# -lt 1 ]; then
  echo "Usage: $(basename "$0") executable.elf"
  exit
fi

# Segger VID = 0x1366; ST VID = 0x0483
if [ -n "$( lsusb | grep 1366 )" ]; then
  OCD_CONF=stm32f207.cfg;
elif [ -n "$( lsusb | grep 0483 )" ]; then
  OCD_CONF=stlink.cfg
else
  echo "No debugger found.  Please connect a Segger J-Link or ST-Link v2 USB-to-JTAG/SWD debugger"
  exit 1
fi

ELF="$1"

if [ ! -e "$ELF" ]; then
  echo "File $ELF does not exist"
  exit 1
else
  openocd -f "$TOP/$OCD_CONF" -c "program $ELF reset exit"
fi

exit 0
