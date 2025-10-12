#ifndef LOGS_WEB_HANDLER_H
#define LOGS_WEB_HANDLER_H

#include "config.h"
#include <Ethernet.h>

// Logs page functions
void sendLogsPage(EthernetClient& client);
void sendLogsDataStream(EthernetClient& client);
String getCurrentLogsDataJSON();

#endif