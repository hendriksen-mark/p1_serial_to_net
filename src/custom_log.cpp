#include "custom_log.h"
#include "log_server.h"
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

// Send log message to remote clients
void sendRemoteLog(const char* level, const String& message) {
	if (getConnectedLogClientCount() > 0) {
		String formattedMessage = String("[") + level + "] " + message;
		logToRemoteClients(formattedMessage.c_str());
	}
}

// Helper functions to build log messages from multiple parameters
String buildLogMessage(const char* msg) {
	return String(msg);
}

String buildLogMessage(const char* msg, int value) {
	return String(msg) + " " + String(value);
}

String buildLogMessage(const char* msg, unsigned long value) {
	return String(msg) + " " + String(value);
}

String buildLogMessage(const char* msg, const String& value) {
	return String(msg) + " " + value;
}

String buildLogMessage(const char* msg, IPAddress ip) {
	return String(msg) + " " + ip.toString();
}

String buildLogMessage(const char* msg, int v1, const char* msg2, IPAddress ip, const char* msg3) {
	return String(msg) + String(v1) + String(msg2) + ip.toString() + String(msg3);
}

String buildLogMessage(const char* msg, unsigned long v1, const char* msg2, unsigned int v2, const char* msg3) {
	return String(msg) + String(v1) + String(msg2) + String(v2) + String(msg3);
}
