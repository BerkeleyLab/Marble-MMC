# Marble MMC Pmod Use Cases

The Marble board provides two Pmod connectors attached to the FPGA and one attached to
the MMC.  That latter Pmod is not generally available to a given application because
it is controlled by the MMC which should be running "universal" (not application-specific)
firmware.  Certain discrete features have been implemented in the universal firmware
(i.e. this codebase) to provide some utility for this otherwise unused connector.

## Selecting the Pmod usage mode
Selection of the Pmod usage mode is made via the UART console and is stored as a non-volatile
parameter.  Connect to the MMC's UART at 115200 baud and use the `x` command to query or select
the current/desired usage mode.

## Usage Modes

### Disabled
The default Pmod mode is `disabled` which puts all the Pmod data pins into hi-Z input mode with
pulldown resistors.  This is a safe default for just about anything that could be attached to
the Pmod connector.

### ALS OLED UI Board (not supported)
For applications wanting a bit of visual feedback on the generic state of the Marble board, a
SPI-based interface with the custom OLED [ui\_board](https://gitlab.lbl.gov/llrf-projects/analog_psu_board/-/tree/master/ui_board)
developed and used at the ALS.  This board does not interface with the FPGA at all, but provides
common useful parameters on the display, including:
- Non-volatile IP and MAC Addresses (used by the FPGA Golden image, available for use by user images)
- Bitfile state (best guess "Golden" or "User" FPGA image)
- Voltages and currents of core internal power supplies
- FPGA core temperature
- Various temperatures measured across the board

To use:
1. Connect a 2x6 pin cable between the MMC Pmod (J16 on Marble v1.x) and the UI board's J1
connector. __WARNING__: the UI board has the pin assignment rotated 180 degrees with respect to the
Pmod standard.
2. Connect to the MMC's UART console (115200 baud, usually the last of 4 tty or COM devices).
3. Show the current Pmod usage setting with `x<CR/LF>`
4. Select the `ALS UI board` usage mode with `x 1<CR/LF>`

After the above, the UI board will be used on all subsequent power cycles - no future configuration is needed.

### Indicator LEDs (general-purpose outputs)
Another common mode of visual feedback implemented in a chassis is an array of simple LEDs indicating some
internal state.  An application can use either of the two FPGA Pmod connectors for this purpose if available.
If both Pmod connectors are already occupied, the MMC's Pmod can provide slow-updating visual feedback to a
chassis front panel via 8 general-purpose outputs on the Pmod connector.

Application control of the outputs (LEDs) is provided via the MMC's pseudo-SPI mailbox feature.  A page has
been dedicated to this purpose which is read as inputs from the FPGA to the MMC.  See the
[mailbox documentation](doc/mailbox.md) for the specific memory address.  Note that when the indicator LEDs
feature is not in use, this page is not read by the MMC during its mailbox update, minimizing the SPI transaction
length.

Each Pmod GPIO pin (1-8) is controlled by a single byte (8 bits) in the mailbox memory.  The bytes are each
interpreted as a bitmask, but with some complications due to the principle of not acting on "junk data" (i.e.
if the SPI interface is not actually talking to anything, don't act on any resulting data).  Thus, the values
`0x00` and `0xff` are both interpreted as "LED off".

For static "On" and "Off", the following values should be used:
|State   |Byte Value|
|--------|----------|
|Off     |`0x00`    |
|Off     |`0xff`    |
|On      |`0x80`    |

The LEDs can also be used in a "blinking" mode where the toggling on/off is controlled by an interrupt-driven
state machine in the MMC (no toggling required on the FPGA side).  In __blink__ mode, there are options to
control the frequency, phase, and duty cycle of the blinking.

|Bit Slice   |Function|
|------------|--------|
|`[7:5]`     |Duty    |
|`[4:3]`     |Phase   |
|`[2:0]`     |Freq    |

In Verilog syntax, the bitmask is:
```verilog
wire [7:0] bitmask = {duty[2:0], phase[1:0], freq[2:0]};
```

|Freq Bits   |Frequency|
|------------|---------|
|`0b000`     |0 Hz (static) |
|`0b001`     |0.5 Hz   |
|`0b010`     |1.0 Hz   |
|`0b011`     |2.0 Hz   |
|`0b100`     |~3.0 Hz  |
|`0b101`     |4.0 Hz   |
|`0b110`     |~6.0 Hz  |
|`0b111`     |8.0 Hz   |

|Phase Bits  |Phase   |
|------------|--------|
|`0b00`      |-90 deg |
|`0b01`      |0 deg   |
|`0b10`      |90 deg  |
|`0b11`      |180 deg |

_Note_: The absolute phase is arbitrary.  I list it this way to make the point below that for a 50% duty-cycle
waveform, shifting by 180 degrees and inverting are the same operation.

|Duty Bits   |Duty Cycle|
|------------|----------|
|`0b000`     |12.5%     |
|`0b001`     |25.0%     |
|`0b010`     |37.5%     |
|`0b011`     |50.0%     |
|`0b100`     |87.5% (12.5%, negated)|
|`0b101`     |75.0% (25.0%, negated)|
|`0b110`     |62.5% (37.5%, negated)|
|`0b111`     |50.0% (50.0%, negated - i.e. 180-degree phase shift)|

__NOTE__: Because of the degeneracy of inverting/negating a 50% duty-cycle waveform and shifting
by 180 degrees, the values `0bxxxxx000` are all mapped to "Static On" except for the special `0b00000000`
reserved value (which maps to "Static Off").  Hence, the recommended `0x80` value above for "Static On"
could also be `0xf8`, `0xf0`, `0x10`, etc.

Similarly, the selection of 50.0% duty cycle (negated), 180 deg phase, 8.0 Hz (`0xff`) maps to "Static Off".  Fortunately,
this is identical to the 50% duty cycle (not negated), 0 deg phase, 8.0 Hz (`0x6f`) waveform.

__Common Bit Patterns__ for easy reference.

|Pattern     |State     |
|------------|----------|
|`0x00`      |Static Off|
|`0xff`      |Static Off|
|`0x80`      |Static On |
|`0x10`      |Static On |
|`0x61`      |Slow blink (0.5 Hz, 50% duty)|
|`0x67`      |Fast blink (8.0 Hz, 50% duty)|

### Slow GPIO (not supported)
A future feature (not yet supported) is the possibility of using the Pmod pins not just as generic outputs
(as the indicator LED feature does) but also generic inputs or a mix of the two.
