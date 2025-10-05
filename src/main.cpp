#include <Arduino.h>
#include <Ethernet.h>
#define DEBUGLOG_DEFAULT_LOG_LEVEL_INFO
#include <DebugLog.h>
#include "config.h"
#include "clients.h"
#include "p1_handler.h"
#include "led_status.h"
#include "network_init.h"
#include "diagnostics.h"

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

	LOG_INFO("P1 Serial-to-Network Bridge Starting...");

	// Initialize status LED
	initializeStatusLED();

	// Initialize network (W5500 Ethernet)
	initializeNetwork();

	// Initialize client management
	initializeClients();

	// Initialize P1 protocol handler
	initializeP1();

	LOG_INFO("Bridge ready!");
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

		// Handle new client connections
		handleNewConnections();

		// Handle client communication
		handleClientCommunication();

		// Clean up disconnected clients
		cleanupClients();
	}

	// Always read P1 data from serial (high priority)
	readP1Data();

	// Small delay to prevent overwhelming the system
	delay(1);
}
