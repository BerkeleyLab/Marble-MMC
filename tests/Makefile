# Not for production use!
# Simple demo that the files in ../src can be compiled (not linked)
# See ../Makefile for production build

# different search strategy than main build
SOURCE_DIR = filler
include ../build/sources.mk

# VARIANT_D = -DMARBLE_STM32F207 -DMARBLE_V2 -DXRP_AUTOBOOT
VARIANT_D = -DMARBLE_LPC1776 -DMARBLEM_V1

PYTHON = python3
CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m3 --std=c99 -O2 -I. -I ../inc $(VARIANT_D)
CFLAGS += -Wall -Wextra -Wundef -Wshadow -Wstrict-prototypes -Wwrite-strings -pedantic
CFLAGS += -Wmissing-prototypes
CFLAGS += -Wpointer-arith -Wcast-align -Wcast-qual -Wredundant-decls -Wunreachable-code
# Too strict for now: -Wconversion
vpath %.c ../src
vpath %.def ../inc
MKMBOX = ../scripts/mkmbox.py

# OBJS = hexrec.o i2c_fpga.o i2c_pm.o main.o phy_mdio.o mailbox.o syscalls.o
OBJS = $(subst $(SOURCE_DIR)/,,$(SOURCES:.c=.o))

all: $(OBJS) hexrec_check sip_check

mailbox.o console.o system.o: mailbox_def.h
mailbox.o: mailbox_def.c

mailbox_def.c: mbox.def
	$(PYTHON) $(MKMBOX) -d $< -o $@
mailbox_def.h: mbox.def
	$(PYTHON) $(MKMBOX) -d $< -o $@

hexrec_check:
	make -C hex

sip_check:
	make -C sip

clean:
	rm -f *.o mailbox_def.h mailbox_def.c
	make -C hex clean
	make -C sip clean
