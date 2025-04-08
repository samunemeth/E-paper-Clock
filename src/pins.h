#ifndef USED_PINS
#define USED_PINS

// --- Pin Definitions ---

#define BATT_SENSE_PIN       0             // May need aux power
#define BATT_SENSE_PIN_NUM   GPIO_NUM_0

#define OTA_SW_PIN           1             // Active low
#define OTA_SW_PIN_NUM       GPIO_NUM_1

#define BTN_TOP_PIN          2             // Active low
#define BTN_TOP_PIN_NUM      GPIO_NUM_2

#define BTN_BOTTOM_PIN       3             // Active low
#define BTN_BOTTOM_PIN_NUM   GPIO_NUM_3

#define CHARGE_SENSE_PIN     4             // Active low
#define CHARGE_SENSE_PIN_NUM GPIO_NUM_4

#define AUX_PWR_PIN          5             // Active high
#define AUX_PWR_PIN_NUM      GPIO_NUM_5

#define DISP_RES_PIN         6
#define DISPLAY_BUSY_PIN     7

#define EXT_LED_PIN          8             // May need aux power, Internal and External LED

#define DISP_CS_PIN          9             // SS, GLOBALLY-SET
#define DISP_DC_PIN          10
#define DISP_MOSI_PIN        20            // MOSI, GLOBALLY-SET
#define DISP_SCL_PIN         21            // SCK, GLOBALLY-SET


#endif /* USED_PINS */
