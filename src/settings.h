
// --- Behaviour ---

// Uses fast refresh whenever possible. This makes the display a bit less clear, but preserves power.
#define PREFER_FAST_REFRESH

// Powers down display in deep sleep.
// After wakeup, a full refresh must be performed.
//#define POWER_DOWN_DISPLAY

// Displays "STARTING" text after a reset. Can cause ghosting.
//#define DISPLAY_START_MESSAGE

// The maximum number of seconds to count out in USER mode.
#define MAX_USER_SECONDS        20

// --- General Settings ---

#define LOOP_WAIT_TIME          20                             // The amount of time to wait im miliseconds per loop while waiting for something.

#define SNTP_1                  "0.pool.ntp.org"               // Primary SNTP server.
#define SNTP_2                  "1.pool.ntp.org"               // Secondary SNTP server.
#define TIMEZONE                "CET-1CEST,M3.5.0,M10.5.0/3"   // Time zone for the clock. Here I'm using Budapest time.

#define ADC_MAGIC_VAL           0.0014                         // The conversion factor between the ADC output and battery voltage. It includes the voltage divider.

//uint8_t resync_at[3] =       {5, 11, 17};                    // Resync the clock at there hours. (24h format)
uint8_t resync_at[3] =          {12};                          // Resync the clock at there hours. (24h format)

#define OMIT_SLEEP              3                              // If there are less than this many seconds to the minute, we wait instead of going to sleep.
#define SLEEP_MARGIN            150                            // The processor wakes this many milliseconds up before the designated update time.
