#include "diagnostics.h"
#include "clients.h"
#include "p1_handler.h"
#include "log_server.h"
#include "custom_log.h"
#include <Ethernet.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

void printStatus() {
	REMOTE_LOG_INFO("=== Bridge Status ===");
	REMOTE_LOG_INFO("IP Address:", Ethernet.localIP());
	REMOTE_LOG_INFO("P1 Server Port:", SERVER_PORT);
	REMOTE_LOG_INFO("Log Server Port:", LOG_SERVER_PORT);
	REMOTE_LOG_INFO("Connected P1 Clients:");
	int connectedCount = getConnectedClientCount();
	for (int i = 0; i < MAX_CONNECTIONS; i++) {
		if (clientConnected[i]) {
			REMOTE_LOG_INFO(" [", i, ": ", clients[i].remoteIP(), "]");
		}
	}
	REMOTE_LOG_INFO("Connected Log Clients:");
	int logConnectedCount = getConnectedLogClientCount();
	for (int i = 0; i < MAX_LOG_CONNECTIONS; i++) {
		if (logClientConnected[i]) {
			REMOTE_LOG_INFO(" [", i, ": ", logClients[i].remoteIP(), "]");
		}
	}
	REMOTE_LOG_INFO("P1 Messages:", totalP1Messages);
	REMOTE_LOG_INFO("P1 Bytes Received:", totalBytesReceived);
	REMOTE_LOG_INFO("P1 Bytes Sent:", totalBytesSent);
	REMOTE_LOG_INFO("Log Messages Sent:", totalLogMessages);
	REMOTE_LOG_INFO("Log Bytes Sent:", totalLogBytesSent);
	REMOTE_LOG_INFO("Uptime:", millis() / 1000);
	REMOTE_LOG_INFO(" seconds");
	REMOTE_LOG_INFO("===================");
}
