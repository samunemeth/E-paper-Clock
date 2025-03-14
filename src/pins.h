#ifndef USED_PINS
#define USED_PINS

// --- Pin Definitions ---

#define BATT_SENSE_PIN   0             // Battery voltage divider, needs auxiliary power
#define OTA_SW_PIN       1             // Active low
#define OTA_SW_PIN_NUM   GPIO_NUM_1    // For deep sleep wakeup
#define BTN_TOP_PIN      2             // Active low
#define BTN_TOP_PIN_NUM  GPIO_NUM_2    // For deep sleep wakeup
#define AUX_PWR_PIN      3             // High to enable auxiliary power
#define AUX_PWR_PIN_NUM  GPIO_NUM_3    // For deep sleep holding
#define EXT_LED_PIN      4             // Active low
#define DISP_RES_PIN     5
#define DISP_CS_PIN      6             // SS, GLOBALLY-SET
#define DISP_DC_PIN      7
#define BTN_BOTTOM_PIN   8             // Active low, not yet wired
#define UNUSED_PIN       9             // MISO, GLOBALLY-SET, Not connected
#define DISPLAY_BUSY_PIN 10
#define DISP_MOSI_PIN    20            // MOSI, GLOBALLY-SET
#define DISP_SCL_PIN     21            // SCK, GLOBALLY-SET

#endif /* USED_PINS */
