Originally based on lpc-toolchain. See its [README here](README-lpc-toolchain.md).

Pro-tip: People using Debian Buster (and related Linux distributions) can
get the required ARM compilers etc. with `apt-get install gcc-arm-none-eabi`.

## UART connectivity ##
Due to an odd interaction of the Linux kernel with the FTDI UART,
as discussed in Marble-Mini
[Issue #60](https://github.com/BerkeleyLab/Marble-Mini/issues/60),
establishing communication to the MMC on the prototype (v1.0) Marble-Mini
over the UART port involves the following steps:
- Connect USB cable to micro-USB port.
- Open serial port terminal emulator (e.g. gtkterm) on `/dev/ttyUSB3` with 115200 baud rate.
- The MMC will most likely be put in reset after the previous step. To restart it:
  - Disable RTS (F8 on gtkterm)
  - Disable DTR (F7 on gtkterm)
- At this point the MMC should restart and print out a menu to the terminal emulator.
- Make selections based on the options provided.

On newer boards, the MMC reset lines are connected to another FTDI channel,
so the concerns above can be ignored: just attach to `/dev/ttyUSB3` at
115200 baud to get the MMC console.
If you actually do want to reset the MMC, start a terminal emulator
on `/dev/ttyUSB1`.

# Marble-Mini bringup
  * Connect Lab-psu to J17
      - 12 V @ 0.5 A current limit
      - GND pin is the one closer to the barrel connector
  * Connect Segger j-link mini to J14
  * Download MMC firmware: `make download`
  * Connect to Marble-Mini USB port and enter serial terminal:
    `miniterm /dev/ttyUSB3 115200`
  * The menu should be printed on key press
  * Push `f` for `f) XRP7724 flash`
  * Push `g` for `g) XRP7724 go`
  * All power LEDs (close to J12) should be on
  * Current consumption should be around 200 mA

## Program FTDI
The FTDI works fine in its default configuration. This step just programs serial number and product name into its USB descriptors.

You need ftdi_eeprom from libfdti:

```bash
# building fdti_eeprom 2020-11-12
# from libftdi commit 45ebed37
# cloned from git://developer.intra2net.com/libftdi
# works when running out-of-tree, and different user
# Debian Buster: apt-get install libconfuse2 libconfuse-dev
cd ~/src
mkdir libftdi-git-bin
cd libftdi-git-bin
cmake ~larry/git/libftdi
make ftdi_eeprom
ls -l ftdi_eeprom/ftdi_eeprom
```

  * Enter the unique serial number in `ftdi/marble_mini.conf`
  * Make sure `lsusb -d 0403:6011` shows exactly 0 devices, then plug in the Marble-Mini, and see exactly 1 device
  * Run `ftdi/program_ftdi.sh`

## Credits

This code base was initially developed as a collaboration between Michal Gaska (WUT) and Larry Doolittle (LBNL), with important contribuitons from Sergio Paiagua, Vamsi Vytla and Michael Betz (LBNL).

## Copyright

Marble MMC v1.0 Copyright (c) 2021, The Regents of the University of
California, through Lawrence Berkeley National Laboratory (subject to
receipt of any required approvals from the U.S. Dept. of Energy) and
Creotech Instruments SA. All rights reserved.

If you have questions about your rights to use or distribute this software,
please contact Berkeley Lab's Intellectual Property Office at
IPO@lbl.gov.

NOTICE.  This Software was developed under funding from the U.S. Department
of Energy and the U.S. Government consequently retains certain rights.  As
such, the U.S. Government has been granted for itself and others acting on
its behalf a paid-up, nonexclusive, irrevocable, worldwide license in the
Software to reproduce, distribute copies to the public, prepare derivative 
works, and perform publicly and display publicly, and to permit others to do so.


