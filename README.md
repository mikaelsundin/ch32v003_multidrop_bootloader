# Purpose 
Be able to update multiple firmware's on a multidrop bus with CH32V003.

# Challenges
* Fit the code for 1920 bytes of flash
* Expose flash and crc32 functions for main firmware. 
  RISC-V global pointer cannot be used in bootloader code called from main firmware.
* Find a way to search for nodes on the bus.

# Features
* Search for nodes
* Set Node-id and firmware-id for specific node
* Get Node-id and firmware-id for specific node
* Update firmware on all nodes with specific firmware-id
* Calculate and check CRC32 for firmware.



# Hardware
Simple hardware for a limited number of devices is to use a USB to Serial adapter with a 1kohm resistor between TX and RX.

For longer wires and/or more devices a active open-drain buffer shall be used.

# Schematic
    ┌───────────────────┐
    │ USB-TO-UART-5V    │          1k pull-up Resistor
    │          [ TX ]───│───────[██████]──────┐
    │                   │                     │     OUTPUT BUS
    │          [ RX ]───│─────────────────────┴─────── ( BUS )
    │                   │              
    │          [ 5V ]───│───────────────────────────── ( +5V )
    │                   │                              
    │         [ GND ]───│───────────────────────────── ( GND )
    │                   │
    └───────────────────┘

