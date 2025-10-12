#ifndef CLIENTS_H
#define CLIENTS_H

#include <Arduino.h>
#include <Ethernet.h>
#include "config.h"

// Client management
extern EthernetServer server;
extern EthernetClient clients[MAX_CONNECTIONS];
extern unsigned long clientLastActivity[MAX_CONNECTIONS];
extern bool clientConnected[MAX_CONNECTIONS];

// Statistics
extern unsigned long totalBytesSent;

// Function declarations
void initializeClients();
void handleNewConnections();
void sendToAllClients(const String& data);
void handleClientCommunication();
void cleanupClients();
int getConnectedClientCount();

#endif // CLIENTS_H
