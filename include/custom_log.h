#ifndef CUSTOM_LOG_H
#define CUSTOM_LOG_H

#include <Arduino.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>

// Helper function to build log messages
void sendRemoteLog(const char* level, const String& message);

// Macros that use the original DebugLog and also send to remote clients
#define REMOTE_LOG_INFO(...) do { LOG_INFO(__VA_ARGS__); sendRemoteLog("INFO", buildLogMessage(__VA_ARGS__)); } while(0)
#define REMOTE_LOG_DEBUG(...) do { LOG_DEBUG(__VA_ARGS__); sendRemoteLog("DEBUG", buildLogMessage(__VA_ARGS__)); } while(0)
#define REMOTE_LOG_WARN(...) do { LOG_WARN(__VA_ARGS__); sendRemoteLog("WARN", buildLogMessage(__VA_ARGS__)); } while(0)
#define REMOTE_LOG_ERROR(...) do { LOG_ERROR(__VA_ARGS__); sendRemoteLog("ERROR", buildLogMessage(__VA_ARGS__)); } while(0)

// Helper function to build log messages from multiple parameters
String buildLogMessage(const char* msg);
String buildLogMessage(const char* msg, int value);
String buildLogMessage(const char* msg, unsigned long value);
String buildLogMessage(const char* msg, const String& value);
String buildLogMessage(const char* msg, IPAddress ip);
String buildLogMessage(const char* msg, int v1, const char* msg2, IPAddress ip, const char* msg3);
String buildLogMessage(const char* msg, unsigned long v1, const char* msg2, unsigned int v2, const char* msg3);

#endif // CUSTOM_LOG_H
