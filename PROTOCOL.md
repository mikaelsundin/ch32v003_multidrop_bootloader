# Multi-drop CH32V003 Bootloader Protocol Specification

## 0. CH32V003 flash usage
The bootloader is using Bootloader sector and DATA0,DATA1 in options bytes.
DATA0 is storing node-id, DATA1 storing firmware-id


## 1. Physical Layer
- **Interface:** Half-duplex UART (Single-wire)
- **Baud Rate:** 9600 bps
- **Pinout:** PD6 (TX/RX combined)
- **Topology:** Multi-drop bus allowing one Master and multiple Slave nodes.


## 2. Packet Format
All frames begin with a n-byte preamble to synchronize the receiver's state machine.
The state machine require at least 5 preambles to work.

| Field | Size | Value / Description |
| :--- | :--- | :--- |
| **Preamble** | min 5 Bytes | `0x7F...0x7F` |
| **Header** | 1 Byte | `0x80` | `AddrLen` | `Type` |
| **Address** | 1 or 16 Bytes | Node ID (8-bit) or Unique ID (128-bit) |
| **Command** | 1 Byte | Operation Code (see Section 3) |
| **Length** | 1 Byte | Payload size ($0$ to $255$) |
| **Data** | $N$ Bytes | Command-specific payload |
| **CRC32** | 4 Bytes | IEEE 802.3 CRC (Little-endian) |

### 2.1 Header Byte Definition
- **Bit 7..2:** 0b100000 (Header Identification)
- **Bit 1:** Address Length (`0` = 8-bit ID, `1` = 128-bit UID)
- **Bit 0:** Direction (`0` = Request from Host, `1` = Response from Node)

## 3. Command Definitions

### 3.1 Network Management
- **`BOOT_GET_ID` (0x11):** Node Discovery. 
        Nodes respond with their 128-bit UID after a optional pseudo-random delay based on the formula: $Delay = (UID \pmod{Slots}) \times 40\text{ms}$.
        
- **`BOOT_SILENT_ID` (0x12):** 
    Sets a node to "Silent Mode." While silent, a node will ignore `BOOT_GET_ID` requests. Used to clear the bus for remaining nodes during discovery retries.

### 3.2 Node Information
- **`BOOT_GET_NODE_INFO` (0xC1):** 
    Returns the current 8-bit Node-ID and 8-bit Firmware-ID of the target.

- **`BOOT_SET_NODE_INFO` (0xC2):** 
    Set 8-bit Node-ID and 8-bit Firmware-ID of the target.
    
### 3.3 Flash Operations
- **`BOOT_ERASE` (0x44):** Erases a 64-byte block. Requires matching `Firmware_ID` to proceed.
- **`BOOT_WRITE` (0x31):** Writes 64 bytes to a specific address. 
    - **Payload:** `[Firmware_ID, Correction, Addr(4), Data(64)]`
    - **Note:** Data is transmitted as $(Byte - Correction)$ to avoid the sequence `0x7F 0x7F 0x7F` which triggers a receiver resync.

### 3.4. Out of sync strategy (0x7F Avoidance)
To avoid 0x7F when sendingBOOT_WRITE a stratergy is implemented by adding a correction value.
The host shall search for a correction byte that not containing a 0x7F 0x7F 0x7F in the chunk.


### 3.5 System
- **`BOOT_GO` (0x21):** Start the application. 
The chip reboots, typically jumping to the application at `0x08000000`.


## 5. Security & Integrity
- **CRC32:** Covers Header, Address, Command, Length, and Data. Polynomial: `0xEDB88320`.
- **Firmware ID:** A safety lock. If a Host tries to flash a "Type A" binary to a "Type B" node, the node rejects the `BOOT_WRITE` and 'BOOT_ERASE' command.


