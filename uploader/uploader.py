import serial
import time
import struct
import binascii
import threading
import queue
import sys
import argparse

# Protocol Constants
PREAMBLE_BYTE = 0x7F
PREAMBLE_RX_COUNT = 5
PREAMBLE_TX_COUNT = 12

HDR_MASK_BASE = 0x80      
HDR_FLAG_64BIT = 0x02    
BROADCAST_ID = 0xFF

BOOT_GET_INFO = 0x01
BOOT_GET_CHIP_ID = 0x02

# firmware update commands
BOOT_WRITE = 0x31
BOOT_ERASE = 0x44
BOOT_GET_CRC = 0xA1
BOOT_GO = 0x21

#search commands
BOOT_GET_ID = 0x11
BOOT_SILENCE = 0x12
BOOT_UNSILENCE = 0x13

#Node info commands
BOOT_GET_NODE_INFO = 0xC1
BOOT_SET_NODE_INFO = 0xC2


class CH32V003Bootloader:
    HDR_MASK_TYPE = 0x01   # 0b0000 0001 (0 = Request, 1 = Response)
    
    def __init__(self, port, baud=9600, verbose=False):
        self.verbose = verbose
        try:
            self.ser = serial.Serial()
            self.ser.port     = port
            self.ser.baudrate = baud
            self.ser.timeout  = 0.01
            self.ser.stopbits = serial.STOPBITS_TWO

            # keep both control lines low while opening
            self.ser.setDTR(False)
            self.ser.setRTS(False)
            self.ser.open()


            #self.ser = serial.Serial(port, baud, timeout=0.01, stopbits=serial.STOPBITS_TWO)
        except serial.SerialException as e:
            self._log(f"Error opening serial port {port}: {e}")
            return
            
        self.rx_queue = queue.Queue()
        self.stop_thread = False
        self.serial_lock = threading.Lock()
        
        self.thread = threading.Thread(target=self._uart_reader_thread, daemon=True)
        self.thread.start()

    def _log(self, message, end='\n'):
        if self.verbose:
            print(message, end=end)
            sys.stdout.flush()
            
    def close(self):
        self.stop_thread = True
        if self.thread.is_alive():
            self.thread.join()
        self.ser.close()

    # --- Communication Core ---

    def _calculate_crc32(self, data):
        return binascii.crc32(data) & 0xFFFFFFFF

    def _uart_reader_thread(self):
        raw_buffer = bytearray()
        HDR_MASK_TYPE = 0x01

        while not self.stop_thread:
            try:
                if self.ser.in_waiting > 0:
                    with self.serial_lock:
                        raw_buffer.extend(self.ser.read(self.ser.in_waiting))
                
                while len(raw_buffer) >= (PREAMBLE_RX_COUNT + 8):
                    pre_len = 0
                    hdr_pos = -1
                    
                    for i in range(len(raw_buffer)):
                        if raw_buffer[i] == PREAMBLE_BYTE:
                            pre_len += 1
                        else:
                            if pre_len >= PREAMBLE_RX_COUNT:
                                hdr_pos = i
                                break
                            pre_len = 0
                    
                    if hdr_pos == -1:
                        raw_buffer = raw_buffer[-PREAMBLE_RX_COUNT:]
                        break

                    hdr = raw_buffer[hdr_pos]
                    if (hdr & 0xF0) != HDR_MASK_BASE:
                        raw_buffer = raw_buffer[hdr_pos + 1:]
                        continue
                    
                    if not (hdr & HDR_MASK_TYPE):
                        raw_buffer = raw_buffer[hdr_pos + 1:]
                        continue

                    is_64 = (hdr & HDR_FLAG_64BIT) != 0
                    addr_len = 8 if is_64 else 1
                    len_idx = hdr_pos + 1 + addr_len + 1
                    
                    if len(raw_buffer) <= len_idx: 
                        break 
                    
                    data_len = raw_buffer[len_idx]
                    total_packet_len = 1 + addr_len + 1 + 1 + data_len + 4 
                    
                    if len(raw_buffer) < (hdr_pos + total_packet_len):
                        break 

                    payload_for_crc = raw_buffer[hdr_pos : hdr_pos + total_packet_len - 4]
                    rx_crc = struct.unpack('<I', raw_buffer[hdr_pos + total_packet_len - 4 : hdr_pos + total_packet_len])[0]
                    
                    if rx_crc == self._calculate_crc32(payload_for_crc):
                        packet_data = raw_buffer[hdr_pos : hdr_pos + total_packet_len]
                        addr_raw = packet_data[1 : 1 + addr_len]
                        
                        self.rx_queue.put({
                            'node_id': addr_raw[0] if addr_len == 1 else None,
                            'uid': addr_raw.hex().upper() if addr_len == 8 else None,
                            'cmd': packet_data[1 + addr_len],
                            'data': packet_data[1 + addr_len + 2 : 1 + addr_len + 2 + data_len],
                            'raw': packet_data
                        })
                        raw_buffer = raw_buffer[hdr_pos + total_packet_len:]
                    else:
                        raw_buffer = raw_buffer[hdr_pos + 1:]
                
                time.sleep(0.001)
            except Exception:
                time.sleep(0.01)

    def send_packet(self, address, cmd, data=None):
        if data is None: data = []
        while not self.rx_queue.empty(): self.rx_queue.get_nowait()

        if isinstance(address, str):
            address = bytes.fromhex(address)

        hdr = HDR_MASK_BASE
        if isinstance(address, (bytes, bytearray, list)) and len(address) == 8:
            hdr |= HDR_FLAG_64BIT
            addr_bytes = bytes(address)
        elif isinstance(address, int):
            addr_bytes = bytes([address & 0xFF])
        elif isinstance(address, (bytes, bytearray)) and len(address) == 1:
            addr_bytes = address
        else:
            raise ValueError(f"Invalid Address: {address}")

        payload = bytes([hdr]) + addr_bytes + bytes([cmd & 0xFF, len(data) & 0xFF]) + bytes(data)
        crc = self._calculate_crc32(payload)
        full_packet = bytes([PREAMBLE_BYTE] * PREAMBLE_TX_COUNT) + payload + struct.pack('<I', crc)
        
        with self.serial_lock:
            self.ser.write(full_packet)
            self.ser.flush()

    def get_response(self, timeout=0.5):
        try: return self.rx_queue.get(timeout=timeout)
        except queue.Empty: return None

    # --- High Level Commands ---

    def set_fw_id(self, address, fw_id):
        """Sets the Firmware ID for a specific node."""
        self._log(f"Setting FW_ID to {fw_id} for {address}...")
        # Payload: [Type (0x00), fw_id]
        self.send_packet(address, BOOT_SET_NODE_INFO, [0x00, fw_id & 0xFF])
        resp = self.get_response()
        if resp:
            self._log("FW_ID updated successfully.")
            return True
        self._log("No response from node.")
        return False

    def set_node_id(self, address, node_id):
        """Sets the 8-bit Node ID for a specific node."""
        self._log(f"Setting Node ID to {node_id} for {address}...")
        # Payload: [Type (0x01), node_id]
        self.send_packet(address, BOOT_SET_NODE_INFO, [0x01, node_id & 0xFF])
        resp = self.get_response()
        if resp:
            self._log("Node ID updated successfully.")
            return True
        self._log("No response from node.")
        return False

    def enter_bootloader(self, duration=1.0):
        self._log(f"Holding synchronization (entering bootloader)...")
        start_time = time.time()
        with self.serial_lock:
            while (time.time() - start_time) < duration:
                self.ser.write([0x7F])
        time.sleep(0.2)

    def search_nodes(self, slots=100, retries=3):
        """
        1. Scans for nodes using collision avoidance.
        2. Unsilences all nodes.
        3. Queries each discovered UID for extended info.
        """
        self._log(f"Scanning for nodes ({slots} slots)...")
        uids_found = [] 

        # Discover UIDs
        self.send_packet(BROADCAST_ID, BOOT_UNSILENCE)
        for attempt in range(retries):
            # Request IDs from nodes
            self.send_packet(BROADCAST_ID, BOOT_GET_ID, [max(0, slots-32)])
            end_search = time.time() + (slots * 0.05) + 0.2
            
            while time.time() < end_search:
                resp = self.get_response(timeout=0.02)
                if resp and resp['cmd'] == BOOT_GET_ID:
                    uid_hex = bytes(resp['data']).hex().upper()
                    if uid_hex not in uids_found:
                        uids_found.append(uid_hex)
                        self._log(f"Found {uid_hex}")
                        # Silence this specific node so others can respond
                        self.send_packet(uid_hex, BOOT_SILENCE)
        
        # Unsilence all nodes before querying info
        self.send_packet(BROADCAST_ID, BOOT_UNSILENCE)
        time.sleep(0.05) # Brief pause to ensure bus is ready

        self._log("")
        self._log(f"Found {len(uids_found)} unique nodes:")
        self._log(f"{'UID':<20} | {'Node-ID':<10} | {'FW-ID':<10}")
        self._log("-" * 46)
        
        # Query every found node for its specific info
        discovered_devices = {}        
        for uid in uids_found:
            info = self.get_node_info(uid)
            if info:
                discovered_devices[uid] = info
                self._log(f"{uid:<20} | {info['node_id']:<10} | {info['fw']:<10}")
            else:
                self._log(f"{uid:<20} | {'Error':<10} | {'Error':<10}")
        
        self._log("") # Padding newline
        return discovered_devices

    def get_node_info(self, address):
        self.send_packet(address, BOOT_GET_NODE_INFO)
        resp = self.get_response(timeout=0.5)
        if resp and resp['cmd'] == BOOT_GET_NODE_INFO and len(resp['data']) >= 2:
            return {'node_id': resp['data'][0], 'fw': resp['data'][1]}
        return None

    def update_firmware(self, firmware_data, fw_id=0):
        if len(firmware_data) % 64 != 0:
            padding = 64 - (len(firmware_data) % 64)
            firmware_data += b'\xFF' * padding

        total_blocks = len(firmware_data) // 64
        self._log(f"Flashing {len(firmware_data)} bytes ({total_blocks} blocks) to FW-ID: 0x{fw_id:02X}")
        
        start_time = time.perf_counter()
        self.send_packet(BROADCAST_ID, BOOT_SILENCE)

        for i, offset in enumerate(range(0, len(firmware_data), 64)):
            chunk = firmware_data[offset:offset+64]
            self._broadcast_update_block(i, chunk, fw_id)
            
            # Progress bar
            percent = (i + 1) / total_blocks * 100
            sys.stdout.write(f"\rWriting Block {i+1}/{total_blocks} [{percent:.1f}%]")
            sys.stdout.flush()

        self.send_packet(BROADCAST_ID, BOOT_UNSILENCE)
        self._log(f"\nFinished in {time.perf_counter() - start_time:.2f}s")

    def _broadcast_update_block(self, block_index, data, fw_id):
        address = 0x08000000 + (block_index * 64)
        raw_block = struct.pack('<I', address) + data
        
        corr = 0
        for attempt in range(256):
            if all((b - attempt) % 256 != PREAMBLE_BYTE for b in raw_block):
                corr = attempt
                break
        
        corrected_payload = bytes([(b - corr) % 256 for b in raw_block])
        write_payload = bytes([fw_id & 0xFF, corr & 0xFF]) + corrected_payload
        self.send_packet(BROADCAST_ID, BOOT_WRITE, write_payload)

    def get_verify_crc(self, address, length):
        payload = struct.pack('<II', 0x08000000, length)
        self.send_packet(address, BOOT_GET_CRC, payload)
        resp = self.get_response(timeout=1.0)
        if resp and resp['cmd'] == BOOT_GET_CRC and len(resp['data']) == 4:
            return struct.unpack('<I', resp['data'])[0]
        return None

    def start_app(self):
        time.sleep(0.2)
        self._log("Starting application...")
        self.send_packet(BROADCAST_ID, BOOT_GO)


def main():
    parser = argparse.ArgumentParser(description='CH32V003 Bootloader Tool')
    parser.add_argument('--port', '-p', default='COM13')
    parser.add_argument('--baud', '-b', type=int, default=9600)
    parser.add_argument('--uid', help='Target UID')
    parser.add_argument('-i', '--file', help='Firmware file')
    parser.add_argument('--fw', type=int, default=0)
    
    # Use "nargs='?'" to make the value optional but immediately following the flag
    # Use "const" to set the value if the flag is present but no value is provided
    parser.add_argument('--search', type=int, nargs='?', const=63, help='Scan nodes. Optional: slot size (default 63)')
    parser.add_argument('--verify', type=int, nargs='?', const=63, help='Verify CRC. Optional: slot size (default 63)')
    
    parser.add_argument('--write', action='store_true', help='Write firmware using -i file')
    parser.add_argument('--run', action='store_true', help='Start application')

    args = parser.parse_args()
    loader = CH32V003Bootloader(args.port, args.baud, verbose=True)

    try:
        loader.enter_bootloader()
        
        # Handle Writing firmware
        if args.write:
            if not args.file:
                print("Error: -i (file) is required for --write")
                return
            with open(args.file, 'rb') as f:
                loader.update_firmware(f.read(), args.fw)

        # Handle Verification (Accepts value from --verify)
        if args.verify is not None:
            if not args.file:
                print("Error: -i (file) is required for --verify")
                return
            
            slot_count = args.verify # This will be the number after 
            targets = [args.uid] if args.uid else [u for u, inf in loader.search_nodes(slot_count).items() if inf['fw'] == args.fw]
            
            with open(args.file, 'rb') as f:
                data = f.read()
                expected = binascii.crc32(data) & 0xFFFFFFFF
                for uid in targets:
                    res = loader.get_verify_crc(uid, len(data))
                    status = "MATCH" if res == expected else "FAIL"
                    print(f"Node {uid} | Expected: 0x{expected:08X} | Node: {f'0x{res:08X}' if res else 'TIMEOUT'} | {status}")

        # Handle Standalone Search (Accepts value from --search)
        if args.search is not None and args.verify is None:
            slot_count = args.search
            nodes = loader.search_nodes(slot_count)
            for u, inf in nodes.items():
                print(f"UID: {u} | Node-ID: {inf['node_id']} | FW-ID: {inf['fw']}")

        if args.run:
            loader.start_app()
    finally:
        loader.close()
        
if __name__ == "__main__":
    main()
    
    