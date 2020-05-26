Based on lpc-toolchain. See [README here](README-lpc-toolchain.md)

## UART connectivity ##
Due to some not-yet fully-understood board-level connections, communicating with Marble over the UART port
involves the following steps:
- Connect USB cable to micro-USB port.
- Open serial port terminal emulator (e.g. gtkterm) on `/dev/ttyUSB3` with 115200 baud rate.
- The MMC will most likely be put in reset after the previous step. To restart it:
  - Disable RTS (F8 on gtkterm)
  - Disable DTR (F7 on gtkterm)
- At this point the MMC should restart and print out a menu to the terminal emulator.
- Make selections based on the options provided.
