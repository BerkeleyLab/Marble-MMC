# Identify the debug probe used
export DEBUG_ADAPTER=$(shell ./id_probe.sh)

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

.PHONY: sim
sim:
	make -f makefile.sim BOARD=sim all

sim_clean:
	make -f makefile.sim BOARD=sim clean

.PHONY: run
run:
	make -f makefile.sim BOARD=sim run

clean: marble_mini_clean marble_clean sim_clean
