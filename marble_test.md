## Mechanical Checkout
1. Install heatsinks
    1 x big, with grease or thermal pad
    2 x small, adhesive
    5 x feet on back

2. Add or check sticker, take note

3. Measure resistance in ohms at the test points (skip)

4. 12V 2A bench supply e.g., Keysight E36313A
   Plug in, see all 8 power LEDs on, record current (0.36 A)

5. Measure voltage at the test points (skip)

6. Connect cables
    USB micro from host to J10
    Segger J-Link from host to J14
    Connect ethernet between host and J4
      Configure host adapter settings

## Program and Validate
1. UART connection to FPGA (ttyUSB2)
```sh
    python3 -m serial.tools.miniterm /dev/ttyUSB2 9600
```
    leave miniterm running

2. Configure and run bringup script
    Configure paths at top of 'master.sh'
```sh
    BEDROCK_PATH=~/repos/bedrock
    MMC_PATH=~/repos/marble_mmc
    BITFILE=~/hardwareBin/marble2.c0222ba4.bit
    UDPRTX=~/bin/udprtx
```
    Run master.sh
    `./master.sh $SERIAL_NUMBER`
    Configures FTDI product ID and serial number
    Programs marble_mmc
    Assigns IP and MAC addresses based on serial number
    Verifies ping and udprtx tests

3. Confirm frequency monitor output on FPGA UART (ttyUSB2)
    channel 0 (Ethernet Rx - 125 MHz)
    channel 1 (20 MHz)
    channel 2 (SI570 - 125 MHz)
    channel 3 (unused)

4. Record various device readouts and save it to a file
    three INA219 Voltage + current, SI570 output frequency
```sh
    cd bedrock/projects/test_marble_family
    sh first_readout.sh $IP 2>&1 | tee first_readout_$IP
```

5. Set FPGA boot flash OTP bits
    see instructions in bedrock/badger/flash.md
```sh
    python3 spi_test.py --ip $IP --id
    python3 spi_test.py --ip $IP --config_init
```
    note that we don't yet have enough features implemented
    to claim unbrickability: need watchdog on MMC and FREEZE bit setting
    by FPGA when write-protect switch enabled.

6. Power cycle, and send bitfile to FPGA over USB again
    `BITFILE=$BITFILE ./mutil usb`

7. Burn bitfile into address 0
    `python3 spi_test.py --ip $IP --add 0 --program $BITFILE --force_write_enable`
    takes about 146 seconds

8. Cycle power, see FPGA DONE LED turn on after ~2 sec, and ping again




0. Set up Linux computer (presumably laptop)
    suggest Debian 11 Bullseye or Ubuntu XX or Mint 20
    git clone bedrock
       cd bedrock/badger/tests && make udprtx && cp udprtx ~/bin/
    git clone marble_mmc
    arm-none-eabi toolchain installed
    miniterm
    ftdi_eeprom, maybe fuss with udev rules/permissions?
    openocd, maybe fuss with udev rules/permissions?
    Gigabit Ethernet with route to 192.168.19.0/24
    apt-get install build-essential git gcc-arm-none-eabi python3-serial openocd
    cd bedrock/badger/tests && make udprtx && cp udprtx ~/bin/

Wishlist:
  Device DNA and XADC
  maibox –> MAX6639 temperature in C and speed in rpm
  record current and temperature at some point?
  check absolute cal of 125 MHz?  GPS or Rubidium?
  Pmod LEDs?
  DDR3 memory?  Michal did not check this.

