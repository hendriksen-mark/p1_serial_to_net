#include <Arduino.h>
#include <Ethernet.h>
#include <LittleFS.h>
#include "config.h"
#include "clients.h"
#include "p1_handler.h"
#include "led_status.h"
#include "network_init.h"
#include "diagnostics.h"
#include "log_server.h"
#include "custom_log.h"
#include "http_info.h"
#include "ntp_client.h"

void setup() {
	// Initialize serial for debugging
	Serial.begin(115200);
	if (DEBUG_SERIAL) {
		LOG_SET_LEVEL(DebugLogLevel::LVL_TRACE);
	} else {
		LOG_SET_LEVEL(DebugLogLevel::LVL_INFO);
	}
	while (!Serial && millis() < 3000) {
		; // Wait for serial port to connect, but timeout after 3 seconds
	}

	REMOTE_LOG_INFO("P1 Serial-to-Network Bridge Starting...");

	// Initialize LittleFS for OTA support
	if (!LittleFS.begin()) {
		REMOTE_LOG_ERROR("LittleFS initialization failed - OTA updates may not work");
	} else {
		REMOTE_LOG_INFO("LittleFS initialized successfully for OTA support");
	}

	// Initialize status LED
	initializeStatusLED();

	// Initialize network (W5500 Ethernet)
	initializeNetwork();

	// Initialize client management
	initializeClients();

	// Initialize log server for remote debugging
	initializeLogServer();

	// Initialize HTTP info server for web interface (includes OTA endpoints)
	initializeHTTPInfoServer();

	// Initialize NTP time synchronization
	initializeNTP();

	// Initialize P1 protocol handler
	initializeP1();

	REMOTE_LOG_INFO("Bridge ready!");
	setStatusLEDColor(0, 255, 0); // Green to indicate ready
}

void loop() {
	// Maintain Ethernet connection
	Ethernet.maintain();

	// Update status LED with new smooth behavior
	updateStatusLED();

	// Handle network events (optimized with interrupt)
	if (w5500InterruptFlag || (millis() % 100 == 0)) {
		// Process network events when interrupt occurs or every 100ms as fallback
		w5500InterruptFlag = false; // Clear the flag

		// Handle new P1 client connections
		handleNewConnections();

		// Handle P1 client communication
		handleClientCommunication();

		// Clean up disconnected P1 clients
		cleanupClients();

		// Handle new log client connections
		handleNewLogConnections();

		// Handle log client communication
		handleLogClientCommunication();

		// Clean up disconnected log clients
		cleanupLogClients();

		// Handle HTTP info server requests (includes OTA endpoints)
		handleHTTPInfoConnections();
	}
	
	// Handle NTP time updates
	handleNTPUpdate();
	
	// Always read P1 data from serial (high priority)
	readP1Data();

	// Small delay to prevent overwhelming the system
	delay(1);
}
