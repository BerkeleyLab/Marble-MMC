# Scripts and Usage

[config.sh](#config.sh)

[decodembox.py](#decodembox.py)

[htools.py](#htools.py)

[id_probe.sh](#id_probe.sh)

[load.py](#load.py)

[mboxexchange.py](#mboxexchange.py)

[mgtmux.sh](#mgtmux.sh)

[mgtmux_mbox.sh](#mgtmux_mbox.sh)

[mkmbox.py](#mkmbox.py)

[readfromtty.py](#readfromtty.py)

[reset](#reset)

[testscript.txt](#testscript.txt)

## config.sh
```sh
config.sh [-d /dev/ttyUSB3] serial_number
```

## decodembox.py
Used for reading and decoding the contents of the SPI mailbox shared between the MMC
and the FPGA.  The mailbox is defined in inc/mbox.def using an almost-JSON syntax
(JSON with single-line comments).  The contents of the mailbox can be fetched from
the FPGA via LEEP protocol or read from a binary file.

Read via LEEP and decode according to inc/mbox.def
```sh
cd marble_mmc
# $IP is the IP address of the device to read via LEEP
PYTHONPATH=/path/to/leep python3 scripts/decodembox.py -i $IP -d inc/mbox.def
```

Read via LEEP and print raw data
```sh
cd marble_mmc
PYTHONPATH=/path/to/leep python3 scripts/decodembox.py -i $IP -r
```

Read via LEEP and store raw data to 'mbox.bin'
```sh
cd marble_mmc
PYTHONPATH=/path/to/leep python3 scripts/decodembox.py -i $IP -s mbox.bin
```

Read from file 'mbox.bin' and decode according to inc/mbox.def
```sh
cd marble_mmc
python3 scripts/decodembox.py -i mbox.bin -d inc/mbox.def
```

Read from file 'mbox.bin' and print raw data
```sh
cd marble_mmc
python3 scripts/decodembox.py -i mbox.bin -r
```


## htools.py
Common functions for generating C source and header files.

## id\_probe.sh
Identify the type of debug adapter probe attached via USB (currently just segger or st-link)
```sh
source ip_probe.sh
echo $DEBUG_ADAPTER
```

## load.py
Load commands to the MMC over the UART console (typically /dev/ttyUSB3).  The commands can come
from a script file (see for example 'testscript.txt') or from the command line.

Send commands from 'myCommands.txt' to default device ('/dev/ttyUSB3')
```sh
python3 load.py -f myCommands.txt
```

Do the above while explicitly specifying the device and baud rate.  Note that the baud rate
must agree with that used by the marble\_mmc firmware or else undefined behavior will occur.
```sh
python3 load.py -f myCommands.txt -d /dev/ttyUSB3 -b 115200
```

Send commands from the command line.  NOTE THE QUOTING to keep commands together!
```sh
python3 load.py -d /dev/ttyUSB3 "p 50%" "m 192.168.19.40" "n 12:55:55:0:1:22"
```

## mboxexchange.py
Perform a single read from or write to an item in a mailbox page.  This script uses the lower-
level `lbus_access.py` utility in 'bedrock/badger' rather than the LEEP protocol utility used
by `decodembox.py` since the latter only allows for reading/writing of the entire mailbox.

Read the value of entry "COUNT" from mailbox page 3.
```sh
cd marble_mmc
PYTHONPATH=/path/to/bedrock/badger python3 scripts/decodembox.py -i $IP -p 3 COUNT
```

Same as above using fully-resolved name (see inc/mailbox\_def.h)
```sh
cd marble_mmc
PYTHONPATH=/path/to/bedrock/badger python3 scripts/decodembox.py -i $IP MB3_COUNT
```

Same as above, but explicitly specifying the location of the mailbox definition file.
```sh
cd marble_mmc
PYTHONPATH=/path/to/bedrock/badger python3 scripts/decodembox.py -i $IP -d 'inc/mbox.def' MB3_COUNT
```

Write value 255 to entry "I2C\_BUS\_STATUS" from mailbox page 5.
```sh
cd marble_mmc
PYTHONPATH=/path/to/bedrock/badger python3 scripts/decodembox.py -i $IP -p 5 I2C_BUS_STATUS 255
```

## mgtmux.sh
A utility using `load.py` for reading and setting the configuration of the MGT MUX pins via the UART
console. __NOTE__: This method stores the selection in non-volatile memory.  See 'mgtmux\_mbox.sh' for
an alternate method which uses the FPGA mailbox and avoids non-volatile memory writes.

Usage: `mgtmux.sh [-h | --help] [-d $TTY_MMC] [muxstate]`

In the above, 'muxstate' can be string of one or more assignments, 1=x 2=x 3=x (where x=1,0) or one of
the following.
muxstate |MUX3 |MUX2 |MUX1 |State
---------|-----|-----|-----|-----
"M0"     |0    |0    |0    |MGT4-MGT6 => FMC2; MGT7 => FMC1
"M1"     |0    |0    |1    |MGT4-MGT5 => FMC2; MGT6-MGT7 => FMC1
"M2"     |0    |1    |0    |MGT4-MGT7 => FMC2;
"M3"     |0    |1    |1    |MGT4-MGT5 => FMC2; MGT6 => FMC1; MGT7 => FMC2
"M4"     |1    |X    |X    |MGT4-MGT7 => QSFP2
"m0"     |0    |0    |0    |Same as "M0" but FMC power disabled
"m1"     |0    |0    |1    |Same as "M1" but FMC power disabled
"m2"     |0    |1    |0    |Same as "M2" but FMC power disabled
"m3"     |0    |1    |1    |Same as "M3" but FMC power disabled
"m4"     |1    |X    |X    |Same as "M4" but FMC power disabled

Print usage:
```sh
cd marble_mmc
scripts/mgtmux.sh -h
```

Setting MGT MUX configuration to MUX1=MUX2=MUX3=0, FMC power ON.
```sh
cd marble_mmc
scripts/mgtmux.sh -d /dev/ttyUSB3 M0
```

Setting MGT MUX configuration to MUX1=MUX2=1, MUX3=0, FMC power OFF.  Using $TTY\_MMC definition
from environment rather than explicitly passing as script arg.
```sh
cd marble_mmc
TTY_MMC=/dev/ttyUSB3 scripts/mgtmux.sh m3
```

Read the current MGT MUX and FMC power configuration.
```sh
cd marble_mmc
scripts/mgtmux.sh -d /dev/ttyUSB3
```

## mgtmux\_mbox.sh
A script which functions in the same ways as 'mgtmux.sh' but uses the FPGA mailbox to set the
MGT MUX configuration. __NOTE__: This method does not store the selection in non-volatile memory.
See 'mgtmux.sh' for an alternate method which uses the MMC console and performs a non-volatile
memory write.

Usage: `PYTHONPATH=bedrock/badger mgtmux_mbox.sh [-h | --help] [-i \$IP] [muxstate]`

In the above, 'muxstate' can be single-byte integer in decimal (0-255) or hex (0x00-0xff)" or one of
the following.
muxstate |MUX3 |MUX2 |MUX1 |State
---------|-----|-----|-----|-----
"M0"     |0    |0    |0    |MGT4-MGT6 => FMC2; MGT7 => FMC1
"M1"     |0    |0    |1    |MGT4-MGT5 => FMC2; MGT6-MGT7 => FMC1
"M2"     |0    |1    |0    |MGT4-MGT7 => FMC2;
"M3"     |0    |1    |1    |MGT4-MGT5 => FMC2; MGT6 => FMC1; MGT7 => FMC2
"M4"     |1    |X    |X    |MGT4-MGT7 => QSFP2
"m0"     |0    |0    |0    |Same as "M0" but FMC power disabled
"m1"     |0    |0    |1    |Same as "M1" but FMC power disabled
"m2"     |0    |1    |0    |Same as "M2" but FMC power disabled
"m3"     |0    |1    |1    |Same as "M3" but FMC power disabled
"m4"     |1    |X    |X    |Same as "M4" but FMC power disabled

When 'muxstate' is given as a 1-byte integer in decimal or hex, the value is interpreted as a
bitfield with the following significance:
nBit |Function
-----|--------
0    |FMC power state ('1' = ON, '0' = OFF)
1    |'1' = apply FMC power state, '0' = do not change FMC power state
2    |MGTMUX pin 0 state (MUX0\_MMC on schematic)
3    |'1' = apply MGTMUX pin 0 state, '0' = do not change MGTMUX pin 0 state
4    |MGTMUX pin 1 state (MUX1\_MMC on schematic)
5    |'1' = apply MGTMUX pin 1 state, '0' = do not change MGTMUX pin 1 state
6    |MGTMUX pin 2 state (MUX2\_MMC on schematic)
7    |'1' = apply MGTMUX pin 2 state, '0' = do not change MGTMUX pin 2 state

Print usage:
```sh
cd marble_mmc
scripts/mgtmux_mbox.sh -h
```

Setting MGT MUX configuration to MUX1=MUX2=MUX3=0, FMC power ON with explicit IP
address and PYTHONPATH assignment.
```sh
cd marble_mmc
PYTHONPATH=bedrock/badger scripts/mgtmux_mbox.sh -i 192.168.10.20 M0
```

Setting MGT MUX configuration to MUX1=MUX2=1, MUX3=0, FMC power OFF.  Using $IP definition
from environment rather than explicitly passing as script arg (implied previous definition
of PYTHONPATH).
```sh
cd marble_mmc
IP=192.168.10.20 scripts/mgtmux_mbox.sh m3
```

Read the current MGT MUX and FMC power configuration with explicit PYTHONPATH assignment
and implicit (previously defined) IP address (via 'IP' environment variable).
```sh
cd marble_mmc
PYTHONPATH=bedrock/badger scripts/mgtmux_mbox.sh -d /dev/ttyUSB3
```

## mkmbox.py
Create mailbox definition C outputs (header and/or source files) based on inc/mbox.def.  This
is used during the build process to ensure the firmware keeps accurate track of the definition
of the mailbox structure.

Generate header file 'inc/mailbox\_def.h' based on 'inc/mbox.def'.  This is done automatically
in 'makefile.board'.
```sh
cd marble_mmc
python3 scripts/mkmbox.py -d inc/mbox.def -o inc/mailbox_def.h
```

Generate source file 'src/mailbox\_def.c' based on 'inc/mbox.def'.  This is done automatically
in 'makefile.board'.
```sh
cd marble_mmc
python3 scripts/mkmbox.py -d inc/mbox.def -o src/mailbox_def.c
```

Generate source and header files (same directory) for testing purposes.  The below generates
'foo/test.h' and 'foo/test.c'.
```sh
cd marble_mmc
python3 scripts/mkmbox.py -d inc/mbox.def -o foo/test
```

## readfromtty.py
A handy script to read and return N lines from a TTY device.  It supports a few additional features
like counting characters to discard any lines that are too short.

Read 5 lines from '/dev/ttyUSB2' at 9600 baud, skipping any lines shorter than 8 characters.
```sh
cd scripts
python3 readfromtty.py -d /dev/ttyUSB2 -b 9600 -m 8 5
```

## reset
A simple script to open /dev/ttyUSB1 which is the reset FTDI channel if the marble board is the
only ttyUSB attached.

## testscript.txt
See 'load.py'




