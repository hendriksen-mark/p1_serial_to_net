#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include <Adafruit_NeoPixel.h>
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
		Serial.println("New client attempting to connect");
		
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
			
			Serial.print("Client connected on slot ");
			Serial.print(availableSlot);
			Serial.print(" from ");
			Serial.println(newClient.remoteIP());
			
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
				Serial.print("Kicked client from slot ");
				Serial.println(oldestSlot);
			}
			
			// Accept new client in the freed slot
			clients[oldestSlot] = newClient;
			clientConnected[oldestSlot] = true;
			clientLastActivity[oldestSlot] = millis();
			
			Serial.print("New client connected on slot ");
			Serial.println(oldestSlot);
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
				
				Serial.print("P1 message #");
				Serial.print(totalP1Messages);
				Serial.print(" sent to clients (");
				Serial.print(p1Buffer.length());
				Serial.println(" bytes)");
				p1Buffer = "";
				p1MessageComplete = false;
			}
		} else if (p1Buffer.length() > 0) {
			// Add character to buffer if we're in a message
			p1Buffer += c;
			
			// Prevent buffer overflow
			if (p1Buffer.length() > P1_BUFFER_SIZE) {
				Serial.println("P1 buffer overflow, resetting");
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
				Serial.print("Client timeout on slot ");
				Serial.println(i);
				clients[i].stop();
				clientConnected[i] = false;
			}
		}
	}
}

void cleanupClients() {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i] && !clients[i].connected()) {
			Serial.print("Client disconnected from slot ");
			Serial.println(i);
			clientConnected[i] = false;
			clients[i].stop();
		}
	}
}

// Optional: Add a function to get connection status
void printStatus() {
	Serial.println("=== Bridge Status ===");
	Serial.print("IP Address: ");
	Serial.println(Ethernet.localIP());
	Serial.print("Server Port: ");
	Serial.println(SERVER_PORT);
	Serial.print("Connected Clients: ");
	int connectedCount = 0;
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i]) {
			connectedCount++;
			Serial.print(" [");
			Serial.print(i);
			Serial.print(": ");
			Serial.print(clients[i].remoteIP());
			Serial.print("]");
		}
	}
	Serial.println();
	Serial.print("P1 Messages: ");
	Serial.println(totalP1Messages);
	Serial.print("Bytes Received: ");
	Serial.println(totalBytesReceived);
	Serial.print("Bytes Sent: ");
	Serial.println(totalBytesSent);
	Serial.print("Uptime: ");
	Serial.print(millis() / 1000);
	Serial.println(" seconds");
	Serial.println("===================");
}

void setup() {
	// Initialize serial for debugging
	Serial.begin(115200);
	while (!Serial && millis() < 3000) {
		; // Wait for serial port to connect, but timeout after 3 seconds
	}
	
	// Initialize status LED (WS2812 NeoPixel)
	statusLed.begin();
	statusLed.setPixelColor(0, statusLed.Color(255, 0, 0)); // Red during startup
	statusLed.show();
	
	Serial.println("P1 Serial-to-Network Bridge Starting...");
	
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
		Serial.print("W5500 interrupt enabled on GPIO ");
		Serial.println(W5500_INT_PIN);
	}
	
	// Initialize SPI for W5500
	SPI.begin();
	
	// Initialize Ethernet with W5500
	Ethernet.init(W5500_CS_PIN);
	
	Serial.println("Starting Ethernet connection...");
	Serial.println("Requesting IP address from DHCP...");
	
	// Configure DHCP
	if (Ethernet.begin(mac) == 0) {
		Serial.println("Failed to configure Ethernet using DHCP");
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
		Serial.println("Ethernet shield was not found. Check wiring.");
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
		Serial.println("Ethernet cable is not connected.");
	}
	
	Serial.print("DHCP IP assigned: ");
	Serial.println(Ethernet.localIP());
	Serial.print("Gateway: ");
	Serial.println(Ethernet.gatewayIP());
	Serial.print("Subnet: ");
	Serial.println(Ethernet.subnetMask());
	Serial.print("DNS: ");
	Serial.println(Ethernet.dnsServerIP());
	
	// Start the server
	server.begin();
	Serial.print("Server listening on port ");
	Serial.println(SERVER_PORT);
	Serial.print("MAC Address: ");
	for (int i = 0; i < 6; i++) {
		if (i > 0) Serial.print(":");
		if (mac[i] < 16) Serial.print("0");
		Serial.print(mac[i], HEX);
	}
	Serial.println();
	
	// Initialize P1 serial port (pins are predefined for Serial1)
	Serial1.begin(P1_BAUD_RATE, SERIAL_8N1);
	
	Serial.println("P1 Serial initialized at 115200 baud");
	Serial.println("Bridge ready!");
	
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
