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
  uint32_t *page0;
  uint32_t *page1;
  uint32_t *page2;
  uint32_t *page3;
} four_page_periph_t;

uint32_t _page0[] = {
  0x0,    0x80,   0x1e,   0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x0-0xf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x10-0x1f
  0x0,    0x2000, 0x0,    0x0,    0x8000, 0x219a, 0x1e66, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x20-0x2f
  0x0,    0x0,    0x0,    0x0,    0x0,    0xca40, 0xca33, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x30-0x3f
  0x2333, 0x80,   0x223d, 0x1dc3, 0x1ccd, 0x7f,   0xda20, 0x0,    0x0,    0x0,    0xd340, 0xc500, 0x0,    0x0,    0x0,    0xf200,  // 0x40-0x4f
  0xb8,   0xebe8, 0xdd80, 0xe530, 0xb8,   0xd3c0, 0x80,   0xd380, 0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x1eb8, 0x1e14,  // 0x50-0x5f
  0xba00, 0xe320, 0xf258, 0xb8,   0xba00, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x60-0x6f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x70-0x7f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x80-0x8f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x90-0x9f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xa0-0xaf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,     // 0xb0-0xbf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xc0-0xcf
  0x88,   0xf73,  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0xf320, 0xfb20, 0x0,    0x0,    0x0,     // 0xd0-0xdf
  0x1ff,  0xeb20, 0x8000, 0x8000, 0xf,    0x0,    0x5c,   0x0,    0xca80, 0xc200, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xe0-0xef
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x7,    0x4000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xf0-0xff
};
uint32_t _page1[] = {
  0x1,    0x80,   0x1e,   0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x0-0xf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x10-0x1f
  0x0,    0x399a, 0x0,    0x0,    0xffff, 0x3c7b, 0x36b9, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x20-0x2f
  0x0,    0x0,    0x0,    0x0,    0x0,    0xca40, 0xca33, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x30-0x3f
  0x3f5d, 0x80,   0x3dec, 0x3548, 0x33d7, 0x7f,   0xd200, 0x0,    0x0,    0x0,    0xcb00, 0xbd00, 0x0,    0x0,    0x0,    0xf200,  // 0x40-0x4f
  0xb8,   0xebe8, 0xdd80, 0xe530, 0xb8,   0xd3c0, 0x80,   0xd380, 0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x372f, 0x3643,  // 0x50-0x5f
  0xeb20, 0xe320, 0xf258, 0xb8,   0xba00, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x60-0x6f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x70-0x7f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x80-0x8f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x90-0x9f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xa0-0xaf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,     // 0xb0-0xbf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xc0-0xcf
  0x1088, 0xf73,  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0xf320, 0xfb20, 0x0,    0x0,    0x0,     // 0xd0-0xdf
  0x245,  0xeb20, 0x8000, 0x8000, 0xf,    0x0,    0x5c,   0x0,    0xca80, 0xc200, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xe0-0xef
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x7,    0x4000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xf0-0xff
};
uint32_t _page2[] = {
  0x2,    0x80,   0x1e,   0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x0-0xf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x10-0x1f
  0x0,    0x5000, 0x0,    0x0,    0xffff, 0x5400, 0x4c00, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x20-0x2f
  0x0,    0x0,    0x0,    0x0,    0x0,    0xca40, 0xca33, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x30-0x3f
  0x5800, 0x80,   0x563d, 0x4a3d, 0x4800, 0x7f,   0xd200, 0x0,    0x0,    0x0,    0xcb00, 0xbd00, 0x0,    0x0,    0x0,    0xf200,  // 0x40-0x4f
  0xb8,   0xebe8, 0xdd80, 0xe530, 0xb8,   0xd3c0, 0x80,   0xd380, 0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x4ce1, 0x4b1f,  // 0x50-0x5f
  0xf320, 0xe320, 0xf258, 0xb8,   0xba00, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x60-0x6f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x70-0x7f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x80-0x8f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x90-0x9f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xa0-0xaf
  0x0,    0xb48f, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,     // 0xb0-0xbf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xc0-0xcf
  0x2088, 0xf73,  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0xf320, 0xfb20, 0x0,    0x0,    0x0,     // 0xd0-0xdf
  0x218,  0xeb20, 0x8000, 0x8000, 0xf,    0x0,    0x5c,   0x0,    0xca80, 0xc200, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xe0-0xef
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x7,    0x4000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xf0-0xff
};
uint32_t _page3[] = {
  0x3,    0x80,   0x1e,   0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x0-0xf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x10-0x1f
  0x0,    0x699a, 0x0,    0x0,    0xffff, 0x6ee2, 0x6452, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x20-0x2f
  0x0,    0x0,    0x0,    0x0,    0x0,    0xca40, 0xca33, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x30-0x3f
  0x7429, 0x80,   0x71d7, 0x615d, 0x5f0b, 0x7f,   0xda20, 0x0,    0x0,    0x0,    0xd340, 0xc500, 0x0,    0x0,    0x0,    0xf200,  // 0x40-0x4f
  0xb8,   0xebe8, 0xdd80, 0xe530, 0xb8,   0xd3c0, 0x80,   0xd380, 0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x64f5, 0x63b0,  // 0x50-0x5f
  0xfa58, 0xe320, 0xf258, 0xb8,   0xba00, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x60-0x6f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x70-0x7f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x80-0x8f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0x90-0x9f
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xa0-0xaf
  0x0,    0x3322, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x8000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,     // 0xb0-0xbf
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xc0-0xcf
  0x3088, 0xf73,  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0xf320, 0xfb20, 0x0,    0x0,    0x0,     // 0xd0-0xdf
  0x1cb,  0xeb20, 0x8000, 0x8000, 0xf,    0x0,    0x5c,   0x0,    0xca80, 0xc200, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xe0-0xef
  0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x0,    0x7,    0x4000, 0x8000, 0x0,    0x0,    0x0,    0x0,    0x0,    0x0,     // 0xf0-0xff
};

static four_page_periph_t ltm4673 = {_page0, _page1, _page2, _page3};
static uint8_t ltm4673_addrs[] = {0xb8, 0xba, 0xbc, 0xbe, 0xc0, 0xc2, 0xc4, 0xc6, 0xc8};
#define LTM4673_MATCH_ADDRS     (sizeof(ltm4673_addrs)/sizeof(uint8_t))

static int i2c_emu(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
                    int cmd, uint8_t *data, int len);
static int i2c_emu_ltm4673(uint8_t rnw, int reg, uint8_t *data, int len);
static void init_sim_ltm4673_telem(void);
static void init_sim_ltm4673_config(void);

/* static int i2c_emu(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
 *                     int cmd, uint8_t *data, int len);
 *  Note that this hook function is fundamentally different from that of
 *  marble_board.c because this is actually emulating the behavior of the
 *  I2C hardware (which is why the 'const' qualifier is absent from data).
 *  For reads (rnw=1) the value of 'data' will not be read but may be
 *  changed, so a pointer to uninitialized memory is valid. For writes 
 *  (rnw=0), data is only read, so behaves as if qualified with 'const'.
 */
static int i2c_emu(I2C_BUS I2C_bus, uint8_t addr, uint8_t rnw,
                    int cmd, uint8_t *data, int len)
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
  if (!rnw) {
    ltm4673_hook_write(addr, cmd, data, len);
  }
  // Device emulator hooks
  return i2c_emu_ltm4673(rnw, cmd, data, len);
}

static int i2c_emu_ltm4673(uint8_t rnw, int reg, uint8_t *data, int len) {
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
    //printf("Reading from page %d, reg 0x%x = 0x%x\r\n", page, reg, regval);
    for (int n = 0; n < max_rw; n++) {
      // LSB-to-MSB
      data[n] = (regval >> 8*n) & 0xff;
    }
  } else {
    if (reg < 0) {
      // Treat first datum as nreg
      reg = (int)data[0];
      offset = 1;
    } else if (reg == -2) {
      // Treat first two data as nreg (LSB, MSB)
      reg = (int)(data[0] + (data[1]<<8));
      offset = 2;
    }
    for (int n = offset; n < max_rw; n++) {
      // LSB-to-MSB
      regval |= (data[n] << 8*(n-offset));
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
  return i2c_emu(I2C_bus, addr, 0, -1, (uint8_t *)data, size);
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, const uint8_t *data, int size) {
  return i2c_emu(I2C_bus, addr, 0, cmd, (uint8_t *)data, size);
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
  return i2c_emu(I2C_bus, addr, 1, 0, data, size);
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  return i2c_emu(I2C_bus, addr, 1, cmd, data, size);
}

int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, const uint8_t *data, int size) {
  return i2c_emu(I2C_bus, addr, 0, cmd, (uint8_t *)data, size);
}

int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  return i2c_emu(I2C_bus, addr, 1, cmd, data, size);
}

int getI2CBusStatus(void) {
  return 0;
}

void resetI2CBusStatus(void) {
  return;
}

void init_sim_ltm4673(void) {
  if (0) init_sim_ltm4673_config();
  init_sim_ltm4673_telem();
  return;
}

static void init_sim_ltm4673_telem(void) {
  ltm4673.page0[LTM4673_READ_VIN] = 0xd33c;
  ltm4673.page0[LTM4673_READ_IIN] = 0xaa48;
  ltm4673.page0[LTM4673_READ_PIN] = 0xc3b0;
  ltm4673.page0[LTM4673_READ_VOUT] = 0x2001;
  ltm4673.page0[LTM4673_READ_IOUT] = 0x9b54;
  ltm4673.page0[LTM4673_READ_TEMPERATURE_1] = 0xe20d;
  ltm4673.page0[LTM4673_READ_TEMPERATURE_2] = 0xdbe8;
  ltm4673.page0[LTM4673_READ_POUT] = 0x9b3c;
  ltm4673.page0[LTM4673_MFR_READ_IOUT] = 0x28;
  ltm4673.page0[LTM4673_MFR_IIN_PEAK] = 0xab73;
  ltm4673.page0[LTM4673_MFR_IIN_MIN] = 0x9313;
  ltm4673.page0[LTM4673_MFR_PIN_PEAK] = 0xcac2;
  ltm4673.page0[LTM4673_MFR_PIN_MIN] = 0xb289;
  ltm4673.page0[LTM4673_MFR_IOUT_SENSE_VOLTAGE] = 0x61;
  ltm4673.page0[LTM4673_MFR_VIN_PEAK] = 0xd34c;
  ltm4673.page0[LTM4673_MFR_VOUT_PEAK] = 0x2003;
  ltm4673.page0[LTM4673_MFR_IOUT_PEAK] = 0x9b7d;
  ltm4673.page0[LTM4673_MFR_TEMPERATURE_1_PEAK] = 0xe210;
  ltm4673.page0[LTM4673_MFR_VIN_MIN] = 0xd332;
  ltm4673.page0[LTM4673_MFR_VOUT_MIN] = 0x1fdf;
  ltm4673.page0[LTM4673_MFR_IOUT_MIN] = 0x931f;
  ltm4673.page0[LTM4673_MFR_TEMPERATURE_1_MIN] = 0xdb45;
  ltm4673.page1[LTM4673_READ_VIN] = 0xd33c;
  ltm4673.page1[LTM4673_READ_IIN] = 0xaa4b;
  ltm4673.page1[LTM4673_READ_PIN] = 0xc3b8;
  ltm4673.page1[LTM4673_READ_VOUT] = 0x399a;
  ltm4673.page1[LTM4673_READ_IOUT] = 0xa27d;
  ltm4673.page1[LTM4673_READ_TEMPERATURE_1] = 0xe238;
  ltm4673.page1[LTM4673_READ_TEMPERATURE_2] = 0xdbec;
  ltm4673.page1[LTM4673_READ_POUT] = 0xaa3f;
  ltm4673.page1[LTM4673_MFR_READ_IOUT] = 0x3e;
  ltm4673.page1[LTM4673_MFR_IIN_PEAK] = 0xab73;
  ltm4673.page1[LTM4673_MFR_IIN_MIN] = 0x9313;
  ltm4673.page1[LTM4673_MFR_PIN_PEAK] = 0xcac2;
  ltm4673.page1[LTM4673_MFR_PIN_MIN] = 0xb289;
  ltm4673.page1[LTM4673_MFR_IOUT_SENSE_VOLTAGE] = 0x307;
  ltm4673.page1[LTM4673_MFR_VIN_PEAK] = 0xd34c;
  ltm4673.page1[LTM4673_MFR_VOUT_PEAK] = 0x39fb;
  ltm4673.page1[LTM4673_MFR_IOUT_PEAK] = 0xa280;
  ltm4673.page1[LTM4673_MFR_TEMPERATURE_1_PEAK] = 0xe238;
  ltm4673.page1[LTM4673_MFR_VIN_MIN] = 0xd332;
  ltm4673.page1[LTM4673_MFR_VOUT_MIN] = 0x393e;
  ltm4673.page1[LTM4673_MFR_IOUT_MIN] = 0x8082;
  ltm4673.page1[LTM4673_MFR_TEMPERATURE_1_MIN] = 0xdb57;
  ltm4673.page2[LTM4673_READ_VIN] = 0xd33c;
  ltm4673.page2[LTM4673_READ_IIN] = 0xaa52;
  ltm4673.page2[LTM4673_READ_PIN] = 0xc3c0;
  ltm4673.page2[LTM4673_READ_VOUT] = 0x5001;
  ltm4673.page2[LTM4673_READ_IOUT] = 0x9afb;
  ltm4673.page2[LTM4673_READ_TEMPERATURE_1] = 0xe240;
  ltm4673.page2[LTM4673_READ_TEMPERATURE_2] = 0xdbef;
  ltm4673.page2[LTM4673_READ_POUT] = 0xa3bd;
  ltm4673.page2[LTM4673_MFR_READ_IOUT] = 0x25;
  ltm4673.page2[LTM4673_MFR_IIN_PEAK] = 0xab73;
  ltm4673.page2[LTM4673_MFR_IIN_MIN] = 0x9313;
  ltm4673.page2[LTM4673_MFR_PIN_PEAK] = 0xcac2;
  ltm4673.page2[LTM4673_MFR_PIN_MIN] = 0xb289;
  ltm4673.page2[LTM4673_MFR_IOUT_SENSE_VOLTAGE] = 0x1cb;
  ltm4673.page2[LTM4673_MFR_VIN_PEAK] = 0xd34c;
  ltm4673.page2[LTM4673_MFR_VOUT_PEAK] = 0x5007;
  ltm4673.page2[LTM4673_MFR_IOUT_PEAK] = 0x9b01;
  ltm4673.page2[LTM4673_MFR_TEMPERATURE_1_PEAK] = 0xe245;
  ltm4673.page2[LTM4673_MFR_VIN_MIN] = 0xd332;
  ltm4673.page2[LTM4673_MFR_VOUT_MIN] = 0x4f54;
  ltm4673.page2[LTM4673_MFR_IOUT_MIN] = 0x8004;
  ltm4673.page2[LTM4673_MFR_TEMPERATURE_1_MIN] = 0xdb64;
  ltm4673.page3[LTM4673_READ_VIN] = 0xd33b;
  ltm4673.page3[LTM4673_READ_IIN] = 0xaa48;
  ltm4673.page3[LTM4673_READ_PIN] = 0xc3b4;
  ltm4673.page3[LTM4673_READ_VOUT] = 0x6998;
  ltm4673.page3[LTM4673_READ_IOUT] = 0xb27f;
  ltm4673.page3[LTM4673_READ_TEMPERATURE_1] = 0xe226;
  ltm4673.page3[LTM4673_READ_TEMPERATURE_2] = 0xdbf3;
  ltm4673.page3[LTM4673_READ_POUT] = 0xc219;
  ltm4673.page3[LTM4673_MFR_READ_IOUT] = 0xfe;
  ltm4673.page3[LTM4673_MFR_IIN_PEAK] = 0xab73;
  ltm4673.page3[LTM4673_MFR_IIN_MIN] = 0x9313;
  ltm4673.page3[LTM4673_MFR_PIN_PEAK] = 0xcac2;
  ltm4673.page3[LTM4673_MFR_PIN_MIN] = 0xb289;
  ltm4673.page3[LTM4673_MFR_IOUT_SENSE_VOLTAGE] = 0x26d;
  ltm4673.page3[LTM4673_MFR_VIN_PEAK] = 0xd34c;
  ltm4673.page3[LTM4673_MFR_VOUT_PEAK] = 0x6a1b;
  ltm4673.page3[LTM4673_MFR_IOUT_PEAK] = 0xb387;
  ltm4673.page3[LTM4673_MFR_TEMPERATURE_1_PEAK] = 0xe238;
  ltm4673.page3[LTM4673_MFR_VIN_MIN] = 0xd332;
  ltm4673.page3[LTM4673_MFR_VOUT_MIN] = 0x697c;
  ltm4673.page3[LTM4673_MFR_IOUT_MIN] = 0x8725;
  ltm4673.page3[LTM4673_MFR_TEMPERATURE_1_MIN] = 0xdb49;
  return;
}

static void init_sim_ltm4673_config(void) {
  // page 0xff
  ltm4673.page0[LTM4673_WRITE_PROTECT] = 0x00;
  ltm4673.page1[LTM4673_WRITE_PROTECT] = 0x00;
  ltm4673.page2[LTM4673_WRITE_PROTECT] = 0x00;
  ltm4673.page3[LTM4673_WRITE_PROTECT] = 0x00;
  ltm4673.page0[LTM4673_VIN_ON] = 0xca40;
  ltm4673.page1[LTM4673_VIN_ON] = 0xca40;
  ltm4673.page2[LTM4673_VIN_ON] = 0xca40;
  ltm4673.page3[LTM4673_VIN_ON] = 0xca40;
  ltm4673.page0[LTM4673_VIN_OFF] = 0xca33;
  ltm4673.page1[LTM4673_VIN_OFF] = 0xca33;
  ltm4673.page2[LTM4673_VIN_OFF] = 0xca33;
  ltm4673.page3[LTM4673_VIN_OFF] = 0xca33;
  ltm4673.page0[LTM4673_VIN_OV_FAULT_LIMIT] = 0xd3c0;
  ltm4673.page1[LTM4673_VIN_OV_FAULT_LIMIT] = 0xd3c0;
  ltm4673.page2[LTM4673_VIN_OV_FAULT_LIMIT] = 0xd3c0;
  ltm4673.page3[LTM4673_VIN_OV_FAULT_LIMIT] = 0xd3c0;
  ltm4673.page0[LTM4673_VIN_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page1[LTM4673_VIN_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page2[LTM4673_VIN_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page3[LTM4673_VIN_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page0[LTM4673_VIN_OV_WARN_LIMIT] = 0xd380;
  ltm4673.page1[LTM4673_VIN_OV_WARN_LIMIT] = 0xd380;
  ltm4673.page2[LTM4673_VIN_OV_WARN_LIMIT] = 0xd380;
  ltm4673.page3[LTM4673_VIN_OV_WARN_LIMIT] = 0xd380;
  ltm4673.page0[LTM4673_VIN_UV_WARN_LIMIT] = 0x8000;
  ltm4673.page1[LTM4673_VIN_UV_WARN_LIMIT] = 0x8000;
  ltm4673.page2[LTM4673_VIN_UV_WARN_LIMIT] = 0x8000;
  ltm4673.page3[LTM4673_VIN_UV_WARN_LIMIT] = 0x8000;
  ltm4673.page0[LTM4673_VIN_UV_FAULT_LIMIT] = 0x8000;
  ltm4673.page1[LTM4673_VIN_UV_FAULT_LIMIT] = 0x8000;
  ltm4673.page2[LTM4673_VIN_UV_FAULT_LIMIT] = 0x8000;
  ltm4673.page3[LTM4673_VIN_UV_FAULT_LIMIT] = 0x8000;
  ltm4673.page0[LTM4673_VIN_UV_FAULT_RESPONSE] = 0x00;
  ltm4673.page1[LTM4673_VIN_UV_FAULT_RESPONSE] = 0x00;
  ltm4673.page2[LTM4673_VIN_UV_FAULT_RESPONSE] = 0x00;
  ltm4673.page3[LTM4673_VIN_UV_FAULT_RESPONSE] = 0x00;
  ltm4673.page0[LTM4673_USER_DATA_00] = 0x00;
  ltm4673.page1[LTM4673_USER_DATA_00] = 0x00;
  ltm4673.page2[LTM4673_USER_DATA_00] = 0x00;
  ltm4673.page3[LTM4673_USER_DATA_00] = 0x00;
  ltm4673.page0[LTM4673_USER_DATA_02] = 0x00;
  ltm4673.page1[LTM4673_USER_DATA_02] = 0x00;
  ltm4673.page2[LTM4673_USER_DATA_02] = 0x00;
  ltm4673.page3[LTM4673_USER_DATA_02] = 0x00;
  ltm4673.page0[LTM4673_USER_DATA_04] = 0x00;
  ltm4673.page1[LTM4673_USER_DATA_04] = 0x00;
  ltm4673.page2[LTM4673_USER_DATA_04] = 0x00;
  ltm4673.page3[LTM4673_USER_DATA_04] = 0x00;
  ltm4673.page0[LTM4673_MFR_EIN_CONFIG] = 0x00;
  ltm4673.page1[LTM4673_MFR_EIN_CONFIG] = 0x00;
  ltm4673.page2[LTM4673_MFR_EIN_CONFIG] = 0x00;
  ltm4673.page3[LTM4673_MFR_EIN_CONFIG] = 0x00;
  ltm4673.page0[LTM4673_MFR_IIN_CAL_GAIN_TC] = 0x00;
  ltm4673.page1[LTM4673_MFR_IIN_CAL_GAIN_TC] = 0x00;
  ltm4673.page2[LTM4673_MFR_IIN_CAL_GAIN_TC] = 0x00;
  ltm4673.page3[LTM4673_MFR_IIN_CAL_GAIN_TC] = 0x00;
  ltm4673.page0[LTM4673_MFR_CONFIG_ALL_LTM4673] = 0xf73;
  ltm4673.page1[LTM4673_MFR_CONFIG_ALL_LTM4673] = 0xf73;
  ltm4673.page2[LTM4673_MFR_CONFIG_ALL_LTM4673] = 0xf73;
  ltm4673.page3[LTM4673_MFR_CONFIG_ALL_LTM4673] = 0xf73;
  ltm4673.page0[LTM4673_MFR_PWRGD_EN] = 0x00;
  ltm4673.page1[LTM4673_MFR_PWRGD_EN] = 0x00;
  ltm4673.page2[LTM4673_MFR_PWRGD_EN] = 0x00;
  ltm4673.page3[LTM4673_MFR_PWRGD_EN] = 0x00;
  ltm4673.page0[LTM4673_MFR_FAULTB0_RESPONSE] = 0x00;
  ltm4673.page1[LTM4673_MFR_FAULTB0_RESPONSE] = 0x00;
  ltm4673.page2[LTM4673_MFR_FAULTB0_RESPONSE] = 0x00;
  ltm4673.page3[LTM4673_MFR_FAULTB0_RESPONSE] = 0x00;
  ltm4673.page0[LTM4673_MFR_FAULTB1_RESPONSE] = 0x00;
  ltm4673.page1[LTM4673_MFR_FAULTB1_RESPONSE] = 0x00;
  ltm4673.page2[LTM4673_MFR_FAULTB1_RESPONSE] = 0x00;
  ltm4673.page3[LTM4673_MFR_FAULTB1_RESPONSE] = 0x00;
  ltm4673.page0[LTM4673_MFR_CONFIG2_LTM4673] = 0x00;
  ltm4673.page1[LTM4673_MFR_CONFIG2_LTM4673] = 0x00;
  ltm4673.page2[LTM4673_MFR_CONFIG2_LTM4673] = 0x00;
  ltm4673.page3[LTM4673_MFR_CONFIG2_LTM4673] = 0x00;
  ltm4673.page0[LTM4673_MFR_CONFIG3_LTM4673] = 0x00;
  ltm4673.page1[LTM4673_MFR_CONFIG3_LTM4673] = 0x00;
  ltm4673.page2[LTM4673_MFR_CONFIG3_LTM4673] = 0x00;
  ltm4673.page3[LTM4673_MFR_CONFIG3_LTM4673] = 0x00;
  ltm4673.page0[LTM4673_MFR_RETRY_DELAY] = 0xf320;
  ltm4673.page1[LTM4673_MFR_RETRY_DELAY] = 0xf320;
  ltm4673.page2[LTM4673_MFR_RETRY_DELAY] = 0xf320;
  ltm4673.page3[LTM4673_MFR_RETRY_DELAY] = 0xf320;
  ltm4673.page0[LTM4673_MFR_RESTART_DELAY] = 0xfb20;
  ltm4673.page1[LTM4673_MFR_RESTART_DELAY] = 0xfb20;
  ltm4673.page2[LTM4673_MFR_RESTART_DELAY] = 0xfb20;
  ltm4673.page3[LTM4673_MFR_RESTART_DELAY] = 0xfb20;
  ltm4673.page0[LTM4673_MFR_POWERGOOD_ASSERTION_DELAY] = 0xeb20;
  ltm4673.page1[LTM4673_MFR_POWERGOOD_ASSERTION_DELAY] = 0xeb20;
  ltm4673.page2[LTM4673_MFR_POWERGOOD_ASSERTION_DELAY] = 0xeb20;
  ltm4673.page3[LTM4673_MFR_POWERGOOD_ASSERTION_DELAY] = 0xeb20;
  ltm4673.page0[LTM4673_MFR_WATCHDOG_T_FIRST] = 0x8000;
  ltm4673.page1[LTM4673_MFR_WATCHDOG_T_FIRST] = 0x8000;
  ltm4673.page2[LTM4673_MFR_WATCHDOG_T_FIRST] = 0x8000;
  ltm4673.page3[LTM4673_MFR_WATCHDOG_T_FIRST] = 0x8000;
  ltm4673.page0[LTM4673_MFR_WATCHDOG_T] = 0x8000;
  ltm4673.page1[LTM4673_MFR_WATCHDOG_T] = 0x8000;
  ltm4673.page2[LTM4673_MFR_WATCHDOG_T] = 0x8000;
  ltm4673.page3[LTM4673_MFR_WATCHDOG_T] = 0x8000;
  ltm4673.page0[LTM4673_MFR_PAGE_FF_MASK] = 0x0f;
  ltm4673.page1[LTM4673_MFR_PAGE_FF_MASK] = 0x0f;
  ltm4673.page2[LTM4673_MFR_PAGE_FF_MASK] = 0x0f;
  ltm4673.page3[LTM4673_MFR_PAGE_FF_MASK] = 0x0f;
  ltm4673.page0[LTM4673_MFR_I2C_BASE_ADDRESS] = 0x5c;
  ltm4673.page1[LTM4673_MFR_I2C_BASE_ADDRESS] = 0x5c;
  ltm4673.page2[LTM4673_MFR_I2C_BASE_ADDRESS] = 0x5c;
  ltm4673.page3[LTM4673_MFR_I2C_BASE_ADDRESS] = 0x5c;
  ltm4673.page0[LTM4673_MFR_IIN_CAL_GAIN] = 0xca80;
  ltm4673.page1[LTM4673_MFR_IIN_CAL_GAIN] = 0xca80;
  ltm4673.page2[LTM4673_MFR_IIN_CAL_GAIN] = 0xca80;
  ltm4673.page3[LTM4673_MFR_IIN_CAL_GAIN] = 0xca80;
  ltm4673.page0[LTM4673_MFR_RETRY_COUNT] = 0x07;
  ltm4673.page1[LTM4673_MFR_RETRY_COUNT] = 0x07;
  ltm4673.page2[LTM4673_MFR_RETRY_COUNT] = 0x07;
  ltm4673.page3[LTM4673_MFR_RETRY_COUNT] = 0x07;
  // page 0x00
  ltm4673.page0[LTM4673_OPERATION] = 0x80;
  ltm4673.page0[LTM4673_ON_OFF_CONFIG] = 0x1e;
  ltm4673.page0[LTM4673_VOUT_COMMAND] = 0x2000;
  ltm4673.page0[LTM4673_VOUT_MAX] = 0x8000;
  ltm4673.page0[LTM4673_VOUT_MARGIN_HIGH] = 0x219a;
  ltm4673.page0[LTM4673_VOUT_MARGIN_LOW] = 0x1e66;
  ltm4673.page0[LTM4673_VOUT_OV_FAULT_LIMIT] = 0x2333;
  ltm4673.page0[LTM4673_VOUT_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page0[LTM4673_VOUT_OV_WARN_LIMIT] = 0x223d;
  ltm4673.page0[LTM4673_VOUT_UV_WARN_LIMIT] = 0x1dc3;
  ltm4673.page0[LTM4673_VOUT_UV_FAULT_LIMIT] = 0x1ccd;
  ltm4673.page0[LTM4673_VOUT_UV_FAULT_RESPONSE] = 0x7f;
  ltm4673.page0[LTM4673_IOUT_OC_FAULT_LIMIT] = 0xda20;
  ltm4673.page0[LTM4673_IOUT_OC_FAULT_RESPONSE] = 0x00;
  ltm4673.page0[LTM4673_IOUT_OC_WARN_LIMIT] = 0xd340;
  ltm4673.page0[LTM4673_IOUT_UC_FAULT_LIMIT] = 0xc500;
  ltm4673.page0[LTM4673_IOUT_UC_FAULT_RESPONSE] = 0x00;
  ltm4673.page0[LTM4673_OT_FAULT_LIMIT] = 0xf200;
  ltm4673.page0[LTM4673_OT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page0[LTM4673_OT_WARN_LIMIT] = 0xebe8;
  ltm4673.page0[LTM4673_UT_WARN_LIMIT] = 0xdd80;
  ltm4673.page0[LTM4673_UT_FAULT_LIMIT] = 0xe530;
  ltm4673.page0[LTM4673_UT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page0[LTM4673_POWER_GOOD_ON] = 0x1eb8;
  ltm4673.page0[LTM4673_POWER_GOOD_OFF] = 0x1e14;
  ltm4673.page0[LTM4673_TON_DELAY] = 0xba00;
  ltm4673.page0[LTM4673_TON_RISE] = 0xe320;
  ltm4673.page0[LTM4673_TON_MAX_FAULT_LIMIT] = 0xf258;
  ltm4673.page0[LTM4673_TON_MAX_FAULT_RESPONSE] = 0xb8;
  ltm4673.page0[LTM4673_TOFF_DELAY] = 0xba00;
  ltm4673.page0[LTM4673_USER_DATA_01] = 0x00;
  ltm4673.page0[LTM4673_USER_DATA_03] = 0x00;
  ltm4673.page0[LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV] = 0x8000;
  ltm4673.page0[LTM4673_MFR_IOUT_CAL_GAIN_THETA] = 0x8000;
  ltm4673.page0[LTM4673_MFR_CONFIG_LTM4673] = 0x88;
  ltm4673.page0[LTM4673_MFR_FAULTB0_PROPAGATE] = 0x00;
  ltm4673.page0[LTM4673_MFR_FAULTB1_PROPAGATE] = 0x00;
  ltm4673.page0[LTM4673_MFR_DAC] = 0x1ff;
  ltm4673.page0[LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD] = 0xc200;
  ltm4673.page0[LTM4673_MFR_IOUT_CAL_GAIN_TC] = 0x00;
  ltm4673.page0[LTM4673_MFR_TEMP_1_GAIN] = 0x4000;
  ltm4673.page0[LTM4673_MFR_TEMP_1_OFFSET] = 0x8000;
  // page 0x01
  ltm4673.page1[LTM4673_OPERATION] = 0x80;
  ltm4673.page1[LTM4673_ON_OFF_CONFIG] = 0x1e;
  ltm4673.page1[LTM4673_VOUT_COMMAND] = 0x399a;
  ltm4673.page1[LTM4673_VOUT_MAX] = 0xffff;
  ltm4673.page1[LTM4673_VOUT_MARGIN_HIGH] = 0x3c7b;
  ltm4673.page1[LTM4673_VOUT_MARGIN_LOW] = 0x36b9;
  ltm4673.page1[LTM4673_VOUT_OV_FAULT_LIMIT] = 0x3f5d;
  ltm4673.page1[LTM4673_VOUT_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page1[LTM4673_VOUT_OV_WARN_LIMIT] = 0x3dec;
  ltm4673.page1[LTM4673_VOUT_UV_WARN_LIMIT] = 0x3548;
  ltm4673.page1[LTM4673_VOUT_UV_FAULT_LIMIT] = 0x33d7;
  ltm4673.page1[LTM4673_VOUT_UV_FAULT_RESPONSE] = 0x7f;
  ltm4673.page1[LTM4673_IOUT_OC_FAULT_LIMIT] = 0xd200;
  ltm4673.page1[LTM4673_IOUT_OC_FAULT_RESPONSE] = 0x00;
  ltm4673.page1[LTM4673_IOUT_OC_WARN_LIMIT] = 0xcb00;
  ltm4673.page1[LTM4673_IOUT_UC_FAULT_LIMIT] = 0xbd00;
  ltm4673.page1[LTM4673_IOUT_UC_FAULT_RESPONSE] = 0x00;
  ltm4673.page1[LTM4673_OT_FAULT_LIMIT] = 0xf200;
  ltm4673.page1[LTM4673_OT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page1[LTM4673_OT_WARN_LIMIT] = 0xebe8;
  ltm4673.page1[LTM4673_UT_WARN_LIMIT] = 0xdd80;
  ltm4673.page1[LTM4673_UT_FAULT_LIMIT] = 0xe530;
  ltm4673.page1[LTM4673_UT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page1[LTM4673_POWER_GOOD_ON] = 0x372f;
  ltm4673.page1[LTM4673_POWER_GOOD_OFF] = 0x3643;
  ltm4673.page1[LTM4673_TON_DELAY] = 0xeb20;
  ltm4673.page1[LTM4673_TON_RISE] = 0xe320;
  ltm4673.page1[LTM4673_TON_MAX_FAULT_LIMIT] = 0xf258;
  ltm4673.page1[LTM4673_TON_MAX_FAULT_RESPONSE] = 0xb8;
  ltm4673.page1[LTM4673_TOFF_DELAY] = 0xba00;
  ltm4673.page1[LTM4673_USER_DATA_01] = 0x00;
  ltm4673.page1[LTM4673_USER_DATA_03] = 0x00;
  ltm4673.page1[LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV] = 0x8000;
  ltm4673.page1[LTM4673_MFR_IOUT_CAL_GAIN_THETA] = 0x8000;
  ltm4673.page1[LTM4673_MFR_CONFIG_LTM4673] = 0x1088;
  ltm4673.page1[LTM4673_MFR_FAULTB0_PROPAGATE] = 0x00;
  ltm4673.page1[LTM4673_MFR_FAULTB1_PROPAGATE] = 0x00;
  ltm4673.page1[LTM4673_MFR_DAC] = 0x245;
  ltm4673.page1[LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD] = 0xc200;
  ltm4673.page1[LTM4673_MFR_IOUT_CAL_GAIN_TC] = 0x00;
  ltm4673.page1[LTM4673_MFR_TEMP_1_GAIN] = 0x4000;
  ltm4673.page1[LTM4673_MFR_TEMP_1_OFFSET] = 0x8000;
  // page 0x02
  ltm4673.page2[LTM4673_OPERATION] = 0x80;
  ltm4673.page2[LTM4673_ON_OFF_CONFIG] = 0x1e;
  ltm4673.page2[LTM4673_VOUT_COMMAND] = 0x5000;
  ltm4673.page2[LTM4673_VOUT_MAX] = 0xffff;
  ltm4673.page2[LTM4673_VOUT_MARGIN_HIGH] = 0x5400;
  ltm4673.page2[LTM4673_VOUT_MARGIN_LOW] = 0x4c00;
  ltm4673.page2[LTM4673_VOUT_OV_FAULT_LIMIT] = 0x5800;
  ltm4673.page2[LTM4673_VOUT_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page2[LTM4673_VOUT_OV_WARN_LIMIT] = 0x563d;
  ltm4673.page2[LTM4673_VOUT_UV_WARN_LIMIT] = 0x4a3d;
  ltm4673.page2[LTM4673_VOUT_UV_FAULT_LIMIT] = 0x4800;
  ltm4673.page2[LTM4673_VOUT_UV_FAULT_RESPONSE] = 0x7f;
  ltm4673.page2[LTM4673_IOUT_OC_FAULT_LIMIT] = 0xd200;
  ltm4673.page2[LTM4673_IOUT_OC_FAULT_RESPONSE] = 0x00;
  ltm4673.page2[LTM4673_IOUT_OC_WARN_LIMIT] = 0xcb00;
  ltm4673.page2[LTM4673_IOUT_UC_FAULT_LIMIT] = 0xbd00;
  ltm4673.page2[LTM4673_IOUT_UC_FAULT_RESPONSE] = 0x00;
  ltm4673.page2[LTM4673_OT_FAULT_LIMIT] = 0xf200;
  ltm4673.page2[LTM4673_OT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page2[LTM4673_OT_WARN_LIMIT] = 0xebe8;
  ltm4673.page2[LTM4673_UT_WARN_LIMIT] = 0xdd80;
  ltm4673.page2[LTM4673_UT_FAULT_LIMIT] = 0xe530;
  ltm4673.page2[LTM4673_UT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page2[LTM4673_POWER_GOOD_ON] = 0x4ce1;
  ltm4673.page2[LTM4673_POWER_GOOD_OFF] = 0x4b1f;
  ltm4673.page2[LTM4673_TON_DELAY] = 0xf320;
  ltm4673.page2[LTM4673_TON_RISE] = 0xe320;
  ltm4673.page2[LTM4673_TON_MAX_FAULT_LIMIT] = 0xf258;
  ltm4673.page2[LTM4673_TON_MAX_FAULT_RESPONSE] = 0xb8;
  ltm4673.page2[LTM4673_TOFF_DELAY] = 0xba00;
  ltm4673.page2[LTM4673_USER_DATA_01] = 0xb48f;
  ltm4673.page2[LTM4673_USER_DATA_03] = 0x00;
  ltm4673.page2[LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV] = 0x8000;
  ltm4673.page2[LTM4673_MFR_IOUT_CAL_GAIN_THETA] = 0x8000;
  ltm4673.page2[LTM4673_MFR_CONFIG_LTM4673] = 0x2088;
  ltm4673.page2[LTM4673_MFR_FAULTB0_PROPAGATE] = 0x00;
  ltm4673.page2[LTM4673_MFR_FAULTB1_PROPAGATE] = 0x00;
  ltm4673.page2[LTM4673_MFR_DAC] = 0x218;
  ltm4673.page2[LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD] = 0xc200;
  ltm4673.page2[LTM4673_MFR_IOUT_CAL_GAIN_TC] = 0x00;
  ltm4673.page2[LTM4673_MFR_TEMP_1_GAIN] = 0x4000;
  ltm4673.page2[LTM4673_MFR_TEMP_1_OFFSET] = 0x8000;
  // page 0x03
  ltm4673.page3[LTM4673_OPERATION] = 0x80;
  ltm4673.page3[LTM4673_ON_OFF_CONFIG] = 0x1e;
  ltm4673.page3[LTM4673_VOUT_COMMAND] = 0x699a;
  ltm4673.page3[LTM4673_VOUT_MAX] = 0xffff;
  ltm4673.page3[LTM4673_VOUT_MARGIN_HIGH] = 0x6ee2;
  ltm4673.page3[LTM4673_VOUT_MARGIN_LOW] = 0x6452;
  ltm4673.page3[LTM4673_VOUT_OV_FAULT_LIMIT] = 0x7429;
  ltm4673.page3[LTM4673_VOUT_OV_FAULT_RESPONSE] = 0x80;
  ltm4673.page3[LTM4673_VOUT_OV_WARN_LIMIT] = 0x71d7;
  ltm4673.page3[LTM4673_VOUT_UV_WARN_LIMIT] = 0x615d;
  ltm4673.page3[LTM4673_VOUT_UV_FAULT_LIMIT] = 0x5f0b;
  ltm4673.page3[LTM4673_VOUT_UV_FAULT_RESPONSE] = 0x7f;
  ltm4673.page3[LTM4673_IOUT_OC_FAULT_LIMIT] = 0xda20;
  ltm4673.page3[LTM4673_IOUT_OC_FAULT_RESPONSE] = 0x00;
  ltm4673.page3[LTM4673_IOUT_OC_WARN_LIMIT] = 0xd340;
  ltm4673.page3[LTM4673_IOUT_UC_FAULT_LIMIT] = 0xc500;
  ltm4673.page3[LTM4673_IOUT_UC_FAULT_RESPONSE] = 0x00;
  ltm4673.page3[LTM4673_OT_FAULT_LIMIT] = 0xf200;
  ltm4673.page3[LTM4673_OT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page3[LTM4673_OT_WARN_LIMIT] = 0xebe8;
  ltm4673.page3[LTM4673_UT_WARN_LIMIT] = 0xdd80;
  ltm4673.page3[LTM4673_UT_FAULT_LIMIT] = 0xe530;
  ltm4673.page3[LTM4673_UT_FAULT_RESPONSE] = 0xb8;
  ltm4673.page3[LTM4673_POWER_GOOD_ON] = 0x64f5;
  ltm4673.page3[LTM4673_POWER_GOOD_OFF] = 0x63b0;
  ltm4673.page3[LTM4673_TON_DELAY] = 0xfa58;
  ltm4673.page3[LTM4673_TON_RISE] = 0xe320;
  ltm4673.page3[LTM4673_TON_MAX_FAULT_LIMIT] = 0xf258;
  ltm4673.page3[LTM4673_TON_MAX_FAULT_RESPONSE] = 0xb8;
  ltm4673.page3[LTM4673_TOFF_DELAY] = 0xba00;
  ltm4673.page3[LTM4673_USER_DATA_01] = 0x3322;
  ltm4673.page3[LTM4673_USER_DATA_03] = 0x00;
  ltm4673.page3[LTM4673_MFR_IOUT_CAL_GAIN_TAU_INV] = 0x8000;
  ltm4673.page3[LTM4673_MFR_IOUT_CAL_GAIN_THETA] = 0x8000;
  ltm4673.page3[LTM4673_MFR_CONFIG_LTM4673] = 0x3088;
  ltm4673.page3[LTM4673_MFR_FAULTB0_PROPAGATE] = 0x00;
  ltm4673.page3[LTM4673_MFR_FAULTB1_PROPAGATE] = 0x00;
  ltm4673.page3[LTM4673_MFR_DAC] = 0x1cb;
  ltm4673.page3[LTM4673_MFR_VOUT_DISCHARGE_THRESHOLD] = 0xc200;
  ltm4673.page3[LTM4673_MFR_IOUT_CAL_GAIN_TC] = 0x00;
  ltm4673.page3[LTM4673_MFR_TEMP_1_GAIN] = 0x4000;
  ltm4673.page3[LTM4673_MFR_TEMP_1_OFFSET] = 0x8000;
  return;
}

