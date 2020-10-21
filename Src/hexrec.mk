CFLAGS = -DSELFTEST -O2 --std=c99
CFLAGS += -pedantic -Wall -Wextra -Wstrict-prototypes

all: hexrec_check

hexrec_check: hexrec Marble_runtime_3A.hex
	./hexrec 433 < $(word 2, $^)

clean:
	rm -f hexrec
