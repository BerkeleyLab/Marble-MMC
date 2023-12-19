# Watchdog feature

This microcontroller code implements a network-based watchdog feature
that is somewhat unusual.  In combination with a write-protected Golden Image
in FPGA boot flash, it lets us claim "unbrickability" of the system.
This has been a key feature-bullet of the
[Marble](https://github.com/BerkeleyLab/Marble) board and its relatives
since 2018.
If someone accidentally (and at-scale) loads a bad bitfile onto the board,
the board (as controlled by this microcontroller) can always recover control.

If this interests you, read on.  If not, know that at the microcontroller
console (the same place you configure the IP and MAC into non-volatile
memory) type `u 0` and the system will respond with
```
u 0
Disabling watchdog
Success
```
That's it.  This feature won't get in your way.

## Theory

To guarantee a non-spoofable watchdog, a secret has to be shared
between the microcontroller and a secure server.  That server is likely
outside the bounds of the normal application control software, and needs
a network route to the FPGA chassis.  In response to a "random number"
(nonce) generated in the microcontroller, the server computes a secure
[Message Authentication Code (MAC)](https://en.wikipedia.org/wiki/Message_authentication_code)
and sends it back.  The MAC computation used here is called
[SipHash](https://en.wikipedia.org/wiki/SipHash).  It is based
on a 128-bit shared secret, a 64-bit nonce, and a 64-bit MAC.

There are multiple scenarios that can trigger a recovery process:

1. A defective bitfile is loaded, that interferes with the networking
ability of the board.  This is the primary motivator for this feature.

2. Network connectivity between the board and the controlling server
is interrupted.  This can be anything from a rebooting Ethernet switch
to a network cable getting run over by a forklift.  This can easily turn
into an unsafe situation, if the operators of the machine don't have
enough control to turn things off.

3. The chain of hardware and software somehow prevents operators from
exercising normal control over the board -- e.g., loading another bitfile.
Then those operators can intentionally halt the watchdog server.  After
the configured timeout: boom!  The board reboots to safe mode.

The FPGA hardware and gateware needed to make the watchdog work are:

1. SPI link to the microcontroller

2. Ethernet link to the outside work

3. A passive array of mailbox registers, with access shared between
the SPI link and a UDP read/write protocol.

Gateware for all of this is provided in an example project
(`projects/test_marble_family`) in
[Bedrock](https://github.com/BerkeleyLab/Bedrock).  In particular,
its UDP/IP/Ethernet layer called Packet Badger has been a bulletproof
component of the system since 2019.

Given that infrastructure, the protocol is extremely simple in concept.
The microcontroller stores a 64-bit nonce in the mailbox.  Every two seconds,
it reads a 64-bit MAC from another part of the mailbox.  If this matches
the output of the secretly-keyed SipHash, it resets the watchdog timer,
generates a new nonce, and stores that in the mailbox.  If the watchdog timer
hits zero, it reboots the FPGA.

To avoid useless reboots _from_ the Golden image if the problem
is in the outside world, the microcontroller uses a state machine to keep
track of FPGA boots.  A reboot _not_ caused by a hardware reset is assumed
to leave the FPGA in a User/Application state, with the watchdog running.
But by design, a hardware reset leaves the FPGA running a Golden bitfile,
and there's no point to rebooting that.

There are just two things that you need to configure in the
microcontroller, both of which are held in non-volatile memory:

1. The timeout interval, with the "u" command mentioned above.
0 is for disable.  Valid active values are 2 to 510 seconds,
with a granularity of 2 seconds.

2. The 128-bit shared secret key, with the "v" command.  While you
_can_ type (or cut-and-paste) a 16-digit hex number at the console,
the python script `scripts/genkey.py` is provided to give a robust
and useful interface.

A proof-of-concept server is provided in `scripts/keepalive.py`.

If all of this seems needlessly complicated, well, (a) maybe you
don't need it, and (b) feel free to deploy another fail-safe reboot
mechanism, like a remote-controlled power strip.  This watchdog covers
the case where you don't want to add extra hardware, that itself
might have reliability, safety, and loss-of-control issues.

## Functional Demonstration

Using default key files
```sh
$ # Generate a secret key, store it in the default key file, and load it to
$ # the MMC via console at /dev/ttyUSB3
$ cd marble_mmc
$ python3 scripts/genkey.py
Generating new key file
Key stored to: /home/$USER/.marble_mmc_keys/mmc_key
>   Wrote 1 lines
>   done
>   closing
Key loaded to MMC

SUCCESS

$ # Network-reboot FPGA to USER bitfile
$ # TODO insert instructions?

$ # Start server to satisfy watchdog condition and prevent FPGA reset
$ PYTHONPATH=path/to/bedrock/badger python3 scripts/keepalive.py -i $IP

Getting from default: /home/$USER/.marble_mmc_keys/mmc_key
2023-12-15T06:20:58Z Handshake 127.0.0.1  67458b6bc6237b32 -> 6969805026d14ffb
2023-12-15T06:21:06Z Handshake 127.0.0.1  69983c6473483366 -> 59dfd60e1c0f0fcd
...
```

Key files can be generated and used on a per-board basis.  The `--id` argument
to both `scripts/genkey.py` and `scripts/keepalive.py` will resolve to the same
file path.  The value supplied to argument `--id` can be any string identifier
to use to distinguish individual boards/chassis (i.e., serial number).

```sh
$ # Generate a new key file for serial number 204
$ python3 scripts/genkey.py --id 204
Generating new key file
Key stored to: /home/$USER/.marble_mmc_keys/mmc_key_204
SUCCESS

$ # Load the secret key for serial number 204 to the board at /dev/ttyUSB7
$ python3 scripts/genkey.py --id 204
Using existing keyfile /home/$USER/.marble_mmc_keys/mmc_key_204.
Use --regen to generate new key and overwrite.
>   Wrote 1 lines
>   done
>   closing
Key loaded to MMC

SUCCESS

$ # Run the watchdog server for a particular board
$ PYTHONPATH=path/to/bedrock/badger python3 scripts/keepalive.py -i $IP --id 204
Getting from: /home/$USER/.marble_mmc_keys/mmc_key_204
2023-12-15T06:42:12Z Handshake 127.0.0.1  15c32a4a5c01ee39 -> 2213978af621190e
2023-12-15T06:42:20Z Handshake 127.0.0.1  bb4ffc576f01c10c -> 2f99515f23bacf04
...
```

## Key File Storage
The key storage directory can be specified via environment variable `MMC_KEY_PATH`.
```sh
$ export MMC_KEY_PATH=/home/$USER/my_mmc_key_path

$ # Generate a new key
$ python3 scripts/genkey.py --id 11
Generating new key file
Key stored to: /home/$USER/my_mmc_key_path/mmc_key_11
SUCCESS

$ Use an existing key
$ PYTHONPATH=path/to/bedrock/badger python3 scripts/keepalive.py -i $IP --id 11
Getting key from: /home/$USER/my_mmc_key_path/mmc_key_11
2023-12-15T06:45:25Z Handshake 127.0.0.1  9812c97132f6da09 -> d0c82846e981bb93
...
```
