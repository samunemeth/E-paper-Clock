
// --- Behaviour ---

/* 
    Concerning the two different types of displays
    On the older, FPC-A005 B/W display:
      - Full refreshes take 4x the time and power of a partial refresh.
      - Display remains clear after multiple partial refreshes.
      - I recommend preferring fast refreshes, and full refreshing sparsely.
    On the newer, FPC-7519rev.b display:
      - Full refreshes take 2x the time and power of a partial refresh.
      - Display quality quickly degrades after a few partial refreshes.
      - I recommend preferring full refreshes, and full refreshing often.
    See README.md for more info!
*/

/* 
    Prefers partial refreshes over full refreshes.
    This does not affect normal mode!
    Normal mode is affected by the next option.
*/
//#define PREFER_FAST_REFRESH

/* 
    Do a full refresh in normal mode after this many boots.
    As in normal mode a boot occurs approximately every minute,
    this is basically in minutes.
*/
#define FULL_REFRESH_EVERY 5

/* 
    The following options have to be configured as to reflect
    the solder jumpers connected on the motherboard.
    Enable options where the jumper is connected to the aux power.
    The recommended configuration for the new display panel is shown below.
*/
//#define AUX_FOR_DISP
#define AUX_FOR_BATT_SENSE
#define AUX_FOR_EXT_LED

/* 
    Read the battery after this many boots.
    As in normal mode a boot occurs approximately every minute,
    this is basically in minutes.
*/
#define BATT_SENSE_EVERY 30

/* 
    Resync after this many boots.
    As in normal mode a boot occurs approximately every minute,
    this is basically in minutes.
*/
#define RESYNC_EVERY 240
//#define SKIP_SYNC

/* 
    Report telemetry data to a web server on resync.
    The url to report to can be configured in `wifi_secrets.h`
    This may consume a bit more power, but is really usefull for data logging.
*/
#define REPORT_TELEMETRY

// --- General Settings ---

#define LOOP_WAIT_TIME          20                             // The amount of time to wait im miliseconds per loop while waiting for something.

#define SNTP_1                  "0.pool.ntp.org"               // Primary SNTP server.
#define SNTP_2                  "1.pool.ntp.org"               // Secondary SNTP server.
#define TIMEZONE                "CET-1CEST,M3.5.0,M10.5.0/3"   // Time zone for the clock. Here I'm using Budapest time.

#define ADC_FACTOR              2                              // The factor to multiply the measured voltage with. Depends on the voltage divider.
#define ADC_OVER_SAMPLE_COUNT   16                             // How many times to sample and average the 

#define FULL_BATTERY_TOLERANCE  50                             // How much off can the battery voltage be from full, to be still considered as full.
#define CRITICAL_BATTERY_LEVEL  3500                           // Below this battery voltage, critical mode will be activated.

#define OMIT_SLEEP              1                              // If there are less than this many seconds to the minute, we wait instead of going to sleep.
#define SLEEP_MARGIN            100                            // The processor wakes this many milliseconds up before the designated update time.

#define MAX_DISPLAYED_SECONDS   10                             // The maximum number of seconds to count out in USER mode.