#ifndef OLED_SPI_H
#define OLED_SPI_H

// This needs to be generically named "spi.h"

/* STM32F2 target-specific support code for generic OLED driver
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "settings.h"

void ui_board_set_gpio(_reg_patch_t reg, _gpio_patch_t index, int val);
void ui_board_spi_init(SPI_TypeDef *spi, int init, int nbits, uint32_t clk_div);
void ui_board_send_data_blocking(SPI_TypeDef *spi, uint8_t data, uint32_t timeout);
uint8_t ui_board_get_last_data(void);

// DONE - Define this to start the transmission of val and block until done
//        spi_base=IO_SPI, OLED_SPI: SPI peripheral base (OLED_SPI defined in settings.h)
//        val: value to write to the SPI PICO (MOSI) line
#define SPI_SET_DAT_BLOCK(spi_base, val)   ui_board_send_data_blocking(spi_base, val, 10000UL);

// DONE - Define this to set the state of GPIO pin 'pin' to state 'val'
//        base_addr=IO_GPIO, OLED_GPIO: GPIO peripheral base
//        reg=GPIO_OUT_REG, GPIO_OE_REG: Data output register or output enable register
//        index=IO_CSN, IO_RSTN, IO_INT, OLED_BIT_D_C, OLED_BIT_CSN
//        val: 0 or 1
#define SET_GPIO1(base_addr, reg, index, val) \
  do { \
    ui_board_set_gpio(reg, index, val); \
  } while (0)

// DONE - Define this to initialize SPI peripheral
//        spi_base_addr=OLED_SPI: SPI peripheral base (OLED_SPI defined in settings.h)
//        ss_man=1: Enables manual chip-select (can be ignored if using auto-CS)
//        ss_ctrl=1: ?
//        cpol=0: SPI clock polarity
//        cpha=0: SPI clock phase
//        lsb=0: bool LSB-first or MSB-first?
//        nbits=8: number of bits per datum
//        clk_div=OLED_SPI_CLKDIV: SPI clock division (OLED_SPI_CLKDIV defined in settings.h)
#define SPI_INIT(spi_base_addr, ss_man, ss_ctrl, cpol, cpha, lsb, nbits, clk_div) \
  ui_board_spi_init(spi_base_addr, ( \
      ((ss_man & 1)  << INIT_SS_MAN_BIT)  | \
      ((ss_ctrl & 1) << INIT_SS_CTRL_BIT) | \
      ((cpol & 1)    << INIT_CPOL_BIT)    | \
      ((cpha & 1)    << INIT_CPHA_BIT)    | \
      ((lsb & 1)     << INIT_LSB_BIT)       \
      ), nbits, clk_div);

// DONE - Define to return the last received data word
//        spi_base_addr=OLED_SPI: SPI peripheral base (OLED_SPI defined in settings.h)
//#define SPI_GET_DAT(spi_base_addr) (uint8_t)((spi_base_addr->DR) & 0xff)
#define SPI_GET_DAT(spi_base_addr) ui_board_get_last_data()

// DONE - Define this or push a change to the repo to abstract this target-dependence away
//        returns uint64_t ts
#define _picorv32_rd_cycle_64()     (F_CLK/1000)*BSP_GET_SYSTICK()

// DONE - Define these to spin wheels and wait
#define DELAY_US(val)   HAL_Delay((uint32_t)val)
#define DELAY_MS(val)   HAL_Delay((uint32_t)val)

/* ======================== Non-Blocking Driver ==============================
 * The driver as-written expects blocking behavior for simplicity.  However, the
 * nature of displays is highly asymmetric data transfer (mostly host-to-display).
 * So for writes, I could simply implement a queue which returns immediately and
 * then regularly pipes out transactions from the queue to the SPI peripheral.
 * This would allow my app to remain responsive (to e.g. power-down conditions, or
 * mailbox updates) without requiring a re-write of the driver.
 *
 * Would need to use CS_N(val) = SET_GPIO1(IO_GPIO, GPIO_OUT_REG, IO_CSN, val)
 * to mark the boundaries of transactions in the queue.
 *
 * To enable blocking reads, I need to:
 *   0. Add the written parts of the transaction as normal (to the queue)
 *   1. Block until the queue's read pointer catches up with its write pointer
 *   2. Return the data from the SPI peripheral's RDATA register
 *
 * The only time the driver reads from SPI is during encoderPoll()
 */

#ifdef __cplusplus
}
#endif

#endif // ifndef OLED_SPI_H
