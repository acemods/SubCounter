# Changelog

All builds are in [`builds/`](../builds). Source snapshots for most versions are in the git history.

## v12.3
- **Settings page cut off part-way down:** fixed. The page (20-30 KB) used to be built in memory in one piece before sending; when memory was tight the end of it was silently dropped. It's now sent in 1 KB pieces as it's built. The dashboard's data and the settings backup are sent the same way.

## v12.2
- **More reliable fetching:** each secure connection needs about 40 KB of memory in one piece. When memory is tight the board now closes its other kept-open connections (Twitch, Spotify, GitHub) before connecting, and if a request can't connect at all it frees memory and tries once more straight away instead of showing an error until the next refresh
- Clearer error messages: "Board low on memory", "YouTube didn't answer in time - weak Wi-Fi?" or "Can't reach YouTube - Wi-Fi or DNS", instead of always blaming DNS/firewall
- `/debug` now names the web page behind a slow moment and lists the last 12 failed internet requests with the error code and free memory at the time

## v12.1
- **Diagnostics page** at `http://subcounter.local/debug`: uptime, free memory, Wi-Fi signal, what the network task is doing, and a log of any moment the screen froze for more than 0.4 s and why. Added to track down a freeze some boards show for about a minute after start-up.

## v12.0
- New setting: **Updates → Check for updates at start-up** (on by default). Untick it and the start-up QR screen shows for 6 seconds without checking GitHub or waiting 20 seconds. The once-a-day background check still runs, so the home menu and dashboard still tell you when a new version is out.

## v11.9
- **Update check at start-up:** the QR-code screen you see after the board joins Wi-Fi now checks GitHub straight away. If a new version is out it says so and stays up for 20 seconds: tap **Install** to update there and then, or swipe / press BOOT to skip. Its button works now (before, it was just a picture until the board finished starting).

## v11.8
- Dashboard header uses the same **Creators** badge as the home-menu tile
- **Browser icon:** the dashboard, settings and update pages now have a tab/bookmark icon (SVG for modern browsers, PNG fallback at `/favicon.ico`), plus an `apple-touch-icon` for Add to Home Screen on phones
- Docs: dashboard screenshots updated

## v11.7
- The main home-menu tile is now **Creators**, with a half YouTube red, half Twitch purple badge (it used to say YouTube, which undersold Twitch)
- Docs: home menu and Device screen pictures updated

## v11.6
- Home menu: the page dots no longer overlap the IP address line at the bottom
- Docs: new screenshots of the dashboard (desktop and phone), compare chart, settings page, Wi-Fi scan, home menu and Device screen

## v11.5
- **YouTube / Twitch logos** next to the names in the subscriber race: on the board's Race screen, on the dashboard, and in the race settings (with a preview under the pickers, which are now grouped YouTube / Twitch)
- The dashboard leaderboard shows the logos too when it mixes YouTube and Twitch channels

## v11.4
- **Dashboard filter:** *All · YouTube · Twitch* buttons at the top (shown when you follow both). Everything follows the filter: last 24 hours, leaderboard, monthly table, channel cards and the compare chart; the race shows when both of its channels are in view. Your choice is remembered in that browser.

## v11.3
- Home menu back to **three big tiles** (YouTube, Weather, Spotify); **swipe left** for a second page with **Device** (room for more later). Page dots show where you are; a red dot means an update is waiting on the Device page.

## v11.2
- **Device screen** (long-press → Device): QR code to open the dashboard on your phone, IP address, version, and a **Check for update / Install** button right on the touch screen. The QR screen also shows for a few seconds at start-up.
- **Phone notifications** with the free ntfy app: milestones, went live, new records, overtakes, your video's view milestones (and optionally every new subscriber). Settings → Phone notifications, with a *Send a test notification* button.
- **Top videos:** new card (swipe up) ranking each channel's last 10 uploads by views per day; the dashboard has a sortable *Recent uploads* table (views, per day, likes %). Uses about 2 quota units per channel every 6 hours.
- **Colour themes** (Classic gold, YouTube red, Twitch purple, Ocean, Mint, High contrast) for the board and dashboard, plus **screen and night-clock brightness** sliders
- **Settings backup & restore:** download all settings as a file (optionally with passwords and keys) and restore them on a re-flashed or second board

## v11.1
- **Updates from GitHub:** the board checks `builds/latest/version.json` once a day. When there's a newer version it shows on the home menu, the dashboard header and the Update page, with a one-tap **Install** (downloaded straight from GitHub, size and MD5 checked). Optional: install automatically at 3 am (Settings → Updates).
- **Automatic builds:** a GitHub Actions workflow compiles the firmware on every change and can publish a Release with the three .bin files (Actions → Build firmware → Run workflow → tick "Make a release")

## v11.0
- **Background networking:** all internet requests now run in their own task, so touch, swipes, long-press, animations and the web pages stay smooth however slow the network is (no more freezes while Spotify, YouTube or Twitch answer)
- **Faster refreshes:** secure connections to YouTube, Twitch and Spotify are kept open and re-used for a short while instead of being set up for every request
- Spotify taps and swipes are sent in the background; Wi-Fi drop-outs no longer freeze the screen while reconnecting
- **Code split into files** (`firmware/SubCounter/a01_… a18_…`), see [BUILDING.md](BUILDING.md)

## v10.4
- **Fixed: stuck in the Spotify app.** When Spotify sign-in failed, the board retried every 3 s and froze briefly each time, so long-presses were missed. It now backs off (15 s, 30 s, 1 min … up to 5 min), polls every 6 s when paused, and uses shorter timeouts.
- **Hold BOOT for about 1 second** to open the home menu from anywhere (3 seconds is still setup mode)
- The Spotify screen shows the error with "Check Spotify in settings / Hold BOOT 1 s for the menu"
- Spotify and Twitch secret boxes no longer get a saved password auto-filled into them by the browser

## v10.3
- Fixed a stray "Â" in the home menu between the IP address and subcounter.local (now a proper · dot)

## v10.2
- **Twitch has its own section in settings** with its own channel box: type just the name (e.g. `shroud`); pasted `twitch.tv/…` links work too
- Twitch lines from the old mixed list move across automatically; a Twitch link pasted in the YouTube box is moved over on save
- Twitch app details fold away once saved; settings hint now says `https://localhost` (Twitch requires https)

## v10.1
- **Fixed: the settings PIN couldn't be set.** The PIN box was below the Save button, outside the form, so it was never sent. Everything is now in one form.
- PIN now has a *Type it again* box (must match, at least 4 characters); nothing is saved if they don't match
- **Settings page reorganised:** jump links at the top, then Channels, Display, Alerts, Weather, Spotify, Motion, Wi-Fi, Security; a Save button that stays at the bottom of the screen; firmware version and update link at the end
- Spotify app details fold away once set up; Connect / Disconnect and motion calibration sit in their own sections
- Saved Wi-Fi network names no longer get cut off

## v10.0
- **Version number shown everywhere:** start-up screen, home menu (top right), bottom of the settings page, update page ("Currently running v10.0") and the dashboard header
- One `FW_VERSION` setting in the code drives all of them

## v9.9
- **Twitch channels:** add `twitch.tv/name` to the channel list. Exact follower counts, live status with viewers, title and game, LIVE ring and went-live alerts, plus history, records, milestones and the monthly recap. Needs a free Twitch app (Client ID + Secret, see [TWITCH.md](TWITCH.md)).
- YouTube API key only required if you track YouTube channels
- New library: **PNGdec** (Twitch profile pictures are PNG)

## v9.8
- **Comments card** (swipe up): the 3 newest comments on each channel's latest video, also on the dashboard cards
- **Records card** (swipe up): best day, best week and (your channel) best video's first-day views, with a **NEW RECORD!** alert when your channel beats one
- **Monthly recap:** on the 1st, the 9 am summary becomes a recap of last month (your growth, your rank, top grower); the dashboard has a *Monthly* card (last month and this month so far)
- **Compare chart** on the dashboard: up to 4 channels on one chart (gained or % growth, 7 or 30 days) with hover details
- **CSV download** of subscriber history, per channel or for everything
- History now kept for 40 days (was 31) and tidied once a day instead of on every save

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
