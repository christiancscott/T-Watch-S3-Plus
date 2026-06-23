<p align="center">
<img src="https://img.shields.io/github/last-commit/christiancscott/T-Watch-S3-Plus.svg?style=for-the-badge" />
&nbsp;
<img src="https://img.shields.io/github/license/christiancscott/T-Watch-S3-Plus.svg?style=for-the-badge" />
&nbsp;
<img src="https://img.shields.io/github/actions/workflow/status/christiancscott/T-Watch-S3-Plus/build-t-watch-s3-plus.yml?style=for-the-badge&label=S3%20build" />
</p>
<hr/>

# T-Watch S3 Plus

A port of the [**My-TTGO-Watch**](https://github.com/sharandac/My-TTGO-Watch) firmware (the
*hedge* GUI) to the **LilyGo T-Watch S3 Plus** — the newer **ESP32-S3** smartwatch. It brings the
mature My-TTGO-Watch app/watchface ecosystem to the S3 hardware, with a new board target, a
browser-based web flasher, and a CI cloud build so you can flash it without a local toolchain.

> ⚠️ **Status: hardware bring-up.** The `t-watch-s3-plus` firmware **compiles green in CI** and the
> board boots, but it has **not yet been fully validated on real hardware**. Some subsystems are still
> being verified (AXP2101 power rails, audio I²S, GPS/LoRa RF). Flashing is reversible — you can always
> re-flash LilyGo's stock firmware. See [`port-plan.md`](port-plan.md) for the detailed status matrix.

This fork is built on top of the upstream My-TTGO-Watch project, which also supports the
T-Watch2020 (V1/V2/V3), T-Watch2021 (V1/V2), M5Paper, M5Core2, WT32-SC01 and native Linux (SDL) for
testing. Those targets are preserved; this fork adds the `LILYGO_WATCH_S3_PLUS` board.

## Why a separate port?

The T-Watch S3 Plus differs from the older T-Watch 2020 in three hard ways that the upstream code
could not absorb as a simple pin remap:

* **MCU: ESP32 → ESP32-S3.** The 2020 builds rely on a custom ESP32-only Arduino fork
  (`arduino-esp32-hedge`) for dynamic frequency scaling. The S3 uses the **stock arduino-esp32 2.0.x**
  framework (`espressif32@^6.5.0`) with **OPI (octal) PSRAM** and **native USB-CDC** serial.
* **PMU: AXP202 → AXP2101.** A different power-management chip with a different register map and rail
  layout, rewritten against **XPowersLib**.
* **Audio: internal DAC → MAX98357A I²S amp.** The S3 has no DAC; audio is pure digital I²S out.

The display is driven by **LovyanGFX** (TFT_eSPI faults on the S3), and the touch panel lives on its
own **second I²C bus**.

## T-Watch S3 Plus hardware

| Subsystem | Part | Notes |
|---|---|---|
| MCU | ESP32-S3 (LX7), 240 MHz, native USB | 16 MB flash / 8 MB OPI PSRAM |
| Display | ST7789V3 240×240 SPI | driven by **LovyanGFX**, backlight PWM on GPIO45 |
| Touch | FocalTech **FT6336U** | on a separate I²C bus (SDA 39 / SCL 40) |
| PMU | **AXP2101** (XPowersLib) | battery gauge, charging, PWRKEY |
| RTC | PCF8563 | |
| IMU | BMA423 | step counter, tilt-to-wake |
| Haptics | DRV2605 | |
| Audio | **MAX98357A** I²S class-D amp | digital-only output |
| Microphone | PDM digital mic | hardware present, driver deferred |
| GPS | u-blox **MIA-M10Q** (UART) | feeds the GPS / tracker / map apps |
| LoRa | **SX1262** (RadioLib) | new `lora` app (US915 default) |
| IR | IR emitter (GPIO2) | full IRController app |
| Radio | Wi-Fi 2.4 GHz / **BLE 5** | BLE-only (no BT classic) |
| Battery | 940 mAh | |
| SD card | **none** | this unit has no microSD slot |

Full GPIO map and chip addresses are in
[`lib/twatch_s3_plus/twatch_s3_plus_config.h`](lib/twatch_s3_plus/) and [`port-plan.md`](port-plan.md).

## Features

Inherited from My-TTGO-Watch and working on the S3 Plus:

* BLE communication, time sync and notifications (companion app: **Gadgetbridge**)
* Step counting and wake-up on wrist rotation
* Quick settings: WiFi, Bluetooth, GPS, brightness, volume
* Watchfaces — embedded digital plus [community watchfaces](https://sharandac.github.io/My-TTGO-Watchfaces/)
* Built-in webserver and FTP server (over WiFi)
* Apps: Music control, Navigation, Map (OSM), Notifications, Stopwatch, Alarm, Step counter,
  Weather, Calendar, Calculator, **IR remote**, **LoRa**, GPS status / tracker, and more

New on the S3 Plus port:

* **LoRa app** driving the SX1262 radio (RadioLib)
* **GPS** via the u-blox MIA-M10Q
* **IR remote** on GPIO2

## Flashing

### Easiest: web flasher (no toolchain)

A single-file, browser-based flasher (like the Meshtastic/Bruce flashers) ships in
[`flasher/`](flasher/) and is published to **GitHub Pages** by CI. It uses Espressif's `esptool-js`
and the **Web Serial API**, so it runs entirely in your **Chromium-based browser** (Chrome, Edge,
Brave, Opera — *not* Firefox/Safari) and talks to the watch over USB-C. Nothing is uploaded anywhere.

1. Open the hosted flasher (latest CI build): **https://christiancscott.github.io/T-Watch-S3-Plus/**
   — or download `firmware-merged.bin` from the
   [Actions build artifact](../../actions) and open `flasher/index.html` locally.
2. Plug in the watch, click **Connect**, pick the serial port, then **Flash**.
3. If the device isn't detected, hold **BOOT**, tap **RST**, release **BOOT** to force download mode.

See [`flasher/README.md`](flasher/README.md) for the flash layout and details.

### From source (PlatformIO)

Clone the repository, open it in [PlatformIO](https://platformio.org/), select the
**`t-watch-s3-plus`** environment, then build and upload:

```bash
pio run -e t-watch-s3-plus -t upload
```

The CI workflow [`.github/workflows/build-t-watch-s3-plus.yml`](.github/workflows/build-t-watch-s3-plus.yml)
builds this env on every push, merges a flashable `firmware-merged.bin`, verifies the image is
bootable, and publishes the web flasher to Pages.

### Native Linux (SDL emulator)

For UI work without hardware, install the SDL/curl/mosquitto dev libs and pick an `emulator_*` env:

```bash
sudo apt-get install libsdl2-dev libcurl4-gnutls-dev libmosquitto-dev build-essential
```

## Known issues / limitations

* **Not yet validated on real hardware** — treat first builds as experimental.
* AXP2101 rail map, display RST/backlight polarity, and BMA axis remap still need on-device tuning.
* Audio (MAX98357A I²S) and the PDM microphone are not yet fully wired up.
* **Power tuning is pending** — without the ESP32-only `hedge` fork's frequency scaling, idle battery
  life will be worse until AXP2101 standby rails + light sleep are tuned.
* **No magnetometer / environmental sensor** on this board — the Compass app and temp/humidity
  readouts are no-ops.
* **No SD card slot** — storage falls back to the 16 MB flash (SPIFFS/FATFS).
* The webserver can be unstable on ESP32 (inherited upstream issue).

## How to use

See [`USAGE.md`](USAGE.md) and the watchface guide in [`WATCHFACE.md`](WATCHFACE.md).

## For developers

* Porting status and architecture: [`port-plan.md`](port-plan.md)
* Contribution guide: [`CONTRIBUTING.md`](CONTRIBUTING.md)
* Apps integrate without touching core code via the autocall mechanism:
  [`autocall.md`](autocall.md)

The codebase abstracts hardware per-module in `src/hardware/*.cpp` using
`#if defined( LILYGO_WATCH_S3_PLUS )` ladders, with one PlatformIO `[env:...]` per board in
`platformio.ini`. The S3 port adds one `#elif` branch per module plus the new pin/config header.

## Credits

This is a downstream port. All credit for the firmware, GUI and app ecosystem goes to
[**sharandac/My-TTGO-Watch**](https://github.com/sharandac/My-TTGO-Watch) and its contributors:

[5tormChild](https://github.com/5tormChild),
[bwagstaff](https://github.com/bwagstaff),
[chrismcna](https://github.com/chrismcna),
[datacute](https://github.com/datacute),
[fliuzzi02](https://github.com/fliuzzi02),
[guyou](https://github.com/guyou),
[jakub-vesely](https://github.com/jakub-vesely),
[joshvito](https://github.com/joshvito),
[JoanMCD](https://github.com/JoanMCD),
[NorthernDIY](https://github.com/NorthernDIY),
[Neuroplant](https://github.com/Neuroplant),
[paulstueber](https://github.com/paulstueber),
[pavelmachek](https://github.com/pavelmachek),
[rnisthal](https://github.com/rnisthal),
[ssspeq](https://github.com/ssspeq).

Built on these projects: [LVGL](https://github.com/lvgl),
[LovyanGFX](https://github.com/lovyan03/LovyanGFX),
[XPowersLib](https://github.com/lewisxhe/XPowersLib),
[SensorLib / BMA423_Library](https://github.com/lewisxhe/BMA423_Library),
[RadioLib](https://github.com/jgromes/RadioLib),
[IRremoteESP8266](https://github.com/crankyoldgit/IRremoteESP8266),
[TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus),
[NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino),
[ArduinoJson](https://github.com/bblanchon/ArduinoJson),
[AsyncTCP](https://github.com/me-no-dev/AsyncTCP),
[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer),
[ESP32SSDP](https://github.com/luc-github/ESP32SSDP),
[ESP32-targz](https://github.com/tobozo/ESP32-targz),
[ESP8266Audio](https://github.com/earlephilhower/ESP8266Audio),
[PubSubClient](https://github.com/knolleary/pubsubclient),
[Adafruit DRV2605 Library](https://github.com/adafruit/Adafruit_DRV2605_Library),
and the [esptool-js](https://github.com/espressif/esptool-js) web flasher.

## Interface

Screenshots below are from the upstream supported boards (T-Watch 2020 shown); the S3 Plus runs the
same GUI.

![screenshot](images/screen1.png)
![screenshot](images/screen2.png)
![screenshot](images/screen3.png)
![screenshot](images/screen4.png)
![screenshot](images/screen5.png)
![screenshot](images/screen6.png)
</content>
</invoke>
