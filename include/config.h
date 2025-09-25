#ifndef CONFIG_H
#define CONFIG_H

// Hardware Configuration for Waveshare RP2040 Zero + W5500 Lite
#define W5500_CS_PIN    17    // Chip Select pin for W5500
#define W5500_RST_PIN   20    // Reset pin for W5500 (optional, set to -1 if not used)

// P1 Serial Configuration
// Note: Serial1 uses predefined pins on RP2040 (typically GPIO0=TX, GPIO1=RX)
// For Waveshare RP2040 Zero: Serial1 RX=GPIO1, TX=GPIO0
#define P1_BAUD_RATE    115200

// Network Configuration - Using DHCP
// IP address will be automatically assigned by your router/modem

// Server Configuration
#define SERVER_PORT     2000
#define MAX_CONNECTIONS 3
#define CLIENT_TIMEOUT  30000  // 30 seconds in milliseconds

// Buffer Configuration
#define P1_BUFFER_SIZE  2048   // Maximum P1 message size
#define ETHERNET_BUFFER_SIZE 1024

// Debug Configuration
#define DEBUG_SERIAL    true
#define STATUS_LED_PIN  25    // GPIO25 - Onboard LED for Waveshare RP2040 Zero

// P1 Protocol Configuration
#define P1_START_CHAR   '/'
#define P1_END_CHAR     '!'
#define P1_CHECKSUM_LEN 4

#endif // CONFIG_H