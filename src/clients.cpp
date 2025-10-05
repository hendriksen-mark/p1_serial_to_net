#include "clients.h"
#include "custom_log.h"
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

// Global variables
EthernetServer server(SERVER_PORT);
EthernetClient clients[MAX_CONNECTIONS];
unsigned long clientLastActivity[MAX_CONNECTIONS];
bool clientConnected[MAX_CONNECTIONS];

// Statistics
unsigned long totalBytesSent = 0;

void initializeClients() {
	// Initialize client arrays
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		clientConnected[i] = false;
		clientLastActivity[i] = 0;
	}

	// Start the server
	server.begin();
	REMOTE_LOG_INFO("P1 Server listening on port:", SERVER_PORT);
}

void handleNewConnections() {
	EthernetClient newClient = server.accept();
	if (newClient) {
		REMOTE_LOG_DEBUG("New client attempting to connect");

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

			REMOTE_LOG_DEBUG("Client connected on slot:", availableSlot);
			REMOTE_LOG_DEBUG("Client IP:", newClient.remoteIP());

			// Send telnet negotiation if needed
			// IAC WILL ECHO, IAC WILL SUPPRESS_GO_AHEAD
			clients[availableSlot].write(0xFF); // IAC
			clients[availableSlot].write(0xFB); // WILL
			clients[availableSlot].write(0x01); // ECHO
			clients[availableSlot].write(0xFF); // IAC
			clients[availableSlot].write(0xFB); // WILL
			clients[availableSlot].write(0x03); // SUPPRESS_GO_AHEAD

		} else {
			// No available slots - implement kick oldest user
			REMOTE_LOG_WARN("Max connections reached. Kicking oldest client.");
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
				REMOTE_LOG_DEBUG("Kicked client from slot:", oldestSlot);
			}

			// Accept new client in the freed slot
			clients[oldestSlot] = newClient;
			clientConnected[oldestSlot] = true;
			clientLastActivity[oldestSlot] = millis();

			REMOTE_LOG_INFO("New client connected on slot:", oldestSlot);
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
				REMOTE_LOG_DEBUG("Client timeout on slot:", i);
				clients[i].stop();
				clientConnected[i] = false;
			}
		}
	}
}

void cleanupClients() {
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i] && !clients[i].connected()) {
			REMOTE_LOG_DEBUG("Client disconnected from slot:", i);
			clientConnected[i] = false;
			clients[i].stop();
		}
	}
}

int getConnectedClientCount() {
	int count = 0;
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i]) {
			count++;
		}
	}
	return count;
}
