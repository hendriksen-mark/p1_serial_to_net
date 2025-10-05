#include "network_init.h"
#include "led_status.h"
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

// Network configuration - using DHCP
byte mac[] = W5500_MAC_ADDRESS;

// W5500 Interrupt handling
volatile bool w5500InterruptFlag = false;

// W5500 interrupt service routine
void w5500InterruptHandler() {
	w5500InterruptFlag = true;
}

void initializeNetwork() {
	// Initialize W5500 reset pin
	if (W5500_RST_PIN >= 0) {
		pinMode(W5500_RST_PIN, OUTPUT);
		digitalWrite(W5500_RST_PIN, LOW);
		delay(10);
		digitalWrite(W5500_RST_PIN, HIGH);
		delay(100);
	}

	// Initialize W5500 interrupt pin
	if (W5500_INT_PIN >= 0) {
		pinMode(W5500_INT_PIN, INPUT_PULLUP);
		attachInterrupt(digitalPinToInterrupt(W5500_INT_PIN), w5500InterruptHandler, FALLING);
		LOG_DEBUG("W5500 interrupt enabled on GPIO:", W5500_INT_PIN);
	}

	// Initialize SPI for W5500
	SPI.begin();

	// Initialize Ethernet with W5500
	Ethernet.init(W5500_CS_PIN);

	LOG_DEBUG("Starting Ethernet connection...");
	LOG_DEBUG("Requesting IP address from DHCP...");

	// Configure DHCP
	if (Ethernet.begin(mac) == 0) {
		LOG_ERROR("Failed to configure Ethernet using DHCP");
		// Indicate failure with rapid LED blinking (red)
		while (true) {
			setStatusLEDColor(255, 0, 0); // Red
			delay(100);
			setStatusLEDColor(0, 0, 0); // Off
			delay(100);
		}
	}

	// Check if Ethernet hardware is found
	if (Ethernet.hardwareStatus() == EthernetNoHardware) {
		LOG_ERROR("Ethernet shield was not found. Check wiring.");
		while (true) {
			setStatusLEDColor(255, 255, 0); // Yellow
			delay(200);
			setStatusLEDColor(0, 0, 0); // Off
			delay(200);
		}
	}

	// Check if cable is connected
	if (Ethernet.linkStatus() == LinkOFF) {
		LOG_ERROR("Ethernet cable is not connected.");
	}

	LOG_INFO("DHCP IP assigned:", Ethernet.localIP());
	LOG_INFO("Gateway:", Ethernet.gatewayIP());
	LOG_INFO("Subnet:", Ethernet.subnetMask());
	LOG_INFO("DNS:", Ethernet.dnsServerIP());

	// Format MAC address for logging
	char macStr[18];
	sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	LOG_INFO("MAC Address:", macStr);
}

bool isNetworkConnected() {
	return (Ethernet.linkStatus() == LinkON);
}
