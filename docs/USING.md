# Using SubCounter

## Touch gestures

| Gesture | YouTube | Weather | Spotify |
|---|---|---|---|
| Swipe left / right | Next / previous channel | – | Next / previous track |
| Swipe up | Next detail card | Next card | Volume +10% |
| Swipe down | Previous card → Leaderboard → Race | Previous card | Volume −10% |
| Tap | – | – | Play / pause |
| **Long-press (≈1 s)** | **Home menu** | **Home menu** | **Home menu** |

**BOOT button:** short press = next channel (YouTube) / next card (Weather) / play-pause (Spotify). Hold for 3 s = setup mode.

After 2 minutes untouched, each app returns to its main screen.

## YouTube cards

Swipe up from the main count:

1. **Overview:** profile picture, total views, video count, average views per video, year joined, country
2. **Latest video:** title, age, length, views, likes and comments, plus a red **LIVE** badge with viewer count while a stream is on. On *your* channel, for 48 hours after an upload, this becomes the **New video tracker** (views, views per hour, a graph, and a comparison with your usual uploads).
3. **Growth:** gains today, over 7 days and over 30 days, plus the average per day
4. **7-day graph**
5. **Next milestone:** progress bar, how many to go, predicted date

Swipe down from the main count for the **Leaderboard** (5 per page; swipe left or right for more), and again for the **Race**. Choose the two race channels in settings.

### About the numbers

YouTube rounds public subscriber counts to 3 significant figures (e.g. 3,180 moves in steps of 10; 1.2M in steps of 10,000). With **Estimated live counts** on, the board estimates the count between steps from each channel's growth rate, and labels it **est.** Growth figures need a few hours of history first. The board records every channel hourly and keeps 31 days.

## LIVE badges

When a channel is streaming, its profile picture gets a red ring and a **LIVE** pill, just like on YouTube, on the board and on the dashboard. The leaderboard shows a LIVE tag next to the name. Streams are picked up within about 10 minutes of starting, and the badge goes within 2 minutes of them ending.

## Alerts

- **New subscribers:** the screen flashes with the channel and how many it gained
- **Milestones:** confetti; it rains for small milestones, and from 100K up it explodes from the middle
- **Overtake:** when one race channel passes the other
- **Your video hit 100 / 250 / 500 / 1K … views**

Alerts are skipped at night, when the board is face-down or on its side, and while Spotify is playing.

## Motion

| Movement | What happens |
|---|---|
| Shake | Refresh now |
| Face-down | Screen off (turn it back over to wake it) |
| Stand in portrait | Tall leaderboard (tick *flip* in settings if it's upside down) |
| Double-tap the desk | Next channel (exactly two knocks; typing is ignored) |

Each can be switched off in settings, and desk-tap sensitivity can be set to Low, Medium or High.

## Idle clock

After **3 minutes** with no touch, button press or movement (change it, or turn it off, in settings), the board shows a **large clock** with the date, the weather, any rain warning and your channel's count. It goes back to what you were looking at as soon as you touch it, press BOOT or pick it up. It doesn't appear while Spotify is playing, and overnight the dim night clock is used instead. Alerts still pop up over it.

## Daily summary and night clock

- **9 am:** "Good morning!" with the top 3 growers since yesterday and any new videos. Touch it to dismiss.
- **Night** (default 23:00–07:00, adjustable): after a minute untouched, the screen dims to a large clock with your channel's count. Touch it to wake.

## Weather app

- **Now:** clock, temperature, conditions, high and low, and a blue rain warning if rain is likely within 3 hours
- **Next hours:** every 2 hours for the next 12, with rain chance
- **Tomorrow:** forecast, sunrise and sunset

## Web dashboard

Open **`http://subcounter.local`** (or `http://<board-ip>/`) on any device on the same network. The IP address is shown for a few seconds after the board joins Wi-Fi, and at the bottom of the home menu (long-press):

- a side column with top growers in the last 24 hours, weather, race and leaderboard (above the channels on tablets and phones)
- a card for every channel: live (estimated) count, growth chips, milestone and ETA, a 7 or 30-day graph, and the latest video with thumbnail
- the new video tracker on your channel's card

Settings are at `/settings`, and wireless updates at `/update`.

## API quota

| What | How often | Cost |
|---|---|---|
| All channel stats | every 2 min | 1 unit |
| Latest videos | every 10 min | channels + 1 |
| Live streams (only while someone is live) | every 2 min | 1 |
| Your new video | every 5 min for 48 h | 1 |
| Your typical views | once a day while tracking | 2 |

With 10 channels this is about 2,500–3,000 units a day, well inside the free 10,000.
