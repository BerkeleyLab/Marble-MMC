################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Src/i2c_fpga.c \
../Src/i2c_pm.c \
../Src/main.c \
../Src/marble_board.c \
../Src/stm32f2xx_hal_msp.c \
../Src/stm32f2xx_it.c \
../Src/syscalls.c \
../Src/system_stm32f2xx.c 

OBJS += \
./Src/i2c_fpga.o \
./Src/i2c_pm.o \
./Src/main.o \
./Src/marble_board.o \
./Src/stm32f2xx_hal_msp.o \
./Src/stm32f2xx_it.o \
./Src/syscalls.o \
./Src/system_stm32f2xx.o 

C_DEPS += \
./Src/i2c_fpga.d \
./Src/i2c_pm.d \
./Src/main.d \
./Src/marble_board.d \
./Src/stm32f2xx_hal_msp.d \
./Src/stm32f2xx_it.d \
./Src/syscalls.d \
./Src/system_stm32f2xx.d 


# Each subdirectory must supply rules for building sources it contributes
Src/%.o: ../Src/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: MCU GCC Compiler'
	@echo $(PWD)
	arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -mfloat-abi=soft '-D__weak=__attribute__((weak))' '-D__packed=__attribute__((__packed__))' -DUSE_HAL_DRIVER -DSTM32F207xx -I"C:/Users/micha/Desktop/Projekty/AMC FMC/Marblesoft/Inc" -I"C:/Users/micha/Desktop/Projekty/AMC FMC/Marblesoft/Drivers/STM32F2xx_HAL_Driver/Inc" -I"C:/Users/micha/Desktop/Projekty/AMC FMC/Marblesoft/Drivers/STM32F2xx_HAL_Driver/Inc/Legacy" -I"C:/Users/micha/Desktop/Projekty/AMC FMC/Marblesoft/Drivers/CMSIS/Device/ST/STM32F2xx/Include" -I"C:/Users/micha/Desktop/Projekty/AMC FMC/Marblesoft/Drivers/CMSIS/Include"  -Og -g3 -Wall -fmessage-length=0 -ffunction-sections -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


