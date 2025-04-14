// --- GxEPD2 Settings ---
#define ENABLE_GxEPD2_GFX 0


// --- Libraries ---
#include <Arduino.h>
#include <WiFi.h>

#include <esp_sntp.h>
#include <esp_adc_cal.h>
#include <driver/temp_sensor.h>


// --- Settings and Pins ---
#include "settings.h"
#include "pins.h"
#include "wifi_secret.h"


// --- Components ---
#include "display_helper/display_helper.h"


// --- Calculated Constants ---
const TickType_t loop_tick_delay = LOOP_WAIT_TIME / portTICK_PERIOD_MS;


// --- Mode Numbering ---
#define NULL_MODE     0
#define NORMAL_MODE   1 << 0
#define UPDATE_MODE   1 << 1
#define USER_MODE     1 << 2
#define RESET_MODE    1 << 3
#define RESYNC_MODE   1 << 4
#define CRITICAL_MODE 1 << 5

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

// Battery variable in RTC memory
char RTC_NOINIT_ATTR strf_battery_value_buf[8];
char RTC_NOINIT_ATTR strf_temp_value_buf[8];

// Time related variables
time_t now;
struct tm timeinfo;

// String buffers
char strf_hour_buf[3];
char strf_minute_buf[3];
char strf_date_buf[16];

// Loop status
bool loop_running;


// --- Function Declarations ---


// Interrupt functions.
void IRAM_ATTR intUpdateMode();
void IRAM_ATTR intUserMode();
void IRAM_ATTR intNormalMode();
void IRAM_ATTR intLoopStop();

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
    // In case of any other wakeup, we need to increment the boot number.
    if ((reset_cause != ESP_RST_DEEPSLEEP) && (reset_cause != ESP_RST_SW)) {

        // Clear persistent variables.
        desired_mode = RESET_MODE;
        mode = NULL_MODE;
        boot_num = 0;
        
        /*
            As there will be a resync after a hard reset, there is no need to
            initialize the following variables:

            strf_last_sync_hour_buf;
            strf_last_sync_minute_buf;
            last_sync_hour;
            last_sync_minute;
            strf_battery_value_buf;
            strf_temp_value_buf;
        */

    } else {

        boot_num++;

    }

    // Rotate mode variable to last mode.
    const uint8_t last_mode = mode;

    // Check if we have a valid desired mode.
    if (desired_mode == NORMAL_MODE ||
        desired_mode == UPDATE_MODE ||
        desired_mode == USER_MODE || 
        desired_mode == RESET_MODE) {

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

            if      ( (digitalRead(OTA_SW_PIN) == LOW) && (last_mode != UPDATE_MODE) ) { mode = UPDATE_MODE; }
            else if ( digitalRead(BTN_TOP_PIN) == LOW )                                { mode = USER_MODE;   }

        }

    }

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

        // Initialize variable.
        bool needs_resync = false;

        // Do a resync every time if we are in demo mode.
        #if defined(RESYNC_DEMO)
        needs_resync = true;
        #endif

        // Check for all the times.
        for (uint8_t i = 0; i < sizeof(resync_at) / sizeof(resync_at[0]); i++) {
            needs_resync = needs_resync || (timeinfo.tm_hour == resync_at[i]) && (timeinfo.tm_min == 0);
        }

        // If we need a resync, we can change the mode.
        if (needs_resync) {
            mode = RESYNC_MODE;
        }

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

    }

    /* 
    // Measure temperature if needed
    if (boot_num % TEMP_EVERY == 0) {

        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor.dac_offset = TSENS_DAC_L2;  // TSENS_DAC_L2 is default; L4(-40°C ~ 20°C), L2(-10°C ~ 80°C), L1(20°C ~ 100°C), L0(50°C ~ 125°C)
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();

        // Get converted sensor data.
        float temp_value;
        temp_sensor_read_celsius(&temp_value);

        // Round, convert to a string.
        sprintf(strf_temp_value_buf, "%.0fC", temp_value);

        // Disable the temperature sensor.
        temp_sensor_stop();

    }
    */

    // The order of operations is intentional.
    // This way the display can initialize while we read sensors.


    // --- Using the Modes ---


    // Initialize the display.
    // If we are in RESET mode, we have to wipe the screen.
    displayInit(mode == RESET_MODE || mode == CRITICAL_MODE);

    // If we are in critical mode, we need to display a warning message, and shut down the processor.
    if (mode == CRITICAL_MODE) {

        // Draw to display
        #if !defined(AUX_FOR_DISP) && defined(PREFER_FAST_REFRESH)
            displayStartDraw(/*fast=*/ true);
        #else
            displayStartDraw();
        #endif /* !AUX_FOR_DISP && PREFER_FAST_REFRESH */

        displayRenderCriticalMessage();

        displayEndDraw();

        // Turn off the display and auxiliary power. We do not need them any more.
        displayHibernate();
        digitalWrite(AUX_PWR_PIN, LOW);

        // Disable wakeup sources
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

        // Stop pin holding
        // There is no pin holding in this version.
        //gpio_hold_dis(AUX_PWR_PIN_NUM);
        //gpio_deep_sleep_hold_dis();

        // Go into INDEFINITE deep sleep. (Basically shut down.)
        // Nothing is run after this.
        esp_deep_sleep_start();
        
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

        // Draw to display
        #if !defined(AUX_FOR_DISP) && defined(PREFER_FAST_REFRESH)
            displayStartDraw(/*fast=*/ true);
        #else
            displayStartDraw();
        #endif /* !AUX_FOR_DISP && PREFER_FAST_REFRESH */

        displayRenderUpdateMessage();

        displayEndDraw();

        // Turn off the display and auxiliary power. We do not need them any more.
        displayHibernate();
        digitalWrite(AUX_PWR_PIN, LOW);

        // Block all other tasks.
        for (;;) vTaskDelay(loop_tick_delay);

    }

    // Display a start message
    
    // If we are in RESET mode, the clock may not be set yet. Instead of a time, 
    // we display a message, or just clear the display.
    if (mode == RESET_MODE) {
        
        displayStartDraw(/*fast=*/ true);
        displayEndDraw();

    }

    // In RESET and RESYNC mode, we need to connect to a wifi network, and sync with and SNTP server.
    #if !defined(SKIP_RESYNC)
    if (mode == RESET_MODE || mode == RESYNC_MODE) {

        // Configure SNTP time sync.
        sntp_setservername(1, SNTP_1);
        sntp_setservername(2, SNTP_2);
        sntp_init();
        
        // Set up WiFi
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);

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

        // Update the strings
        sprintf(strf_last_sync_hour_buf, "%02d", last_sync_hour);
        sprintf(strf_last_sync_minute_buf, "%02d", last_sync_minute);

    }
    #endif /* !SKIP_RESYNC */

    // Display seconds in the user mode.
    if (mode == USER_MODE) {

        // If we are powering the display down,
        // a full refresh is required first.
        #if defined(AUX_FOR_DISP)

            // Format time for display
            getTime();
            formatStrings();
        
            // Print the time to the display
            displayStartDraw();

            displayRenderBorders();
            displayRenderStatusBar(strf_battery_value_buf, strf_temp_value_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
            displayRenderTime(strf_hour_buf, strf_minute_buf);
            displayRenderDate(strf_date_buf);
        
            displayEndDraw();

        #endif /* AUX_FOR_DISP */


        
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
            formatStrings();
            
            // Print the time and seconds to the display
            displayStartDraw(/*fast=*/ true);
            
            displayRenderBorders();
            displayRenderStatusBar(strf_battery_value_buf, strf_temp_value_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
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
    formatStrings();

finalRender:

    // Print the time to the display
    #if defined(PREFER_FAST_REFRESH)
        #if defined(AUX_FOR_DISP)

            // See if a fast refresh can be done.
            bool do_fast_refresh = false;
            do_fast_refresh = (mode == RESET_MODE) || (mode == USER_MODE);

            // If we can, do a fast refresh.
            displayStartDraw(/*fast=*/ do_fast_refresh);

        #else

            displayStartDraw(/*fast=*/ true);
            
        #endif /* AUX_FOR_DISP */
    #else
        
        displayStartDraw();

    #endif /* PREFER_FAST_REFRESH */

    displayRenderBorders();
    displayRenderStatusBar(strf_battery_value_buf, strf_temp_value_buf, strf_last_sync_hour_buf, strf_last_sync_minute_buf);
    displayRenderTime(strf_hour_buf, strf_minute_buf);
    displayRenderDate(strf_date_buf);

    displayEndDraw();

    // Make the display go into deep sleep.
    displayHibernate();
    
    // Power down all peripherals.
    digitalWrite(AUX_PWR_PIN, LOW);
        
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

void IRAM_ATTR intUserMode() {

    // Set desired mode to user mode.
    desired_mode = USER_MODE;

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