## Simulated Platform ##
Mostly useful as a simulation tool, there is a bare-bones version of the
marble_mmc firmware which compiles for and runs on the host platform.
Very few of the features of the full Marble board hardware are currently
emulated (see feature list).

To build the simulation version, simply execute
```bash
  make sim
```

To run the simulation, use
```bash
  make sim run
```
or,
```bash
  out_sim/marble_mmc_sim
```

The simulation build defines the macro SIMULATION which is used in the common
code areas where behavioral forks are required.  An ongoing effort is to
minimize the occurrence of such `#ifdef SIMULATION ... #else ... #endif` blocks.
They're an eyesore and violate encapsulation principles.

# Features Implemented #
* Flash memory emulated with binary file on disk
* UART character-based I/O emulated with stdio

# Advantages #
A subjective list of perceived advantages of the simulated platform over the
hardware:
* No hardware required
* Advanced debugging possibilities
* Essentially infinite compute power
* Command-line arguments enhance testing flexibility
* Can connect to other simulated utilities via sockets/pipes

# Disadvantages #
The corollary to the above:
* It's fake (it's not actually the product to be delivered)
* Simulated hackery may not faithfully reproduce hardware behavior
* Additional development efforts
* Not useful for timing-related tests

# Comparison to ARM simulators/emulators #
I don't know much about these tools, so this may be a blind spot.  I have used
some simulators built into closed-source tools (e.g. Keil's uVision simulator)
which are nice for low-level debugging but don't provide an obvious way to
simulate the "outside world" (i.e. additional devices connected to the processor).

Perhaps QEMU or another instruction translator could do the job better. Please
enlighten me.
