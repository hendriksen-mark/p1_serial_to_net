#ifndef CONFIG_H
#define CONFIG_H

// Hardware Configuration for Waveshare RP2040 Zero + W5500 Lite
// Available GPIO pins: 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,26,27,28,29
// Reserved pins:
// - GPIO0: Serial1 TX (P1 port communication)
// - GPIO1: Serial1 RX (P1 port communication)
// - GPIO2: SPI0 SCK (for W5500)
// - GPIO3: SPI0 MOSI (for W5500)  
// - GPIO4: SPI0 MISO (for W5500)
// - GPIO5: W5500 Chip Select
// - GPIO6: W5500 Reset
// - GPIO7: W5500 Interrupt (optional)
// - GPIO16: WS2812 NeoPixel LED (onboard)
#define W5500_CS_PIN    5     // Chip Select pin for W5500 (can use GPIO5 as it's SPI0_SS default)
#define W5500_RST_PIN   6     // Reset pin for W5500
#define W5500_INT_PIN   7     // Interrupt pin for W5500 (optional but recommended for better performance)
							  // INT pin improves efficiency by triggering network event processing
							  // instead of constant polling. Set to -1 to disable.
							  
#define W5500_MAC_ADDRESS {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED} // Default MAC address (change if needed)

// P1 Serial Configuration
// ⚠️  CRITICAL VOLTAGE WARNING ⚠️
// P1 port Pin 5 (data) outputs 5V TTL levels but RP2040 is 3.3V!
// YOU MUST USE LEVEL SHIFTING to avoid damaging the RP2040:
//
// Option 1 - Voltage Divider (simple):
//   P1 Pin 5 --[10kΩ]--+--[6.8kΩ]-- GND
//                      |
//                      +----> RP2040 GPIO1 (RX)
//
// Option 2 - Level Shifter IC (recommended):
//   Use 74LVC1T45 or 74LV4050 between P1 Pin 5 and GPIO1
//
// Option 3 - Mosfet:
//   Use a N-channel MOSFET (e.g., 2N7000) as a level shifter circuit
//	 P1 Pin 1			RP2040 3v3
//		|					|
//	   10kΩ					4k7
//		|					| ---------------> RP2040 GPIO1 (RX)
//		|			  	 [drain]
//	 P1 Pin 5 -----[gate][2n7000]
//					   	 [source]
//							|
//						   GND(P1 Pin 3,6) --> RP2040 GND
//
// P1 Port Connections:
// Pin 1: +5V (optional, for powering external circuits)
// Pin 2: Data Request (P1 have internal current limit resistor, high=request data)
// Pin 3: Data GND (connect to RP2040 GND)
// Pin 4: NC (not connected)
// Pin 5: Data (5V levels - NEEDS LEVEL SHIFTING to GPIO1 RX)
// Pin 6: Power GND (connect to RP2040 GND)
//
// Note: Serial1 uses predefined pins on RP2040 (GPIO0=TX, GPIO1=RX)
// For Waveshare RP2040 Zero: Serial1 RX=GPIO1, TX=GPIO0
#define P1_BAUD_RATE    115200
#define P1_DATA_REQUEST_PIN  29  // Optional: GPIO to control P1 data request (Pin 2)
								 // Set to -1 if not used (most meters transmit continuously)

// Network Configuration - Using DHCP
// IP address will be automatically assigned by your router/modem

// Server Configuration
#define SERVER_PORT     2000
#define MAX_CONNECTIONS 3
#define CLIENT_TIMEOUT  30000  // 30 seconds in milliseconds

// Log Server Configuration
#define LOG_SERVER_PORT     2001
#define MAX_LOG_CONNECTIONS 2
#define LOG_CLIENT_TIMEOUT  60000  // 60 seconds in milliseconds (longer for log clients)

// Buffer Configuration
#define P1_BUFFER_SIZE  2048   // Maximum P1 message size
#define ETHERNET_BUFFER_SIZE 1024

// Debug Configuration
#define DEBUG_SERIAL    true
#define STATUS_LED_PIN  PIN_NEOPIXEL    // GPIO16 - WS2812 NeoPixel LED (onboard)

// SPI Configuration for W5500
// Using SPI0 (default): MISO=4, MOSI=3, SCK=2, SS=5 (from pins_arduino.h)

// Still available pins for future expansion: 9,10,11,12,13,14,15,26,27,28,29
// Note: GPIO29 is optionally used for P1 data request

// P1 Protocol Configuration
#define P1_START_CHAR   '/'
#define P1_END_CHAR     '!'
#define P1_CHECKSUM_LEN 4

#endif // CONFIG_H