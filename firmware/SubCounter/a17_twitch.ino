// ════════════════════════════════════════════════════════════════════════════
//  Twitch: followers, live status and pictures (Helix API, app access token)
// ════════════════════════════════════════════════════════════════════════════
String twToken; unsigned long twTokenUntil = 0;
bool twLoaded = false;                 // no "went live" alerts for streams already running at start-up
String twError;

bool twGetToken() {
  if (twToken.length() && millis() < twTokenUntil) return true;
  if (!cfgTwId.length() || !cfgTwSecret.length()) { twError = "Twitch: add your Client ID and Secret in settings"; return false; }
  int code; String body;
  String form = "client_id=" + urlEncode(cfgTwId) + "&client_secret=" + urlEncode(cfgTwSecret) + "&grant_type=client_credentials";
  { NetIO io;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(10000);
    if (!http.begin(client, "https://id.twitch.tv/oauth2/token")) return false;
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    code = http.POST(form);
    body = http.getString(); http.end(); }
  if (code != 200) { twError = code == 400 || code == 403 ? "Twitch: Client ID or Secret is wrong" : "Twitch: can't sign in (" + String(code) + ")"; return false; }
  JsonDocument doc; deserializeJson(doc, body);
  twToken = doc["access_token"] | "";
  long exp = doc["expires_in"] | 3600;
  twTokenUntil = millis() + (unsigned long)max(60L, exp - 300) * 1000UL;
  return twToken.length() > 0;
}

int twGet(const String &path, JsonDocument &doc) {
  Hdr hs[] = { { "Client-Id", cfgTwId }, { "Authorization", "Bearer " + twToken } };
  String body;
  int code = httpsCall(connTW, "GET", "https://api.twitch.tv/helix/" + path, hs, 2, &body);
  if (code == 200) deserializeJson(doc, body);
  if (code == 401) twToken = "";         // expired: get a new one next time
  return code;
}

// Twitch pictures: decode once into an 88x88 RGB565 bitmap (nearest-pixel scaling)
uint16_t *twDst; int twSrcW, twSrcH;
static void twPut(int sx, int sy, uint16_t px) {
  int dx = sx * 88 / twSrcW, dy = sy * 88 / twSrcH;
  if (dx >= 0 && dx < 88 && dy >= 0 && dy < 88) twDst[dy * 88 + dx] = px;
}
int twPngLine(PNGDRAW *d) {
  static uint16_t line[320];
  PNG *png = (PNG *)d->pUser;
  png->getLineAsRGB565(d, line, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  for (int x = 0; x < d->iWidth && x < 320; x++) twPut(x, d->y, line[x]);
  return 1;
}
int twJpgBlock(JPEGDRAW *d) {
  for (int y = 0; y < d->iHeight; y++)
    for (int x = 0; x < d->iWidth; x++) twPut(d->x + x, d->y + y, d->pPixels[y * d->iWidth + x]);
  return 1;
}
void twDecodeAvatar(Channel &c, uint8_t *buf, int len) {
  if (ESP.getFreeHeap() < 90000) { connClose(connYT); connClose(connTW); connClose(connSP); }   // PNG decoding needs room
  uint16_t *dst = (uint16_t *)malloc(88 * 88 * 2);
  if (!dst) return;
  memset(dst, 0, 88 * 88 * 2);
  twDst = dst; bool ok = false;
  if (len > 8 && buf[0] == 0x89 && buf[1] == 'P') {
    PNG *png = new (std::nothrow) PNG();
    if (png && png->openRAM(buf, len, twPngLine) == PNG_SUCCESS && png->getWidth() <= 320) {
      twSrcW = png->getWidth(); twSrcH = png->getHeight();
      ok = png->decode(png, 0) == PNG_SUCCESS;
      png->close();
    }
    delete png;
  } else if (len > 4 && buf[0] == 0xFF && buf[1] == 0xD8) {
    if (jpeg.openRAM(buf, len, twJpgBlock)) {
      jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
      twSrcW = jpeg.getWidth(); twSrcH = jpeg.getHeight();
      ok = jpeg.decode(0, 0, 0) == 1;
      jpeg.close();
    }
  }
  if (ok) { if (c.av565) free(c.av565); c.av565 = dst; } else free(dst);
}
void twFetchAvatar(Channel &c, const String &url300) {
  String url = url300; url.replace("300x300", "150x150");
  int len;
  uint8_t *buf = httpDownload(url, len, 90000);
  if (!buf) return;
  twDecodeAvatar(c, buf, len);
  free(buf);
}

void twitchFetch() {
  bool any = false;
  for (int i = 0; i < numCh; i++) if (ch[i].tw) any = true;
  if (!any) { twError = ""; return; }
  if (!twGetToken()) { for (int i = 0; i < numCh; i++) if (ch[i].tw && ch[i].subs < 0) ch[i].err = twError; return; }
  // 1. user ids, names and pictures (until we have them)
  String q;
  for (int i = 0; i < numCh; i++) if (ch[i].tw && !ch[i].twId.length()) q += (q.length() ? "&" : "") + String("login=") + ch[i].twLogin;
  if (q.length()) {
    JsonDocument doc;
    int code = twGet("users?" + q, doc);
    if (code == 200) {
      for (int i = 0; i < numCh; i++) {
        if (!ch[i].tw || ch[i].twId.length()) continue;
        bool found = false;
        for (JsonObject u : doc["data"].as<JsonArray>()) {
          if (ch[i].twLogin != (const char *)(u["login"] | "")) continue;
          ch[i].twId = u["id"] | ""; ch[i].title = u["display_name"] | ch[i].twLogin;
          ch[i].avatarUrl = u["profile_image_url"] | "";
          ch[i].joined = parseIso(u["created_at"] | "");
          found = true;
          if (!ch[i].lastSampleT) loadLastSample(i);
          if (ch[i].avatarUrl.length()) twFetchAvatar(ch[i], ch[i].avatarUrl);
        }
        if (!found) ch[i].err = "Twitch channel not found";
      }
    } else { twError = "Twitch error " + String(code); return; }
  }
  // 2. follower totals (one call each; exact numbers, no rounding)
  bool okAny = false;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (!c.tw || !c.twId.length()) continue;
    JsonDocument doc;
    if (twGet("channels/followers?first=1&broadcaster_id=" + c.twId, doc) != 200) continue;
    long n = doc["total"] | -1L;
    if (n < 0) continue;
    okAny = true;
    if (n != c.subs) c.stepChangedAt = timeValid() ? nowT() : 0;
    if (c.subs >= 0 && n > c.subs && numAlerts < MAX_CH * 2) {
      long m = nextMilestone(c.subs);
      addAlert(n >= m ? Alert{ A_MILESTONE, i, -1, n - c.subs, m } : Alert{ A_GAIN, i, -1, n - c.subs, n });
    }
    c.subs = n; c.err = "";
    recordSample(i); computeStats(i); updateRecords(i);
    pump();
  }
  // 3. who's live (one call for all)
  String ids;
  for (int i = 0; i < numCh; i++) if (ch[i].tw && ch[i].twId.length()) ids += (ids.length() ? "&" : "") + String("user_id=") + ch[i].twId;
  if (ids.length()) {
    JsonDocument doc;
    if (twGet("streams?" + ids, doc) == 200) {
      for (int i = 0; i < numCh; i++) {
        Channel &c = ch[i];
        if (!c.tw || !c.twId.length()) continue;
        bool wasLive = c.live; c.live = false;
        for (JsonObject st : doc["data"].as<JsonArray>()) {
          if (c.twId != (const char *)(st["user_id"] | "")) continue;
          c.live = true;
          c.vidTitle = st["title"] | ""; c.liveViewers = st["viewer_count"] | -1L;
          c.vidPublished = parseIso(st["started_at"] | ""); c.twGame = st["game_name"] | "";
          String th = st["thumbnail_url"] | ""; th.replace("{width}", "320"); th.replace("{height}", "180"); c.twThumb = th;
        }
        if (c.live && !wasLive && twLoaded && cfgLiveAlert && numAlerts < MAX_CH * 2)
          addAlert(Alert{ A_LIVE, i, -1, 0, c.liveViewers });
      }
      twLoaded = true;
    }
  }
  if (okAny) { twError = ""; lastFetchOk = millis(); }
}

// Latest uploads for every channel: 1 unit each + 1 unit for all video details.
// Looks at the newest 3 so a live stream is still spotted if a Short or a
// scheduled premiere sits on top of it.
#define VID_LOOK 3
bool videosLoaded = false;          // no "went live" alerts for streams already running at start-up
void applyVideo(Channel &c, JsonObject it) {
  bool wasLive = c.live;
  c.vidId = it["id"] | "";
  c.vidTitle = it["snippet"]["title"] | "";
  c.vidPublished = parseIso(it["snippet"]["publishedAt"] | "");
  c.live = String(it["snippet"]["liveBroadcastContent"] | "none") == "live";
  c.vidDuration = parseDuration(it["contentDetails"]["duration"] | "");
  c.vidViews = atoll(it["statistics"]["viewCount"] | "-1");
  c.vidLikes = String(it["statistics"]["likeCount"] | "-1").toInt();
  c.vidComments = String(it["statistics"]["commentCount"] | "-1").toInt();
  c.liveViewers = String(it["liveStreamingDetails"]["concurrentViewers"] | "-1").toInt();
  if (c.live && !wasLive && videosLoaded && cfgLiveAlert && numAlerts < MAX_CH * 2)
    addAlert(Alert{ A_LIVE, (int)(&c - ch), -1, 0, c.liveViewers });
}
const char *VID_FIELDS = "part=snippet,statistics,contentDetails,liveStreamingDetails"
  "&fields=items(id,snippet(title,publishedAt,liveBroadcastContent),statistics(viewCount,likeCount,commentCount),"
  "contentDetails/duration,liveStreamingDetails/concurrentViewers)&id=";

void fetchLatestVideos() {
  String cand[MAX_CH][VID_LOOK]; String vids;
  for (int i = 0; i < numCh; i++) {
    if (!ch[i].uploads.length()) continue;
    JsonDocument doc;
    int code = ytGet("playlistItems", "part=contentDetails&maxResults=" + String(VID_LOOK) +
                     "&fields=items/contentDetails/videoId&playlistId=" + ch[i].uploads, doc);
    if (code != 200) continue;
    int k = 0;
    for (JsonObject it : doc["items"].as<JsonArray>()) {
      String v = it["contentDetails"]["videoId"] | "";
      if (v.length() && k < VID_LOOK) { cand[i][k++] = v; vids += (vids.length() ? "," : "") + v; }
    }
  }
  if (!vids.length()) return;
  JsonDocument doc;
  int code = ytGet("videos", String(VID_FIELDS) + vids, doc);
  if (code != 200) return;
  JsonArray items = doc["items"].as<JsonArray>();
  for (int i = 0; i < numCh; i++) {
    if (!cand[i][0].length()) continue;
    JsonObject pick, firstDone;
    for (int k = 0; k < VID_LOOK && cand[i][k].length(); k++) {
      for (JsonObject it : items) {
        if (cand[i][k] != (const char *)(it["id"] | "")) continue;
        String lbc = it["snippet"]["liveBroadcastContent"] | "none";
        if (lbc == "live" && pick.isNull()) pick = it;                    // live wins
        if (lbc == "none" && firstDone.isNull()) firstDone = it;          // newest normal video
      }
    }
    if (pick.isNull()) pick = firstDone;
    if (pick.isNull()) continue;
    applyVideo(ch[i], pick);
  }
  videosLoaded = true;
  bool added = false;
  for (int i = 0; i < numCh; i++) if (trackLatest(i)) added = true;
  if (added) saveLatest();
}

// While someone is live: refresh just their stream (1 unit) every stats round,
// so the viewer count stays current and the badge goes away when they finish
void refreshLiveVideos() {
  String vids;
  for (int i = 0; i < numCh; i++) if (ch[i].live && ch[i].vidId.length()) vids += (vids.length() ? "," : "") + ch[i].vidId;
  if (!vids.length()) return;
  JsonDocument doc;
  if (ytGet("videos", String(VID_FIELDS) + vids, doc) != 200) return;
  for (JsonObject it : doc["items"].as<JsonArray>())
    for (int i = 0; i < numCh; i++) if (ch[i].vidId == (const char *)(it["id"] | "")) applyVideo(ch[i], it);
}

// Newest 3 comments on each channel's latest video: 1 unit per channel
// Each channel's last 10 uploads with their numbers: 2 units per channel, every 6 hours
void saveRecent();
// Returns false if any channel couldn't be fetched (so it's tried again soon, not in 6 hours)
bool fetchRecent() {
  bool allOk = true, any = false;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || !c.uploads.length()) continue;
    JsonDocument pl;
    if (ytGet("playlistItems", "part=contentDetails&maxResults=" + String(RECENT_N) +
              "&fields=items/contentDetails(videoId,videoPublishedAt)&playlistId=" + c.uploads, pl) != 200) { allOk = false; continue; }
    String ids;
    for (JsonObject it : pl["items"].as<JsonArray>()) { String v = it["contentDetails"]["videoId"] | ""; if (v.length()) ids += (ids.length() ? "," : "") + v; }
    if (!ids.length()) continue;
    JsonDocument vd;
    if (ytGet("videos", "part=snippet,statistics&fields=items(id,snippet(title,publishedAt,liveBroadcastContent),statistics(viewCount,likeCount,commentCount))&id=" + ids, vd) != 200) { allOk = false; continue; }
    int n = 0;
    for (JsonObject it : vd["items"].as<JsonArray>()) {
      if (n >= RECENT_N) break;
      if (String(it["snippet"]["liveBroadcastContent"] | "none") != "none") continue;   // skip live / upcoming
      Channel::Recent &r = c.rv[n++];
      r.id = it["id"] | ""; r.title = it["snippet"]["title"] | "";
      r.pub = parseIso(it["snippet"]["publishedAt"] | "");
      r.views = atoll(it["statistics"]["viewCount"] | "-1");
      r.likes = String(it["statistics"]["likeCount"] | "-1").toInt();
      r.comments = String(it["statistics"]["commentCount"] | "-1").toInt();
    }
    c.rvN = n; any = true;
    pump();
  }
  if (any) saveRecent();
  return allOk;
}
// Keep the recent uploads on flash so they're there straight after a restart
void saveRecent() {
  if (!fsOk) return;
  JsonDocument d;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || !c.id.length() || !c.rvN) continue;
    JsonArray a = d[c.id].to<JsonArray>();
    for (int k = 0; k < c.rvN; k++) {
      JsonArray x = a.add<JsonArray>();
      x.add(c.rv[k].id); x.add(c.rv[k].title); x.add((long)c.rv[k].pub); x.add(c.rv[k].views); x.add(c.rv[k].likes); x.add(c.rv[k].comments);
    }
  }
  File f = LittleFS.open("/recent.json", "w"); if (!f) return;
  serializeJson(d, f); f.close();
}
void loadRecent() {
  if (!fsOk || !LittleFS.exists("/recent.json")) return;
  File f = LittleFS.open("/recent.json", "r"); if (!f) return;
  JsonDocument d; DeserializationError e = deserializeJson(d, f); f.close();
  if (e) return;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || !c.id.length() || c.rvN) continue;
    JsonArray a = d[c.id].as<JsonArray>(); if (a.isNull()) continue;
    int n = 0;
    for (JsonArray x : a) {
      if (n >= RECENT_N) break;
      Channel::Recent &r = c.rv[n++];
      r.id = x[0] | ""; r.title = x[1] | ""; r.pub = x[2] | 0L;
      r.views = x[3] | -1LL; r.likes = x[4] | -1L; r.comments = x[5] | -1L;
    }
    c.rvN = n;
  }
}
// views per day since upload (at least one day, so brand-new videos don't jump to the top)
float perDay(const Channel::Recent &r) {
  if (r.views < 0 || !r.pub || !timeValid()) return 0;
  float d = max(1.0f, (float)(nowT() - r.pub) / 86400.0f);
  return r.views / d;
}

void fetchComments() {
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (!c.vidId.length()) continue;
    JsonDocument doc;
    int code = ytGet("commentThreads", "part=snippet&maxResults=3&order=time&textFormat=plainText"
                     "&fields=items/snippet/topLevelComment/snippet(authorDisplayName,textOriginal,publishedAt,likeCount)"
                     "&videoId=" + c.vidId, doc);
    if (code == 403) { c.cmOff = true; c.cmN = 0; continue; }       // comments turned off (or members-only)
    if (code != 200) continue;
    c.cmOff = false; c.cmN = 0;
    for (JsonObject it : doc["items"].as<JsonArray>()) {
      if (c.cmN >= 3) break;
      JsonObject sn = it["snippet"]["topLevelComment"]["snippet"];
      String t = sn["textOriginal"] | ""; t.replace("\n", " "); t.replace("\r", " ");
      if (t.length() > 200) t = t.substring(0, 200);
      c.cmAuthor[c.cmN] = sn["authorDisplayName"] | "";
      c.cmText[c.cmN] = t;
      c.cmT[c.cmN] = parseIso(sn["publishedAt"] | "");
      c.cmLikes[c.cmN] = sn["likeCount"] | 0;
      c.cmN++;
    }
  }
}

// Profile pictures (not API quota — plain image downloads), once per boot
void fetchAvatars() {
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || c.avatar || !c.avatarUrl.length()) continue;      // Twitch pictures are handled in twitchFetch
    String url = c.avatarUrl; int len;
    uint8_t *buf = httpDownload(url, len, 40000);
    if (buf && len > 4 && buf[0] == 0xFF && buf[1] == 0xD8 && !c.avatar) { c.avatar = buf; c.avatarLen = len; }
    else if (buf) free(buf);
    pump();
  }
}
