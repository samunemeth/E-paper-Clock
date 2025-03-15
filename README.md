> [!IMPORTANT]
> More features are hopefully coming soon!
>
> This repository is not even close to being finished.
> I will try to include all the files and useful resources,
> but there are a lot, so it will take time.


# E-papper Clock 

This is an e-papper clock based on a *WeAct Studio 2.9" BW* display, and a custom *Weather Station V1* mainboard.

## Current functionality

The clock can connect to a local network to get the time hen powered on.
After this, and internal timer is calibrated to the current timezone.
The display is refreshed every minute. Between the refreshes, the *ESP32-C3*
enters deep sleep to preserve battery power.

At set hours, the clock
will resync to a time server, and it will show the last resync.
The clock can also monitor it's battery level.
By pressing the update button, the display is kept awake. After a second press, or
reset, it continues normal operation.
Pressing the top button enables user mode, witch shows the seconds for a set
amount of time, or until the user presses the button again.

The settings can be configured easily. There is an option to power down
the display when the *ESP* goes to deep sleep: `POWER_DOWN_DISPLAY`
And an option to prefer fast refreshes where possible: `MAX_USER_SECONDS`

Messages, and time intervals can also be customised without much hassle.
First look at the settings file `settings.h`, and if you dont find anything there,
use a search on the main part of the code.

> [!NOTE]
> I try to document the code as much as I can, but if you have any questions,
> feel free to contact me.

## ToDo

**Done**
  - ~~Periodic re-sync to the time server.~~
  - ~~Show last sync time.~~
  - ~~Show custom page in update mode.~~
  - ~~Change delays to async ones, or sleeps.~~
  - ~~Add GPIO wakeup to buttons.~~
  - ~~Implement user mode~~
  - ~~Single refresh on resync option.~~
  - ~~Calibrate the ADC a bit more.~~

**Implement**
  - Multiple WiFi connections. *[More Info](https://randomnerdtutorials.com/esp32-wifimulti/)*
  - Skip resync if the wifi times out.
  - More text on update mode.
  - Better fonts, for example *Roboto*.
  - Show wifi strength at last sync.
  - Critical battery mode, stop operation.
  - Manual time settings. *(This is hard to do btw.)*
  - Look at "TODO" labeled comments.
  - Clean up `platformio.ini` file.
  - Move `display-helper.h` files, disambiguate `Fonts` directory.
  - Battery voltage to percent.

**Test**
  - Test the two different internal oscillators. *[More Info](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/system_time.html)*
  - Test if powering off the display is actually worth it. 

**Document**
  - Document the font creation process.
  - Document assembly and schematics.
  - Document global SPI port remapping.
  - Document WiFi setup.
  - Document ADC calibration.

**Hardware Fix**
  - Buttons have to be active low.
  - Not all pins can wake up from deep sleep.
  - Should prefer ADC1 over ADC2.
  - Testing, measuring points.

## Setup

  - Set your WiFi credentials.
  - Change your port settings in the project file. You can also use auto port detection on windows but *not* on linux.
  - Select the correct environment for uploading:
    - With `platformio.run -- environment YourEnvironment` on command line.
    - By selecting the correct folder in *VSCode*.
  - Calibrate the ADC if you want it to be really precise. *[More Info](https://w4krl.com/esp32-analog-to-digital-conversion-accuracy/)*

## Design Choices

### Modes

Modes are stored in a `uint8_t`, and have constants with their names
and a `*_MODE` suffix.
A wanted mode can be written to EEPROM, to be selected on the next reboot.

| Mode Name: | NULL | NORMAL | UPDATE | USER | RESET | RESYNC |
| ---: | :---: | :---: | :---: | :---: | :---: | :---: |
| Decimal Value: | 0 | 1 | 2 | 4 | 8 | 16 |
| Bit shifted Value: | 0 | 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 

 
### Timer Choice

The project is currently uses the *8MD256* clock source over the default *RC* oscillator.
This is presumably more accurate, but uses more power, and only lets the ESP enter a lower
level of deep sleep. More info on [sub sleep levels](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-reference/system/sleep_modes.html#sub-sleep-modes).
The compromises are not yet fully clear to me, *further testing and investigation is required*.


## Useful Resources

  - Purchase display: [AliExpress](https://www.aliexpress.com/item/1005004644515880.html?spm=a2g0o.order_list.order_list_main.89.31de1802V2DEme).
  - Library used for the display: [GxEPD2](https://github.com/ZinggJM/GxEPD2)
  - Sleep modes on the *ESP32-C3*: [Espressif Documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32c3/api-reference/system/sleep_modes.html).
  - Showcasing the display and library: [Video](https://youtu.be/KZGjsC-JkR8?si=c3sMc7xT4hFs9A2L).
    And the code they used: [GitHub](https://github.com/devtales-official/screen-test/tree/main/devtales_screentest_ep2).
  - Generating new fonts: [Adafruit Documentation](https://learn.adafruit.com/adafruit-gfx-graphics-library/using-fonts).
  *(tl;dr: You pretty much must do this on linux.)*
  - How to remap global SPI ports: [Arduino Forum Thread](https://forum.arduino.cc/t/understanding-spi-pin-remapping-for-gxepd2-on-a-esp32-c3-mini/1065982).
  *(I looked 3 hours for this.)*
  - Advanced deep sleep wakeup functionality: [Espressif Documentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32c3/api-guides/deep-sleep-stub.html).
  - Deep sleep wakeup with gpio: [StackOverflow](https://stackoverflow.com/questions/76823215/deep-sleep-with-ext0-or-ext1-on-esp32-c3-mini-1).
  - Holding pins in deep sleep: [Reddit](https://www.reddit.com/r/esp32/comments/1dhh5ez/esp32c3_pin_goes_high_on_deep_sleep/).
  - ADC accuracy: [Blog](https://w4krl.com/esp32-analog-to-digital-conversion-accuracy/).

## Ports, Future Updates

The display could be reasonably used with *Esphome*. The configuration however is quite difficult, and the power efficiency will 
definitely be worse. I do not think I will be attempting to port it. I already have code for a [similar project](#related-projects).

I'm already designing a new version of the mainboard. This version will probably have a new pinout, and may not require the global SPI remap. The current board requires some patching, and does not fulfill all my requirements. I will not be uploading this board version,
but I'm working on the new one, that should work out of the box.

## Related Projects

The mainboard is borrowed from *Weather Station V1*. The only hardware difference is the display, as that project uses the 3 color version.
The software however is completely different, as that project uses *Esphome*.

After designing the new mainboard, I will be publishing mor information and configuration regarding that project, in a separate repo.