# Remote Logging Feature

The P1 Serial-to-Network Bridge now includes a **remote logging server** for remote error tracking and debugging. This allows you to monitor logs and debug issues over the network without needing physical access to the device.

## Configuration

The remote logging server is configured in `include/config.h`:

```cpp
// Log Server Configuration
#define LOG_SERVER_PORT     2001                  // Log server port
#define MAX_LOG_CONNECTIONS 2                     // Maximum concurrent log clients
#define LOG_CLIENT_TIMEOUT  60000                 // Log client timeout (60 seconds)
```

## Ports Overview

- **Port 2000**: P1 data server (original functionality)
- **Port 2001**: Remote logging server (new feature)

## How to Connect to the Log Server

### Using Telnet
```bash
telnet <bridge-ip-address> 2001
```

### Using Netcat
```bash
nc <bridge-ip-address> 2001
```

### Using PuTTY
1. Open PuTTY
2. Set Host Name: `<bridge-ip-address>`
3. Set Port: `2001`
4. Connection type: `Telnet` or `Raw`
5. Click "Open"

## Log Format

Logs are sent in the following format:
```
[TIMESTAMP] [LEVEL] MESSAGE
```

Example output:
```
# Connected to P1 Bridge Log Server
# Time: 123456ms
# Log format: [TIMESTAMP] [LEVEL] MESSAGE
[125000] [INFO] P1 Serial-to-Network Bridge Starting...
[125100] [INFO] P1 Server listening on port: 2000
[125150] [INFO] Log server listening on port: 2001
[125200] [INFO] DHCP IP assigned: 192.168.1.100
[125250] [INFO] Bridge ready!
[130000] [DEBUG] New client attempting to connect
[130050] [INFO] P1 message #1 sent to clients (256 bytes)
```

## Log Levels

- **INFO**: General information messages
- **DEBUG**: Detailed debugging information
- **WARN**: Warning messages
- **ERROR**: Error messages

## Features

### Client Management
- Up to 2 concurrent log clients supported
- Automatic client timeout (60 seconds of inactivity)
- Oldest client is disconnected when a new client connects and all slots are full

### Welcome Message
When connecting, clients receive:
- Connection confirmation
- Current timestamp
- Log format explanation

### Statistics
Log server statistics are included in the diagnostics:
- Number of connected log clients
- Total log messages sent
- Total log bytes transmitted

## Usage Examples

### Basic Monitoring
```bash
# Connect and monitor all logs in real-time
telnet 192.168.1.100 2001
```

### Log to File
```bash
# Save logs to a file for later analysis
nc 192.168.1.100 2001 > bridge_logs.txt
```

### Filter Specific Log Levels
```bash
# Monitor only ERROR and WARN messages
nc 192.168.1.100 2001 | grep -E "\[(ERROR|WARN)\]"
```

### Remote Debugging Session
```bash
# Monitor logs while performing debugging
nc 192.168.1.100 2001 | tee debug_session.log
```

## Integration with Existing Code

The remote logging feature integrates seamlessly with the existing DebugLog library. The system uses hybrid macros that send logs to both serial console and network clients:

### Standard DebugLog (Serial Only) - Legacy
```cpp
LOG_INFO("Standard log message");  // Serial console only
```

### Remote Log (Serial + Network) - Current Implementation
```cpp
REMOTE_LOG_INFO("Message sent to both serial and network clients");
REMOTE_LOG_DEBUG("Debug info:", variable);
REMOTE_LOG_WARN("Warning about:", condition);
REMOTE_LOG_ERROR("Error occurred:", errorCode);
```

### Automatic Dual Output
The `REMOTE_LOG_*` macros automatically:
1. Send the log to the serial console (via original LOG_* functions)
2. Format and send the same log to all connected network clients
3. Support multiple parameters just like the original DebugLog functions

**Note**: The bridge now uses `REMOTE_LOG_*` functions throughout the codebase, so all system logs are available both via serial console and the remote logging port.

## Network Architecture

```
┌─────────────────┐    ┌──────────────────────┐    ┌─────────────────┐
│   P1 Clients    │    │                      │    │  Log Clients    │
│  (Port 2000)    │◄──►│   P1 Bridge Device   │◄──►│  (Port 2001)    │
│                 │    │                      │    │                 │
└─────────────────┘    └──────────────────────┘    └─────────────────┘
                                │
                                ▼
                       ┌─────────────────┐
                       │   P1 Smart      │
                       │   Meter         │
                       └─────────────────┘
```

## Benefits

1. **Remote Debugging**: Debug issues without physical access
2. **Real-time Monitoring**: Monitor system status in real-time
3. **Log Archiving**: Save logs for later analysis
4. **Multiple Clients**: Support multiple monitoring sessions
5. **Network Integration**: Integrate with existing monitoring tools
6. **Minimal Overhead**: Logs only sent when clients are connected

## Security Note

The log server is currently unencrypted. Only use on trusted networks. For production environments, consider:
- VPN access
- Network segmentation
- Firewall rules
- SSH tunneling for encrypted access:
  ```bash
  ssh -L 2001:192.168.1.100:2001 user@gateway-server
  telnet localhost 2001
  ```