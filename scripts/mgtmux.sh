#! /bin/sh

# Usage: mgtmux.sh [-h | --help] [-d $TTY_MMC] [muxstate]

# muxstate can be single-byte integer in decimal (0-255) or hex (0x00-0xff) or one of
#   muxstate  MUX3  MUX2  MUX1  State:
#   ----------------------------------
#   "M0"      0     0     0     MGT4-MGT6 => FMC2; MGT7 => FMC1
#   "M1"      0     0     1     MGT4-MGT5 => FMC2; MGT6-MGT7 => FMC1
#   "M2"      0     1     0     MGT4-MGT7 => FMC2;
#   "M3"      0     1     1     MGT4-MGT5 => FMC2; MGT6 => FMC1; MGT7 => FMC2
#   "M4"      1     X     X     MGT4-MGT7 => QSFP2

ttynext=0
tty=
muxstate=
help=0
for arg in "$@"; do
  if [ $ttynext != 0 ]; then
    tty="-d $arg"
    ttynext=0
  elif [ "$arg" = "-d" ]; then
    ttynext=1
  elif [ "$arg" = "-h" ]; then
    help=1
  elif [ "$arg" = "--help" ]; then
    help=1
  else
    muxstate=$arg
  fi
done

if [ "$help" != 0 ]; then
  echo "Usage: PYTHONPATH=bedrock/badger $0 [-h] [-d \$TTY_MMC] muxstate"
  echo "  If environment variable TTY_MMC is defined, it will be used as the TTY_MMC address"
  echo "  unless overridden by '-d \$TTY_MMC' argument."
  echo "  'muxstate' can be string of one or more assignments, 1=x 2=x 3=x (where x=1,0)"
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

if [ -z "$tty" ]; then
  if [ ! -z "$TTY_MMC" ]; then
    tty="-d $TTY_MMC"
  else
    echo "Must specify TTY_MMC address either by defining environment variable 'TTY_MMC' or"
    echo "with '-i \$TTY_MMC' argument."
    exit 1
  fi;
fi;

if [ "$muxstate" = "M0" ]; then
  muxval="h 1=0 2=0 3=0"
  fmcstate="4 A"
elif [ "$muxstate" = "M1" ]; then
  muxval="h 1=1 2=0 3=0"
  fmcstate="4 A"
elif [ "$muxstate" = "M2" ]; then
  muxval="h 1=0 2=1 3=0"
  fmcstate="4 A"
elif [ "$muxstate" = "M3" ]; then
  muxval="h 1=1 2=1 3=0"
  fmcstate="4 A"
elif [ "$muxstate" = "M4" ]; then
  muxval="h 1=0 2=0 3=1"
  fmcstate="4 A"
elif [ "$muxstate" = "m0" ]; then
  muxval="h 1=0 2=0 3=0"
  fmcstate="4 a"
elif [ "$muxstate" = "m1" ]; then
  muxval="h 1=1 2=0 3=0"
  fmcstate="4 a"
elif [ "$muxstate" = "m2" ]; then
  muxval="h 1=0 2=1 3=0"
  fmcstate="4 a"
elif [ "$muxstate" = "m3" ]; then
  muxval="h 1=1 2=1 3=0"
  fmcstate="4 a"
elif [ "$muxstate" = "m4" ]; then
  muxval="h 1=0 2=0 3=1"
  fmcstate="4 a"
else
  muxval="h $muxstate"
fi

SCRIPT_DIR=$( dirname -- "$0"; )

if [ -z "$muxstate" ]; then
  # Read FMC state and MGT MUX config
  python3 $SCRIPT_DIR/load.py $tty "4?" "h?" | grep -E "FMC|MUX"
else
  # Write both FMC power and MGT MUX state
  python3 $SCRIPT_DIR/load.py $tty "$muxval" "$fmcstate"
fi

exit 0
