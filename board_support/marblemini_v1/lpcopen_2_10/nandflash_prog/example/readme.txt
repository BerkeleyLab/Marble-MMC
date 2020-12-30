External NAND Flash tests

Example description
The Flash example shows how to use external NAND Flash using EMC.
The example allows you to place a small string in a sector of
the Flash and the string will remain across reset cycles. No attempt at
ECC is made in this example (which is required for data reliability for
NAND FLASH).

To use the example, connect a serial cable to the board's RS232/UART port
and start a terminal program to monitor the port. The terminal program on
the host PC should be setup for 115K8N1.

Special connection requirements
JP2: 3-4 is ON

