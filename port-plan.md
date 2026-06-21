# Porting My‑TTGO‑Watch (hedge) to the LilyGo T‑Watch S3 Plus

> Status: **Planning draft**
> Source platform: **TTGO T‑Watch 2020 (V1/V2/V3)** — Espressif **ESP32** (Xtensa LX6)
> Target platform: **LilyGo T‑Watch S3 Plus** — Espressif **ESP32‑S3** (Xtensa LX7)
> Chosen strategy (confirmed with maintainer): **granular component libraries** dropped into the
> existing per‑module `#if defined( BOARD )` pattern, delivered in **two phases** (core watch first,
> new S3‑Plus hardware second).

---

## 0. Live progress

> Tracking the actual implementation on branch `claude/eloquent-galileo-yxdevo` (PR #1). The
> `t-watch-s3-plus` PlatformIO env **compiles green in CI**, which also publishes the web flasher
> to GitHub Pages.

| Area | Status |
|---|---|
| Phase 0 — build system (env, pin header, `config.h`) | ✅ done |
| Phase 1a — display (ST7789), framebuffer, touch (FT6336U on Wire1) | ✅ done |
| Phase 1b — AXP2101 PMU (XPowersLib) + power button | ✅ done |
| Phase 1c — BMA423 IMU + PCF8563 RTC + DRV2605 haptics | ✅ done |
| Phase 1d — audio (MAX98357A) | ⏸️ **deferred** — ESP8266Audio has no version compatible with both the S3 and arduino‑esp32 2.0.x; needs a direct ESP‑IDF `driver/i2s.h` driver |
| GUI/app guards (IRController stub, tile layouts) | ✅ done |
| CI cloud build + downloadable artifact + **web flasher on GitHub Pages** | ✅ done |
| Phase 2 — GPS (MIA‑M10Q over Serial2) | ✅ (needs AXP2101 GPS rail confirmed for live fix) |
| Phase 2 — LoRa (SX1262, RadioLib) + mini LoRa app | ✅ (US915 default; verify TCXO/RF‑switch on hardware) |
| Phase 2 — IR remote (GPIO2, IRremoteESP8266) | ✅ full IRController app enabled on S3 |
| Phase 2 — PDM microphone | ⏸️ deferred — no consumer yet; needs an app/use‑case + ESP‑IDF PDM RX |
| On‑device validation | ⏳ not yet flashed/tested on real hardware |

**Key follow‑ups for real hardware:** audio I²S driver; AXP2101 rail map (which rails feed display/
touch/GPS/LoRa); confirm display RST/backlight polarity; tune BMA axis remap and power saving.

---


## 1. Executive summary

The good news: this is a **far smaller job than a typical port** because most of the T‑Watch S3 Plus
peripherals are the *same chips* (or the same chip families) already supported for the T‑Watch 2020
and 2021. The codebase is already structured for multi‑board support: every hardware module in
`src/hardware/` switches behaviour with `#if defined( LILYGO_WATCH_2020_V1 ) … #elif … #endif`
blocks. Porting means **adding one new board macro (`LILYGO_WATCH_S3_PLUS`) and filling in a new
`#elif` branch in each module**, plus a new PlatformIO environment and pin‑config header.

The three genuinely hard changes are:

1. **MCU change: ESP32 → ESP32‑S3.** The existing T‑Watch 2020 build relies on a *custom* Arduino‑ESP32
   fork (`sharandac/arduino-esp32-hedge`, `espressif32@3.3.0`) that is **ESP32‑only** and provides
   dynamic frequency scaling / power tuning. The S3 needs a newer, S3‑capable framework. This is the
   single biggest source of risk (toolchain + power management regression).
2. **PMU change: AXP202 → AXP2101.** Different chip, different register map, different power rails.
   The AXP202 calls are pervasive in `pmu.cpp`, `display.cpp`, `sound.cpp`, and `button.cpp`. These
   must be rewritten against **XPowersLib**.
3. **Audio change: ESP32 internal DAC → MAX98357A I²S class‑D amp (+ PDM microphone).** The S3 has no
   DAC; audio becomes pure digital I²S out, and the mic is new input hardware.

Everything else (display, touch, IMU, RTC, haptics) reuses an existing chip family and is mostly a
pin‑map + library‑selection exercise.

---

## 2. Hardware comparison: T‑Watch 2020 vs T‑Watch S3 Plus

| Subsystem | T‑Watch 2020 (current) | T‑Watch S3 Plus (target) | Port difficulty |
|---|---|---|---|
| MCU | ESP32 (LX6), 240 MHz | **ESP32‑S3** (LX7), 240 MHz, native USB | **High** (toolchain) |
| Flash / PSRAM | 16 MB / 8 MB (QSPI) | **16 MB / 8 MB OPI (octal) PSRAM** | Medium (build flags) |
| Display | ST7789V 240×240 SPI | **ST7789V3 240×240 SPI** (same family) | Low |
| Backlight | AXP202 / TTGO BL class | GPIO **45** PWM (ledc) | Low |
| Touch | FocalTech FT6236 (I²C) | **FocalTech FT6336U** (I²C, *separate* bus) | Low–Med |
| PMU | **AXP202** | **AXP2101** | **High** |
| RTC | PCF8563 | **PCF8563** (same) | Low |
| IMU | BMA423 | **BMA423** (same) | Low |
| Haptics | GPIO motor (V1/V3) / DRV2605 (V2) | **DRV2605** (same as 2020 V2) | Low |
| Audio out | ESP32 internal DAC via I²S + AXP LDO | **MAX98357A** I²S class‑D amp | **High** |
| Microphone | none | **PDM digital mic** (new) | Medium (new feature) |
| GPS | external/optional | **u‑blox MIA‑M10Q** on UART | Medium (Phase 2) |
| LoRa radio | none | **SX1262** *(confirmed for this unit)* | Medium (new feature, Phase 2) |
| IR | optional IR LED | **IR emitter GPIO 2** | Low (Phase 2) |
| Wi‑Fi / BLE | ESP32 2.4 GHz / BT classic+BLE | ESP32‑S3 2.4 GHz / **BLE 5 only** | Low |
| Buttons | AXP202 PEK power key | **AXP2101 PWRKEY** + BOOT | Low–Med |
| SD card | model dependent | **none — no slot on this unit** | n/a (stub) |
| Battery | ~380–500 mAh | **940 mAh** *(confirmed)* | n/a |

### Confirmed S3 Plus GPIO map (from LilyGo documentation)

```
Main I²C  (AXP2101 / PCF8563 / BMA423):  SDA = 10, SCL = 11
Touch I²C (FT6336U, separate bus):       SDA = 39, SCL = 40, INT = 16
Interrupts:  AXP2101 = 21,  PCF8563 RTC = 17,  BMA423 = 14
Display SPI: CS = 12, MOSI = 13, SCK = 18, DC = 38, BL = 45   (RST: verify — often -1/shared)
Audio I²S (MAX98357A):  BCLK = 48, WCLK/LRCLK = 15, DOUT = 46
PDM mic:                CLK/SCK = 44, DATA = 47
GPS (MIA-M10Q UART):    TX = 42, RX = 41
LoRa (SX1262):  SCK = 3, MISO = 4, MOSI = 1, CS = 5, IRQ/DIO1 = 9, RST = 8, BUSY = 7
IR emitter: 2
Power button: dedicated (via AXP2101 PWRKEY);  BOOT: built-in
```

> ⚠️ Confirm display RST/backlight polarity and the AXP2101 rail map against **LilyGoLib's pin
> header / board‑init source** for this exact board before writing drivers — see §10. (Radio = SX1262,
> no SD card, 940 mAh battery are confirmed.)

---

## 3. How the codebase abstracts hardware (what we are extending)

* `src/config.h` — top‑level per‑board macro block: defines `HARDWARE_NAME`, `RES_X_MAX/Y_MAX`,
  feature toggles (`USE_PSRAM_ALLOC_LVGL`, `ENABLE_WEBSERVER`, `ENABLE_FTPSERVER`, …) and pulls in the
  board library header (e.g. `#include <LilyGoWatch.h>` for 2020).
* `src/hardware/hardware.cpp` — `hardware_setup()` brings up LVGL, the board, I²C, SPIFFS, then calls
  each module's `*_setup()` in order.
* `src/hardware/*.cpp` — one module per subsystem, each with a `#if defined(BOARD)` ladder:
  `display`, `framebuffer` (LVGL flush), `touch`, `motion` (BMA423), `rtcctl`, `pmu`, `sound`
  (+`config/soundconfig`), `motor`, `gpsctl`, `compass`, `sdcard`, `sensor`, `button`.
* `platformio.ini` — one `[env:...]` per board with `platform`, `board`, `framework`, build flags,
  partition CSV, and `lib_deps`.

Approximate per‑module ifdef branch counts (to scope the edit surface):
`rtcctl` ~71, `motion` ~45, `compass` ~40, `touch` ~29, `framebuffer` ~26, `hardware`/`button`/
`sensor` ~19, `sound`/`gpsctl` ~18, `motor`/`sdcard` ~15. We add **one branch per ladder**.

There is currently **zero ESP32‑S3 awareness** in the tree (no `CONFIG_IDF_TARGET`, no `ESP32S3`),
so build‑system work in Phase 0 is unavoidable.

---

## 4. Library / toolchain plan (Phase 0 — foundation)

This must land first; nothing else compiles without it.

### 4.1 PlatformIO environment

Add `[env:t-watch-s3-plus]`:

```ini
[env:t-watch-s3-plus]
platform = espressif32@^6.5.0            ; arduino-esp32 2.0.x — first S3-capable line
board = esp32-s3-devkitc-1               ; or a custom board JSON for 16MB/8MB-OPI
framework = arduino
board_build.mcu = esp32s3
board_build.flash_mode = qio
board_build.f_flash = 80000000L
board_build.arduino.memory_type = qio_opi ; 8 MB OPI PSRAM
board_build.partitions = default_16MB.csv ; reuse existing 16 MB CSV (already in repo)
monitor_speed = 115200
build_flags =
    -D LILYGO_WATCH_S3_PLUS
    -D BOARD_HAS_PSRAM
    -D ARDUINO_USB_CDC_ON_BOOT=1          ; native-USB serial console
    -D LV_LVGL_H_INCLUDE_SIMPLE
    -D CORE_DEBUG_LEVEL=3
    -D SERIAL_RX_BUFFER_SIZE=256
    ; --- TFT_eSPI ST7789 config (see 4.3) ---
    -D USER_SETUP_LOADED=1
    -D ST7789_DRIVER=1
    -D TFT_WIDTH=240
    -D TFT_HEIGHT=240
    -D TFT_MOSI=13 -D TFT_SCLK=18 -D TFT_CS=12 -D TFT_DC=38 -D TFT_RST=-1
    -D TFT_BL=45
    -D SPI_FREQUENCY=80000000
    -ffunction-sections -fdata-sections -Wl,--gc-sections -Os
build_src_filter = +<*>
lib_deps =
    https://github.com/lvgl/lvgl.git#v7.11.0
    Bodmer/TFT_eSPI@~2.5.43
    lewisxhe/XPowersLib            ; AXP2101
    lewisxhe/SensorLib             ; BMA423 + PCF8563 + FT6336 helpers (or per-chip libs)
    adafruit/Adafruit DRV2605 Library
    me-no-dev/ESPAsyncWebServer@~1.2.4
    me-no-dev/AsyncTCP@~1.1.1
    ArduinoJson@~6.21.0
    knolleary/PubSubClient@~2.8
    mikalhart/TinyGPSPlus@~1.1.0   ; Phase 2 (GPS)
    h2zero/NimBLE-Arduino@~1.4.3
    jgromes/RadioLib               ; Phase 2 (SX1262 LoRa — confirmed for this unit)
```

> ⚠️ **Critical toolchain note.** The 2020 envs use `sharandac/arduino-esp32-hedge` (an ESP32‑only
> framework fork that adds dynamic frequency scaling and 80 MHz flash/PSRAM tuning). **That fork does
> not support the S3.** The S3 must use stock `arduino-esp32` 2.0.x (≈ `espressif32@6.x`). Two
> consequences to plan for:
> 1. **Power‑management regression risk:** the hedge fork's DFS/low‑power tricks are unavailable.
>    Battery life on the first S3 build will likely be worse until we re‑implement power saving via
>    AXP2101 rails + `esp_pm`/light‑sleep. (See §8.)
> 2. Remove ESP32‑only flags: **`-mfix-esp32-psram-cache-issue` must be deleted** for the S3 env (it
>    is an LX6 silicon workaround and is invalid on S3).

### 4.2 New pin/config header

Create `lib/twatch_s3_plus/twatch_s3_plus_config.h` (mirrors the existing `lib/twatch2021/`
`twatch2021_config.h` approach) holding all GPIO `#define`s from §2, I²C bus objects, and chip I²C
addresses (AXP2101 0x34, PCF8563 0x51, BMA423 0x18/0x19, FT6336 0x38, DRV2605 0x5A).

### 4.3 Display library

Follow the **2021/WT32‑SC01 pattern** (TFT_eSPI), *not* the 2020 pattern (TTGO library). ST7789 is a
first‑class TFT_eSPI driver, so this is config‑only (flags above). Backlight via `ledc` PWM on GPIO 45
(reuse the LILYGO_WATCH_2021 backlight code in `display.cpp` almost verbatim).

### 4.4 `config.h` board block

Add an `#elif defined( LILYGO_WATCH_S3_PLUS )` block: `HARDWARE_NAME "T-WatchS3Plus"`,
`RES_X_MAX/Y_MAX 240`, `USE_PSRAM_ALLOC_LVGL`, `ENABLE_WEBSERVER`, `ENABLE_FTPSERVER`, and include
the new config header instead of `<LilyGoWatch.h>`.

**Phase 0 done‑when:** the project links and boots to a blank/splash screen with serial logging over
native USB; LVGL ticker runs; SPIFFS mounts.

---

## 5. Phase 1 — core watch bring‑up

Order matches `hardware_setup()`. Each item = add one `#elif defined( LILYGO_WATCH_S3_PLUS )` branch.

| # | Module(s) | Chip / bus | Approach |
|---|---|---|---|
| 1 | `hardware.cpp` | I²C init | Init **two** `Wire` buses: main `Wire(10,11)` and `Wire1(39,40)` for touch. Scan + log. |
| 2 | `framebuffer.cpp` + `display.cpp` | ST7789 (TFT_eSPI) | TFT_eSPI flush like 2021; backlight ledc on GPIO45; rotation/double‑buffer/DMA flags. |
| 3 | `pmu.cpp` + `config/pmuconfig` | **AXP2101** (XPowersLib) | **Rewrite branch.** Map: battery %, voltage, charge/VBUS status, charge‑current, PWRKEY IRQ on GPIO21, enable/disable rails feeding display/touch/audio, light‑sleep wake on IRQ. Replace all `AXP202_*` constants. |
| 4 | `button.cpp` | AXP2101 PWRKEY | Route short/long press via XPowersLib IRQ → existing `PMUCTL_SHORT_PRESS/LONG_PRESS` events (mirror 2020 PEK logic). |
| 5 | `motion.cpp` + `config/bmaconfig` | **BMA423** | Reuse 2021 standalone‑BMA path (I²C 0x18 on main bus, INT GPIO14). Set correct **axis remap** for S3‑Plus mounting/rotation; keep step counter `__NOINIT_ATTR` persistence. |
| 6 | `rtcctl.cpp` + `config/rtcctlconfig` | **PCF8563** | Reuse 2021 `PCF8563_Class` path on main bus; INT GPIO17; alarm + `gpio_wakeup_enable`. |
| 7 | `touch.cpp` + `config/touchconfig` | **FT6336U** | FocalTech family — reuse FT6206/FT6236 driver but on **`Wire1` (39/40)**, INT GPIO16. Watch the separate bus carefully. |
| 8 | `motor.cpp` + `config/motorconfig` | **DRV2605** | Reuse the **2020 V2** `Adafruit_DRV2605` branch (I²C 0x5A on main bus). |
| 9 | `sound.cpp` + `config/soundconfig` | **MAX98357A** I²S | New branch: `AudioOutputI2S` in *external‑DAC* mode (BCLK48/LRCLK15/DOUT46). Replace AXP LDO power gating with AXP2101 rail / amp `SD` enable. Keep MP3/WAV/SAM generators. |
| 10 | `wifictl.cpp`, `blectl.cpp` | ESP32‑S3 radio | Should compile unchanged (NimBLE + WiFi work on S3). Verify BT‑classic assumptions — S3 is **BLE‑only**. |
| 11 | `compass.cpp`, `sensor.cpp` | none on S3 Plus | Add empty/no‑op S3 branch (no magnetometer, no SHT30). |
| 12 | `sdcard.cpp` | none | **No SD slot on this unit** — add a no‑op S3 branch / leave `LILYGO_WATCH_HAS_SDCARD` undefined; hide the SD‑card settings tile. |
| 13 | GUI/app guards | TTGO refs | Guard files that `#include`/call TTGO directly: `gui/splashscreen.cpp`, `screenshot.cpp`, `setup_tile/utilities`, `watchface`, `sdcard_settings`, `app/gps_status`, `app/IRController`, `app/FindPhone`, `app/activity`. Add S3 branches or `#ifndef` guards. |

**Phase 1 done‑when:** watch boots to the watchface; touch navigates the UI; time keeps via RTC;
battery %/charging shown correctly; step counter increments; vibration works; tilt‑to‑wake and
PWRKEY sleep/wake work; BLE pairs with Gadgetbridge; WiFi/webserver/FTP reachable; audio beeps/plays.

---

## 6. Phase 2 — new S3‑Plus hardware

| Feature | Chip | Library | Notes |
|---|---|---|---|
| GPS | u‑blox **MIA‑M10Q** | TinyGPSPlus | `gpsctl.cpp` already supports configurable HardwareSerial pins → set UART TX42/RX41. Likely needs an AXP2101 rail powered on. Wire into the existing GPS/tracker/osmmap apps. |
| Microphone | **PDM mic** (CLK44/DATA47) | ESP‑IDF I²S PDM RX | New capability with no existing consumer. Suggest a minimal “mic level / voice memo” demo or defer until an app needs it. |
| LoRa | **SX1262** *(confirmed)* | RadioLib | Entirely new subsystem (no LoRa anywhere in the firmware today). Needs a new `hardware/lora.cpp` + an app (messaging/Meshtastic‑style or raw RX/TX demo). Pins: SCK 3 / MISO 4 / MOSI 1 / CS 5 / DIO1 9 / RST 8 / BUSY 7. RadioLib's `SX1262` class maps directly to these. |
| IR | IR emitter GPIO2 | IRremoteESP8266 | Re‑point the existing `app/IRController` TX pin to GPIO2. |

---

## 7. File‑by‑file change checklist

**Build / config**
- [ ] `platformio.ini` — add `[env:t-watch-s3-plus]` (§4.1)
- [ ] `lib/twatch_s3_plus/twatch_s3_plus_config.h` — new pin/addr header (§4.2)
- [ ] `src/config.h` — add board macro block (§4.4)
- [ ] partition CSV — reuse `default_16MB.csv` (already present)

**Hardware modules (add one `#elif` branch each)**
- [ ] `hardware.cpp` (dual I²C init)  ⬩ `framebuffer.cpp`  ⬩ `display.cpp`
- [ ] `pmu.cpp` + `config/pmuconfig.*` (AXP2101 — biggest)
- [ ] `button.cpp`  ⬩ `motion.cpp` + `config/bmaconfig.*`  ⬩ `rtcctl.cpp` + `config/rtcctlconfig.*`
- [ ] `touch.cpp` + `config/touchconfig.*` (Wire1)
- [ ] `motor.cpp` + `config/motorconfig.*` (DRV2605)
- [ ] `sound.cpp` + `config/soundconfig.*` (MAX98357A)
- [ ] `compass.cpp`, `sensor.cpp`, `sdcard.cpp` (no‑op / conditional)

**GUI/app TTGO guards** (compile‑blockers)
- [ ] `gui/splashscreen.cpp`, `gui/screenshot.cpp`
- [ ] `gui/mainbar/setup_tile/utilities/utilities.cpp`, `.../watchface/*`, `.../sdcard_settings/*`
- [ ] `app/gps_status/*`, `app/IRController/*`, `app/FindPhone/*`, `app/activity/*`

**Phase 2**
- [ ] `gpsctl.cpp` (UART pins)  ⬩ new `hardware/lora.cpp` + app  ⬩ PDM mic  ⬩ IR pin remap

---

## 8. Hardware limitations & concerns (read this carefully)

1. **Toolchain power regression (biggest risk).** Losing the `arduino-esp32-hedge` fork means losing
   its dynamic frequency scaling. Expect worse idle battery life initially. Mitigation: drive
   low‑power via AXP2101 rail control + `esp_pm`/automatic light sleep + display/touch gating in
   standby. Budget real time for power tuning; it won't match the tuned 2020 build on day one.
2. **AXP2101 ≠ AXP202.** Not register‑compatible. Every AXP202 call (charging targets, LDO/DCDC
   rails, coulomb counter, IRQ flags) must be re‑derived. AXP2101 *does* provide a battery‑percentage
   gauge, which can simplify `pmu_get_battery_percent()`. Get the **rail map right** — which LDO/DCDC
   feeds the display, touch, audio amp, and GPS — or peripherals won't power up. This needs the
   schematic/LilyGoLib power‑init sequence as ground truth.
3. **OPI (octal) PSRAM.** Must build with `qio_opi` memory type; wrong PSRAM mode = boot crash.
   Remove `-mfix-esp32-psram-cache-issue`.
4. **Native USB CDC.** Serial console is USB‑CDC, not UART0. Set `ARDUINO_USB_CDC_ON_BOOT`. OTA flow
   and any “reboot to bootloader” UX differs from the 2020.
5. **Two I²C buses.** Touch is on its own bus (39/40), separate from PMU/RTC/IMU (10/11). The current
   code mostly assumes a single bus — `touch.cpp` must use `Wire1`. Don't scan/init it on the wrong bus.
6. **Audio is digital‑only.** No internal DAC. ESP8266Audio must output to the external I²S amp
   (MAX98357A). Volume is digital (software) + amp gain pin, not an AXP LDO‑voltage trick like 2020.
7. **No magnetometer / no environmental sensor.** `compass.cpp` and `sensor.cpp` are no‑ops on this
   board — the **Compass app and any temp/humidity readouts won't function**; hide or stub them.
8. **BLE‑only (no BT classic).** Confirm nothing in `blectl`/audio assumes A2DP/classic.
9. **No SD card.** This unit has no microSD slot — `sdcard.cpp` gets a no‑op S3 branch and the
   SD‑card settings tile should be hidden. Any feature that assumes SD storage (e.g. some map tile
   caching) must fall back to SPIFFS/FATFS in the 16 MB flash.
10. **LVGL 7.11 + TFT_eSPI on S3.** Both are fine on S3 but the TFT_eSPI build must use the
    project‑flag config (above), not a `User_Setup.h` in the library, to stay reproducible.
11. **`board_build` / custom board JSON.** `esp32-s3-devkitc-1` may not declare 16 MB flash / 8 MB OPI
    correctly; a small custom board JSON may be needed for clean flash/PSRAM sizing.
12. **GPS/LoRa power draw.** MIA‑M10Q and SX1262 are significant current draws — they must be on
    switchable AXP2101 rails and powered down in standby, or battery life craters.

---

## 9. Suggested milestones & rough effort

| Phase | Milestone | Rough effort* |
|---|---|---|
| 0 | Builds, boots, splash on screen, USB serial, SPIFFS | 1–2 days |
| 1a | Display + touch + backlight (interactive UI) | 1–2 days |
| 1b | AXP2101 PMU + button + battery/charge UI | 2–4 days (hardest) |
| 1c | BMA423 + PCF8563 + DRV2605 (sensors/time/haptics) | 1–2 days |
| 1d | Audio (MAX98357A) + BLE/WiFi/web verify | 1–2 days |
| 1e | GUI/app TTGO guards + polish + power tuning | 2–4 days |
| 2 | GPS, then LoRa, then IR, then mic | 1–3 days each, independent |

\* Engineering days for someone with the board in hand; the gating factor is **hardware access** for
on‑device debugging (you cannot validate PMU rails, I²C addresses, or audio in the SDL emulator).

---

## 10. Open questions, ideas & suggestions

**Resolved for this unit (locked in):**
* ✅ **Radio = SX1262** → Phase 2 LoRa uses RadioLib's `SX1262` class on the pins in §2.
* ✅ **No SD card** → `sdcard.cpp` stubbed, SD settings tile hidden, storage falls back to flash.
* ✅ **Battery = 940 mAh** → set `pmu_config.designed_battery_cap = 940` and seed the
  voltage→percent curve / AXP2101 gauge accordingly.

**Still to confirm (will resolve from LilyGoLib's pin header — non‑blocking):**
1. **Display RST and backlight polarity** — the public pin map omits RST (often `-1`/shared on these
   modules). Confirm before finalizing `display.cpp`.
2. **AXP2101 rail map** — exactly which LDO/DCDC feeds display, touch, audio amp, GPS, and LoRa.
   This is the gating detail for the PMU rewrite; take it from LilyGoLib's board‑init source.

**Ideas / suggestions:**
* **Treat LilyGoLib as the reference, not the dependency.** Since we chose granular libs, I suggest
  mining LilyGoLib's board‑init source for the *exact* AXP2101 rail setup and pin definitions (it is
  the authoritative source for this board), but keeping our drivers on XPowersLib/SensorLib to stay
  consistent with the existing 2021 code. This avoids dragging a large monolithic dependency into the
  per‑module `#ifdef` architecture.
* **Land Phase 0 + a “hello display/touch” build first** and push it early — it de‑risks the toolchain
  (the scariest part) before investing in PMU work.
* **Add a CI build env** for `t-watch-s3-plus` so the new board can't silently break (and keep the
  SDL emulator build green for UI work without hardware).
* **Reuse the 2021 branches aggressively** — for BMA423, PCF8563, and TFT_eSPI display, the 2021 code
  is closer to the S3 Plus than the 2020 code is. In several modules the cleanest move is
  `#elif defined( LILYGO_WATCH_2021 ) || defined( LILYGO_WATCH_S3_PLUS )` sharing one branch with
  pin differences pushed into the config header.
* **Power tuning is a project of its own** — set expectations that first‑boot battery life will be
  rough until AXP2101 standby rails + light sleep are tuned to replace the lost hedge‑fork DFS.
* **The PDM microphone unlocks new features** (voice memo, simple wake‑word, audio level) — worth a
  small proof‑of‑concept once core is stable, since it's hardware the 2020 never had.

---

## 11. TL;DR

Add a `LILYGO_WATCH_S3_PLUS` board: new PlatformIO env on stock S3 Arduino framework, new pin header,
and one new `#elif` branch per hardware module. **~70 % of peripherals reuse existing chip support**
(display, touch, IMU, RTC, haptics — lean on the 2021 branches). The real work is **toolchain/PSRAM
setup, the AXP2101 PMU rewrite, and MAX98357A audio**, with GPS/LoRa/IR/mic as independent Phase‑2
add‑ons. Biggest risk is **power management**, because the S3 can't use the 2020's custom low‑power
Arduino fork.
