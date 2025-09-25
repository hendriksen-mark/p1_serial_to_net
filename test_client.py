#!/usr/bin/env python3
"""
P1 Serial-to-Network Bridge Test Client

This script connects to the RP2040 bridge and displays P1 smart meter data.
It can also be used to test the bridge functionality.
"""

import socket
import time
import sys
import threading
from datetime import datetime

# Configuration
BRIDGE_IP = "192.168.1.100"  # Default - replace with actual DHCP assigned IP
BRIDGE_PORT = 2000
RECONNECT_DELAY = 5  # seconds

class P1Client:
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.socket = None
        self.connected = False
        self.running = False
        
    def connect(self):
        """Connect to the P1 bridge"""
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            self.socket.connect((self.host, self.port))
            self.connected = True
            print(f"Connected to P1 bridge at {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"Failed to connect: {e}")
            return False
            
    def disconnect(self):
        """Disconnect from the bridge"""
        self.connected = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        print("Disconnected from bridge")
        
    def listen(self):
        """Listen for P1 data from the bridge"""
        buffer = ""
        p1_message = ""
        in_message = False
        
        while self.connected and self.running:
            try:
                data = self.socket.recv(1024).decode('utf-8', errors='ignore')
                if not data:
                    print("Connection closed by server")
                    break
                    
                buffer += data
                
                # Process the buffer character by character
                while buffer:
                    char = buffer[0]
                    buffer = buffer[1:]
                    
                    if char == '/':
                        # Start of P1 message
                        in_message = True
                        p1_message = char
                        print(f"\n--- P1 Message at {datetime.now().strftime('%H:%M:%S')} ---")
                        
                    elif in_message:
                        p1_message += char
                        
                        # Check for end of message
                        if char == '!' and len(p1_message) > 1:
                            # Read the 4-character checksum
                            checksum = ""
                            while len(checksum) < 4 and buffer:
                                checksum += buffer[0]
                                p1_message += buffer[0]
                                buffer = buffer[1:]
                            
                            if len(checksum) == 4:
                                # Complete P1 message received
                                print(p1_message)
                                print(f"--- End of P1 Message (Length: {len(p1_message)}) ---")
                                
                                # Parse some key values
                                self.parse_p1_data(p1_message)
                                
                                p1_message = ""
                                in_message = False
                                
            except socket.timeout:
                continue
            except Exception as e:
                print(f"Error receiving data: {e}")
                break
                
        self.connected = False
        
    def parse_p1_data(self, message):
        """Parse and display key P1 data values"""
        lines = message.split('\n')
        
        # Dictionary of common P1 codes and their descriptions
        p1_codes = {
            '1-0:1.8.1': 'Energy Consumed Tariff 1',
            '1-0:1.8.2': 'Energy Consumed Tariff 2', 
            '1-0:2.8.1': 'Energy Produced Tariff 1',
            '1-0:2.8.2': 'Energy Produced Tariff 2',
            '1-0:1.7.0': 'Power Consumed',
            '1-0:2.7.0': 'Power Produced',
            '1-0:32.7.0': 'Voltage L1',
            '1-0:31.7.0': 'Current L1',
            '0-0:96.14.0': 'Current Tariff'
        }
        
        print("\nKey Values:")
        for line in lines:
            for code, description in p1_codes.items():
                if line.startswith(code):
                    # Extract value between parentheses
                    start = line.find('(')
                    end = line.find(')')
                    if start != -1 and end != -1:
                        value = line[start+1:end]
                        print(f"  {description}: {value}")
        
    def run(self):
        """Main run loop with automatic reconnection"""
        self.running = True
        
        while self.running:
            if not self.connected:
                if self.connect():
                    # Start listening thread
                    listen_thread = threading.Thread(target=self.listen)
                    listen_thread.daemon = True
                    listen_thread.start()
                else:
                    print(f"Retrying connection in {RECONNECT_DELAY} seconds...")
                    time.sleep(RECONNECT_DELAY)
                    continue
            
            try:
                # Keep main thread alive
                time.sleep(1)
            except KeyboardInterrupt:
                print("\nShutting down...")
                self.running = False
                self.disconnect()
                break
                
        print("Client stopped")

def main():
    if len(sys.argv) > 1:
        bridge_ip = sys.argv[1]
    else:
        bridge_ip = BRIDGE_IP
        
    if len(sys.argv) > 2:
        bridge_port = int(sys.argv[2])
    else:
        bridge_port = BRIDGE_PORT
    
    print(f"P1 Bridge Test Client")
    print(f"Connecting to {bridge_ip}:{bridge_port}")
    print("Note: Check the serial monitor for the actual DHCP-assigned IP address")
    print("Usage: python3 test_client.py <IP_ADDRESS> [PORT]")
    print("Press Ctrl+C to exit")
    
    client = P1Client(bridge_ip, bridge_port)
    client.run()

if __name__ == "__main__":
    main()