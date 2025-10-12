#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <Arduino.h>
#include <Ethernet.h>
#include "config.h"

// OTA server state
enum OTAState {
	OTA_IDLE,
	OTA_RECEIVING,
	OTA_FLASHING,
	OTA_SUCCESS,
	OTA_ERROR
};

// OTA server management
extern EthernetServer otaServer;
extern OTAState otaState;
extern unsigned long otaStartTime;
extern unsigned long otaBytesReceived;
extern unsigned long otaTotalBytes;

// Statistics
extern unsigned long totalOTAAttempts;
extern unsigned long successfulOTAUpdates;
extern unsigned long lastOTAUpdate;

// Function declarations
void handleOTAUpload(EthernetClient& client);
bool verifyOTACredentials(const String& authHeader);
void sendOTAResponse(EthernetClient& client, int statusCode, const String& message);
String getOTAStatus();
void resetOTAState();

// Statistics functions for diagnostics
unsigned long getOTARequestCount();
unsigned long getOTAUpdateCount();  
unsigned long getLastOTATimestamp();

#endif // OTA_SERVER_H
