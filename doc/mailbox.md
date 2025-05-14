# MMC Mailbox Documentation

The MMC communicates with the FPGA via a dedicated SPI bus between the two ICs.  The MMC is the bus controller and the
FPGA is an (optional) bus responder/peripheral.  Each SPI transaction consists of 16 consecutive bits with the
first octet representing the operation and address and the second octet representing the data.  An example implementation
on the FPGA side is provided in [mmc\_mailbox.v](https://github.com/BerkeleyLab/Bedrock/blob/master/projects/test_marble_family/mmc_mailbox.v)

Many of the board status and control features implemented in the console are also available from the mailbox, such as:
- Non-volatile IP and MAC addresses sent from the MMC to the FPGA automatically in response to the FPGA's ``DONE'' interrupt.
- MGT multiplexer selection settings are periodically read from the FPGA giving the application logic the ability to get or set the mux settings.
- Board status is periodically sent to the FPGA allowing the application logic to monitor the power supply voltages, currents, temperature readings, and fan speeds.
- The watchdog hash values (see section \ref{sec:MMC:Watchdog}) are passed between the MMC and the FPGA via this mailbox interface
- When the MMC is configured to use its Pmod connector in ``LED mode'', the FPGA can set the status of each Pmod I/O pin via the mailbox interface.

## Mailbox Contents

The mailbox contents are defined by the fake JSON file [mbox.def](../inc/mbox.def) which is used to auto-generate the mailbox
update function and documentation.

The auto-generated "memory map" of the mailbox is available in several formats:
- [Human readable documentation](mailbox_map.md)
- [LEEP-Style JSON](mailbox_map.json)
- [C Syntax](mailbox_map.h)
- [Verilog Syntax](mailbox_map.vh)
