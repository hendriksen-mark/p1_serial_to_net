# LED Status - P1 Serial to Network Bridge

### Primary Status Colors (Solid, Non-Blinking)
- **Green (Ready)**: System running, waiting for client connections
- **Blue (Active)**: One or more clients connected
  - Brightness increases with number of connected clients
  - Base blue: 60 brightness
  - Additional 30 brightness per client

### Activity Indication (Brightness Modulation Only)
- **P1 Data Activity**: Brief brightness boost for 200ms after receiving P1 data
  - No color change, just brightness boost (+100)
  - Subtle indication that doesn't disrupt primary status

### Heartbeat (Gentle Breathing Effect)
- **Breathing Pattern**: Smooth 3-second cycle
  - 1.5 seconds fade up, 1.5 seconds fade down
  - Gentle 20-point brightness variation
  - Indicates system is alive without being distracting

## Implementation Details

### Key Variables
- `lastLedUpdate`: Throttles LED updates to 50ms intervals for smooth effects
- `heartbeatTimer`: Tracks 3-second heartbeat cycle
- `heartbeatDirection`: Controls breathing fade direction
- `lastP1DataReceived`: Used for P1 activity brightness boost

### Brightness Calculations
- Base brightness: 25-40 depending on status
- Activity boost: +100 for 200ms after P1 data
- Heartbeat boost: +0 to +20 in smooth sine pattern
- Final brightness: Clamped to 255 maximum

### Color Strategy
- No rapid color changes during normal operation
- Colors indicate primary system state only
- All activity shown through brightness modulation
- Error states (if added later) can override with red/yellow

## Benefits
1. **Clear Status Reading**: Primary state always visible in color
2. **Subtle Activity**: P1 data activity shown without disruption  
3. **Calm Operation**: No chaotic flashing or rapid color changes
4. **Scalable**: Easy to add error states or additional status indicators
5. **Resource Efficient**: 50ms update interval prevents system overload

## Future Enhancements
- Error states: Red for critical errors, Yellow for warnings
- Network activity: Brief orange brightness boost for TCP traffic
- Configuration mode: Distinct pattern for setup/config states
