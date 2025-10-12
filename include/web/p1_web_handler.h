#ifndef P1_WEB_HANDLER_H
#define P1_WEB_HANDLER_H

#include "config.h"
#include <Ethernet.h>

// P1 page functions
void sendP1DataPage(EthernetClient& client);
void sendP1DataStream(EthernetClient& client);
String getCurrentP1DataJSON();

#endif