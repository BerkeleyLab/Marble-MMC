#!/bin/bash

set -e  # Exit on any error

LOGFILE="marble_bringup2_$(date +%Y%m%d_%H%M%S).log"
exec > >(tee -a "$LOGFILE") 2>&1

echo "Please cycle power manually and wait ~2 sec. Remove the FMC loopback boards."
read -p "Press Enter once DONE LED is on..."

# DDR3 test
echo "In a new terminal…….
BITFILE=bitfiles/berkeleylab_marble.bit ./mutil usb
Look at the FPGA UART console, and check if the following is present
Switching SDRAM to hardware control.
Memtest at 0x40000000 (2.0MiB)...
  Write: 0x40000000-0x40200000 2.0MiB	 
   Read: 0x40000000-0x40200000 2.0MiB	 
Memtest OK
Memspeed at 0x40000000 (Sequential, 2.0MiB)...
  Write speed: 112.6MiB/s
   Read speed: 93.6MiB/s
"

echo "Run the following command in the same terminal, to write and read 1GiB of memory
mem_test 0x40000000 0x40000000
check if it is successful and output should look like…
Memtest at 0x40000000 (1.0GiB)...
  Write: 0x40000000-0x80000000 1.0GiB 	 
   Read: 0x40000000-0x80000000 1.0GiB 	 
Memtest OK
"

read -p "Press Enter when you are ready..."

echo "Launching DDR3 test"
python3 -m serial.tools.miniterm /dev/ttyUSB3 115200

echo "DDR3 test completed."

echo "=== Bring-up Complete ==="
