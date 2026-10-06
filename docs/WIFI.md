# Wi-Fi & networks

## Saved networks

You can save up to **5 networks** (e.g. work, home, phone hotspot). Each has its own password, optional username, fixed IP and compatibility setting.

- **On start-up**, and after losing Wi-Fi for more than a minute, the board scans and joins your **Preferred** network if it's in range, otherwise the **strongest** saved one.
- **Connect now** (settings → Wi-Fi networks) switches straight away. The settings page stops responding, because the board has left that network; use its new IP.
- **Scan for networks** lists everything nearby. Tap one to fill in its name exactly, which handles names with curly apostrophes, like *Alex’s iPhone*.
- If none of your networks are around, the board goes into setup mode, and checks every 3 minutes for one to come back.

## iPhone hotspot

- **Settings → Personal Hotspot → Maximise Compatibility: ON** (otherwise newer iPhones use 5 GHz, which the board can't see)
- Keep the Personal Hotspot screen open while the board connects
- The hotspot name is the phone's name (*Settings → General → About → Name*)

## Advanced (per network)

| Setting | When to use it |
|---|---|
| **Fixed IP + gateway** | The network won't hand out an address (DHCP), e.g. some office networks |
| **DNS server** | Needed with a fixed IP if the router doesn't answer DNS (a SonicWall without DNS Proxy, for example). 8.8.8.8 is always used as a backup. |
| **Compatibility mode** | Uses Wi-Fi 4 instead of Wi-Fi 6. The board also tries this automatically if it joins but gets no address. |
| **Username** | Enterprise (WPA2-Enterprise / PEAP) networks; these are marked *needs username* in the scan |

## Error messages

| On screen | Meaning / fix |
|---|---|
| Wrong password | Re-enter it; use *Show password* to check what was pasted |
| Network not found | Out of range, or 5 GHz only |
| Access point refused the board | MAC filtering or a client limit; allow the MAC shown on screen |
| Joined Wi-Fi but got no IP address | DHCP isn't serving the board: use a fixed IP, or allow its MAC on the router or firewall |
| Can't reach YouTube – check DNS/firewall | Connected, but no internet: set a DNS server, or allow the board through the firewall |
| API key not valid / API not enabled / quota used up | See [GETTING-STARTED.md](GETTING-STARTED.md#2-get-a-youtube-data-api-key) |

The board's Wi-Fi MAC address is shown at the bottom of the settings page and on the failure screen.
