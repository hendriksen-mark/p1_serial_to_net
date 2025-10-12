#ifndef P1_HANDLER_H
#define P1_HANDLER_H

#include <Arduino.h>
#include "config.h"

// P1 message buffer and state
extern String p1Buffer;
extern bool p1MessageComplete;

// Statistics
extern unsigned long totalP1Messages;
extern unsigned long totalBytesReceived;
extern unsigned long lastP1DataReceived;

// Function declarations
void initializeP1();
void readP1Data();

#endif // P1_HANDLER_H
