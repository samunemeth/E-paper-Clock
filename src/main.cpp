// --- GxEPD2 Settings ---
#define ENABLE_GxEPD2_GFX 0


// --- Libraries ---
#include <Arduino.h>
#include <WiFi.h>
#include <SPI.h>
#include <EEPROM.h>
#include <time.h>


// --- Settings and Pins ---
#include "settings.h"
#include "pins.h"
#include "wifi_secret.h"


// --- Components ---
#include "display-helpers.h"


// --- Calculated Constants ---
const TickType_t loop_tick_delay = LOOP_WAIT_TIME / portTICK_PERIOD_MS;


// --- Mode Numbering ---
#define NULL_MODE   0
#define NORMAL_MODE 1
#define UPDATE_MODE 2
#define USER_MODE   4
#define RESET_MODE  8
#define RESYNC_MODE 16

// --- Global Variables ---

// EEPROM values
#define EEPROM_SIZE 4
uint8_t mode = NULL_MODE;
uint8_t last_mode = NULL_MODE;
uint8_t last_sync_hour = 0;
uint8_t last_sync_minute = 0;

// Time
time_t now;
struct tm timeinfo;

// String buffers
char strf_hour_buf[4];
char strf_minute_buf[4];
char strf_date_buf[16];
char strf_last_sync_hour_buf[4];
char strf_last_sync_minute_buf[4];
char strf_battery_voltage_buf[8];

// Loop status
bool loop_running;


// --- Function Declarations ---


// Interrupt functions.
void IRAM_ATTR intUpdateMode();
void IRAM_ATTR intUserMode();
void IRAM_ATTR intNormalMode();
void IRAM_ATTR intLoopStop();

// EEPROM related functions.
void readModeEEPROM();
void writeModeEEPROM(uint8_t new_mode = NULL_MODE);

void readSyncEEPROM();
void writeSyncEEPROM();

// Time related functions.
void getTime();
void formatTime();
void configureTimeZone();


// --- Code ---
void setup() {


    // --- Getting the Mode ---


    //   Mode Priority and Numbering
    // NORMAL -> UPDATE -> USER -> RESET -> RESYNC
    //   1         2        4        8        16
    
    // Initialize the EEPROM to the defined size.
    EEPROM.begin(EEPROM_SIZE);

    // Read the mode from the EEPROM. If we have something in the values, it will
    // be stored in the variables.
    readModeEEPROM();

    // At this point, the read from the EEPROM may have over written the mode variable
    // with update or user mode, as these can be triggered from an interrupt.

    // Get the wakeup cause.
    const esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

    // If the timer triggered the wakeup, we can be sure that we are either in normal mode.
    if (mode == NULL_MODE && (wakeup_cause == ESP_SLEEP_WAKEUP_TIMER)) { mode = NORMAL_MODE; }

    // Next, we may need these inputs.
    pinMode(OTA_SW_PIN, INPUT_PULLUP);
    pinMode(BTN_TOP_PIN, INPUT_PULLUP);

    // Here, we can check for a GPIO wakeup, if there is still no mode defined.
    if (mode == NULL_MODE && (wakeup_cause == ESP_SLEEP_WAKEUP_GPIO)) {

        // We can read the GPIO pins connected to the buttons.
        // As the ESP reboots pretty fast, if the press is average length, it will still be pressed here.
        // If the last mode was the same, do not enter update mode again, as this was a button press to exit.
        bool update_btn_pressed = (digitalRead(OTA_SW_PIN) == LOW) && (last_mode != UPDATE_MODE);
        bool user_btn_pressed = (digitalRead(BTN_TOP_PIN) == LOW);

        // Update mode has a higher priority, we will check that first.
        if      (update_btn_pressed) { mode = UPDATE_MODE; }
        else if (user_btn_pressed)   { mode = USER_MODE;   }

    }

    // Here we will know if we are in NORMAL, UPDATE or USER mode.
    // If we aren't in any of those, we can just default to a hard reset.
    if (mode == NULL_MODE) { mode = RESET_MODE; }

    // Here, we are in one of the modes. We can write a zero to the EEPROM to clear it.
    writeModeEEPROM(NULL_MODE);


    // --- Using the Modes ---


    // Regardless of the mode, the display will be used right after this.
    pinMode(AUX_PWR_PIN, OUTPUT);
    digitalWrite(AUX_PWR_PIN, HIGH);

    // If we are not in update mode, we can attach the interrupt to the update button.
    if (mode != UPDATE_MODE) {
        attachInterrupt(digitalPinToInterrupt(OTA_SW_PIN), intUpdateMode, FALLING);
    }

    // If we are in normal mode, we can attach the interrupt to the user button.
    if (mode == NORMAL_MODE) {
        attachInterrupt(digitalPinToInterrupt(BTN_TOP_PIN), intUserMode, FALLING);
    }

    // Configure the time zone, regardless of the mode. We have to do it either way.
    configureTimeZone();

    // If we are in normal mode, we may need to do a resync.
    if (mode == NORMAL_MODE) {

        // Get the time for the check.
        getTime();

        // Check for all the times.
        // TODO: There may be an accidenteal resync at midnight?
        bool needs_resync = false;
        for (uint8_t i = 0; i < sizeof(resync_at) / sizeof(resync_at[0]); i++) {
            needs_resync = needs_resync || (timeinfo.tm_hour == resync_at[i]) && (timeinfo.tm_min == 0);
        }

        // If we need a resync, we can change the mode, and also update the EEPROM.
        if (needs_resync) {
            mode = RESYNC_MODE;
            writeModeEEPROM(NULL_MODE);
        }

    }

    // We can also read the last sync times if we are not in reset mode.
    // If we were in a reset, we can clear them.
    if (mode == RESET_MODE) {
        writeSyncEEPROM();
    } else {
        readSyncEEPROM();
    }

    // Read the battery voltage, and convert it to a string.
    pinMode(BATT_SENSE_PIN, INPUT);
    float battery_voltage = (float)analogRead(BATT_SENSE_PIN) * ADC_MAGIC_VAL;
    sprintf(strf_battery_voltage_buf, "%.1fV", battery_voltage);

    // The order of operations is intentional.
    // This way the display can initialize while we read sensors.

    // Initialize the display.
    // If we are in RESET mode, we have to wipe the screen.
    displayInit(mode == RESET_MODE);

    // If we are in update mode, we basically have to stall the processor.
    if (mode == UPDATE_MODE) {
  
        // Turn on led
        pinMode(EXT_LED_PIN, OUTPUT);
        digitalWrite(EXT_LED_PIN, LOW);

        // Turn the update button into an additional reset button
        // This may be causing issues. For now, the interrupt will just be detached.
        attachInterrupt(digitalPinToInterrupt(OTA_SW_PIN), intNormalMode, FALLING);

        // Draw to display
        #if !defined(POWER_DOWN_DISPLAY) && defined(PREFER_FAST_REFRESH)
            displayStartDraw(/*fast=*/ true);
        #else
            displayStartDraw();
        #endif /* !POWER_DOWN_DISPLAY && PREFER_FAST_REFRESH */

        displayRenderClaim((char*)"UPDATE");

        displayEndDraw();

        // Turn off the display and auxiliary power. We do not need them any more.
        displayOff();
        digitalWrite(AUX_PWR_PIN, LOW);

        // Block all other tasks.
        for (;;) vTaskDelay(loop_tick_delay);
    }

    // Display a start message
    
    // If we are in RESET mode, the clock may not be set yet. Instead of a time, 
    // we display a message, or just clear the display.
    if (mode == RESET_MODE) {
        
        #if defined(PREFER_FAST_REFRESH)
            displayStartDraw(/*fast=*/ true);
        #else
            displayStartDraw();
        #endif /* PREFER_FAST_REFRESH */
        
        #if defined(DISPLAY_START_MESSAGE)
            displayRenderClaim((char*)"STARTING");
        #endif /* DISPLAY_START_MESSAGE */

        displayEndDraw();

    }

    // If we are in RESYNC mode, we already know the approximate time.
    // Display that and a flag indicating the resync.
    if (mode == RESYNC_MODE) {

        // Format the time.
        formatTime();

        // Draw to display.
        #if !defined(POWER_DOWN_DISPLAY) && defined(PREFER_FAST_REFRESH)
            displayStartDraw(/*fast=*/ true);
        #else
            displayStartDraw();
        #endif /* !POWER_DOWN_DISPLAY && PREFER_FAST_REFRESH */

        displayRenderBorders();
        displayRenderStatusBar(strf_battery_voltage_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
        displayRenderTime(strf_hour_buf, strf_minute_buf);
        displayRenderDate(strf_date_buf);

        displayRenderFlag((char*)"RESYNC");

        displayEndDraw();

    }

    // In RESET and RESYNC mode, we need to connect to a wifi network, and sync with and SNTP server.
    if (mode == RESET_MODE || mode == RESYNC_MODE) {

        // Set up WiFi
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        // Set the desired SNTP server
        configTime(0, 0, SNTP_1, SNTP_2);

        // Configure the time zone again, as the settings get lost here.
        configureTimeZone();

        // Wait for WiFi to connect, and time to sync.
        do {
            getTime();
            vTaskDelay(loop_tick_delay);
        } while ((timeinfo.tm_year + 1900 < 2000) || (WiFi.status() != WL_CONNECTED));

        // Turn off the Wifi
        WiFi.mode(WIFI_OFF);

        // Set the last sync times
        last_sync_hour = timeinfo.tm_hour;
        last_sync_minute = timeinfo.tm_min;
        writeSyncEEPROM();

    }

    // Display seconds in the user mode.
    if (mode == USER_MODE) {

        // If we are powering the display down, or not preferring fast refresh,
        // a full refresh is required first.
        #if defined(POWER_DOWN_DISPLAY) || !defined(PREFER_FAST_REFRESH)

            // Format time for display
            getTime();
            formatTime();
        
            // Print the time to the display
            displayStartDraw();

            displayRenderBorders();
            displayRenderStatusBar(strf_battery_voltage_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
            displayRenderTime(strf_hour_buf, strf_minute_buf);
            displayRenderDate(strf_date_buf);
        
            displayEndDraw();

        #endif /* POWER_DOWN_DISPLAY || !PREFER_FAST_REFRESH */


        
        // Loop and print seconds
        loop_running = true;
        uint8_t last_second = timeinfo.tm_sec;
        for (uint8_t i = 0; i < MAX_USER_SECONDS; i++) {

            if (i == 2) {

                // Attach a loop stopping interrupt to the button.
                // This is done here to mitigate the issue of the user mode instantly quitting
                // when the button is pressed for a bit too long.
                attachInterrupt(digitalPinToInterrupt(BTN_TOP_PIN), intLoopStop, FALLING);

            }
            
            // Format time for display
            getTime();
            formatTime();
            
            // Print the time and seconds to the display
            displayStartDraw(/*fast=*/ true);
            
            displayRenderBorders();
            displayRenderStatusBar(strf_battery_voltage_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
            displayRenderTime(strf_hour_buf, strf_minute_buf);
            displayRenderDate(strf_date_buf);
            displayRenderSecond(timeinfo.tm_sec);
            
            displayEndDraw();

            do {
                getTime();
                vTaskDelay(loop_tick_delay);
            } while (timeinfo.tm_sec == last_second);
            
            last_second = timeinfo.tm_sec;

            if (!loop_running) break;

        }

        // Detach the interrupt and attach the original one.
        detachInterrupt(digitalPinToInterrupt(BTN_TOP_PIN));

        // Clear the seconds with the final render
        goto finalRender;

    };

    // If we are really close to the minute, wait for it.
    // In other cases, we will just wake up before it.
    do {
        getTime();
        vTaskDelay(loop_tick_delay);
    } while (timeinfo.tm_sec > (59 - OMIT_SLEEP));

    // Format time for display
    formatTime();

finalRender:

    // Print the time to the display
    #if defined(PREFER_FAST_REFRESH)

        #if defined(POWER_DOWN_DISPLAY)

            // If we are in RESYNC or RESET or USER mode, we can do a fast refresh to save some time and power.
            if (mode == RESYNC_MODE || mode == RESET_MODE || mode == USER_MODE) {
                displayStartDraw(/*fast=*/ true);
            } else {
                displayStartDraw(/*fast=*/ false);
            }

        #else

            displayStartDraw(/*fast=*/ true);
            
        #endif /* POWER_DOWN_DISPLAY */

    #else

        displayStartDraw();

    #endif /* PREFER_FAST_REFRESH */

    displayRenderBorders();
    displayRenderStatusBar(strf_battery_voltage_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
    displayRenderTime(strf_hour_buf, strf_minute_buf);
    displayRenderDate(strf_date_buf);

    displayEndDraw();

    #if defined(POWER_DOWN_DISPLAY)
    
        // Turn off the display and auxiliary power. We do not need them any more.
        displayOff();
        digitalWrite(AUX_PWR_PIN, LOW);
        
    #endif /* POWER_DOWN_DISPLAY */

    // As the display refresh takes time, we have to get the time again.
    getTime();

    // Calculate the time for the wakeup, and enable th e timer. There is a margin included.
    int64_t time_to_sleep = (60L - (int64_t)timeinfo.tm_sec) * 1000000L - ((int64_t)SLEEP_MARGIN * 1000L);
    esp_sleep_enable_timer_wakeup(time_to_sleep);

    // Disables the RTC FAST memory, that is not used in our case. This saves power.
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);

    // Set the pins that will wake up from deep sleep.
    // We check witch one caused the wakeup at the start.
    esp_deep_sleep_enable_gpio_wakeup(1 << OTA_SW_PIN_NUM, ESP_GPIO_WAKEUP_GPIO_LOW);
    esp_deep_sleep_enable_gpio_wakeup(1 << BTN_TOP_PIN_NUM, ESP_GPIO_WAKEUP_GPIO_LOW);

    #if !defined(POWER_DOWN_DISPLAY)
        
        // Keep the gpio states as is.
        gpio_deep_sleep_hold_en();
        gpio_hold_en(AUX_PWR_PIN_NUM);

    #endif /* POWER_DOWN_DISPLAY */


    // Go into deep sleep.
    // Nothing is run after this.
    esp_deep_sleep_start();

}

void loop() {};


// --- Function Definitions ---





// --- Interrupt Functions ---

void IRAM_ATTR intUpdateMode() {

    // Set mode to update mode, and save to the EEPROM.
    writeModeEEPROM(UPDATE_MODE);

    // Restart with the new mode in EEPROM.
    ESP.restart();

}

void IRAM_ATTR intUserMode() {

    // Set mode to user, and save to the EEPROM.
    writeModeEEPROM(USER_MODE);

    // Restart with the new mode in EEPROM.
    ESP.restart();

}

void IRAM_ATTR intNormalMode() {

    // Set mode to user, and save to the EEPROM.
    writeModeEEPROM(NORMAL_MODE);

    // Restart with the new mode in EEPROM.
    ESP.restart();

}

/// @brief Stops the current loop that the interrupt occurred in.
void IRAM_ATTR intLoopStop() {

    loop_running = false;

}

// --- EEPROM Related Functions ---

/// @brief Read the mode variables from the EEPROM
void readModeEEPROM() {

    mode = EEPROM.read(0);
    last_mode = EEPROM.read(1);

    // If the mode is invalid, set it to NULL mode.
    if (mode != NORMAL_MODE && mode != UPDATE_MODE && mode != USER_MODE && mode != RESET_MODE && mode != RESYNC_MODE) {
        mode = NULL_MODE;
    }

}

/// @brief Write the mode variables from the EEPROM
void writeModeEEPROM(uint8_t new_mode) {

    EEPROM.write(0, new_mode);
    EEPROM.write(1, mode);
    EEPROM.commit();

}

/// @brief
void readSyncEEPROM() {

    last_sync_hour = EEPROM.read(2);
    last_sync_minute = EEPROM.read(3);

}

/// @brief
void writeSyncEEPROM() {

    EEPROM.write(2, last_sync_hour);
    EEPROM.write(3, last_sync_minute);
    EEPROM.commit();

}


// --- Time related functions ---

/// @brief Get the time and local time, and save them to the global variables.
void getTime() {

    time(&now);
    localtime_r(&now, &timeinfo);

}

/// @brief Get the time, and set the timezone.
void configureTimeZone() {

    time(&now);
    setenv("TZ", TIMEZONE, 1);
    tzset();
    localtime_r(&now, &timeinfo);

}

/// @brief Update the global time string buffers with the corresponding values.
void formatTime() {

    strftime(strf_hour_buf, sizeof(strf_hour_buf), "%H", &timeinfo);
    strftime(strf_minute_buf, sizeof(strf_minute_buf), "%M", &timeinfo);
    strftime(strf_date_buf, sizeof(strf_date_buf), "%F", &timeinfo);

    sprintf(strf_last_sync_hour_buf, "%02d", last_sync_hour);
    sprintf(strf_last_sync_minute_buf, "%02d", last_sync_minute);

}