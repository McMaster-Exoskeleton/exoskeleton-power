# exoskeleton-power
Repository containing code for the power subteam for McMaster Exoskeleton

The Power Architecture team is running an STM32 MCU on the Low Voltage (LV) PCB for the 2025-2026 year. It is being used to monitor voltage and current throughout the power system, as well as controlling a pre-charge circuit. The MCU will communicate with the Dashboard via CAN protocol.

When working on code:
- Code gets pushed to main in a new folder once it has been tested. 
- New programs should incorporate as much of the previously tested code as practical, to reduce testing complexity.
- Leave clear instructions in a README file or commented at the top of the code on how to implement the code.

Refer to the Embedded Repository's README for coding standards, APIs, CAN communication, etc...

We use the STM32CubeIDE Version 1.19 for all testing with the NUCLEO-F446RE development board. 
Pyserial library is used for UART communication for testing purposes. 
