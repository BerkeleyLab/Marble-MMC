# Not for production use!
# Simple demo that the files in ../src can be compiled (not linked)
# See ../Makefile for production build

CC = arm-none-eabi-gcc
# XXX add -Wall, and fix warnings
CFLAGS = -mcpu=cortex-m3 --std=c99 -I. -I ../inc -DMARBLE_LPC1776 -DMARBLE_V2
vpath %.c ../src

OBJS = i2c_fpga.o i2c_pm.o main.o phy_mdio.o ssp.o syscalls.o

all: $(OBJS) hexrec_check

hexrec_check:
	make -f hexrec.mk

clean:
	rm -f *.o
	make -f hexrec.mk clean
