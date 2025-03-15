# include "display_helper.h"


// --- Globals ---
GxEPD2_BW<GxEPD2_290_BS, GxEPD2_290_BS::HEIGHT> display(GxEPD2_290_BS(/*CS=*/ DISP_CS_PIN, /*DC=*/ DISP_DC_PIN, /*RST=*/ DISP_RES_PIN, /*BUSY=*/ DISPLAY_BUSY_PIN));


// --- Display Related Functions ---

/// @brief Starts a new page to draw on.
/// @param fast If true, does a partial refresh of the full window insted of a full refresh.
/// @param full If true, a full windows refresh is selected. If false, the refresh style has to be manually set after this.
void displayStartDraw(bool fast, bool full) {
    
    display.firstPage();
    display.setTextColor(GxEPD_BLACK);
    if (!full) return;
    if (fast) {
        display.setPartialWindow(0, 0, display.width(), display.height());
    } else {
        display.setFullWindow();
    }

}

/// @brief Refreshes the display.
void displayEndDraw() {

    display.nextPage();

}

/// @brief Set the cursor, and print text to the display, such that the text is centered at the coordinates.
/// @param text The text to print, or set the cursor for.
/// @param x The X coordinate.
/// @param y The Y coordinate.
/// @param onlyCursor If true, the text will not be rendered, but the cursor will be positioned.
void displayCenterText(char* text, uint16_t x = 148, uint16_t y = 64, bool onlyCursor) {

    // Get bounding box of text.
    int16_t tbx, tby; uint16_t tbw, tbh;
    display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

    // Move cursor to the correct location.
    display.setCursor(x - (tbw / 2) - tbx, y - (tbh / 2) - tby);
    if (onlyCursor) return; // Skip printing text if only cursor is set.
    display.print(text);

}

/// @brief Renders the visual borders of the screen.
void displayRenderBorders() {

    /*
    display.drawRect(0, 12, display.width(), display.height() - 12, GxEPD_BLACK);
    display.drawRect(1, 13, display.width() - 2, display.height() - 14, GxEPD_BLACK);
    */

}

/// @brief Renders the status bar on the top of the display.
/// @param battery_value_buf Battery voltage in string format.
/// @param last_sync_hour_buf Last sync hours in string format.
/// @param last_sync_minute_buf Last sync minutes in string format.
void displayRenderStatusBar(char* battery_value_buf, char* last_sync_hour_buf, char* last_sync_minute_buf) {

    display.setFont(&FreeMonoBold9pt7b);
    displayCenterText(battery_value_buf, 32, 6);

    displayCenterText(last_sync_hour_buf, 260, 6);
    displayCenterText((char*)":", 273, 6);
    displayCenterText(last_sync_minute_buf, 286, 6);

}

/// @brief Renders the time to the center of the display.
/// @param hour_buf Hours in string format.
/// @param minute_buf Minutes in string format.
void displayRenderTime(char* hour_buf, char* minute_buf) {

    display.setFont(&FreeMonoBold48pt7b);
    displayCenterText(hour_buf, 74, 62);
    displayCenterText((char*)":", 148, 62);
    displayCenterText(minute_buf, 222, 62);

}

/// @brief Render the date to the bottom of the display.
/// @param date_buf Date in string format.
void displayRenderDate(char* date_buf) {

    display.setFont(&FreeMonoBold12pt7b);
    displayCenterText(date_buf, 148, 114);

}

/// @brief Render the seconds to the display over the large colon.
/// @param seconds Seconds in integer format.
void displayRenderSecond(uint8_t seconds) {

    display.setFont(&FreeMonoBold12pt7b);
    displayCenterText((char*)"00", 148, 28, true);
    display.printf("%02d", seconds);

}

/// @brief Renders text to the status bar area.
/// @param text Text to reder. Recommended to be fully capitalised.
void displayRenderFlag(char* text) {

    display.setFont(&FreeMonoBold9pt7b);
    displayCenterText(text, 148, 6);

}

/// @brief Renders large text to the center of the display.
/// @param text Text to reder. Recommended to be fully capitalised.
void displayRenderClaim(char* text) {

    display.setFont(&FreeMonoBold24pt7b);
    displayCenterText(text);

}

/// @brief Renders the update message to the display.
void displayRenderUpdateMessage() {

    display.setFont(&FreeMonoBold12pt7b);
    displayCenterText((char*)"ISOLATE", 148, 12);
    display.setFont(&FreeMonoBold9pt7b);
    displayCenterText((char*)"before plugging in", 148, 32);

    display.setFont(&FreeMonoBold24pt7b);
    displayCenterText((char*)"UPDATE", 148, 74);

    display.setFont(&FreeMonoBold9pt7b);
    displayCenterText((char*)"Connect to ESP32 directly!", 148, 116);

}

/// @brief Renders the critical battery level message to the display.
void displayRenderCriticalMessage() {

    display.setFont(&FreeMonoBold24pt7b);
    displayCenterText((char*)"CRITICAL", 148, 32);
    displayCenterText((char*)"BATTERY", 148, 72);

    display.setFont(&FreeMonoBold9pt7b);
    displayCenterText((char*)"Charge the device!", 148, 116);

}

/// @brief Initialize the display.
/// @param wipe If true, does an initial wipe.
void displayInit(bool wipe) {

    display.init(115200, wipe, 2, false);
    display.setRotation(3); // Landscape with the connector on the left

}

/// @brief Turns off the display.
void displayOff() {

    display.powerOff();

}