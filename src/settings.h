
// --- Behaviour ---

// Uses fast refresh whenever possible. This makes the display a bit less clear, but preserves power.
#define PREFER_FAST_REFRESH

// Powers down display in deep sleep.
// After wakeup, a full refresh must be performed.
//#define POWER_DOWN_DISPLAY

// Displays "STARTING" text after a reset. Can cause ghosting.
//#define DISPLAY_START_MESSAGE

// Uses a single refresh instead of two when resyncing.
// Doesn't display "RESYNC" message.
#define RESYNC_SINGLE_REFRESH

// Activates a demo mode, with resyncs every minute.
// Useful for checking problems with resync mode.
//#define RESYNC_DEMO

// Displays battery voltage rounded to one decimals, insted of approximated battery precentage.
//#define USE_BATTERY_VOLTAGE

// The maximum number of seconds to count out in USER mode.
#define MAX_USER_SECONDS        20


// --- General Settings ---

#define LOOP_WAIT_TIME          20                             // The amount of time to wait im miliseconds per loop while waiting for something.

#define SNTP_1                  "0.pool.ntp.org"               // Primary SNTP server.
#define SNTP_2                  "1.pool.ntp.org"               // Secondary SNTP server.
#define TIMEZONE                "CET-1CEST,M3.5.0,M10.5.0/3"   // Time zone for the clock. Here I'm using Budapest time.

#define ADC_FACTOR              2                              // The factor to multiply the measured voltage with. Depends on the voltage divider.
#define ADC_OVER_SAMPLE_COUNT   8                              // How many times to sample and average the 

#define FULL_BATTERY_TOLERANCE  50                             // How much off can the battery voltage be from full, to be still considered as full.
#define CRITICAL_BATTERY_LEVEL  3500                           // Below this battery voltage, critical mode will be activated.

uint8_t resync_at[3] =          {5, 11, 17};                   // Resync the clock at there hours. (24h format)

#define OMIT_SLEEP              1                              // If there are less than this many seconds to the minute, we wait instead of going to sleep.
#define SLEEP_MARGIN            150                            // The processor wakes this many milliseconds up before the designated update time.
