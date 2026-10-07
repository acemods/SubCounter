# Building from source

The firmware is a single Arduino sketch: `firmware/SubCounter/SubCounter.ino`.

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

| Section | What it does |
|---|---|
| Settings | Preferences (NVS) load and save, saved networks |
| Subscriber history | LittleFS hourly samples, growth stats, estimates |
| Screens | Main and detail cards, leaderboard (U8g2 fonts) |
| Touch / Motion | AXS5106L swipes, taps and long-press; QMI8658 shake, tap and orientation |
| Celebrations / race / summary / night | Fun features |
| Wireless updates / New video tracker | OTA via `Update`, own-upload tracking |
| Apps | Home menu, Weather (Open-Meteo), Spotify (Web API) |
| Web | Dashboard, JSON API, settings, setup portal |
| Wi-Fi | Multi-network connect, diagnostics, fixed IP, roaming |
| YouTube API | Channels, latest videos, avatars |
| Setup & loop | Modes (normal, sleep, portrait, summary, night) and app dispatch |
