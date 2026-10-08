// ════════════════════════════════════════════════════════════════════════════
//  New video tracker — your own channel (the first one in the list)
// ════════════════════════════════════════════════════════════════════════════


long vtViews() { return vt.n ? (long)vt.v[vt.n - 1] : -1; }
long vtAgeMin() { return (vt.pub && timeValid()) ? (long)((nowT() - vt.pub) / 60) : -1; }

// views per hour over the last ~hour of samples
long vtRate() {
  if (vt.n < 2) return -1;
  int a = vt.n - 1, b = a;
  while (b > 0 && vt.t[a] - vt.t[b - 1] <= 3600) b--;
  if (b == a) b = a - 1;
  long dt = vt.t[a] - vt.t[b];
  if (dt < 300) return -1;
  return (long)((vt.v[a] - vt.v[b]) * 3600.0 / dt);
}

// views at a given age (linear between samples); -1 if we didn't see it
long vtViewsAt(long ageSec) {
  for (int i = 1; i < vt.n; i++) {
    long a0 = vt.t[i - 1] - vt.pub, a1 = vt.t[i] - vt.pub;
    if (a0 <= ageSec && a1 >= ageSec && a1 > a0)
      return vt.v[i - 1] + (long)((vt.v[i] - vt.v[i - 1]) * (double)(ageSec - a0) / (a1 - a0));
  }
  return -1;
}

// Past results: up to 8 (views@1h, views@24h) pairs kept in flash
void vtLoadBaseline() {
  prefs.begin("vtrack", true);
  int n = prefs.getInt("n", 0);
  long s1 = 0, s24 = 0; int c1 = 0, c24 = 0;
  for (int i = 0; i < min(n, 8); i++) {
    long a = prefs.getLong(("h1_" + String(i)).c_str(), -1), b = prefs.getLong(("h24_" + String(i)).c_str(), -1);
    if (a >= 0) { s1 += a; c1++; }
    if (b >= 0) { s24 += b; c24++; }
  }
  prefs.end();
  vt.base1h = c1 ? s1 / c1 : -1;
  vt.base24h = c24 ? s24 / c24 : -1;
}

void vtSaveResult(bool is24h, long views) {
  prefs.begin("vtrack", false);
  String last = prefs.getString("lastvid", "");
  int n = prefs.getInt("n", 0);                 // total videos recorded; the last 8 are kept (slot = index % 8)
  int slot;
  if (last == vt.vid && n > 0) slot = (n - 1) % 8;          // same video: add its other figure
  else {
    slot = n % 8; n++;
    prefs.putInt("n", n); prefs.putString("lastvid", vt.vid);
    prefs.putLong(("h1_" + String(slot)).c_str(), -1); prefs.putLong(("h24_" + String(slot)).c_str(), -1);
  }
  prefs.putLong(((is24h ? "h24_" : "h1_") + String(slot)).c_str(), views);
  prefs.end();
}

void vtAddSample(time_t t, long views) {
  if (vt.n && (long)(t - vt.t[vt.n - 1]) < 240) { vt.v[vt.n - 1] = views; return; }   // too soon: update last
  if (vt.n == VT_MAX) {          // thin out the older half
    int k = 0;
    for (int i = 0; i < VT_MAX; i++) if (i >= VT_MAX / 2 || i % 2 == 0) { vt.t[k] = vt.t[i]; vt.v[k] = vt.v[i]; k++; }
    vt.n = k;
  }
  vt.t[vt.n] = t; vt.v[vt.n] = views; vt.n++;
}

void vtMilestoneStart(long views) {
  const long steps[] = { 100, 250, 500, 1000, 2500, 5000, 10000, 25000, 50000, 100000, 250000, 500000, 1000000 };
  vt.nextMilestone = 0;
  for (long s : steps) if (views < s) { vt.nextMilestone = s; break; }
}

// Typical lifetime views of your previous ~10 uploads (2 API units, once a day)
void vtFetchTypical() {
  if (!numCh || !ch[0].uploads.length()) return;
  JsonDocument doc;
  if (ytGet("playlistItems", "part=contentDetails&maxResults=11&fields=items/contentDetails/videoId&playlistId=" + ch[0].uploads, doc) != 200) return;
  String ids;
  for (JsonObject it : doc["items"].as<JsonArray>()) {
    String id = it["contentDetails"]["videoId"] | "";
    if (id.length() && id != vt.vid) ids += (ids.length() ? "," : "") + id;
  }
  if (!ids.length()) return;
  JsonDocument d2;
  if (ytGet("videos", "part=statistics&fields=items/statistics/viewCount&id=" + ids, d2) != 200) return;
  long v[12]; int n = 0;
  for (JsonObject it : d2["items"].as<JsonArray>()) if (n < 12) v[n++] = atol(it["statistics"]["viewCount"] | "0");
  if (!n) return;
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++) if (v[b] < v[a]) { long t = v[a]; v[a] = v[b]; v[b] = t; }
  vt.typical = v[n / 2];
}

// Called after fetchLatestVideos(): start tracking when your latest upload is under 2 days old
void vtCheckNewUpload() {
  if (!numCh || !timeValid()) return;
  Channel &c = ch[0];
  if (!c.vidId.length() || !c.vidPublished || c.live) { vt.active = false; return; }
  bool fresh = nowT() - c.vidPublished < 48L * 3600;
  if (c.vidId != vt.vid) {
    vt = VideoTrack();
    vt.vid = c.vidId; vt.pub = c.vidPublished;
    vtLoadBaseline();
    if (c.vidViews >= 0) { vtAddSample(nowT(), c.vidViews); vtMilestoneStart(c.vidViews); }
    lastTypicalFetch = 0;
  }
  vt.active = fresh;
}

// Every 5 minutes while active: just this video's numbers (1 API unit)
void vtPoll() {
  if (!vt.active) return;
  JsonDocument doc;
  if (ytGet("videos", "part=statistics&fields=items/statistics(viewCount,likeCount,commentCount)&id=" + vt.vid, doc) != 200) return;
  JsonObject st = doc["items"][0]["statistics"];
  if (st.isNull()) return;
  long views = atol(st["viewCount"] | "-1");
  if (views < 0) return;
  ch[0].vidViews = views;
  ch[0].vidLikes = String(st["likeCount"] | "-1").toInt();
  ch[0].vidComments = String(st["commentCount"] | "-1").toInt();
  time_t now = nowT();
  vtAddSample(now, views);
  long age = now - vt.pub;
  if (vt.at1h < 0 && age >= 3600) { vt.at1h = vtViewsAt(3600); if (vt.at1h < 0 && age < 3 * 3600) vt.at1h = views; if (vt.at1h >= 0) vtSaveResult(false, vt.at1h); }
  if (vt.at24h < 0 && age >= 86400) { vt.at24h = vtViewsAt(86400); if (vt.at24h < 0 && age < 30 * 3600) vt.at24h = views; if (vt.at24h >= 0) { vtSaveResult(true, vt.at24h); recordVideo24h(vt.at24h, ch[0].vidTitle); } }
  if (vt.nextMilestone && views >= vt.nextMilestone && numAlerts < MAX_CH * 2) {
    alerts[numAlerts++] = { A_VIEWS, 0, -1, 0, vt.nextMilestone };
    vtMilestoneStart(views);
  }
  if (age >= 48L * 3600) vt.active = false;
}

String ageStr(long mins) {
  if (mins < 0) return "";
  if (mins < 60) return String(mins) + " min";
  if (mins < 48 * 60) return String(mins / 60) + "h " + String(mins % 60) + "m";
  return String(mins / 1440) + " days";
}

// Compare with your usual: "+40% vs your usual" etc.
String vtCompare() {
  long age = vtAgeMin();
  long views = vtViews();
  if (views < 0) return "";
  if (age >= 60 && vt.at1h >= 0 && vt.base1h > 0 && age < 24 * 60) {
    long pct = (vt.at1h - vt.base1h) * 100 / vt.base1h;
    return String("1st hour ") + (pct >= 0 ? "+" : "") + pct + "% vs usual";
  }
  if (age >= 24 * 60 && vt.at24h >= 0 && vt.base24h > 0) {
    long pct = (vt.at24h - vt.base24h) * 100 / vt.base24h;
    return String("1st day ") + (pct >= 0 ? "+" : "") + pct + "% vs usual";
  }
  if (vt.typical > 0) return String((long)(views * 100 / vt.typical)) + "% of a typical video";
  return "";
}

// YouTube card 2 for your own channel while a new upload is being tracked
void drawVideoTracker() {
  Channel &c = ch[0];
  drawDetailHeader("New video");
  ft(8, 56, fit(asciiOnly(c.vidTitle), F_S, RIGHT_EDGE - 8), F_S, C_WHITE);
  long views = vtViews();
  String v = views >= 0 ? withCommas(views) : String("...");
  const uint8_t *f = numFont(v, 180) == F_N42 ? F_N32 : F_N24;
  ft(8, 98, v, f, C_WHITE);
  ft(12 + tw(v, f), 98, "views", F_S, C_GREY);
  long r = vtRate();
  ft(8, 124, "in " + ageStr(vtAgeMin()) + (r >= 0 ? "  -  " + compact(r) + "/hour" : String("")), F_S, C_GREY);
  String cmp = vtCompare();
  ft(8, 148, fit(cmp, F_S, RIGHT_EDGE - 8), F_S, cmp.indexOf("+") >= 0 ? C_GREEN : (cmp.indexOf("-") >= 0 ? C_RED : C_BLUE));
  String lc = (c.vidLikes >= 0 ? compact(c.vidLikes) + " likes" : String("")) + (c.vidComments >= 0 ? "   " + compact(c.vidComments) + " comments" : String(""));
  ft(8, 168, fit(lc, F_S, RIGHT_EDGE - 8), F_S, C_GREEN);
  // tiny sparkline of views, top right
  if (vt.n >= 2) {
    int gx = 196, gy = 64, gw = 100, gh = 40;
    long mn = vt.v[0], mx = vt.v[vt.n - 1]; if (mx <= mn) mx = mn + 1;
    uint32_t t0 = vt.t[0], t1 = vt.t[vt.n - 1]; if (t1 <= t0) t1 = t0 + 1;
    int px = -1, py = -1;
    for (int i = 0; i < vt.n; i++) {
      int x = gx + (int)((float)(vt.t[i] - t0) / (t1 - t0) * gw);
      int y = gy + gh - (int)((float)(vt.v[i] - mn) / (mx - mn) * gh);
      if (px >= 0) gfx->drawLine(px, py, x, y, C_GREEN);
      px = x; py = y;
    }
  }
}
