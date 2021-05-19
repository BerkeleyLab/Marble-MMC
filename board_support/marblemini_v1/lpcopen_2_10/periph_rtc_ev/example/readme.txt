Event Monitor/Recorder example

Example description
This example describes how to use Event Monitor/Recorder Block to
track event occurs on input pins.

After initialize RTC, set the current time for the RTC. When an event occurs
on an input channel, an interrupt is generated. The first and  last
timestamp of the event is printed out.

Special connection requirements
Connect desired event input channel pin to GND/VCC to generate events.
Event Input pins:
Channel         Pin         EA
0               P0.7        J5.17
1               P0.8        J3.19
2               P0.9        J5.18

Build procedures:
Visit the LPCOpen quickstart guides at link "http://www.lpcware.com/content/project/lpcopen-platform-nxp-lpc-microcontrollers/lpcopen-v200-quickstart-guides"
to get started building LPCOpen projects.

