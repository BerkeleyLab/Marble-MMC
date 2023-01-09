# Scripts and Usage

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




