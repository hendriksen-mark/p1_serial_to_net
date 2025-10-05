#ifndef LOG_SERVER_H
#define LOG_SERVER_H

#include <Arduino.h>
#include <Ethernet.h>
#include "config.h"

// Log server and client management
extern EthernetServer logServer;
extern EthernetClient logClients[MAX_LOG_CONNECTIONS];
extern unsigned long logClientLastActivity[MAX_LOG_CONNECTIONS];
extern bool logClientConnected[MAX_LOG_CONNECTIONS];

// Log statistics
extern unsigned long totalLogMessages;
extern unsigned long totalLogBytesSent;

// Function declarations
void initializeLogServer();
void handleNewLogConnections();
void sendToAllLogClients(const String& logMessage);
void handleLogClientCommunication();
void cleanupLogClients();
int getConnectedLogClientCount();
void logToRemoteClients(const char* message);

#endif // LOG_SERVER_H