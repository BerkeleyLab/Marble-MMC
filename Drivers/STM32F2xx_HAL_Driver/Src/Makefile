# Not for production use!
# Simple demo that the files in this directory can be compiled (not linked)
# See ../../../Makefile for production build
CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m3 --std=c99 -I ../Inc -I ../../../Inc -I../../../Drivers/CMSIS/Device/ST/STM32F2xx/Include -I ../../../Drivers/CMSIS/Include -DSTM32F207xx

OBJS = stm32f2xx_hal.o stm32f2xx_hal_cortex.o stm32f2xx_hal_dma.o stm32f2xx_hal_dma_ex.o stm32f2xx_hal_eth.o stm32f2xx_hal_flash.o stm32f2xx_hal_flash_ex.o stm32f2xx_hal_gpio.o stm32f2xx_hal_i2c.o stm32f2xx_hal_pwr.o stm32f2xx_hal_pwr_ex.o stm32f2xx_hal_rcc.o stm32f2xx_hal_rcc_ex.o stm32f2xx_hal_spi.o stm32f2xx_hal_tim.o stm32f2xx_hal_tim_ex.o stm32f2xx_hal_uart.o

all: $(OBJS)

clean:
	rm -f *.o
