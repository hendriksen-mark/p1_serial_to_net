#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Adafruit_NeoPixel.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>
#include "config.h"

// NeoPixel LED setup
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// Network configuration - using DHCP
byte mac[] = W5500_MAC_ADDRESS;

// Server and client management
EthernetServer server(SERVER_PORT);
EthernetClient clients[MAX_CONNECTIONS];
unsigned long clientLastActivity[MAX_CONNECTIONS];
bool clientConnected[MAX_CONNECTIONS];

// P1 message buffer
String p1Buffer = "";
bool p1MessageComplete = false;

// Status LED
unsigned long lastLedBlink = 0;
bool ledState = false;
unsigned long lastP1DataReceived = 0;
bool p1DataIndicator = false;

// W5500 Interrupt handling
volatile bool w5500InterruptFlag = false;

// W5500 interrupt service routine
void w5500InterruptHandler() {
	w5500InterruptFlag = true;
}

// Statistics
unsigned long totalP1Messages = 0;
unsigned long totalBytesReceived = 0;
unsigned long totalBytesSent = 0;

void handleNewConnections() {
	EthernetClient newClient = server.accept();
	if (newClient) {
		LOG_DEBUG("New client attempting to connect");
		
		// Find available slot
		int availableSlot = -1;
		for (int i = 0; i < MAX_CONNECTIONS; i++) {
			if (!clientConnected[i]) {
				availableSlot = i;
				break;
			}
		}
		
		if (availableSlot >= 0) {
			// Accept the connection
			clients[availableSlot] = newClient;
			clientConnected[availableSlot] = true;
			clientLastActivity[availableSlot] = millis();

			LOG_DEBUG("Client connected on slot:", availableSlot);
			LOG_DEBUG("Client IP:", newClient.remoteIP());

			// Send telnet negotiation if needed
			// IAC WILL ECHO, IAC WILL SUPPRESS_GO_AHEAD
			clients[availableSlot].write(0xFF); // IAC
			clients[availableSlot].write(0xFB); // WILL
			clients[availableSlot].write(0x01); // ECHO
			clients[availableSlot].write(0xFF); // IAC
			clients[availableSlot].write(0xFB); // WILL
			clients[availableSlot].write(0x03); // SUPPRESS_GO_AHEAD
			
		} else {
			// No available slots - implement kick old user
			Serial.println("No available slots, kicking oldest client");
			
			// Find oldest client
			int oldestSlot = 0;
			unsigned long oldestActivity = clientLastActivity[0];
			for (int i = 1; i < MAX_CONNECTIONS; i++) {
				if (clientConnected[i] && clientLastActivity[i] < oldestActivity) {
					oldestActivity = clientLastActivity[i];
					oldestSlot = i;
				}
			}
			
			// Disconnect oldest client
			if (clientConnected[oldestSlot]) {
				clients[oldestSlot].println("Connection terminated: New client connecting");
				clients[oldestSlot].stop();
				clientConnected[oldestSlot] = false;
				LOG_DEBUG("Kicked client from slot:", oldestSlot);
			}
			
			// Accept new client in the freed slot
			clients[oldestSlot] = newClient;
			clientConnected[oldestSlot] = true;
			clientLastActivity[oldestSlot] = millis();

			LOG_INFO("New client connected on slot:", oldestSlot);
		}
	}
}

void sendToAllClients(const String& data) {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i] && clients[i].connected()) {
			clients[i].print(data);
			clientLastActivity[i] = millis();
			totalBytesSent += data.length();
		}
	}
}

void readP1Data() {
	while (Serial1.available()) {
		char c = Serial1.read();
		
		if (c == '/') {
			// Start of new P1 message
			p1Buffer = "/";
			p1MessageComplete = false;
		} else if (c == '!') {
			// End marker, read the checksum
			p1Buffer += c;
			// Read the 4-character checksum
			unsigned long timeout = millis() + 100;
			int checksumChars = 0;
			while (checksumChars < 4 && millis() < timeout) {
				if (Serial1.available()) {
					char checksumChar = Serial1.read();
					p1Buffer += checksumChar;
					checksumChars++;
				}
			}
			
			// Read final CR/LF
			timeout = millis() + 50;
			while (millis() < timeout && Serial1.available()) {
				char endChar = Serial1.read();
				if (endChar == '\r' || endChar == '\n') {
					p1Buffer += endChar;
					if (endChar == '\n') {
						p1MessageComplete = true;
						break;
					}
				}
			}
			
			if (p1MessageComplete) {
				// Send complete P1 message to all connected clients
				sendToAllClients(p1Buffer);
				totalP1Messages++;
				totalBytesReceived += p1Buffer.length();
				
				// Mark P1 data received for LED indication
				lastP1DataReceived = millis();
				p1DataIndicator = true;

				LOG_DEBUG("P1 message #", totalP1Messages, " sent to clients (", p1Buffer.length(), " bytes)");
				p1Buffer = "";
				p1MessageComplete = false;
			}
		} else if (p1Buffer.length() > 0) {
			// Add character to buffer if we're in a message
			p1Buffer += c;
			
			// Prevent buffer overflow
			if (p1Buffer.length() > P1_BUFFER_SIZE) {
				LOG_WARN("P1 buffer overflow, resetting");
				p1Buffer = "";
				p1MessageComplete = false;
			}
		}
	}
}

void handleClientCommunication() {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i] && clients[i].connected()) {
			// Check for client data (commands sent to P1 meter)
			if (clients[i].available()) {
				clientLastActivity[i] = millis();
				
				// Read data from client and forward to P1 serial
				while (clients[i].available()) {
					char c = clients[i].read();
					Serial1.write(c);
				}
			}
			
			// Check for client timeout
			if (millis() - clientLastActivity[i] > CLIENT_TIMEOUT) {
				LOG_DEBUG("Client timeout on slot:", i);
				clients[i].stop();
				clientConnected[i] = false;
			}
		}
	}
}

void cleanupClients() {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i] && !clients[i].connected()) {
			LOG_DEBUG("Client disconnected from slot:", i);
			clientConnected[i] = false;
			clients[i].stop();
		}
	}
}

// Optional: Add a function to get connection status
void printStatus() {
	LOG_INFO("=== Bridge Status ===");
	LOG_INFO("IP Address:", Ethernet.localIP());
	LOG_INFO("Server Port:", SERVER_PORT);
	LOG_INFO("Connected Clients:");
	int connectedCount = 0;
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i]) {
			connectedCount++;
			LOG_INFO(" [", i, ": ", clients[i].remoteIP(), "]");
		}
	}
	LOG_INFO("P1 Messages:", totalP1Messages);
	LOG_INFO("Bytes Received:", totalBytesReceived);
	LOG_INFO("Bytes Sent:", totalBytesSent);
	LOG_INFO("Uptime:", millis() / 1000);
	LOG_INFO(" seconds");
	LOG_INFO("===================");
}

void setup() {
	// Initialize serial for debugging
	Serial.begin(115200);
	if (DEBUG_SERIAL) {
		LOG_SET_LEVEL(DebugLogLevel::LVL_TRACE);
	} else {
		LOG_SET_LEVEL(DebugLogLevel::LVL_INFO);
	}
	while (!Serial && millis() < 3000) {
		; // Wait for serial port to connect, but timeout after 3 seconds
	}

	// Initialize status LED (WS2812 NeoPixel)
	statusLed.begin();
	statusLed.setPixelColor(0, statusLed.Color(255, 0, 0)); // Red during startup
	statusLed.show();

	LOG_INFO("P1 Serial-to-Network Bridge Starting...");

	// Initialize client arrays
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		clientConnected[i] = false;
		clientLastActivity[i] = 0;
	}

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
			statusLed.setPixelColor(0, statusLed.Color(255, 0, 0)); // Red
			statusLed.show();
			delay(100);
			statusLed.setPixelColor(0, statusLed.Color(0, 0, 0)); // Off
			statusLed.show();
			delay(100);
		}
	}
	
	// Check if Ethernet hardware is found
	if (Ethernet.hardwareStatus() == EthernetNoHardware) {
		LOG_ERROR("Ethernet shield was not found. Check wiring.");
		while (true) {
			statusLed.setPixelColor(0, statusLed.Color(255, 255, 0)); // Yellow
			statusLed.show();
			delay(200);
			statusLed.setPixelColor(0, statusLed.Color(0, 0, 0)); // Off
			statusLed.show();
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
	
	// Start the server
	server.begin();
	LOG_INFO("Server listening on port:", SERVER_PORT);
	
	// Format MAC address for logging
	char macStr[18];
	sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X", 
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	LOG_INFO("MAC Address:", macStr);
	
	// Initialize P1 serial port (pins are predefined for Serial1)
	Serial1.begin(P1_BAUD_RATE, SERIAL_8N1);
	
	LOG_INFO("P1 Serial initialized at 115200 baud");
	LOG_INFO("Bridge ready!");

	statusLed.setPixelColor(0, statusLed.Color(0, 255, 0)); // Green to indicate ready
	statusLed.show();
}

void loop() {
	// Maintain Ethernet connection
	Ethernet.maintain();
	
	// Handle status LED blinking
	// Fast purple blink when P1 data received (300ms), then back to normal pattern
	if (p1DataIndicator && (millis() - lastP1DataReceived < 300)) {
		// Fast blink purple for P1 data indication
		if (millis() - lastLedBlink > 100) {
			ledState = !ledState;
			if (ledState) {
				statusLed.setPixelColor(0, statusLed.Color(128, 0, 128)); // Purple
			} else {
				statusLed.setPixelColor(0, statusLed.Color(0, 0, 0)); // Off
			}
			statusLed.show();
			lastLedBlink = millis();
		}
	} else {
		// Reset P1 data indicator after the blink period
		if (p1DataIndicator) {
			p1DataIndicator = false;
		}
		
		// Normal slow blink to show system activity (blue/green alternating)
		if (millis() - lastLedBlink > 1000) {
			ledState = !ledState;
			if (ledState) {
				statusLed.setPixelColor(0, statusLed.Color(0, 0, 255)); // Blue
			} else {
				statusLed.setPixelColor(0, statusLed.Color(0, 255, 0)); // Green
			}
			statusLed.show();
			lastLedBlink = millis();
		}
	}
	
	// Handle network events (optimized with interrupt)
	if (w5500InterruptFlag || (millis() % 100 == 0)) {
		// Process network events when interrupt occurs or every 100ms as fallback
		w5500InterruptFlag = false; // Clear the flag
		
		// Handle new client connections
		handleNewConnections();
		
		// Handle client communication
		handleClientCommunication();
		
		// Clean up disconnected clients
		cleanupClients();
	}
	
	// Always read P1 data from serial (high priority)
	readP1Data();
	
	// Small delay to prevent overwhelming the system
	delay(1);
}
