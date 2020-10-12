CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m3 --std=c99 -I ../Inc -I ../Drivers/STM32F2xx_HAL_Driver/Inc -I ../Drivers/CMSIS/Device/ST/STM32F2xx/Include -I ../Drivers/CMSIS/Include -DSTM32F207xx

OBJS = i2c_fpga.o i2c_pm.o main.o marble_board.o stm32f2xx_hal_msp.o stm32f2xx_it.o syscalls.o system_stm32f2xx.o

all: $(OBJS)  # temporary, should be main

main: $(OBJS)

clean:
	rm -f main *.o
