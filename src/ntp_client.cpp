#include "ntp_client.h"
#include "custom_log.h"

// Global NTP variables (no permanent UDP socket)
NTPTime ntpTime = {0, 0, false};
unsigned long lastNTPUpdate = 0;

void initializeNTP() {
    if (!NTP_ENABLED) return;
    
    // No permanent socket allocation - use temporary approach
    REMOTE_LOG_INFO("NTP client initialized (temporary socket mode)");
    
    // Try to get initial time
    if (updateNTPTime()) {
        REMOTE_LOG_INFO("Initial NTP time synchronized");
    } else {
        REMOTE_LOG_WARN("Failed to get initial NTP time");
    }
}

bool updateNTPTime() {
    if (!NTP_ENABLED) return false;
    
    // Create temporary UDP socket for this NTP request
    EthernetUDP tempNtpUdp;
    if (!tempNtpUdp.begin(8888)) {
        REMOTE_LOG_WARN("Failed to create temporary NTP socket");
        return false;
    }
    
    // Send NTP packet
    byte packetBuffer[NTP_PACKET_SIZE];
    memset(packetBuffer, 0, NTP_PACKET_SIZE);
    
    // Initialize values needed to form NTP request
    packetBuffer[0] = 0b11100011;   // LI, Version, Mode
    packetBuffer[1] = 0;     // Stratum, or type of clock
    packetBuffer[2] = 6;     // Polling Interval
    packetBuffer[3] = 0xEC;  // Peer Clock Precision
    
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;
    
    // Send packet to NTP server
    IPAddress ntpServerIP;
    // Use Google's public NTP server IP (time.google.com)
    ntpServerIP = IPAddress(216, 239, 35, 0);
    REMOTE_LOG_DEBUG(("Using NTP server: " + ntpServerIP.toString()).c_str());
    
    tempNtpUdp.beginPacket(ntpServerIP, 123); // NTP requests are to port 123
    tempNtpUdp.write(packetBuffer, NTP_PACKET_SIZE);
    tempNtpUdp.endPacket();
    
    // Wait for response
    unsigned long startTime = millis();
    bool success = false;
    
    while (millis() - startTime < NTP_TIMEOUT) {
        if (tempNtpUdp.parsePacket()) {
            // Read packet
            tempNtpUdp.read(packetBuffer, NTP_PACKET_SIZE);
            
            // Extract timestamp from bytes 40-43 (seconds) and 44-47 (fraction)
            unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
            unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
            unsigned long secsSince1900 = highWord << 16 | lowWord;
            
            // Convert to Unix timestamp
            ntpTime.epoch = secsSince1900 - SEVENTY_YEARS;
            ntpTime.lastUpdate = millis();
            ntpTime.valid = true;
            lastNTPUpdate = millis();
            
            String timeStr = getFormattedDateTime(ntpTime.epoch);
            REMOTE_LOG_INFO(("NTP time updated: " + timeStr).c_str());
            
            success = true;
            break;
        }
        delay(10);
    }
    
    // Close temporary socket to free up the socket for other uses
    tempNtpUdp.stop();
    
    if (!success) {
        REMOTE_LOG_WARN("NTP request timeout");
    }
    
    return success;
}

unsigned long getCurrentEpoch() {
    if (!ntpTime.valid) {
        return 0; // Time not available
    }
    
    // Calculate current time based on stored time + elapsed milliseconds
    unsigned long elapsed = (millis() - ntpTime.lastUpdate) / 1000;
    return ntpTime.epoch + elapsed;
}

String getFormattedTime(unsigned long epoch) {
    if (epoch == 0) {
        epoch = getCurrentEpoch();
    }
    
    if (epoch == 0) {
        return "00:00:00";
    }
    
    // Apply timezone offset
    epoch = getLocalEpoch(epoch);
    
    unsigned long hours = (epoch % 86400L) / 3600;
    unsigned long minutes = (epoch % 3600) / 60;
    unsigned long seconds = epoch % 60;
    
    String timeStr = "";
    if (hours < 10) timeStr += "0";
    timeStr += String(hours) + ":";
    if (minutes < 10) timeStr += "0";
    timeStr += String(minutes) + ":";
    if (seconds < 10) timeStr += "0";
    timeStr += String(seconds);
    
    return timeStr;
}

String getFormattedDateTime(unsigned long epoch) {
    if (epoch == 0) {
        epoch = getCurrentEpoch();
    }
    
    if (epoch == 0) {
        return "0000-00-00 00:00:00";
    }
    
    // Apply timezone offset
    epoch = getLocalEpoch(epoch);
    
    // Calculate date components
    unsigned long days = epoch / 86400L;
    unsigned long years = 1970;
    
    // Simple year calculation (not accounting for leap years for simplicity)
    while (days >= 365) {
        if (((years % 4 == 0) && (years % 100 != 0)) || (years % 400 == 0)) {
            if (days >= 366) {
                days -= 366;
                years++;
            } else {
                break;
            }
        } else {
            days -= 365;
            years++;
        }
    }
    
    // Month days (non-leap year)
    int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (((years % 4 == 0) && (years % 100 != 0)) || (years % 400 == 0)) {
        monthDays[1] = 29; // Leap year
    }
    
    int month = 1;
    while (month <= 12 && days >= monthDays[month - 1]) {
        days -= monthDays[month - 1];
        month++;
    }
    
    int day = days + 1;
    
    // Time components
    unsigned long timeOfDay = epoch % 86400L;
    unsigned long hours = timeOfDay / 3600;
    unsigned long minutes = (timeOfDay % 3600) / 60;
    unsigned long seconds = timeOfDay % 60;
    
    // Format as YYYY-MM-DD HH:MM:SS
    String dateTime = String(years) + "-";
    if (month < 10) dateTime += "0";
    dateTime += String(month) + "-";
    if (day < 10) dateTime += "0";
    dateTime += String(day) + " ";
    if (hours < 10) dateTime += "0";
    dateTime += String(hours) + ":";
    if (minutes < 10) dateTime += "0";
    dateTime += String(minutes) + ":";
    if (seconds < 10) dateTime += "0";
    dateTime += String(seconds);
    
    return dateTime;
}

bool isNTPTimeValid() {
    return ntpTime.valid;
}

void handleNTPUpdate() {
    if (!NTP_ENABLED) return;
    
    // Update time every NTP_UPDATE_INTERVAL
    if (millis() - lastNTPUpdate > NTP_UPDATE_INTERVAL) {
        REMOTE_LOG_DEBUG("Performing scheduled NTP update");
        updateNTPTime();
    }
}

bool isDST(unsigned long epoch) {
    // Simple DST calculation for Europe (last Sunday in March to last Sunday in October)
    // This is a simplified version - real DST calculation is more complex
    
    // Apply timezone to get local time for DST calculation
    epoch += (NTP_TIMEZONE_OFFSET * 3600);
    
    // Extract month and day
    unsigned long days = epoch / 86400L;
    unsigned long years = 1970;
    
    while (days >= 365) {
        if (((years % 4 == 0) && (years % 100 != 0)) || (years % 400 == 0)) {
            if (days >= 366) {
                days -= 366;
                years++;
            } else {
                break;
            }
        } else {
            days -= 365;
            years++;
        }
    }
    
    int monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (((years % 4 == 0) && (years % 100 != 0)) || (years % 400 == 0)) {
        monthDays[1] = 29;
    }
    
    int month = 1;
    while (month <= 12 && days >= monthDays[month - 1]) {
        days -= monthDays[month - 1];
        month++;
    }
    
    // DST is active from last Sunday in March to last Sunday in October
    if (month >= 4 && month <= 9) {
        return true;  // Definitely DST
    } else if (month < 3 || month > 10) {
        return false; // Definitely not DST
    }
    
    // For March and October, we'd need to calculate the exact Sunday
    // For simplicity, assume DST from March 25 to October 25
    int day = days + 1;
    if (month == 3 && day >= 25) return true;
    if (month == 10 && day <= 25) return true;
    
    return false;
}

unsigned long getLocalEpoch(unsigned long utcEpoch) {
    if (utcEpoch == 0) {
        utcEpoch = getCurrentEpoch();
    }
    
    if (utcEpoch == 0) {
        return 0;
    }
    
    // Add timezone offset
    unsigned long localEpoch = utcEpoch + (NTP_TIMEZONE_OFFSET * 3600);
    
    // Add DST offset if applicable
    if (isDST(utcEpoch)) {
        localEpoch += (NTP_DST_OFFSET * 3600);
    }
    
    return localEpoch;
}