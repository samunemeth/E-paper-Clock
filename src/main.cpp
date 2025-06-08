// --- GxEPD2 Settings ---
#define ENABLE_GxEPD2_GFX 0


// --- Libraries ---
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include <esp_sntp.h>
#include <esp_adc_cal.h>


// --- Settings and Pins ---
#include "settings.h"
#include "pins.h"
#include "wifi_secret.h"


// --- Components ---
#include "display_helper/display_helper.h"


// --- Calculated Constants ---
const TickType_t loop_tick_delay = LOOP_WAIT_TIME / portTICK_PERIOD_MS;


// --- Mode Numbering ---
#define NULL_MODE     0b00000000
#define RESET_MODE    0b00000001
#define NORMAL_MODE   0b00000010
#define RESYNC_MODE   0b00000100
#define SECONDS_MODE  0b00001000
#define STOPPER_MODE  0b00010000
#define UPDATE_MODE   0b00100000
#define CRITICAL_MODE 0b01000000

// --- Global Variables ---

// Mode variables, and counters in RTC memory
uint8_t RTC_NOINIT_ATTR desired_mode = NULL_MODE;
uint8_t RTC_NOINIT_ATTR mode = NULL_MODE;
uint32_t RTC_NOINIT_ATTR boot_num = 0;

// Last sync variables in RTC memory
uint8_t RTC_NOINIT_ATTR last_sync_minute;
uint8_t RTC_NOINIT_ATTR last_sync_hour;
char RTC_NOINIT_ATTR strf_last_sync_hour_buf[3];
char RTC_NOINIT_ATTR strf_last_sync_minute_buf[3];
int32_t RTC_NOINIT_ATTR time_shift_average;
int32_t RTC_NOINIT_ATTR time_shift_samples;

// Battery variable in RTC memory
char RTC_NOINIT_ATTR strf_battery_value_buf[8];
uint8_t RTC_NOINIT_ATTR battery_status;

// Time related variables
time_t now;
struct tm timeinfo;
struct timeval tv_now;

// String buffers
char strf_hour_buf[3];
char strf_minute_buf[3];
char strf_date_buf[16];

// Loop status
bool loop_running;
bool fast_refresh;


// --- Function Declarations ---


// Interrupt functions.
void IRAM_ATTR intUpdateMode();
void IRAM_ATTR intSecondsMode();
void IRAM_ATTR intNormalMode();
void IRAM_ATTR intLoopStop();
void IRAM_ATTR sntpSyncCallback(timeval *tv);

// Time related functions.
void getTime();
void formatStrings();
void configureTimeZone();


// --- Code ---
void setup() {


    // --- Getting the Mode ---


    /*     
        To request modes, and store previous modes, and date from before resets,
        RTC FAST memory is used. Variables labelled with `RTC_NOINIT_ATTR` are
        store in RTC FAST memory, and as the "NOINIT" port suggest, are not
        cleared on resets.
        (As opposed to `RTC_DATA_ATTR`, witch only persists after deep sleep.)

        To filter mangled data after a power on reset - or crash -
        the variables are cleared unless a soft reset or wakeup occurred.
    */
    
    // Get the reason of the reset.
    const esp_reset_reason_t reset_cause = esp_reset_reason();

    // If we were not soft reset, or woken up from deep sleep, we have to wipe some variables.
    if ((reset_cause != ESP_RST_DEEPSLEEP) && (reset_cause != ESP_RST_SW)) {

        // Clear persistent variables.
        desired_mode = RESET_MODE;
        mode = NULL_MODE;
        boot_num = 0;

        time_shift_average = 0;
        time_shift_samples = 0;
        
        /*
            As there will be a resync after a hard reset, there is no need to
            initialize the following variables:

            strf_last_sync_hour_buf;
            strf_last_sync_minute_buf;
            last_sync_hour;
            last_sync_minute;
            strf_battery_value_buf;
            battery_status;
        */

    }

    // Rotate mode variable to last mode.
    const uint8_t last_mode = mode;

    // Check if we have a valid desired mode.
    if (desired_mode & (NORMAL_MODE + UPDATE_MODE + SECONDS_MODE + RESET_MODE)) {

        // Set that as our current mode.
        mode = desired_mode;

    } else {

        // If we do not have a valid desired mode, just clear the variable.
        mode = NULL_MODE;

    }

    // Clear the desired mode
    desired_mode = NULL_MODE;
    
    // Next, we may need these inputs.
    pinMode(OTA_SW_PIN, INPUT_PULLUP);
    pinMode(BTN_TOP_PIN, INPUT_PULLUP);

    // If we have woken up from deep sleep, investigate the cause.
    if (reset_cause == ESP_RST_DEEPSLEEP && mode == NULL_MODE) {

        // Get the wakeup cause.
        const esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();

        // If the timer triggered the wakeup, we can be sure that we are either in normal mode.
        if ( wakeup_cause == ESP_SLEEP_WAKEUP_TIMER ) { mode = NORMAL_MODE; }

        // Here, we can check for a GPIO wakeup, if there is still no mode defined.
        else if ( wakeup_cause == ESP_SLEEP_WAKEUP_GPIO ) {

            // We can read the GPIO pins connected to the buttons.
            // As the ESP reboots pretty fast, if the press is average length, it will still be pressed here.
            // If the last mode was the same, do not enter update mode again, as this was a button press to exit.
            // Update mode has a higher priority, we will check that first.

            if      ( (digitalRead(OTA_SW_PIN) == LOW) && (last_mode != UPDATE_MODE) ) { mode = UPDATE_MODE;  }
            else if ( digitalRead(BTN_TOP_PIN) == LOW )                                { mode = SECONDS_MODE; }

        }

    }

    // If we are in normal mode, we may need to do a resync.
    if ((mode == NORMAL_MODE) && (boot_num % RESYNC_EVERY == 0)) { mode = RESYNC_MODE; }

    // If we are not in any mode yet, just say we were in a reset.
    // This should not be strictly necessary, but it's a good catch all.
    // (An error could occur if we soft reset without a valid desired mode.)
    if (mode == NULL_MODE) { mode = RESET_MODE; }


    // --- Powering Up, Initializing, Detecting Mode Modes ---


    // Setting up the AUX pin for a possible use.
    pinMode(AUX_PWR_PIN, OUTPUT);

    // Turn on aux power if display need it.
    #if defined(AUX_FOR_DISP)
        digitalWrite(AUX_PWR_PIN, HIGH);
    #endif /* AUX_FOR_DISP */

    // If we are not in update mode, we can attach the interrupt to the update button.
    if (mode != UPDATE_MODE) {
        attachInterrupt(digitalPinToInterrupt(OTA_SW_PIN), intUpdateMode, FALLING);
    }

    // If we are in normal mode, we can attach the interrupt to the top button.
    if (mode == NORMAL_MODE) {
        attachInterrupt(digitalPinToInterrupt(BTN_TOP_PIN), intSecondsMode, FALLING);
    }

    // Measure battery voltage if needed.
    if (boot_num % BATT_SENSE_EVERY == 0) {

        // Turn on the aux power if the voltage divider needs it.
        #if defined(AUX_FOR_BATT_SENSE)
            digitalWrite(AUX_PWR_PIN, HIGH);
        #endif /* AUX_FOR_BATT_SENSE */

        // Set the battery voltage pin to input.
        pinMode(BATT_SENSE_PIN, INPUT);

        // Get raw measurement with oversampling.
        uint32_t battery_raw;
        for (uint8_t i = 0; i < ADC_OVER_SAMPLE_COUNT; i++) {
            battery_raw += analogRead(BATT_SENSE_PIN);
        }
        battery_raw = battery_raw / ADC_OVER_SAMPLE_COUNT;

        // Calibrate the battery voltage reading.
        esp_adc_cal_characteristics_t adc_chars;
        esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
        uint32_t battery_voltage = esp_adc_cal_raw_to_voltage(battery_raw, &adc_chars) * ADC_FACTOR;

        // Turn off the aux power if display does not need it.
        #if !defined(AUX_FOR_DISP)
            digitalWrite(AUX_PWR_PIN, LOW);
        #endif /* !AUX_FOR_DISP */

        // Detect critically low battery level, switch to critical mode.
        if (battery_voltage <= CRITICAL_BATTERY_LEVEL) {
            mode = CRITICAL_MODE;
        }

        // Calculate battery percent based on an approximation. 
        uint8_t battery_percent;
        if (battery_voltage >= (4200 - FULL_BATTERY_TOLERANCE)) {
            battery_percent = 100;
        } else if (battery_voltage >= 3870) {
            battery_percent = round(120 * ((float)battery_voltage/1000) - 404);
        } else if (battery_voltage > 3300) {
            battery_percent = round(113 / (1 + exp(46.3 - 12 * ((float)battery_voltage/1000))));
        } else {
            battery_percent = 0;
        }

        // Round, and convert to a string.
        sprintf(strf_battery_value_buf, "%d%%", battery_percent);

        // Set battery status based on level.
        if (battery_percent < 25) {
            battery_status = 0;
        } else if (battery_percent < 50) {
            battery_status = 1;
        } else if (battery_percent < 75) {
            battery_status = 2;
        } else {
            battery_status = 3;
        }

    }

    // Configure the time zone, regardless of the mode. We have to do it either way.
    configureTimeZone();

    // The order of operations is intentional.
    // This way the display can initialize while we read sensors.


    // --- Blocking Modes ---


    // Initialize the display.
    // If we are in RESET mode, we have to wipe the screen.
    displayInit(mode & (CRITICAL_MODE + RESET_MODE));

    // If we are in critical mode, we need to display a warning message, and shut down the processor.
    if (mode == CRITICAL_MODE) {

        // A full refresh may be needed.
        #if !defined(AUX_FOR_DISP) && defined(PREFER_FAST_REFRESH)
            fast_refresh = true;
        #else
            fast_refresh = false;
        #endif /* !AUX_FOR_DISP && PREFER_FAST_REFRESH */

        // Draw to display
        displayStartDraw(/*fast=*/ fast_refresh);

        displayRenderCriticalMessage();

        displayEndDraw();

        // Turn off the display and auxiliary power. We do not need them any more.
        displayHibernate();
        digitalWrite(AUX_PWR_PIN, LOW);

        // Disable wakeup sources
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

        // Go into INDEFINITE deep sleep. (Basically shut down.)
        esp_deep_sleep_start();

        // Nothing is run after this.
        
    }

    // If we are in update mode, we basically have to stall the processor.
    if (mode == UPDATE_MODE) {

        // Turn on aux power for external led if needed.
        #if defined(AUX_FOR_EXT_LED)
            digitalWrite(AUX_PWR_PIN, HIGH);
        #endif /* AUX_FOR_EXT_LED */
  
        // Turn on led
        pinMode(EXT_LED_PIN, OUTPUT);
        digitalWrite(EXT_LED_PIN, LOW);

        // Turn the update button into a button returning to normal mode.
        attachInterrupt(digitalPinToInterrupt(OTA_SW_PIN), intNormalMode, FALLING);

        // A full refresh may be needed.
        #if !defined(AUX_FOR_DISP) && defined(PREFER_FAST_REFRESH)
            fast_refresh = true;
        #else
            fast_refresh = false;
        #endif /* !AUX_FOR_DISP && PREFER_FAST_REFRESH */
        
        // Draw to display
        displayStartDraw(/*fast=*/ fast_refresh);
        
        displayRenderUpdateMessage();

        displayEndDraw();

        // Turn off the display and auxiliary power. We do not need them any more.
        displayHibernate();
        digitalWrite(AUX_PWR_PIN, LOW);

        // Block all other tasks.
        for (;;) vTaskDelay(loop_tick_delay);

        // Nothing is run after this.

    }


    // --- Running Modes ---

 
    // If we are in RESET mode, the clock may not be set yet. Instead of a time, 
    // we display a message, or just clear the display.
    if (mode == RESET_MODE) {
        
        displayStartDraw(/*fast=*/ true);
        displayEndDraw();

    }

    // In RESET and RESYNC mode, we need to connect to a wifi network, and sync with and SNTP server.
    if (mode & (RESYNC_MODE + RESET_MODE)) {
        
        // Skip the sync for development purposes.
        #if !defined(SKIP_SYNC)

        // Get precise time.
        gettimeofday(&tv_now, NULL);
        int64_t time_before_ms = (int64_t)tv_now.tv_sec * (int64_t)1000 + ((int64_t)tv_now.tv_usec / (int64_t)1000);

        // Try to make and educated guess of the sync time.
        int64_t time_waiting_ms = 0;

        // Set up variable for potential interrupt.
        loop_running = true;
        
        // Configure SNTP time sync.
        sntp_setoperatingmode(SNTP_SYNC_MODE_IMMED);
        sntp_setservername(1, SNTP_1);
        sntp_setservername(2, SNTP_2);
        sntp_set_time_sync_notification_cb(sntpSyncCallback);
        sntp_init();
        
        // Set up WiFi
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

        // Configure the time zone again, as the settings get lost here.
        configureTimeZone();
        
        // Wait for WiFi to connect, and time to sync.
        do {
            getTime();
            vTaskDelay(loop_tick_delay);
            time_waiting_ms += LOOP_WAIT_TIME;
        } while (loop_running || (timeinfo.tm_year < 100));
        loop_running = true;
        
        // Wait a bit to allow the clock to actually sync.
        vTaskDelay(loop_tick_delay);
        time_waiting_ms += LOOP_WAIT_TIME;
        
        // Get precise time.
        gettimeofday(&tv_now, NULL);
        int64_t time_after_ms = (int64_t)tv_now.tv_sec * (int64_t)1000 + ((int64_t)tv_now.tv_usec / (int64_t)1000);
        
        // Calculate the resulting time difference from the sync.
        int32_t time_shift_ms = (int32_t)(time_after_ms - (time_before_ms + time_waiting_ms));

        // Skip average calculation after reset, as the shift here can be chaotic.
        if (mode != RESET_MODE) {

           // Calculate new average time shift.
           time_shift_average = (time_shift_ms + (time_shift_average * time_shift_samples)) / (time_shift_samples + (int32_t)1);
           time_shift_samples++;

        } else {

            // After a reset, time shift does not make sense.
            time_shift_ms = 0;

        }

        // Make an HTTP POST request for data logging.
        #if defined(REPORT_TELEMETRY)

            /* 
                The buffer neds to be big enough to fit all the data.
                The size of the different data parts:
                    - Outline:          173
                    - UUID:             36
                    - BatteryLevel:     4
                    - BootNum:          10
                    - currentMode:      3
                    - timeShift:        10
                    - timeShiftAverage: 10
                    - timeShiftSamples: 10
                    - syncDuration:     10
                    - wifiStrength:     4
                Total:                  270
            */
            char strf_post_buf[280];

            // Get wifi network strength.
            int8_t wifi_strength = WiFi.RSSI();

            // Fill the buffer with the formatted string.
            sprintf(strf_post_buf,
                "{\"uuid\":\"%s\",\"batteryLevel\":\"%s\",\"bootNum\":\"%d\",\"currentMode\":\"%d\",\"timeShift\":\"%d\",\"timeShiftAverage\":\"%d\",\"timeShiftSamples\":\"%d\",\"syncDuration\":\"%d\",\"wifiStrength\":\"%d\"}",
                uuid, strf_battery_value_buf, boot_num, mode, time_shift_ms, time_shift_average, time_shift_samples, (int32_t)time_waiting_ms, wifi_strength);

            // Create an http client.
            HTTPClient http;

            // Configure url and data to send.
            http.begin(reportingUrl);
            http.addHeader("Content-Type", "application/json");
            int httpRes = http.POST(strf_post_buf);

            // Close the connection.
            http.end();

        #endif
        
        // Turn off the Wifi
        WiFi.mode(WIFI_OFF);
        
        #endif /* !SKIP_SYNC */

        // Set the last sync times
        last_sync_hour = timeinfo.tm_hour;
        last_sync_minute = timeinfo.tm_min;

        // Update the strings
        sprintf(strf_last_sync_hour_buf, "%02d", last_sync_hour);
        sprintf(strf_last_sync_minute_buf, "%02d", last_sync_minute);

    }

    // Display seconds in the seconds mode.
    if (mode == SECONDS_MODE) {

        // A full refresh may be required first.
        #if defined(AUX_FOR_DISP)
            fast_refresh = false;
        #else
            fast_refresh = true;
        #endif /* AUX_FOR_DISP */
        
        // Loop and print seconds
        loop_running = true;
        uint8_t last_second = timeinfo.tm_sec;
        for (uint8_t i = 0; i < MAX_DISPLAYED_SECONDS; i++) {

            if (i == 2) {

                // Attach a loop stopping interrupt to the button.
                // This is done here to mitigate the issue of the seconds mode instantly quitting
                // when the button is pressed for a bit too long.
                attachInterrupt(digitalPinToInterrupt(BTN_TOP_PIN), intLoopStop, FALLING);

            }
            
            // Format time for display
            getTime();
            formatStrings();
            
            // Print the time and seconds to the display
            displayStartDraw(/*fast=*/ fast_refresh);
            fast_refresh = true;
            
            displayRenderBorders();
            displayRenderStatusBar(strf_battery_value_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf, battery_status);
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

        // Reset the variable.
        loop_running = true;

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
    formatStrings();

finalRender:


    // --- Normal Mode ---

    
    // If the display was not powered off, we have the opportunity to do a partial.
    // But only if we are coming from normal or reset mode.
    // If fast refreshed is preferred, we can do it anyway.
    #if !defined(AUX_FOR_DISP)
        #if defined(PREFER_FAST_REFRESH)
            fast_refresh = true;
        #else
            fast_refresh = (mode & (NORMAL_MODE + RESYNC_MODE)) &&
                (last_mode & (RESET_MODE + NORMAL_MODE + RESYNC_MODE));
        #endif /* PREFER_FAST_REFRESH */
    #else
        fast_refresh = false;
    #endif /* !AUX_FOR_DISP */

    // Depending on the settings, we have to do a full refresh every so often.
    if (boot_num % FULL_REFRESH_EVERY == 0) fast_refresh = false;

    displayStartDraw(fast_refresh);

    displayRenderBorders();
    displayRenderStatusBar(strf_battery_value_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf, battery_status);
    displayRenderTime(strf_hour_buf, strf_minute_buf);
    displayRenderDate(strf_date_buf);

    displayEndDraw();

    // Make the display go into deep sleep.
    displayHibernate();
    
    // Power down all peripherals.
    digitalWrite(AUX_PWR_PIN, LOW);

    // Increment boot counter
    boot_num++;
        
    // As the display refresh takes time, we have to get the time again.
    getTime();

    // Calculate the time for the wakeup, and enable th e timer. There is a margin included.
    const uint64_t time_to_sleep = (60 - timeinfo.tm_sec) * 1000000 - (SLEEP_MARGIN * 1000);
    esp_sleep_enable_timer_wakeup(time_to_sleep);

    // Set the pins that will wake up from deep sleep.
    // We check witch one caused the wakeup at the start.
    esp_deep_sleep_enable_gpio_wakeup((1 << OTA_SW_PIN_NUM) + (1 << BTN_TOP_PIN_NUM), ESP_GPIO_WAKEUP_GPIO_LOW);

    // Go into deep sleep.
    // Nothing is run after this.
    esp_deep_sleep_start();

}

void loop() {};


// --- Function Definitions ---


// --- Interrupt Functions ---

void IRAM_ATTR intUpdateMode() {

    // Set desired mode to update mode.
    desired_mode = UPDATE_MODE;

    // Restart.
    esp_restart();

}

void IRAM_ATTR intSecondsMode() {

    // Set desired mode to seconds mode.
    desired_mode = SECONDS_MODE;

    // Restart.
    esp_restart();

}

void IRAM_ATTR intNormalMode() {

    // Set desired mode to normal mode.
    desired_mode = NORMAL_MODE;

    // Restart.
    esp_restart();

}

/// @brief Stops the current loop that the interrupt occurred in.
void IRAM_ATTR intLoopStop() {

    loop_running = false;

}

void IRAM_ATTR sntpSyncCallback(timeval *tv) {

    loop_running = false;

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
void formatStrings() {

    strftime(strf_hour_buf, sizeof(strf_hour_buf), "%H", &timeinfo);
    strftime(strf_minute_buf, sizeof(strf_minute_buf), "%M", &timeinfo);
    strftime(strf_date_buf, sizeof(strf_date_buf), "%F", &timeinfo);

}