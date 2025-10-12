#ifndef NETWORK_INIT_H
#define NETWORK_INIT_H

#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include "config.h"

// Network configuration
extern byte mac[];

// W5500 Interrupt handling
extern volatile bool w5500InterruptFlag;

// Function declarations
void w5500InterruptHandler();
void initializeNetwork();
bool isNetworkConnected();

#endif // NETWORK_INIT_H
