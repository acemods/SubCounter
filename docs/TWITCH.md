# Twitch channels

SubCounter can track Twitch channels alongside YouTube ones: follower count, live status, viewers and stream title. Twitch follower counts are **exact** (no rounding, so no "est.").

## 1. Create a free Twitch app (5 minutes, once)

1. Sign in at <https://dev.twitch.tv/console/apps>. Twitch requires **two-factor authentication** on your account to create apps.
2. Click **Register Your Application**:
   - **Name:** anything unique, e.g. `SubCounter-yourname`
   - **OAuth Redirect URLs:** `http://localhost` (not used, but the form needs one)
   - **Category:** Other
   - **Client type:** **Confidential**
3. Click **Create**, then **Manage** on the new app.
4. Copy the **Client ID**, click **New Secret** and copy the **Client Secret**.

## 2. Add them to SubCounter

Settings → **Twitch (optional)** → paste the Client ID and Client Secret → **Save & restart**.

## 3. Add Twitch channels

In the channel list, one per line, use the Twitch address:

```
@youryoutubechannel
twitch.tv/somestreamer
twitch.tv/anotherone
```

`twitch:somestreamer` also works. YouTube and Twitch channels can be mixed, up to 10 in total. If your list is Twitch-only, you don't need a YouTube API key.

## What you get

- Follower count on the main screen (labelled *followers*, in Twitch purple), history, growth, records, milestones and the monthly recap, just like YouTube channels
- **LIVE ring and badge** on the picture while they're streaming, plus the **went-live alert**
- The *Stream* card (swipe up): viewers, title, game and how long they've been live
- On the dashboard: a purple **Twitch** tag, the stream thumbnail while live, and links to the channel

Not available for Twitch: views/video counts, the latest video and comments cards.

## Troubleshooting

| Message | Fix |
|---|---|
| *Twitch: add your Client ID and Secret in settings* | Fill in both fields in settings |
| *Twitch: Client ID or Secret is wrong* | Copy them again; make a **New Secret** if unsure (the old one stops working) |
| *Twitch channel not found* | Check the name in `twitch.tv/name` |

The app token is renewed automatically; nothing expires on your side. Checks happen every 2 minutes, well within Twitch's limits.
