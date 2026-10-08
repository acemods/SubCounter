# Spotify setup

Spotify needs a one-time setup. Since February 2026, apps in Spotify's free *Development mode* require the **app owner to have Spotify Premium**, and playback control (play, pause, skip, volume) also needs Premium.

## Why the relay page?

Spotify only sends you back after login to an **https://** address (its dashboard now rejects `http://127.0.0.1` too). The board only speaks http on your local network, so a tiny static page on GitHub Pages receives Spotify's reply and passes the code to the board.

The relay page:

- contains **no secrets** (it's just `spotify-callback/index.html`)
- only forwards to **local network addresses** (10.x, 192.168.x, 172.16–31.x)
- stores nothing; the forwarding happens in your own browser

The sign-in code is useless without your **Client secret**, which only lives on the board.

## 1. Host the relay page (GitHub Pages)

1. Create a **public** repo called `spotify-callback` and upload `spotify-callback/index.html` from this repo.
2. **Settings → Pages** → Branch **main**, folder **/ (root)** → Save.
3. After a minute it's live at `https://<your-github-name>.github.io/spotify-callback/`.

## 2. Create the Spotify app

1. Go to <https://developer.spotify.com/dashboard> → **Create app**.
2. **Redirect URI:** `https://<your-github-name>.github.io/spotify-callback/` (exactly that, including the trailing `/`). Click **Add**.
3. Tick **Web API**, then click **Save** at the **bottom of the page**. Clicking *Add* alone doesn't save it.
4. Open the app's **Settings** and copy the **Client ID** and **Client secret**.

## 3. Connect the board

1. Board settings → **Spotify** → open **Spotify app details**: paste the **same** Redirect URI, the Client ID and the Client secret → **Save & restart**.
2. Open settings again → **Spotify** → press the green **Connect Spotify** button (it appears once the app details are saved) → **Agree**.
3. You're sent back to the board: **"Spotify connected ✓"**.
4. Long-press the board's screen → **Spotify**.

If the relay page shows a code instead (for example, your phone was on mobile data), open *The relay page showed a code instead?* under Connect Spotify, paste the code (or the page's full address) and press **Use this code**.

## Troubleshooting

| Problem | Fix |
|---|---|
| `redirect_uri: Not matching configuration` | The address must match exactly (trailing `/`, capitals, https). Make sure you clicked **Save** at the bottom of the Spotify page, and that the Client ID is from the same app. |
| "Needs Spotify Premium" | Playback control requires Premium |
| "No active device" | Start playing on a phone or computer first |
| Red error on the Spotify screen | Sign-in failed: check the Client secret in Settings → Spotify → Spotify app details, save, and **Connect Spotify** again. The board retries less often after errors, so the screen stays responsive. |
| Stuck on the Spotify screen | Long-press, or hold **BOOT** for about 1 second, for the home menu |
| Want to disconnect | Board settings → Spotify → **Disconnect Spotify**, and/or spotify.com → Account → Apps → Remove access |

The board asks only for these permissions: `user-read-playback-state`, `user-modify-playback-state` and `user-read-currently-playing`.
