# Building from source

The firmware is an Arduino sketch in `firmware/SubCounter/`: open `SubCounter.ino` and the IDE shows the other files as tabs.

## Arduino IDE

**Board package:** *esp32 by Espressif Systems* **3.x** (built and tested with 3.3.12)

**Tools menu:**

| Setting | Value |
|---|---|
| Board | ESP32C6 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 8MB (64Mb) |
| Partition Scheme | 8M with spiffs (3MB APP/1.5MB SPIFFS) |

**Libraries** (Library Manager):

| Library | Version used |
|---|---|
| GFX Library for Arduino (moononournation) | 1.6.8 |
| ArduinoJson (Benoit Blanchon) | 7.4.2 |
| JPEGDEC (bitbank2) | 1.8.4 |
| PNGdec (bitbank2) | 1.1.7 |
| U8g2 (olikraus) | 2.37.1 (used for its fonts) |

## arduino-cli

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=8M,PartitionScheme=default_8MB \
  --output-dir build firmware/SubCounter
```

This produces `SubCounter.ino.bin` (the APP-ONLY file for wireless updates) and `SubCounter.ino.merged.bin` (the full image, to flash at 0x0).

To make the USB update file (0xE000): 8 KB of `0xFF` bytes followed by the app `.bin`:

```bash
python3 -c "import sys;open('usb.bin','wb').write(b'\xff'*0x2000+open('build/SubCounter.ino.bin','rb').read())"
```

## GitHub Actions

`.github/workflows/build.yml` compiles the firmware on every push that changes `firmware/`, and keeps the three .bin files as a downloadable artifact of the run. To publish a **Release**: *Actions → Build firmware → Run workflow*, tick **Make a release**. It uses the version in `FW_VERSION`.

`builds/latest/version.json` is what boards read to see whether there's an update: version, APP-ONLY file name, size and MD5.

## Board pins (ESP32-C6 version)

| Function | GPIO |
|---|---|
| LCD SCK / MOSI / CS / DC / RST / backlight | 1 / 2 / 14 / 15 / 22 / 23 |
| TF card CS (shares the LCD bus) | 4 |
| Touch (AXS5106L) SDA / SCL / RST / INT | 18 / 19 / 20 / 21 |
| Motion sensor QMI8658 | I²C 0x6B (same bus as touch) |
| BOOT button | 9 |
| Battery ADC | 0 (×3 divider) |

> Waveshare's wiki example shows **ESP32-S3** pin numbers; the ESP32-C6 board uses the ones above.

## Code map

Arduino joins the files into one program: `SubCounter.ino` first, then the others in name order (that's why they're numbered).

| File | What it does |
|---|---|
| `SubCounter.ino` | Settings constants, shared data, **background networking** (lock, re-usable HTTPS connections) |
| `a01_panel` | Display start-up sequence (JD9853) |
| `a02_helpers` | Text, numbers, time, URL helpers |
| `a03_settings` | Load/save settings (NVS), channel lists |
| `a04_history` | Subscriber history (LittleFS), growth stats, records, estimates |
| `a05_avatars` | Round profile pictures |
| `a06_screens` | Main count and detail cards, leaderboard (U8g2 fonts) |
| `a07_touch` / `a08_motion` | AXS5106L swipes, taps, long-press; QMI8658 shake, tap, orientation |
| `a09_alerts` | Alerts, confetti, race, daily summary, recap, clocks, tall leaderboard |
| `a10_ota` | Wireless updates |
| `a11_tracker` | New video tracker |
| `a12_apps` | Home menu, Weather (Open-Meteo), Spotify (Web API) |
| `a13_web` | Dashboard, JSON/CSV API, settings page, PIN |
| `a14_portal` / `a15_wifi` | Setup mode; multi-network Wi-Fi, roaming |
| `a16_youtube` / `a17_twitch` | YouTube Data API; Twitch Helix API |
| `a18_main` | `setup()`, `loop()` and the **network task** |

### How the two tasks work

- **`loop()`** (main task) handles the screen, touch, buttons, motion and web pages. It never waits on the internet.
- **The network task** (`netCycle()` in `a18_main`) does every timed request: YouTube, Twitch, Spotify, weather, pictures. It sets flags (`netRefreshed`, `netSpRedraw` …) that tell `loop()` to redraw.
- Shared data is protected by one lock. Each task holds it while it reads or changes data; network calls release it while they wait (`NetIO`). So data is never changed halfway through a redraw, and slow servers never freeze the screen.
- YouTube, Twitch and Spotify requests re-use their secure connection for 20–30 s (`httpsCall`), which cuts each request from 1–2 s to a fraction of that.
