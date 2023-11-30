#include "marble_api.h"
#include <stdio.h>

typedef enum {
   fpga_RESET = 0,
   fpga_GOLDEN,
   fpga_TARNISHED
} fpga_state_t;

/* Persistent state */
fpga_state_t fpga_bitfile_state=fpga_RESET;
uint32_t entropy;  /* want more later */

void watchdog_fpga_reset(void)
{
  fpga_bitfile_state = fpga_RESET;
}

void watchdog_fpga_done(void)
{
   switch (fpga_bitfile_state) {
     case fpga_RESET: fpga_bitfile_state = fpga_GOLDEN; break;
     case fpga_GOLDEN: fpga_bitfile_state = fpga_TARNISHED; break;
     default: ;
   }
}

void watchdog_poll(void)
{
  if (1) {
    int rnd_init_status;
    int rnd_rc = get_hw_rnd(&entropy, &rnd_init_status);
    if (rnd_rc != 0) printf("RNG fault %d\r\n", rnd_rc);
    /* printf("get_hw_rnd %d %d %lx\n", rnd_init_status, rnd_rc, rand); */
    /* Deep in STM32F2xx_HAL_Driver/Inc/stm32f2xx_hal_def.h is
    HAL_OK       = 0x00U,
    HAL_ERROR    = 0x01U,
    HAL_BUSY     = 0x02U,
    HAL_TIMEOUT  = 0x03U
    */
  }
}

void watchdog_show_state(void)
{
   printf("FPGA bitfile state: %d\r\n", fpga_bitfile_state);
   printf("Entropy: %8.8lx\r\n", entropy);
}
