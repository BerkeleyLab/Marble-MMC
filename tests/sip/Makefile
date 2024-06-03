vpath %.c ../../src

CFLAGS = --std=c99 -pedantic -O2 -I../../inc
CFLAGS += -DDEBUG
CFLAGS += -Wall -Wextra -Wshadow -Wundef -pedantic
CFLAGS += -Wstrict-prototypes -Wmissing-prototypes -Wwrite-strings
CFLAGS += -Wpointer-arith -Wcast-align -Wcast-qual -Wredundant-decls -Wunreachable-code
CFLAGS += -Wformat -Wformat-signedness

all: refsip_run

# printf "HGFEDCBA" | ./refsip_test sign
refsip_run: refsip_test
	./refsip_test spot

refsip_test: refsip.o

clean:
	rm -f *.o refsip_test
