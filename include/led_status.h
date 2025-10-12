#ifndef LED_STATUS_H
#define LED_STATUS_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "config.h"

// Status LED
extern Adafruit_NeoPixel statusLed;

// LED timing variables
extern unsigned long lastLedUpdate;
extern unsigned long heartbeatTimer;
extern uint8_t ledBrightness;
extern bool heartbeatDirection;

// Function declarations
void initializeStatusLED();
void updateStatusLED();
void setStatusLEDColor(uint8_t red, uint8_t green, uint8_t blue);

#endif // LED_STATUS_H
