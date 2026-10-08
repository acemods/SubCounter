# Twitch channels

SubCounter can track Twitch channels alongside YouTube ones: follower count, live status, viewers and stream title. Twitch follower counts are **exact** (no rounding, so no "est.").

## 1. Create a free Twitch app (about 5 minutes, once)

**Before you start:** your Twitch account needs a **verified email** and **two-factor authentication** (Twitch → Settings → Security and Privacy → *Set Up Two-Factor Authentication*). Twitch won't let you create apps without it.

1. Go to <https://dev.twitch.tv/console> and **Log in with Twitch** (refresh the page if you've just turned on 2FA).
2. Open the **Applications** tab → **Register Your Application**.
3. Fill in:
   - **Name:** must be unique across all of Twitch, e.g. `SubCounter-yourname`
   - **OAuth Redirect URLs:** `https://localhost` → click **Add**. Twitch now insists on **https**. SubCounter never uses this address, so any https address is fine; if `https://localhost` is refused, use your GitHub Pages address (e.g. `https://yourname.github.io/`).
   - **Category:** *Other*
   - **Client Type** (if shown): *Confidential*
   - Tick **I'm not a robot**
4. Click **Create**.
5. Back on **Applications**, click **Manage** next to your app.
6. Copy the **Client ID**.
7. Click **New Secret**, confirm, and copy the **Client Secret** straight away (it's only shown once; making another one cancels the old one).

## 2. Add the app details and channels to SubCounter

1. Open `http://subcounter.local/settings` (or the board's IP + `/settings`) and tap **Twitch** at the top.
2. In **Twitch channels**, type each channel's name, one per line, exactly as in their address (twitch.tv/**name**):

   ```
   shroud
   pokimane
   ```

   Pasting the full `https://www.twitch.tv/name` link works too. YouTube and Twitch together: up to 10 channels.
3. Open **Twitch app details** and paste the **Client ID** and **Client Secret**.
4. Press **Save & restart**.

If your list is Twitch-only, you don't need a YouTube API key.

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
