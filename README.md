# SubCounter

A desk gadget built on the **Waveshare ESP32-C6-Touch-LCD-1.47** that shows live YouTube subscriber and Twitch follower counts, and has grown into a small desk hub: channel stats, weather, Spotify, a web dashboard, phone notifications, motion gestures and more. It updates itself from this repository.

![Web dashboard](docs/images/dashboard.png)

## What's new

- **v12.4:** "Check for update" on the Device screen no longer gets stuck on "Checking..."
- **v12.3:** settings page no longer gets cut off part-way down
- **v12.2:** more reliable YouTube fetching when memory is tight; more detail on `/debug`
- **v12.1:** diagnostics page at `/debug`
- **v12.0:** setting to turn the start-up update check off
- **v11.9:** start-up screen checks for updates straight away and lets you install from there
- **v11.8:** dashboard uses the new Creators logo; browser tab and phone home-screen icon
- **v11.7:** the main home-menu tile is now **Creators** (half YouTube, half Twitch)
- **v11.6:** home menu tidy-up; all documentation pictures refreshed
- **v11.5:** YouTube / Twitch logos in the race and mixed leaderboard
- **v11.4:** dashboard filter: All / YouTube / Twitch
- **v11.3:** home menu with three big tiles; swipe for Device
- **v11.2:** Device screen with QR code and on-screen updates, phone notifications, top videos, colour themes, settings backup
- **v11.1:** updates itself from GitHub (one tap, or automatically), automatic builds
- **v11.0:** background networking (no more freezes), faster refreshes, code split into files
- **v10.2:** Twitch gets its own settings section – just type the channel name
- **v10.1:** settings PIN fixed, settings page reorganised
- **v10.0:** the firmware version is shown on the board, settings, update page and dashboard

Full history: [docs/CHANGELOG.md](docs/CHANGELOG.md)

## Features

**YouTube**
- Track up to 10 channels: swipe left and right between them on the touch screen
- Swipe up for detail cards: Overview (profile picture, views, videos), Latest video (with a LIVE badge), Comments, Growth (today / 7 / 30 days), Records (best day / week / video), Top videos (last 10 uploads by views per day), a 7-day graph and Next milestone (with a predicted date)
- Monthly recap on the 1st of each month
- Swipe down for the Leaderboard, then the Subscriber race (two channels head to head, with an "OVERTAKE!" alert)
- Alerts for new subscribers, with confetti for milestones (bigger milestone, bigger party), plus a countdown when you're close
- **Went-live alerts:** when a channel starts streaming, the board flashes and switches to it
- **LIVE badges:** a red ring and LIVE pill on a channel's picture while they're streaming, like on YouTube
- Estimated live counts between YouTube's rounded steps (marked "est.")
- **New video tracker** for your own channel: views per hour for 48 hours, compared with your usual uploads, plus view milestone alerts
- Subscriber history saved on the board, so graphs survive restarts

**Twitch**
- Add Twitch channels by name in their own settings section: exact follower counts, live status, viewers and went-live alerts ([setup](docs/TWITCH.md))

**Apps** (long-press the screen, or hold BOOT for 1 s, for the home menu; swipe left for Device)
- **Creators:** your YouTube and Twitch channels (the subscriber counter itself)
- **Weather:** clock, current conditions, next 12 hours, tomorrow, and a rain warning (Open-Meteo, no key needed)
- **Spotify:** album art, scrolling title and artist, progress bar; tap to play or pause, swipe to skip, swipe up or down for volume. One-click sign-in from the settings page (via a small relay page you host on GitHub Pages)

**Motion sensor**
- Shake to refresh · face-down turns the screen off · stand it in portrait for a tall leaderboard · double-tap the desk for the next channel

**Web dashboard** (`http://subcounter.local` or `http://<board-ip>/` from any phone or computer on your network)
- **All / YouTube / Twitch** filter at the top when you follow both
- Side column with the last 24 hours' top growers, weather, the race and the leaderboard
- A card per channel: live (estimated) count, growth chips, milestone progress and predicted date, 7 or 30-day graph, latest video with thumbnail, and the new video tracker on your own channel
- Monthly table, a compare chart (up to 4 channels) and CSV download of the history
- Recent uploads per channel, sortable by views, views per day or likes %
- An **Update** button in the header when a new version is out
- Adapts to the screen: side column on desktops, two columns on tablets, one on phones
- Settings at `/settings` and wireless updates at `/update`

**Everyday**
- **Idle clock:** after 3 minutes without use (adjustable), a large clock with date, weather and your count; pick the board up to go back
- 9 am daily summary, plus a dim night clock overnight
- **Device screen** (home menu, swipe left): QR code for the dashboard, and check for / install updates right on the touch screen
- **Phone notifications** via the free ntfy app: milestones, going live, records, overtakes
- **Colour themes** ([see them all](docs/images/themes.png)) and brightness settings
- **Settings backup & restore** to a file
- Several saved Wi-Fi networks (e.g. work and home) with automatic roaming, a network scanner, "Connect now" and a preferred network
- Works on fussy networks: fixed IP, custom DNS, Wi-Fi 4 compatibility mode, and plain-English connection errors
- **Updates from GitHub:** the board tells you when a new version is out and installs it with one tap (or automatically at 3 am); manual upload still works
- Optional **settings PIN**, so others on your Wi-Fi can view the dashboard but not change anything

## Quick start

1. **Flash the board.** Open <https://espressif.github.io/esptool-js/> in Chrome, connect the board, and flash `builds/latest/SubCounter-v12.4-FULL-new-board-flash-at-0x0.bin` at address **0x0**.
2. **Set it up.** The board shows *SETUP MODE*. Join the Wi-Fi network **SubCounter-Setup** from your phone, and the setup page opens by itself (or go to <http://192.168.4.1>). Pick your Wi-Fi, add your YouTube channels and a YouTube Data API key, then save.
3. **Use it.** The board shows a QR code and its address for a few seconds, then your subscriber count. Scan the code, or open **<http://subcounter.local>**, for the dashboard.

Full instructions: **[docs/GETTING-STARTED.md](docs/GETTING-STARTED.md)**

## Documentation

| Guide | What's in it |
|---|---|
| [Getting started](docs/GETTING-STARTED.md) | Flashing, first setup, getting a YouTube API key |
| [Using SubCounter](docs/USING.md) | Gestures, cards, apps, dashboard, alerts |
| [Wi-Fi & networks](docs/WIFI.md) | Multiple networks, fixed IP, iPhone hotspot, error messages |
| [Twitch setup](docs/TWITCH.md) | Free Twitch app, adding Twitch channels |
| [Spotify setup](docs/SPOTIFY.md) | Developer app, GitHub Pages relay, connecting |
| [Updating & flash layout](docs/UPDATING.md) | Updates from GitHub, uploading a file, which file goes at which address |
| [Building from source](docs/BUILDING.md) | Arduino IDE settings, libraries, code map, GitHub Actions |
| [Changelog](docs/CHANGELOG.md) | What changed in each version |

## Repository layout

```
firmware/SubCounter/     main firmware source (Arduino sketch)
firmware/FirstSketch/    first hello-world / test sketch
builds/latest/           ready-to-flash files for the current version
builds/archive/          every earlier build
spotify-callback/        relay page for Spotify sign-in (host on GitHub Pages)
docs/                    guides and screenshots
```

## Hardware

- Waveshare **ESP32-C6-Touch-LCD-1.47**: ESP32-C6, 8 MB flash, 1.47" 172×320 touch LCD (JD9853 + AXS5106L), QMI8658 motion sensor
- USB-C cable (a data cable, not a charge-only one)

## Credits

Pin mapping and the panel start-up sequence for this board come from
[andreimagic/ESP32_C6_Touch_LCD_1_47_LVGL_Animated_Clock](https://github.com/andreimagic/ESP32_C6_Touch_LCD_1_47_LVGL_Animated_Clock) (MIT). See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
