# Updating & flash layout

## From GitHub (v11.1 and later), easiest

The board checks GitHub **every time it starts up** (on the QR-code screen you see after it joins Wi-Fi) and then once a day. When a new version is out you'll see:

- **at start-up:** the QR screen says "v… available!" and stays up for 20 seconds. Tap **Install v…** to update there and then, or swipe / press BOOT to skip it. Don't want this? Settings → **Updates** → untick *Check for updates at start-up*: the QR screen then shows for its usual 6 seconds without checking (the daily check still runs in the background).
- **on the board:** "v… available" at the top of the home menu and a red dot on the Device page. Long-press → swipe left → **Device** → **Install v…**. Or tap **Check for update** to look straight away.
- **on the dashboard:** a green **Update** button in the header
- **on the Update page** (`http://subcounter.local/update`): an **Install** button, and **Check now**

Press Install, watch the screen (a percentage counts up), and the board restarts on the new version in about a minute. Settings and history are kept. The file's size and MD5 checksum are checked before anything is installed.

To have it install by itself: Settings → **Updates** → *Install new versions from GitHub automatically (at 3 am)*.

## Wireless update (recommended, v9 and later)

1. Open `http://<board-ip>/update` (or Settings → *Firmware* → *Update*).
2. Choose the **APP-ONLY** file, e.g. `SubCounter-v12.6-APP-ONLY.bin`.
3. Wait about 30 seconds; the board installs and restarts.

Settings, Wi-Fi networks, Spotify login and subscriber history are all kept. The page refuses FULL images. If an update fails, the board keeps running the old version.

**Which version am I on?** It's shown on the update page ("Currently running …"), at the bottom of the settings page, in the dashboard header, at the top right of the home menu, on the Device screen and for a few seconds at start-up.

## USB update (any time)

Flash `SubCounter-vX-USB-UPDATE-flash-at-0xE000.bin` at **0xE000** with <https://espressif.github.io/esptool-js/>. This keeps your settings and history.

> ⚠️ After a wireless update, **don't** use the old `APP-ONLY … 0x10000` method over USB: the board would keep starting the wireless copy. The 0xE000 file clears that choice, so it always works.

## New or wiped board

Flash `SubCounter-vX-FULL-new-board-flash-at-0x0.bin` at **0x0**. This **erases settings**; history in the storage area is usually kept.

## Flash layout (8 MB)

| Address | Size | Contents |
|---|---|---|
| 0x0 | | Bootloader |
| 0x8000 | | Partition table |
| 0x9000 | 20 KB | Settings (NVS): Wi-Fi, channels, keys, Spotify login |
| 0xE000 | 8 KB | OTA data (which app copy to start) |
| 0x10000 | 3.2 MB | App, copy 0 |
| 0x340000 | 3.2 MB | App, copy 1 (used by wireless updates) |
| 0x670000 | 1.5 MB | Storage (LittleFS): subscriber history |
| 0x7F0000 | 64 KB | Core dump |

This is Arduino-ESP32's **"8M with spiffs (3MB APP/1.5MB SPIFFS)"** scheme.

## Which file is which

| File | Use for | Address |
|---|---|---|
| `…-APP-ONLY.bin` | Wireless update page | (upload) |
| `…-USB-UPDATE-flash-at-0xE000.bin` | USB update, keeps settings | 0xE000 |
| `…-FULL-new-board-flash-at-0x0.bin` | New or wiped board | 0x0 |
| `…-APP-ONLY-flash-at-0x10000.bin` (v5–v8.5) | Old USB updates, before wireless existed | 0x10000 |
