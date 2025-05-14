# Identify the debug probe used
export DEBUG_ADAPTER=$(shell ./scripts/id_probe.sh)

marble:
	make -f makefile.board BOARD=marble all

marble_download:
	make -f makefile.board BOARD=marble download

marble_clean:
	make -f makefile.board BOARD=marble clean

marble_mini:
	make -f makefile.board BOARD=marble_mini all

marble_mini_download:
	make -f makefile.board BOARD=marble_mini download

marble_mini_clean:
	make -f makefile.board BOARD=marble_mini clean

marble_gdb:
	make -f makefile.board BOARD=marble gdb

marble_reset:
	make -f makefile.board BOARD=marble reset

nucleo:
	make -f makefile.board BOARD=nucleo all

nucleo_download:
	make -f makefile.board BOARD=nucleo download

nucleo_gdb:
	make -f makefile.board BOARD=nucleo gdb

nucleo_clean:
	make -f makefile.board BOARD=nucleo clean

.PHONY: doc
doc:
	make -f doc/Makefile doc

.PHONY: sim
sim:
	make -f makefile.sim BOARD=sim all

sim_clean:
	make -f makefile.sim BOARD=sim clean

.PHONY: run
run:
	make -f makefile.sim BOARD=sim run

.PHONY: wrap
wrap:
	make -f makefile.sim BOARD=sim wrap

.PHONY: wrap_start
wrap_start:
	make -f makefile.sim BOARD=sim wrap_start

clean: marble_mini_clean marble_clean sim_clean nucleo_clean
