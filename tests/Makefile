# Not for production use!
# Simple demo that the files in ../src can be compiled (not linked)
# See ../Makefile for production build

CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m3 --std=c99 -O2 -I. -I ../inc -DMARBLE_LPC1776 -DMARBLE_V2
CFLAGS += -Wall -Wextra -Wundef -Wshadow -Wstrict-prototypes -Wwrite-strings -pedantic
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith -Wcast-align -Wcast-qual -Wredundant-decls -Wunreachable-code
# Too strict for now: -Wconversion
vpath %.c ../src

OBJS = hexrec.o i2c_fpga.o i2c_pm.o main.o phy_mdio.o mailbox.o syscalls.o

all: $(OBJS) hexrec_check

hexrec_check:
	cd hex && make

clean:
	rm -f *.o
	cd hex && make clean
