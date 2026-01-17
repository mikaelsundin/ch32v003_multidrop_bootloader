# Multi-Node Bootloader Documentation

This tool is designed to manage and update firmware on multiple CH32V003 microcontrollers connected to a shared UART bus. 
It supports node discovery, unique ID (UID) addressing, and firmware broadcasting.

## Connection Requirements
* **Baud Rate**: Default is `9600`.
* **Stop Bits**: Uses `2` stop bits for protocol stability.
* **Synchronization**: The tool sends a stream of preamble bytes (`0x7F`) to force nodes into bootloader mode.

## Commands

### --search
Scans the bus for all connected nodes using collision avoidance (silencing/unsilencing).
* **Example**: `python uploader.py --port COM13 --search`

### --write [FILE]
Broadcasts firmware to nodes. Use `--fw_id` to target specific groups.
* **Example**: `python uploader.py --port COM13 --write firmware.bin --fw_id 1`

### --verify [FILE]
Compares local file CRC32 with the node's internal flash CRC32.
* **Note**: Requires `--uid`.
* **Example**: `python uploader.py --port COM13 --uid 0123456789ABCDEF --verify firmware.bin`

### --set_fw_id [INT]
Assigns a new Firmware ID to a specific node for group updates.
* **Example**: `python uploader.py --port COM13 --uid 0123456789ABCDEF --set_fw_id 2`

### --set_node_id [INT]
Assigns a new 8-bit Node ID to a specific node.
* **Example**: `python uploader.py --port COM13 --uid 0123456789ABCDEF --set_node_id 10`

### --run
Sends the `BOOT_GO` command to exit the bootloader and start the application.
* **Example**: `python uploader.py --port COM13 --run`

##  Technical Packet Structure
1. **Preamble**: `0x7F...0x7F`
2. **Header**: Mask for packet type and address mode (8-bit or 64-bit).
3. **Address**: Target Node ID or 64-bit UID.
4. **DataLengt***: Data Length
5. **Data**: Data
6. **CRC32**: 4-byte CRC32 of the header and payload.

