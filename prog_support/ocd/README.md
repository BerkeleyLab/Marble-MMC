# Deployable Programming Directory

This directory can be copied to non-development machines (computers that cannot
build the codebase) to program or update the MMC on a target board.  I.e. it can
stand alone outside of the context of this repository.

## Dependencies
- Some Unix shell: `sh`, `bash`, (`csh`? `zsh`?)
- `openocd` (min version? Works with 0.12.0)

## Usage
Connect the target Marble board (only one connected to the computer, please) via a supported
USB-to-JTAG/SWD debugger (see above).  Then run:
```sh
cd prog_support/ocd
./program.sh my_executable.elf
```

where `my_executable.elf` is an ELF file built for the Marble board (get from CI or from
a machine capable of building the target).
