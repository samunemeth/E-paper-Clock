
// --- Libraries ---
#include <GxEPD2_BW.h>
#include <Arduino.h>

// --- Settings and Pins ---
#include "pins.h"

// --- Fonts ---
#include "custom_fonts/FreeMonoBold48pt7b.h"
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// --- Icons ---
#include "custom_icons.h"

// --- Functions ---
void displayStartDraw(bool fast = false, bool full = true);
void displayEndDraw();

void displayCenterText(char* text, uint16_t x, uint16_t y, bool onlyCursor = false);

void displayRenderBorders();
void displayRenderStatusBar(char* battery_value_buf, char* last_sync_hour_buf, char* last_sync_minute_buf, uint8_t battery_status);
void displayRenderTime(char* hour_buf, char* minute_buf);
void displayRenderDate(char* date_buf);
void displayRenderSecond(uint8_t seconds);
void displayRenderFlag(char* text);
void displayRenderClaim(char* text);
void displayRenderUpdateMessage();
void displayRenderCriticalMessage();

void displayInit(bool wipe);
void displayHibernate();
