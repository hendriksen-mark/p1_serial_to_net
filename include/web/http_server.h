#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "config.h"
#include <Ethernet.h>

// Global variables declaration
extern EthernetServer httpInfoServer;
extern unsigned long totalHTTPRequests;

// Core HTTP server functions
void initializeHTTPInfoServer();
void handleHTTPInfoConnections();
void handleHTTPRequest(EthernetClient& client);

// HTTP utilities
void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
void sendUnauthorizedResponse(EthernetClient& client, const String& realm);
String getHTTPHeaders(int statusCode, const String& contentType, int contentLength);

// Statistics
unsigned long getHTTPRequestCount();

// OTA page generation
String generateOTAUploadPage();

#endif