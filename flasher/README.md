# T-Watch S3 Plus Web Flasher

A single-file, browser-based flasher for the T-Watch S3 Plus — like the
[Meshtastic](https://flasher.meshtastic.org) or [Bruce](https://bruce.computer/flasher)
flashers. It uses Espressif's [esptool-js](https://github.com/espressif/esptool-js)
and the **Web Serial API**, so it runs entirely in your browser and talks to the
watch directly over USB. Nothing is uploaded anywhere.

## Requirements

- A **Chromium browser** (Chrome, Edge, Brave, Opera) on desktop. Web Serial is
  **not** available in Firefox or Safari.
- An internet connection the first time you open it (it loads esptool-js from a CDN).
- A USB-C cable and the watch.

## Usage

1. Get a firmware binary:
   - Download the **`t-watch-s3-plus-firmware`** artifact from the
     [GitHub Actions build](../../actions) and unzip it. Use **`firmware-merged.bin`**.
2. Open **`index.html`** in Chrome/Edge (double-click the downloaded file, or host it).
3. Select the `.bin` file. For `firmware-merged.bin` leave the offset at `0x0`.
   (To flash just the app `firmware.bin`, set the offset to `0x10000`.)
4. Click **Connect**, pick the watch's serial port, then click **Flash**.
5. If the device isn't detected, hold **BOOT**, tap **RST**, release BOOT to force
   download mode, then Connect.

## Flash layout (for reference)

| Offset    | File             |
|-----------|------------------|
| `0x0`     | bootloader.bin   |
| `0x8000`  | partitions.bin   |
| `0xe000`  | boot_app0.bin    |
| `0x10000` | firmware.bin     |

`firmware-merged.bin` contains all four combined, so flashing it at `0x0` is the
simplest path.

## Notes

- This is an early hardware bring-up. Flashing is reversible — you can always
  re-flash the stock LilyGo firmware.
- `file://` works in Chromium (it's treated as a secure context). If your browser
  blocks it, serve the folder locally, e.g. `python3 -m http.server` and open
  `http://localhost:8000/flasher/`.
