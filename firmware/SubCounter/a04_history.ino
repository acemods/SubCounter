// ════════════════════════════════════════════════════════════════════════════
//  Subscriber history (LittleFS): one 8-byte sample per hour or per change
// ════════════════════════════════════════════════════════════════════════════
struct Sample { uint32_t t; int32_t s; };

String histPath(int i) { return "/h_" + ch[i].id + ".bin"; }

void loadLastSample(int i) {
  if (!fsOk || !ch[i].id.length()) return;
  File f = LittleFS.open(histPath(i), "r");
  if (!f) return;
  size_t n = f.size() / sizeof(Sample);
  Sample s;
  if (n) {
    f.read((uint8_t *)&s, sizeof(s)); ch[i].histStart = s.t;
    f.seek((n - 1) * sizeof(Sample)); f.read((uint8_t *)&s, sizeof(s));
    ch[i].lastSampleT = s.t; ch[i].lastSampleS = s.s;
  }
  f.close();
}

void computeStats(int i) {
  Channel &c = ch[i];
  c.statsOk = false;
  if (!fsOk || !timeValid() || c.subs < 0 || !c.id.length()) return;
  File f = LittleFS.open(histPath(i), "r");
  if (!f) return;
  time_t now = nowT(), mid = localMidnight();
  bool haveToday = false, have7 = false, have30 = false;
  Sample s, beforeMid = {0, -1};
  long baseToday = -1, base7 = -1, base30 = -1, base24 = -1; time_t t7 = 0, t30 = 0, tToday = 0;
  time_t m0 = monthStart(0), m1 = monthStart(1);
  long beforeM0 = -1, beforeM1 = -1, firstS = -1; time_t firstT = 0;
  bool first = true;
  while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
    if (first) { c.histStart = s.t; first = false; firstS = s.s; firstT = s.t; }
    if ((time_t)s.t < m0) beforeM0 = s.s;
    if ((time_t)s.t < m1) beforeM1 = s.s;
    if ((time_t)s.t < mid) beforeMid = s;
    else if (!haveToday) { haveToday = true; baseToday = beforeMid.s >= 0 ? beforeMid.s : s.s; tToday = beforeMid.s >= 0 ? beforeMid.t : s.t; }
    if (!have30 && (time_t)s.t >= now - 30L * 86400) { have30 = true; base30 = s.s; t30 = s.t; }
    if (!have7 && (time_t)s.t >= now - 7L * 86400)   { have7 = true;  base7 = s.s;  t7 = s.t; }
    if ((time_t)s.t <= now - 86400 || base24 < 0) base24 = s.s;   // last sample at least 24 h old (or the first one)
  }
  f.close();
  if (first) return;
  if (!haveToday) { baseToday = beforeMid.s >= 0 ? beforeMid.s : c.subs; tToday = beforeMid.t; }
  c.gainToday = c.subs - baseToday;
  // this month so far, and last month (needs history reaching back to its start, give or take 2 days)
  long baseM0 = beforeM0 >= 0 ? beforeM0 : firstS;
  c.gainMonth = c.subs - baseM0;
  long baseM1 = beforeM1 >= 0 ? beforeM1 : ((firstT && firstT <= m1 + 2 * 86400) ? firstS : -1);
  c.lastMonthOk = baseM1 >= 0 && beforeM0 >= 0;
  c.gainLastMonth = c.lastMonthOk ? beforeM0 - baseM1 : 0;
  c.gain24 = c.subs - base24;
  c.gain7 = have7 ? c.subs - base7 : 0;
  c.gain30 = have30 ? c.subs - base30 : 0;
  c.span7Days = have7 ? (now - t7) / 86400.0f : 0;
  c.span30Days = have30 ? (now - t30) / 86400.0f : 0;
  // growth rate from the longest window we have (needs at least 6 hours of data)
  float span = c.span30Days > c.span7Days ? c.span30Days : c.span7Days;
  long gain  = c.span30Days > c.span7Days ? c.gain30 : c.gain7;
  c.ratePerDay = span >= 0.25f ? gain / span : 0;
  c.statsOk = true;
  (void)tToday;
}

void recordSample(int i) {
  Channel &c = ch[i];
  if (!fsOk || !timeValid() || c.subs < 0 || !c.id.length()) return;
  time_t now = nowT();
  if (c.lastSampleS == c.subs && now - c.lastSampleT < 3600) return;
  String p = histPath(i);
  File f = LittleFS.open(p, "a");
  if (!f) return;
  Sample s = { (uint32_t)now, (int32_t)c.subs };
  f.write((uint8_t *)&s, sizeof(s));
  size_t size = f.size();
  f.close();
  c.lastSampleT = now; c.lastSampleS = c.subs;
  if (!c.histStart) c.histStart = now;
  // keep files small: drop samples older than HIST_MAX_AGE (checked by age, so it runs about once a day)
  (void)size;
  if (c.histStart && now - c.histStart > HIST_MAX_AGE + 86400) {
    File in = LittleFS.open(p, "r");
    File out = LittleFS.open("/tmp.bin", "w");
    Sample r;
    while (in && out && in.read((uint8_t *)&r, sizeof(r)) == sizeof(r))
      if ((time_t)r.t >= now - HIST_MAX_AGE) out.write((uint8_t *)&r, sizeof(r));
    if (in) in.close();
    if (out) out.close();
    LittleFS.remove(p);
    LittleFS.rename("/tmp.bin", p);
    c.histStart = 0;
    loadLastSample(i);                 // re-read the new oldest sample
  }
}

// ── Personal bests: one small file per channel ─────────────────────────────
struct PBFile { int32_t day; uint32_t dayT; int32_t week; uint32_t weekT; int32_t vid; uint32_t vidT; char title[64]; };
String pbPath(int i) { return "/pb_" + ch[i].id + ".bin"; }
void pbLoad(int i) {
  Channel &c = ch[i];
  c.pbLoaded = true;
  if (!fsOk || !c.id.length()) return;
  File f = LittleFS.open(pbPath(i), "r");
  PBFile r;
  if (f && f.read((uint8_t *)&r, sizeof(r)) == sizeof(r)) {
    c.pbDay = r.day; c.pbDayT = r.dayT; c.pbWeek = r.week; c.pbWeekT = r.weekT; c.pbVid = r.vid; c.pbVidT = r.vidT;
    r.title[63] = 0; c.pbVidTitle = r.title;
  }
  if (f) f.close();
}
void pbSave(int i) {
  Channel &c = ch[i];
  if (!fsOk || !c.id.length()) return;
  PBFile r = { c.pbDay, c.pbDayT, c.pbWeek, c.pbWeekT, c.pbVid, c.pbVidT, {0} };
  strncpy(r.title, c.pbVidTitle.c_str(), 63);
  File f = LittleFS.open(pbPath(i), "w");
  if (f) { f.write((uint8_t *)&r, sizeof(r)); f.close(); }
}
int pbAlertDay[3] = { -1, -1, -1 };     // one record alert per kind per day
void pbAlert(int i, int kind, long prev, long now) {
  if (i != 0 || prev <= 0 || !timeValid()) return;    // only your channel, and only once a record exists
  time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
  if (pbAlertDay[kind] == lt.tm_yday) return;
  pbAlertDay[kind] = lt.tm_yday;
  if (numAlerts < MAX_CH * 2) addAlert(Alert{ A_RECORD, i, kind, prev, now });
}
// Called after computeStats: best day (since midnight) and best 7 days
void updateRecords(int i) {
  Channel &c = ch[i];
  if (!c.pbLoaded) pbLoad(i);
  if (!c.statsOk || !timeValid()) return;
  time_t now = nowT();
  bool changed = false;
  bool settled = c.histStart && now - c.histStart > 86400;          // ignore the very first day's partial data
  if (settled && now - localMidnight() > 600 && c.gainToday > c.pbDay) {
    pbAlert(i, 0, c.pbDay, c.gainToday);
    c.pbDay = c.gainToday; c.pbDayT = now; changed = true;
  }
  if (c.span7Days >= 6.9f && c.gain7 > c.pbWeek) {
    pbAlert(i, 1, c.pbWeek, c.gain7);
    c.pbWeek = c.gain7; c.pbWeekT = now; changed = true;
  }
  if (changed) pbSave(i);
}
// Your new video's first-24-hour views (from the new video tracker)
void recordVideo24h(long views, const String &title) {
  Channel &c = ch[0];
  if (!c.pbLoaded) pbLoad(0);
  if (views <= c.pbVid) return;
  pbAlert(0, 2, c.pbVid, views);
  c.pbVid = views; c.pbVidT = nowT(); c.pbVidTitle = title.substring(0, 63);
  pbSave(0);
}

// Estimated live count between YouTube's rounded steps
long estimateFor(int i) {
  Channel &c = ch[i];
  if (c.tw || !cfgEst || c.subs < 0 || !timeValid() || c.ratePerDay <= 0 || !c.stepChangedAt) return c.subs;   // Twitch counts are exact
  long step = stepFor(c.subs);
  if (step <= 1) return c.subs;
  long add = (long)(c.ratePerDay * (nowT() - c.stepChangedAt) / 86400.0f);
  if (add > step - 1) add = step - 1;
  if (add < 0) add = 0;
  return c.subs + add;
}
bool isEstimated(int i) { return !ch[i].tw && (estimateFor(i) != ch[i].subs || (cfgEst && stepFor(ch[i].subs) > 1 && ch[i].ratePerDay > 0)); }

// ════════════════════════════════════════════════════════════════════════════
//  Channel views history (LittleFS): one 12-byte sample when the total changes
//  (at most every 30 min, and at least every 6 h). YouTube only updates channel
//  view totals every few hours, so this is a day-by-day figure.
// ════════════════════════════════════════════════════════════════════════════
struct VSample { uint32_t t; uint32_t lo; uint32_t hi; };
static inline long long vsVal(const VSample &s) { return ((long long)s.hi << 32) | s.lo; }
String viewsPath(int i) { return "/v_" + ch[i].id + ".bin"; }

void loadLastViews(int i) {
  Channel &c = ch[i];
  if (!fsOk || c.tw || !c.id.length() || c.lastViewT) return;
  File f = LittleFS.open(viewsPath(i), "r");
  if (!f) return;
  size_t n = f.size() / sizeof(VSample);
  VSample s;
  if (n) {
    f.read((uint8_t *)&s, sizeof(s)); c.vHistStart = s.t;
    f.seek((n - 1) * sizeof(VSample)); f.read((uint8_t *)&s, sizeof(s));
    c.lastViewT = s.t; c.lastViewV = vsVal(s);
  }
  f.close();
}

void recordViews(int i) {
  Channel &c = ch[i];
  if (!fsOk || !timeValid() || c.tw || c.views < 0 || !c.id.length()) return;
  loadLastViews(i);
  time_t now = nowT();
  bool changed = c.views != c.lastViewV;
  if (!(changed && now - c.lastViewT >= 1800) && now - c.lastViewT < 6 * 3600) return;
  String p = viewsPath(i);
  File f = LittleFS.open(p, "a");
  if (!f) return;
  VSample s = { (uint32_t)now, (uint32_t)(c.views & 0xFFFFFFFFLL), (uint32_t)(c.views >> 32) };
  f.write((uint8_t *)&s, sizeof(s));
  f.close();
  c.lastViewT = now; c.lastViewV = c.views;
  if (!c.vHistStart) c.vHistStart = now;
  if (now - c.vHistStart > HIST_MAX_AGE + 86400) {            // trim to the last 40 days, about once a day
    File in = LittleFS.open(p, "r"), out = LittleFS.open("/tmpv.bin", "w");
    VSample r; time_t first = 0;
    while (in && out && in.read((uint8_t *)&r, sizeof(r)) == sizeof(r))
      if ((time_t)r.t >= now - HIST_MAX_AGE) { out.write((uint8_t *)&r, sizeof(r)); if (!first) first = r.t; }
    if (in) in.close();
    if (out) out.close();
    LittleFS.remove(p); LittleFS.rename("/tmpv.bin", p);
    c.vHistStart = first ? first : now;
  }
}

void computeViewStats(int i) {
  Channel &c = ch[i];
  c.vOk = false;
  if (!fsOk || !timeValid() || c.tw || c.views < 0 || !c.id.length()) return;
  File f = LittleFS.open(viewsPath(i), "r");
  if (!f) return;
  time_t now = nowT(), mid = localMidnight();
  long long beforeMid = -1, firstToday = -1, base7 = -1, base30 = -1, base24 = -1;
  VSample s; bool any = false;
  while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
    long long v = vsVal(s); any = true;
    if ((time_t)s.t < mid) beforeMid = v; else if (firstToday < 0) firstToday = v;
    if (base30 < 0 && (time_t)s.t >= now - 30L * 86400) base30 = v;
    if (base7 < 0 && (time_t)s.t >= now - 7L * 86400) base7 = v;
    if ((time_t)s.t <= now - 86400 || base24 < 0) base24 = v;
  }
  f.close();
  if (!any) return;
  long long baseToday = beforeMid >= 0 ? beforeMid : (firstToday >= 0 ? firstToday : c.views);
  c.vToday = c.views - baseToday;
  c.v24 = c.views - base24;
  c.v7 = base7 >= 0 ? c.views - base7 : 0;
  c.v30 = base30 >= 0 ? c.views - base30 : 0;
  c.vOk = true;
}

// ── Latest video tracker for every channel ───────────────────────────────────
// A sample of the latest video's views every hour (from the 10-minute latest-video
// check, so no extra API quota), up to 48 h worth, saved so a restart doesn't lose it.
void saveLatest() {
  if (!fsOk) return;
  JsonDocument d;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || !c.id.length() || !c.lvN) continue;
    JsonObject o = d[c.id].to<JsonObject>();
    o["vid"] = c.lvVid;
    JsonArray a = o["p"].to<JsonArray>();
    for (int k = 0; k < c.lvN; k++) { JsonArray x = a.add<JsonArray>(); x.add(c.lvT[k]); x.add(c.lvV[k]); }
  }
  File f = LittleFS.open("/latest.json", "w"); if (!f) return;
  serializeJson(d, f); f.close();
}
void loadLatest() {
  if (!fsOk || !LittleFS.exists("/latest.json")) return;
  File f = LittleFS.open("/latest.json", "r"); if (!f) return;
  JsonDocument d; DeserializationError e = deserializeJson(d, f); f.close();
  if (e) return;
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.tw || !c.id.length() || c.lvN) continue;
    JsonObject o = d[c.id]; if (o.isNull()) continue;
    c.lvVid = (const char *)(o["vid"] | "");
    for (JsonArray x : o["p"].as<JsonArray>()) { if (c.lvN >= 48) break; c.lvT[c.lvN] = x[0] | 0; c.lvV[c.lvN] = x[1] | 0; c.lvN++; }
  }
}
// Call after the latest videos have been refreshed. Returns true if anything was added.
bool trackLatest(int i) {
  Channel &c = ch[i];
  if (c.tw || !c.vidId.length() || c.vidViews < 0 || c.live || !timeValid()) return false;
  if (c.lvVid != c.vidId) { c.lvVid = c.vidId; c.lvN = 0; }         // a new upload: start again
  uint32_t now = (uint32_t)nowT();
  if (c.lvN > 1 && now - c.lvT[c.lvN - 2] < 3600) {                     // keep the newest number fresh
    c.lvV[c.lvN - 1] = (uint32_t)min(c.vidViews, 4000000000LL); c.lvT[c.lvN - 1] = now; return false;
  }
  if (c.lvN >= 48) { memmove(c.lvT, c.lvT + 1, 47 * sizeof(uint32_t)); memmove(c.lvV, c.lvV + 1, 47 * sizeof(uint32_t)); c.lvN = 47; }
  c.lvT[c.lvN] = now; c.lvV[c.lvN] = (uint32_t)min(c.vidViews, 4000000000LL); c.lvN++;
  return true;
}
