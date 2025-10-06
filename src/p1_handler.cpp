#include "p1_handler.h"
#include "clients.h"
#include "custom_log.h"

// P1 message buffer and state
String p1Buffer = "";
bool p1MessageComplete = false;

// Statistics
unsigned long totalP1Messages = 0;
unsigned long totalBytesReceived = 0;
unsigned long lastP1DataReceived = 0;

void initializeP1() {
	// Initialize P1 data request pin (optional)
	if (P1_DATA_REQUEST_PIN >= 0) {
		pinMode(P1_DATA_REQUEST_PIN, OUTPUT);
		digitalWrite(P1_DATA_REQUEST_PIN, HIGH); // Activate data transmission
		REMOTE_LOG_INFO("P1 data request enabled on GPIO:", P1_DATA_REQUEST_PIN);
	}

	// Initialize P1 serial port (pins are predefined for Serial1)
	// ⚠️ WARNING: Ensure Pin 5 from P1 port uses level shifting (5V->3.3V)
	Serial1.begin(P1_BAUD_RATE, SERIAL_8N1);

	REMOTE_LOG_INFO("P1 Serial initialized at 115200 baud");
	REMOTE_LOG_WARN("⚠️ ENSURE P1 Pin 5 uses level shifting (5V->3.3V) to avoid damage!");
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

				REMOTE_LOG_DEBUG("P1 message #", totalP1Messages, " sent to clients (", p1Buffer.length(), " bytes)");
				p1Buffer = "";
				p1MessageComplete = false;
			}
		} else if (p1Buffer.length() > 0) {
			// Add character to buffer if we're in a message
			p1Buffer += c;

			// Prevent buffer overflow
			if (p1Buffer.length() > P1_BUFFER_SIZE) {
				REMOTE_LOG_WARN("P1 buffer overflow, resetting");
				p1Buffer = "";
				p1MessageComplete = false;
			}
		}
	}
}
