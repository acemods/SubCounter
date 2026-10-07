# Builds

**Current version: v9.9** in [`latest/`](latest)

| File | Use for | How |
|---|---|---|
| `SubCounter-v9.9-APP-ONLY.bin` | Updating a running board (v9 or later) | Upload at `http://<board-ip>/update` |
| `SubCounter-v9.9-USB-UPDATE-flash-at-0xE000.bin` | Updating over USB, keeping settings | esptool-js at **0xE000** |
| `SubCounter-v9.9-FULL-new-board-flash-at-0x0.bin` | New or wiped board | esptool-js at **0x0** (erases settings) |

Older builds are in [`archive/`](archive), one folder per version. The address to use is in each file name.
Before v9 there was no wireless update: `APP-ONLY-flash-at-0x10000` files went at 0x10000, and v1–v4 files were full images at 0x0.

All builds target the **ESP32-C6-Touch-LCD-1.47** with 8 MB flash. See [../docs/UPDATING.md](../docs/UPDATING.md).
