#! /bin/sh

# Usage: mgtmux.sh [-h | --help] [-i $IP] [muxstate]

# muxstate can be single-byte integer in decimal (0-255) or hex (0x00-0xff) or one of
#   muxstate  MUX3  MUX2  MUX1  State:
#   ----------------------------------
#   "M0"      0     0     0     MGT4-MGT6 => FMC2; MGT7 => FMC1
#   "M1"      0     0     1     MGT4-MGT5 => FMC2; MGT6-MGT7 => FMC1
#   "M2"      0     1     0     MGT4-MGT7 => FMC2;
#   "M3"      0     1     1     MGT4-MGT5 => FMC2; MGT6 => FMC1; MGT7 => FMC2
#   "M4"      1     X     X     MGT4-MGT7 => QSFP2

ipnext=0
ip=
muxstate=
help=0
for arg in "$@"; do
  if [ $ipnext != 0 ]; then
    ip=$arg
    ipnext=0
  elif [ "$arg" = "-i" ]; then
    ipnext=1
  elif [ "$arg" = "-h" ]; then
    help=1
  elif [ "$arg" = "--help" ]; then
    help=1
  else
    muxstate=$arg
  fi
done

if [ "$help" != 0 ]; then
  echo "Usage: PYTHONPATH=bedrock/badger $0 [-h] [-i \$IP] muxstate"
  echo "  If environment variable IP is defined, it will be used as the IP address"
  echo "  unless overridden by '-i \$IP' argument."
  echo "  'muxstate' can be single-byte integer in decimal (0-255) or hex (0x00-0xff)"
  echo "  or one of the following shortcut strings:"
  echo ""
  echo "  muxstate  MUX3  MUX2  MUX1  State:"
  echo "  ----------------------------------"
  echo "  \"M0\"      0     0     0     MGT4-MGT6 => FMC2; MGT7 => FMC1"
  echo "  \"M1\"      0     0     1     MGT4-MGT5 => FMC2; MGT6-MGT7 => FMC1"
  echo "  \"M2\"      0     1     0     MGT4-MGT7 => FMC2;"
  echo "  \"M3\"      0     1     1     MGT4-MGT5 => FMC2; MGT6 => FMC1; MGT7 => FMC2"
  echo "  \"M4\"      1     X     X     MGT4-MGT7 => QSFP2"
  echo ""
  echo "  All the above muxstate shortcut strings set FMC power ON."
  echo "  If any of the above strings are used with a lower-case 'm' instead, FMC"
  echo "  power is turned off."
  exit 1
fi

if [ -z "$ip" ]; then
  if [ ! -z "$IP" ]; then
    ip=$IP
  else
    echo "Must specify IP address either by defining environment variable 'IP' or"
    echo "with '-i \$IP' argument."
    exit 1
  fi;
fi;

if [ "$muxstate" = "M0" ]; then
  muxval="0xab" # 0b10101011
elif [ "$muxstate" = "M1" ]; then
  muxval="0xaf" # 0b10101111
elif [ "$muxstate" = "M2" ]; then
  muxval="0xbb" # 0b10111011
elif [ "$muxstate" = "M3" ]; then
  muxval="0xbf" # 0b10111111
elif [ "$muxstate" = "M4" ]; then
  muxval="0xeb" # 0b11101011
elif [ "$muxstate" = "m0" ]; then
  muxval="0xaa" # 0b10101010
elif [ "$muxstate" = "m1" ]; then
  muxval="0xae" # 0b10101110
elif [ "$muxstate" = "m2" ]; then
  muxval="0xba" # 0b10111010
elif [ "$muxstate" = "m3" ]; then
  muxval="0xbe" # 0b10111110
elif [ "$muxstate" = "m4" ]; then
  muxval="0xea" # 0b11101010
else
  muxval=$muxstate
fi

SCRIPT_DIR=$( dirname -- "$0"; )

if [ -z "$PYTHONPATH" ]; then
  echo "Append path to lbus_access to PYTHONPATH"
  echo "e.g. PYTHONPATH=bedrock/badger $0 [-i \$IP] muxstate"
  exit 1
fi

if [ -z "$muxstate" ]; then
  # Read MGTMUX_ST from page 3
  python3 $SCRIPT_DIR/../mboxexchange.py -i "$ip" MB3_MGTMUX_ST
else
  # Write to FMC_MGT_CTL in page 2
  python3 $SCRIPT_DIR/../mboxexchange.py -i "$ip" MB2_FMC_MGT_CTL $muxval
fi

exit 0
