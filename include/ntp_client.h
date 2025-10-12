#ifndef NTP_CLIENT_H
#define NTP_CLIENT_H

#include "config.h"
#include <Ethernet.h>
#include <EthernetUdp.h>

// NTP time is seconds since 1900-01-01 00:00:00 UTC
// Unix time is seconds since 1970-01-01 00:00:00 UTC
// Difference is 70 years = 2208988800 seconds
#define SEVENTY_YEARS 2208988800UL

// NTP packet structure
#define NTP_PACKET_SIZE 48

struct NTPTime {
    unsigned long epoch;        // Unix timestamp (seconds since 1970)
    unsigned long lastUpdate;   // millis() when time was last updated
    bool valid;                 // Whether time is valid
};

// NTP client functions
void initializeNTP();
bool updateNTPTime();
unsigned long getCurrentEpoch();
String getFormattedTime(unsigned long epoch = 0);
String getFormattedDateTime(unsigned long epoch = 0);
bool isNTPTimeValid();
void handleNTPUpdate();

// Time utility functions
bool isDST(unsigned long epoch);
unsigned long getLocalEpoch(unsigned long utcEpoch = 0);

#endif // NTP_CLIENT_H