#include "log_server.h"
#include "custom_log.h"
#include "ntp_client.h"

// Global variables
EthernetServer logServer(LOG_SERVER_PORT);
EthernetClient logClients[MAX_LOG_CONNECTIONS];
unsigned long logClientLastActivity[MAX_LOG_CONNECTIONS];
bool logClientConnected[MAX_LOG_CONNECTIONS];

// Statistics
unsigned long totalLogMessages = 0;
unsigned long totalLogBytesSent = 0;

void initializeLogServer() {
	// Initialize log client arrays
	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		logClientConnected[i] = false;
		logClientLastActivity[i] = 0;
	}

	// Start the log server
	logServer.begin();
	REMOTE_LOG_INFO("Log server listening on port:", LOG_SERVER_PORT);
}

void handleNewLogConnections() {
	EthernetClient newLogClient = logServer.accept();
	if (newLogClient) {
		// Find available slot
		int availableSlot = -1;
		for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
			if (!logClientConnected[i]) {
				availableSlot = i;
				break;
			}
		}

		if (availableSlot >= 0) {
			// Accept the connection
			logClients[availableSlot] = newLogClient;
			logClientConnected[availableSlot] = true;
			logClientLastActivity[availableSlot] = millis();

			// Send welcome message
			String welcomeMsg = "# Connected to P1 Bridge Log Server\r\n";
			welcomeMsg += "# Time: " + getFormattedDateTime() + " (uptime: " + String(millis() / 1000) + "s)\r\n";
			welcomeMsg += "# NTP Status: " + String(isNTPTimeValid() ? "Synchronized" : "Not synced") + "\r\n";
			welcomeMsg += "# Log format: [TIMESTAMP] [LEVEL] MESSAGE\r\n";
			logClients[availableSlot].print(welcomeMsg);

		} else {
			// No available slots - kick oldest client
			// Find oldest client
			int oldestSlot = 0;
			unsigned long oldestActivity = logClientLastActivity[0];
			for (int i = 1; i < MAX_LOG_CONNECTIONS; i++) {
				if (logClientConnected[i] && logClientLastActivity[i] < oldestActivity) {
					oldestActivity = logClientLastActivity[i];
					oldestSlot = i;
				}
			}

			// Disconnect oldest client
			if (logClientConnected[oldestSlot]) {
				logClients[oldestSlot].println("# Connection terminated: New log client connecting");
				logClients[oldestSlot].stop();
				logClientConnected[oldestSlot] = false;
			}

			// Accept new client in the freed slot
			logClients[oldestSlot] = newLogClient;
			logClientConnected[oldestSlot] = true;
			logClientLastActivity[oldestSlot] = millis();

			// Send welcome message
			String welcomeMsg = "# Connected to P1 Bridge Log Server\r\n";
			welcomeMsg += "# Time: " + getFormattedDateTime() + " (uptime: " + String(millis() / 1000) + "s)\r\n";
			welcomeMsg += "# NTP Status: " + String(isNTPTimeValid() ? "Synchronized" : "Not synced") + "\r\n";
			welcomeMsg += "# Log format: [TIMESTAMP] [LEVEL] MESSAGE\r\n";
			logClients[oldestSlot].print(welcomeMsg);
		}
	}
}

void sendToAllLogClients(const String& logMessage) {
	if (getConnectedLogClientCount() == 0) return; // No clients, skip processing

	// Format log message with timestamp
	String timestamp = isNTPTimeValid() ? getFormattedDateTime() : ("+" + String(millis() / 1000) + "s");
	String formattedMessage = "[" + timestamp + "] " + logMessage + "\r\n";

	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		if (logClientConnected[i] && logClients[i].connected()) {
			logClients[i].print(formattedMessage);
			logClientLastActivity[i] = millis();
			totalLogBytesSent += formattedMessage.length();
		}
	}
	totalLogMessages++;
}

void handleLogClientCommunication() {
	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		if (logClientConnected[i] && logClients[i].connected()) {
			// Read any data from log clients (usually just keep-alive or commands)
			if (logClients[i].available()) {
				logClientLastActivity[i] = millis();

				// Read and discard data (log clients usually don't send much)
				while (logClients[i].available()) {
					logClients[i].read();
				}
			}

			// Check for client timeout
			if (millis() - logClientLastActivity[i] > LOG_CLIENT_TIMEOUT) {
				logClients[i].stop();
				logClientConnected[i] = false;
			}
		}
	}
}

void cleanupLogClients() {
	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		if (logClientConnected[i] && !logClients[i].connected()) {
			logClientConnected[i] = false;
			logClients[i].stop();
		}
	}
}

int getConnectedLogClientCount() {
	int count = 0;
	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		if (logClientConnected[i]) {
			count++;
		}
	}
	return count;
}

// Function to send log messages to remote clients
void logToRemoteClients(const char* message) {
	if (getConnectedLogClientCount() > 0) {
		sendToAllLogClients(String(message));
	}
}
