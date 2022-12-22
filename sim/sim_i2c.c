/*
 * Simulated I2C bus
 *
 */
#include "marble_api.h"

I2C_BUS I2C_PM;
I2C_BUS I2C_FPGA;

// TODO
int marble_I2C_probe(I2C_BUS I2C_bus, uint8_t addr) {
  return 0;
}

int marble_I2C_send(I2C_BUS I2C_bus, uint8_t addr, const uint8_t *data, int size) {
  return 0;
}

int marble_I2C_cmdsend(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  return 0;
}

int marble_I2C_recv(I2C_BUS I2C_bus, uint8_t addr, uint8_t *data, int size) {
  return 0;
}

int marble_I2C_cmdrecv(I2C_BUS I2C_bus, uint8_t addr, uint8_t cmd, uint8_t *data, int size) {
  return 0;
}

int marble_I2C_cmdsend_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  return 0;
}

int marble_I2C_cmdrecv_a2(I2C_BUS I2C_bus, uint8_t addr, uint16_t cmd, uint8_t *data, int size) {
  return 0;
}


