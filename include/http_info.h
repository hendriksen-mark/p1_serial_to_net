#ifndef HTTP_INFO_H
#define HTTP_INFO_H

#include <Arduino.h>
#include <Ethernet.h>
#include "config.h"

// HTTP info server management
extern EthernetServer httpInfoServer;

// Function declarations
void initializeHTTPInfoServer();
void handleHTTPInfoConnections();
void handleHTTPRequest(EthernetClient& client);
void sendHTTPResponse(EthernetClient& client, int statusCode, const String& contentType, const String& content);
void sendInfoPage(EthernetClient& client);
void sendP1DataPage(EthernetClient& client);
void sendP1DataStream(EthernetClient& client);
void sendLogsPage(EthernetClient& client);
void sendLogsDataStream(EthernetClient& client);
String getCurrentP1DataJSON();
String getCurrentLogsDataJSON();
void sendRedirect(EthernetClient& client, const String& location);
String getDeviceInfoHTML();
String getHTTPHeaders(int statusCode, const String& contentType, int contentLength = -1);

// Statistics functions
unsigned long getHTTPRequestCount();

#endif // HTTP_INFO_H
