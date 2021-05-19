CFLAGS = -DSELFTEST -O2 --std=c99 -DMARBLE_V2
CFLAGS += -pedantic -Wall -Wextra -Wstrict-prototypes

vpath %.c ../../src

all: hexrec.out

hexrec.out: hexrec Marble_runtime_3A.hex
	./hexrec 433 < $(word 2, $^) > $@
	@echo PASS

clean:
	rm -f hexrec hexrec.out
