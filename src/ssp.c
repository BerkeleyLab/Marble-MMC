#include "chip.h"
#include "marble1.h"

void ssp_init(void)
{
	// Checklist at beginning of UM10470 chapter 21:
	//  1. No action needed
	//  2. ??
	//  3. Done
	//  4. Postponed
	//  5. Done
	//  6. Postponed
	//
	// Choose SSP0 feature for these pins
	LPC_IOCON->p[0][15] = 2;  // FPGA_SCK
	LPC_IOCON->p[0][16] = 2;  // FPGA_SSEL
	LPC_IOCON->p[0][17] = 2;  // FPGA_MISO
	LPC_IOCON->p[0][18] = 2;  // FPGA_MOSI

	// Configure SSP0 using definitions from lpc-toolchain
	LPC_SSP0->CR1 = 0; // disable
	// Use FRF=0, CPOL=1, CPHA=0, to get frame format of Fig 104.
	// SCR=1 for two (prescaled) clocks per bit.
	// Slave should latch MOSI on falling edge of SCLK.
	LPC_SSP0->CR0 = 0x0000014F;  // 16-bit SPI, see table 483
	LPC_SSP0->CPSR = 2;  // clock prescale, minimum allowed
	// Compute one bit time should be 4 PCLK periods, measure 1335 ns.
	LPC_SSP0->CR1 = 2;  // enable
}

// Crude attempt to write a single SPI register,
// just using polling for now to keep things simple.
static void ssp_push(int data) {
	// Spin while Tx FIFO is full
        while ((LPC_SSP0->SR & 0x2) == 0) {}
        LPC_SSP0->DR = data;  // push data into Tx FIFO
}

static void ssp_complete(void) {
	// Spin while Tx FIFO is not empty
        while ((LPC_SSP0->SR & 0x1) == 0) {}
}

// Connection to bedrock/badger/tests/spi_gate.v
void push_fpga_mac_ip(unsigned char data[10])
{
	static unsigned int test_only = 0;  // use 0x4000 to disable
	ssp_push(0x2000 | test_only);  // disable FPGA Ethernet
	for (unsigned ix=0; ix<10; ix++) {
	        ssp_push(data[ix] | (ix<<8) | 0x1000 | test_only);
	}
	ssp_push(0x2001 | test_only);  // enable FPGA Ethernet
	ssp_complete();
}
