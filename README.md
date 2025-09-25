# P1 Serial-to-Network Bridge

A PlatformIO project for Waveshare RP2040 Zero + W5500 Lite that creates a serial-to-network bridge for Dutch smart meter P1 port data, similar to the docker-ser2net functionality.

## Hardware Requirements

- **Waveshare RP2040 Zero** - Main microcontroller board
- **W5500 Lite** - Ethernet module for network connectivity
- **P1 Cable** - For connecting to Dutch smart meter P1 port

## Wiring Connections

### W5500 Lite to Waveshare RP2040 Zero
```
W5500 Lite    ->  RP2040 Zero
VCC           ->  3.3V
GND           ->  GND
MOSI          ->  GPIO19 (SPI TX)
MISO          ->  GPIO16 (SPI RX)
SCK           ->  GPIO18 (SPI SCK)
CS            ->  GPIO17 (Configurable)
RST           ->  GPIO20 (Optional)
```

### P1 Port Connection
```
P1 Port       ->  RP2040 Zero
Pin 5 (Data)  ->  GPIO1 (Serial1 RX - predefined)
Pin 3 (GND)   ->  GND
Pin 2 (RTS)   ->  Not used (P1 is read-only)
```

**Note**: Serial1 on RP2040 uses predefined pins: GPIO0 (TX) and GPIO1 (RX). We only use GPIO1 (RX) for receiving P1 data.

## Features

- **DHCP IP Configuration**: Automatically gets IP address from your router/modem
- **TCP Server**: Listens on port 2000
- **Multiple Connections**: Supports up to 3 simultaneous clients
- **Client Management**: 
  - Kick old user when max connections reached
  - 30-second client timeout
  - Telnet protocol negotiation
- **P1 Protocol Support**:
  - 115200 baud serial communication
  - Complete P1 message parsing (from '/' to '!XXXX')
  - Automatic message forwarding to all connected clients
- **Status Monitoring**: Built-in LED indicates system status

## Network Configuration

The device uses DHCP to automatically get network settings from your router/modem:
- **IP Address**: Automatically assigned by DHCP
- **Port**: 2000 (fixed)
- **Gateway**: Provided by DHCP
- **Subnet**: Provided by DHCP
- **DNS**: Provided by DHCP

## Usage

1. **Build and Upload**:
   ```bash
   pio run -t upload
   ```

2. **Connect to P1 Port**: Wire the device to your smart meter's P1 port

3. **Find Device IP**: Check the serial monitor output to see the assigned IP address

4. **Connect Clients**: Use telnet or any TCP client to connect:
   ```bash
   telnet <DEVICE_IP> 2000
   ```
   Replace `<DEVICE_IP>` with the IP address shown in the serial monitor.

5. **Monitor Serial**: For debugging, connect to the serial monitor:
   ```bash
   pio device monitor
   ```

## P1 Data Format

The bridge handles Dutch smart meter P1 data format automatically. Example P1 message:
```
/KFM5KAIFA-METER

1-3:0.2.8(50)
0-0:1.0.0(250925103739S)
0-0:96.1.1(4530303539303030303036373037343231)
1-0:1.8.1(002522.504*kWh)
1-0:1.8.2(001411.653*kWh)
...
!91FC
```

## Configuration

Key settings can be modified in `include/config.h`:

- **Network Settings**: IP address, gateway, subnet
- **Hardware Pins**: W5500 CS/RST pins, P1 serial pins
- **Connection Limits**: Max clients, timeout values
- **Buffer Sizes**: P1 message and Ethernet buffer sizes

## Troubleshooting

### Network Issues
- Check Ethernet cable connection
- Verify network settings match your LAN configuration
- Monitor serial output for IP assignment messages

### P1 Connection Issues
- Verify P1 cable wiring (especially data and ground pins)
- Check smart meter P1 port is enabled
- Monitor serial output for P1 message reception

### Client Connection Issues
- Ensure firewall allows connections to port 2000
- Check if maximum connections (3) is reached
- Verify telnet client settings

## LED Status Indicators

- **Rapid Blinking**: Ethernet initialization failed
- **Slow Blinking (1Hz)**: Normal operation, ready for connections
- **Solid On**: System startup/initialization

## Compatibility

This project is designed for:
- **Dutch Smart Meters** with P1 port (DSMR 4.0/5.0)
- **115200 baud** serial communication
- **Standard Ethernet networks** (10/100 Mbps)

## License

This project is open source. Feel free to modify and adapt for your specific needs.