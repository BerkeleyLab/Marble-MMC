/*
 * Simulated I2C bus
 *
 */
#include "marble_api.h"
#include "i2c_pm.h"
#include "ltm4673.h"
#include <stdio.h>

I2C_BUS I2C_PM = 0;
I2C_BUS I2C_FPGA = 1;

#define NREGS     (0x100)
typedef struct {
  uint32_t page0[NREGS];
  uint32_t page1[NREGS];
  uint32_t page2[NREGS];
  uint32_t page3[NREGS];
} four_page_periph_t;

static four_page_periph_t ltm4673;
static uint8_t ltm4673_addrs[] = {0xb8, 0xba, 0xbc, 0xbe, 0xc0, 0xc2, 0xc4, 0xc6, 0xc8};
#define LTM4673_MATCH_ADDRS     (sizeof(ltm4673_addrs)/sizeof(uint8_t))

static int i2c_hook(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw, int reg,
                         const uint8_t *wdata, volatile uint8_t *rdata, int len);
static int i2c_hook_ltm4673(uint8_t rnw, int reg, const uint8_t *wdata, volatile uint8_t *rdata, int len);

static int i2c_hook(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw, int reg,
                         const uint8_t *wdata, volatile uint8_t *rdata, int len)
{
  // TODO - Can add more I2C device emulators here
  int matched = 0;
  if (I2C_bus != I2C_PM) {
    return 1;
  }
  for (unsigned int n = 0; n < LTM4673_MATCH_ADDRS; n++) {
    if (addr == ltm4673_addrs[n]) {
      matched = 1;
      break;
    }
  }
  if (!matched) {
    return 1;
  }
  // Like this one
  return i2c_hook_ltm4673(rnw, reg, wdata, rdata, len);
}

static int i2c_hook_ltm4673(uint8_t rnw, int reg, const uint8_t *wdata, volatile uint8_t *rdata, int len) {
  int max_rw = len > 4 ? 4 : len; // Limit to 32-bit for now
  int offset = 0;
  uint32_t *ppage;
  uint32_t regval = 0;
  uint8_t page = ltm4673_get_page();
  switch (page) {
    case 1:
      ppage = ltm4673.page1;
      break;
    case 2:
      ppage = ltm4673.page2;
      break;
    case 3:
      ppage = ltm4673.page3;
      break;
    default:
      ppage = ltm4673.page0;
      break;
  }
  if (rnw) {
    regval = ppage[(uint8_t)(reg & 0xff)];
    printf("Reading from page %d, reg 0x%x = 0x%x\r\n", page, reg, regval);
    for (int n = 0; n < max_rw; n++) {
      // LSB-to-MSB
      rdata[n] = (regval >> 8*n) & 0xff;
    }
  } else {
    if (reg == -1) {
      // Treat first datum as nreg
      reg = (int)wdata[0];
      offset = 1;
    } else if (reg == -2) {
      // Treat first two data as nreg (LSB, MSB)
      reg = (int)(wdata[0] + (wdata[1]<<8));
      offset = 2;
    }
    for (int n = offset; n < max_rw; n++) {
      // LSB-to-MSB
      regval |= (wdata[n] << 8*(n-offset));
    }
    printf("Writing 0x%x to page %d, reg 0x%x\r\n", regval, page, reg);
    if (page == 0xff) {
      // Write to all pages
      ltm4673.page0[(uint8_t)(reg & 0xff)] = regval;
      ltm4673.page1[(uint8_t)(reg & 0xff)] = regval;
      ltm4673.page2[(uint8_t)(reg & 0xff)] = regval;
      ltm4673.page3[(uint8_t)(reg & 0xff)] = regval;
    } else {
      // Write to just ppage
      ppage[(uint8_t)(reg & 0xff)] = regval;
    }
  }
  return 0;
}

int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
  return 0;
}

int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 0, -1, data, NULL, size);
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 0, cmd, data, NULL, size);
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 1, 0, NULL, data, size);
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 1, cmd, NULL, data, size);
}

int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 0, cmd, data, NULL, size);
}

int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  return i2c_hook(I2C_bus, addr, 1, cmd, NULL, data, size);
}

int getI2CBusStatus(void) {
  return 0;
}

void resetI2CBusStatus(void) {
  return;
}
