#include "diagnostics.h"
#include "clients.h"
#include "p1_handler.h"
#include <Ethernet.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

void printStatus() {
	LOG_INFO("=== Bridge Status ===");
	LOG_INFO("IP Address:", Ethernet.localIP());
	LOG_INFO("Server Port:", SERVER_PORT);
	LOG_INFO("Connected Clients:");
	int connectedCount = getConnectedClientCount();
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i]) {
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
