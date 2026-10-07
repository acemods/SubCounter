# Changelog

All builds are in [`builds/`](../builds). Source snapshots for most versions are in the git history.

## v9.7
- **Went-live alert:** red flash with the channel's picture, stream title and viewer count, then the board switches to that channel. Can be turned off in settings.
- **Milestone countdown:** in the last 10% before a milestone, the main screen shows a gold progress bar and "32 to go to 5K!"; the dashboard card highlights it too
- **Settings PIN:** optional PIN for settings, Wi-Fi changes, Spotify and firmware updates (browser asks: user name `admin`, password = PIN). The dashboard stays viewable. Setup mode (hold BOOT 3 s) doesn't need it, so a forgotten PIN can be cleared.
- **Clearer labels:** dashboard leaderboard has *Subs* / *Today* headings, "Last 24 hours" explains it's rolling, the chip reads "Last 24 h", and the tall leaderboard marks the green numbers as *today*

## v9.6
- **LIVE badges:** when a channel is streaming, its profile picture gets YouTube's red ring and LIVE pill, on the board (main screen, overview, alerts) and on the dashboard (cards, race, leaderboard). The leaderboard on the board shows a LIVE tag next to the name.
- Live streams are spotted even when a Short or scheduled stream was posted after them (checks the newest 3 uploads)
- Live viewer counts refresh every 2 minutes, and the badge goes as soon as the stream ends
- Latest videos checked every 10 minutes (was 15)
- Dashboard: clicking a live channel's picture opens the stream; "1 days ago" now reads "1 day ago"

## v9.5
- **`http://subcounter.local`**: open the dashboard without knowing the IP address (iPhone, Mac, Windows 10/11; Android support varies)
- The board shows its IP address and `subcounter.local` for 5 seconds after joining Wi-Fi
- The home menu (long-press) shows the IP address at the bottom
- The board appears as **subcounter** in your router's device list

## v9.4
- **Dashboard layout:** on wide screens, Last 24 hours, Weather, Race and Leaderboard sit in a left column with the channel cards in a grid beside them (no more empty space next to the leaderboard). Tablets show the side cards in two columns above the channels; phones stack everything.
- Tighter leaderboard rows, bigger 7 / 30 day buttons, tidier phone header

## v9.3
- **Idle clock:** after 1–10 minutes with no touch, button press or movement (default 3), shows a large clock with date, weather, rain warning and your subscriber count. Picking the board up, touching it or pressing BOOT goes back to where you were. Skipped while Spotify is playing; the dim night clock still takes over overnight.
- Movement (picking up or nudging the board) now counts as using it

## v9.2
- Spotify: long titles and artist names scroll smoothly (flicker-free off-screen drawing)

## v9.1
- Spotify sign-in via an **https** redirect (Spotify's dashboard now rejects `http://127.0.0.1`)
- `spotify-callback/` relay page for GitHub Pages: one-click connect, forwards only to local-network addresses
- Redirect URI is now a setting

## v9
- **Wireless firmware updates** (`/update`), with app-image check and safe fallback
- **Home menu** (long-press) with apps:
  - **Weather** (Open-Meteo): clock, now, next hours, tomorrow, rain warning; town setting
  - **Spotify**: now playing with album art, progress, play/pause, skip, volume
- **New video tracker** for your own channel: views/hour, vs your usual 1st hour / 1st day, view milestone alerts
- Weather and tracker on the web dashboard
- New USB update file at 0xE000 (works after wireless updates)

## v8.5
- "Connect now" for any saved network; **Preferred** network

## v8.4
- **Scan for networks** button with signal bars; copies names exactly (curly apostrophes)

## v8.3
- **Multiple saved Wi-Fi networks** (up to 5), each with its own fixed IP/DNS/compat settings
- Automatic roaming after losing Wi-Fi for a minute; setup mode retries every 3 minutes

## v8.2
- Portrait detection fixed (uses the sensor's real long axis)

## v8.1
- Tall leaderboard orientation swapped

## v8
- Motion: shake to refresh, face-down sleep, portrait tall leaderboard, desk double-tap
- Milestone confetti (tiers), subscriber race with overtake alerts
- 9 am daily summary, night clock with dimming
- **Web dashboard** (`/`), JSON API; settings moved to `/settings`; motion calibration

## v7
- Readable U8g2 fonts, fewer items per card, 5-per-page leaderboard, accented characters
- First FULL image (0x0) alongside the APP-ONLY (0x10000) build

## v6
- Detail cards: overview with profile pictures, latest video (LIVE badge), growth, milestone
- Leaderboard; subscriber history on LittleFS; estimated live counts

## v5
- Up to 10 channels, swipe left/right, new-subscriber alerts, auto-switch
- Friendly "can't reach YouTube" error; 8.8.8.8 backup DNS

## v4
- Fixed MAC address showing as zeros on the failure screen

## v3
- Longer DHCP wait, automatic Wi-Fi 4 fallback, fixed IP option
- Moved to the 8 MB flash layout

## v2
- Real Wi-Fi failure reasons, cleaned-up pasted passwords, enterprise (username) Wi-Fi

## v1
- YouTube subscriber counter with Wi-Fi setup portal (`SubCounter-Setup`)

## FirstSketch
- Hello screen + demo counter to test the display, pins and backlight
