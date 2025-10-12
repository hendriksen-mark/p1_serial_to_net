#ifndef STATUS_WEB_HANDLER_H
#define STATUS_WEB_HANDLER_H

#include "config.h"
#include <Ethernet.h>

// Status and info page functions
void sendInfoPage(EthernetClient& client);
void handleStatusPage(EthernetClient& client);
void sendRedirect(EthernetClient& client, const String& location);
String getDeviceInfoHTML();

#endif