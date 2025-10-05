#include "led_status.h"
#include "clients.h"
#include "p1_handler.h"

// NeoPixel LED setup
Adafruit_NeoPixel statusLed(1, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// Status LED timing variables
unsigned long lastLedUpdate = 0;
unsigned long heartbeatTimer = 0;
uint8_t ledBrightness = 50;  // Base brightness (0-255)
bool heartbeatDirection = true;

void initializeStatusLED() {
	// Initialize status LED (WS2812 NeoPixel)
	statusLed.begin();
	statusLed.setPixelColor(0, statusLed.Color(255, 0, 0)); // Red during startup
	statusLed.show();
}

void setStatusLEDColor(uint8_t red, uint8_t green, uint8_t blue) {
	statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
	statusLed.show();
}

// Enhanced LED status management
void updateStatusLED() {
	unsigned long now = millis();

	// Update LED every 50ms for smooth effects
	if (now - lastLedUpdate < 50) return;
	lastLedUpdate = now;

	// Count connected clients
	int connectedClients = getConnectedClientCount();

	// Determine base color and brightness based on system status
	uint8_t red = 0, green = 0, blue = 0;
	uint8_t baseBrightness = 30; // Low base brightness for subtle indication

	// Primary status colors (solid, non-blinking)
	if (connectedClients > 0) {
		// Connected clients: Blue (intensity based on client count)
		blue = 60 + (connectedClients * 30); // Brighter blue with more clients
		baseBrightness = 40;
	} else {
		// No clients: Green (ready and waiting)
		green = 80;
		baseBrightness = 25;
	}

	// P1 data activity: Brief brightness boost (not color change)
	uint8_t activityBoost = 0;
	if (now - lastP1DataReceived < 200) { // 200ms boost after P1 data
		activityBoost = 100; // Brightness boost
	}

	// Heartbeat: Gentle breathing effect every 3 seconds
	uint8_t heartbeatBoost = 0;
	if (now - heartbeatTimer > 3000) {
		heartbeatTimer = now;
		heartbeatDirection = !heartbeatDirection;
	}

	// Smooth breathing calculation
	float breathePhase = (now - heartbeatTimer) / 1500.0; // 1.5 second each direction
	if (breathePhase > 1.0) breathePhase = 1.0;
	if (!heartbeatDirection) breathePhase = 1.0 - breathePhase;
	heartbeatBoost = (uint8_t)(breathePhase * 20); // Gentle 20-point brightness variation

	// Calculate final brightness
	uint8_t finalBrightness = baseBrightness + activityBoost + heartbeatBoost;
	if (finalBrightness > 255) finalBrightness = 255;

	// Apply brightness scaling to colors
	red = (red * finalBrightness) / 255;
	green = (green * finalBrightness) / 255;
	blue = (blue * finalBrightness) / 255;

	// Set the LED
	statusLed.setPixelColor(0, statusLed.Color(red, green, blue));
	statusLed.show();
}
