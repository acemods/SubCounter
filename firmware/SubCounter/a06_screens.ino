// ════════════════════════════════════════════════════════════════════════════
//  Screens
// ════════════════════════════════════════════════════════════════════════════
void drawStatus(const String &line1, const String &line2, uint16_t colour) {
  gfx->fillScreen(C_BG);
  centreText(line1, 55, 3, colour);
  centreText(line2, 100, 2, C_GREY);
}

void drawPortalScreen() {
  gfx->fillScreen(C_BG);
  centreText("SETUP MODE", 14, 3, C_GOLD);
  centreText("1. Join Wi-Fi:", 52, 2, C_GREY);
  centreText(AP_NAME, 74, 2, C_WHITE);
  centreText("2. Open in browser:", 104, 2, C_GREY);
  centreText("192.168.4.1", 126, 2, C_WHITE);
}

// ── Readable fonts (U8g2) ───────────────────────────────────────────────────
#define F_S   u8g2_font_helvB14_tf      // smallest text used anywhere
#define F_M   u8g2_font_helvB18_tf      // normal text
#define F_XS  u8g2_font_helvB08_tf      // LIVE badges only
#define F_XS2 u8g2_font_helvB10_tf
#define F_N24 u8g2_font_logisoso24_tn   // numbers
#define F_N32 u8g2_font_logisoso32_tn
#define F_N42 u8g2_font_logisoso42_tn
#define RIGHT_EDGE 300                  // keep clear of the card dots

int tw(const String &s, const uint8_t *f) {
  gfx->setFont(f); gfx->setTextSize(1);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(s.c_str(), 0, 0, &x1, &y1, &w, &h);
  return w;
}
// draw text with its baseline at y
void ft(int x, int y, const String &s, const uint8_t *f, uint16_t c) {
  gfx->setFont(f); gfx->setTextSize(1); gfx->setTextColor(c);
  gfx->setCursor(x, y); gfx->print(s);
}
void ftR(int xr, int y, const String &s, const uint8_t *f, uint16_t c) { ft(xr - tw(s, f), y, s, f, c); }
void ftC(int y, const String &s, const uint8_t *f, uint16_t c, int w = RIGHT_EDGE + 10) { ft(max(0, (w - tw(s, f)) / 2), y, s, f, c); }

// Red "LIVE" pill like YouTube's, centred on cx with its bottom edge at by. Returns its width.
int livePill(int cx, int by, bool big) {
  const uint8_t *f = big ? F_XS2 : F_XS;
  int w = tw("LIVE", f) + (big ? 12 : 8), hgt = big ? 16 : 12;
  int x = cx - w / 2, y = by - hgt;
  gfx->fillRoundRect(x - 1, y - 1, w + 2, hgt + 2, (hgt + 2) / 2, C_BG);   // dark edge so it stands out
  gfx->fillRoundRect(x, y, w, hgt, hgt / 2, C_RED);
  ft(x + (big ? 6 : 4), by - (big ? 4 : 3), "LIVE", f, C_WHITE);
  return w;
}
// Small pill after a name in a list (baseline y); returns space used
int livePillAfter(int x, int baseY) { int w = tw("LIVE", F_XS) + 8; livePill(x + 4 + w / 2, baseY + 1, false); return w + 6; }

// shorten with "..." until it fits maxW pixels
String fit(String s, const uint8_t *f, int maxW) {
  if (tw(s, f) <= maxW) return s;
  while (s.length() > 1 && tw(s + "...", f) > maxW) {
    s.remove(s.length() - 1);
    while (s.length() && ((uint8_t)s[s.length() - 1] & 0xC0) == 0x80) s.remove(s.length() - 1);  // whole UTF-8 chars
  }
  s.trim();
  return s + "...";
}

// pick the biggest number font that fits
const uint8_t *numFont(const String &s, int maxW) {
  if (tw(s, F_N42) <= maxW) return F_N42;
  if (tw(s, F_N32) <= maxW) return F_N32;
  return F_N24;
}

// word-wrap in a font; returns baseline of the line after the last one
int wrapF(const String &msg, int x0, int y, int xMax, const uint8_t *f, uint16_t col, int maxLines, int lh) {
  String line, word, text = msg + " ";
  int lines = 1;
  for (size_t i = 0; i < text.length(); i++) {
    if (text[i] != ' ') { word += text[i]; continue; }
    String trial = line.length() ? line + " " + word : word;
    if (tw(trial, f) > xMax - x0 && line.length()) {
      if (lines >= maxLines) { ft(x0, y, fit(line + " " + word, f, xMax - x0), f, col); return y + lh; }
      ft(x0, y, line, f, col); y += lh; lines++; line = word;
    } else line = trial;
    word = "";
  }
  if (line.length()) { ft(x0, y, fit(line, f, xMax - x0), f, col); y += lh; }
  return y;
}

String nameOf(int i) { String t = ch[i].title.length() ? ch[i].title : ch[i].handle; return asciiOnly(t); }
const char *noun(int i) { return ch[i].tw ? "followers" : "subscribers"; }

// Vertical card position dots on the right edge
void drawCardDots() {
  int x = gfx->width() - 7, y0 = 86 - (NUM_CARDS - 1) * 7;
  for (int k = 0; k < NUM_CARDS; k++)
    if (k == card) gfx->fillCircle(x, y0 + k * 14, 3, C_WHITE);
    else gfx->drawCircle(x, y0 + k * 14, 3, C_DKGREY);
}

// Header for detail cards: channel name left, card name right (gold)
void drawDetailHeader(const char *label) {
  gfx->fillScreen(C_BG);
  int lw = tw(label, F_S);
  ftR(RIGHT_EDGE, 22, label, F_S, C_GOLD);
  ft(8, 22, fit(nameOf(page), F_M, RIGHT_EDGE - lw - 18), F_M, C_WHITE);
  gfx->drawFastHLine(8, 31, RIGHT_EDGE - 8, C_DKGREY);
  drawCardDots();
}

// ── Card 0: main count ──────────────────────────────────────────────────────
void drawMainHeader() {
  gfx->fillRect(0, 0, gfx->width() - 14, 54, C_BG);
  if (!numCh) return;
  if (!drawAvatar(page, 6, 4, true)) {
    gfx->fillRoundRect(8, 12, 40, 28, 8, C_RED);
    gfx->fillTriangle(22, 18, 22, 34, 36, 26, C_WHITE);
  }
  String pos = numCh > 1 ? String(page + 1) + "/" + String(numCh) : String("");
  int pw = pos.length() ? tw(pos, F_S) + 8 : 0;
  ft(58, 24, fit(nameOf(page), F_M, RIGHT_EDGE - 58 - pw), F_M, C_WHITE);
  if (pos.length()) ftR(RIGHT_EDGE, 24, pos, F_S, C_GREY);
  Channel &c = ch[page];
  if (page == 0 && vt.active && vtViews() >= 0) ft(58, 46, "New video: " + compact(vtViews()) + " views", F_S, C_GOLD);
  else if (c.statsOk && c.gainToday != 0) ft(58, 46, signedNum(c.gainToday) + " today", F_S, c.gainToday > 0 ? C_GREEN : C_RED);
  else ft(58, 46, noun(page), F_S, c.tw ? C_TWITCH : C_GREY);
}

void drawMainNumber(long n) {
  gfx->fillRect(0, 54, gfx->width() - 14, 82, C_BG);
  if (!numCh) return;
  Channel &c = ch[page];
  if (c.err.length() && c.subs < 0) { wrapF(c.err, 10, 82, RIGHT_EDGE, F_M, C_RED, 2, 24); return; }
  if (n < 0) { ftC(110, "...", F_N42, C_GREY); return; }
  String s = withCommas(n);
  const uint8_t *f = numFont(s, RIGHT_EDGE - 10);
  ftC(f == F_N42 ? 114 : (f == F_N32 ? 110 : 106), s, f, C_WHITE);
  if (isEstimated(page)) ftR(RIGHT_EDGE, 134, "est.", F_S, C_GREY);
}

void drawChannelDots() {
  gfx->fillRect(0, 138, gfx->width() - 14, 10, C_BG);
  if (numCh < 2) return;
  int gap = 14, x = (RIGHT_EDGE + 10 - (numCh - 1) * gap) / 2;
  for (int i = 0; i < numCh; i++)
    if (i == page) gfx->fillCircle(x + i * gap, 143, 4, C_WHITE);
    else gfx->drawCircle(x + i * gap, 143, 3, C_GREY);
}

// Only shown when something is wrong
// Milestone countdown: shown once a channel is in the last 10% of the way to its next milestone
String countdownText(int i, float &frac) {
  long e = estimateFor(i);
  if (e <= 0 || ch[i].subs < 0) return "";
  long nx = nextMilestone(e), pv = prevMilestone(nx), left = nx - e;
  if (left <= 0 || left > max(10L, (nx - pv) / 10)) return "";
  frac = (float)(e - pv) / (float)(nx - pv);
  return withCommas(left) + " to go to " + compact(nx) + "!";
}

String footerShown = "-";
void drawFooter() {
  if (board || card != 0) { footerShown = "-"; return; }
  String key; uint16_t col = C_GOLD; float frac = -1;
  if (netError.length()) { key = fit(netError, F_S, RIGHT_EDGE - 8); col = C_RED; }
  else if (lastFetchOk && millis() - lastFetchOk > 10UL * 60000UL)
    key = "Not updated for " + String((millis() - lastFetchOk) / 60000UL) + " min";
  else if (numCh) key = countdownText(page, frac);
  String sig = key + String((int)(frac * 200));
  if (sig == footerShown) return;          // unchanged: don't redraw (no flicker)
  footerShown = sig;
  gfx->fillRect(0, 150, gfx->width() - 14, 22, C_BG);
  if (!key.length()) return;
  if (frac >= 0) {                         // countdown: thin progress bar + text
    int bw = RIGHT_EDGE - 8;
    gfx->fillRoundRect(8, 151, bw, 4, 2, C_DKGREY);
    gfx->fillRoundRect(8, 151, max(4, (int)(bw * frac)), 4, 2, C_GOLD);
    ftC(170, key, F_S, C_GOLD);
  } else ft(8, 168, key, F_S, col);
}

void drawMain() {
  gfx->fillScreen(C_BG);
  if (!numCh) { ftC(90, "No channels set", F_M, C_GREY); return; }
  shownSubs = estimateFor(page);
  drawMainHeader();
  drawMainNumber(shownSubs);
  drawChannelDots();
  footerShown = "-";
  drawFooter();
  drawCardDots();
}

// ── Card 1: overview ────────────────────────────────────────────────────────
void drawOverview() {
  Channel &c = ch[page];
  drawDetailHeader("Overview");
  if (!drawAvatar(page, 6, 42, false)) gfx->fillCircle(50, 86, 44, C_DKGREY);
  int x = 104, y = 60;
  if (c.tw) {
    ft(x, y, "Twitch", F_M, C_TWITCH); y += 30;
    String f = c.subs >= 0 ? compact(c.subs) : String("-");
    ft(x, y, f, F_M, C_WHITE); ft(x + tw(f, F_M), y, " followers", F_S, C_GREY); y += 30;
    ft(x, y, c.live ? "Live now" : "Offline", F_M, c.live ? C_RED : C_GREY); y += 30;
    if (c.joined) ft(x, y, "Since " + dateStr(c.joined, "%Y"), F_S, C_GREY);
    return;
  }
  struct { String v; const char *k; } rows[] = {
    { compact(c.views), " views" },
    { c.videos >= 0 ? withCommas(c.videos) : String("-"), " videos" },
    { (c.videos > 0 && c.views >= 0) ? compact(c.views / c.videos) : String("-"), " avg" },
  };
  for (auto &r : rows) {
    ft(x, y, r.v, F_M, C_WHITE);
    ft(x + tw(r.v, F_M), y, r.k, F_S, C_GREY);
    y += 30;
  }
  String joined = c.joined ? "Since " + dateStr(c.joined, "%Y") : "";
  if (c.country.length()) joined += (joined.length() ? "  " : "") + c.country;
  ft(x, y, joined, F_S, C_GREY);
}

// ── Card 2: latest video ────────────────────────────────────────────────────
void drawLatestVideo() {
  Channel &c = ch[page];
  drawDetailHeader(c.live ? "LIVE" : (c.tw ? "Stream" : "Latest video"));
  if (c.tw) {
    if (!c.live) { ftC(92, "Not live right now", F_M, C_GREY); ftC(120, "twitch.tv/" + c.twLogin, F_S, C_TWITCH); return; }
    gfx->fillRoundRect(8, 38, 58, 24, 5, C_RED);
    ft(14, 56, "LIVE", F_M, C_WHITE);
    if (c.liveViewers >= 0) ft(74, 56, compact(c.liveViewers) + " watching", F_M, C_WHITE);
    int y = wrapF(asciiOnly(c.vidTitle), 8, 88, RIGHT_EDGE, F_S, C_WHITE, 2, 20);
    if (c.twGame.length()) ft(8, y + 2, fit(asciiOnly(c.twGame), F_S, RIGHT_EDGE - 8), F_S, C_TWITCH);
    if (c.vidPublished) ft(8, 166, "Started " + ago(c.vidPublished), F_S, C_GREY);
    return;
  }
  if (!c.vidId.length()) { ftC(100, c.uploads.length() ? "Loading..." : "No videos", F_M, C_GREY); return; }
  int y = 56;
  if (c.live) {
    gfx->fillRoundRect(8, 38, 58, 24, 5, C_RED);
    ft(14, 56, "LIVE", F_M, C_WHITE);
    if (c.liveViewers >= 0) ft(74, 56, compact(c.liveViewers) + " watching", F_M, C_WHITE);
    y = 84;
    y = wrapF(asciiOnly(c.vidTitle), 8, y, RIGHT_EDGE, F_S, C_WHITE, 1, 20);
  } else {
    y = wrapF(asciiOnly(c.vidTitle), 8, y, RIGHT_EDGE, F_M, C_WHITE, 2, 24);
    String meta = ago(c.vidPublished);
    if (c.vidDuration > 0) meta += (meta.length() ? "  -  " : "") + durationStr(c.vidDuration);
    ft(8, y - 2, meta, F_S, C_GREY);
    y += 24;
  }
  // views big, likes/comments under it
  String v = compact(c.vidViews);
  ft(8, y + 4, v, F_M, C_WHITE);
  ft(8 + tw(v, F_M), y + 4, " views", F_S, C_GREY);
  String lc = (c.vidLikes >= 0 ? compact(c.vidLikes) + " likes" : String("likes hidden")) +
              "   " + (c.vidComments >= 0 ? compact(c.vidComments) + " comments" : String(""));
  ft(8, min(y + 28, 168), fit(lc, F_S, RIGHT_EDGE - 8), F_S, C_GREEN);
}

// ── Comments card: newest 3 on the latest video ────────────────────────────
bool needData(Channel &c);
String shortAgo(time_t t) {
  if (!timeValid() || !t) return "";
  long d = max(0L, (long)(nowT() - t));
  if (d < 3600) return String(max(1L, d / 60)) + "m";
  if (d < 86400) return String(d / 3600) + "h";
  return String(d / 86400) + "d";
}
void drawComments() {
  Channel &c = ch[page];
  drawDetailHeader("Comments");
  if (c.tw) { ftC(92, "Not available", F_M, C_GREY); ftC(118, "for Twitch channels", F_S, C_GREY); return; }
  if (!c.vidId.length()) { ftC(100, "No videos", F_M, C_GREY); return; }
  if (c.cmOff) { ftC(92, "Comments are off", F_M, C_GREY); ftC(118, "on the latest video", F_S, C_GREY); return; }
  if (!c.cmN) { ftC(92, lastCommentFetch ? "No comments yet" : "Loading...", F_M, C_GREY); ftC(118, "on the latest video", F_S, C_GREY); return; }
  int y = 52;
  for (int k = 0; k < c.cmN; k++) {
    String when = shortAgo(c.cmT[k]);
    if (c.cmLikes[k] > 0) when += "  " + compact(c.cmLikes[k]) + " likes";
    int ww = tw(when, F_S);
    ft(8, y, fit(asciiOnly(c.cmAuthor[k]), F_S, RIGHT_EDGE - ww - 20), F_S, C_GOLD);
    ftR(RIGHT_EDGE, y, when, F_S, C_DKGREY);
    ft(8, y + 19, fit(asciiOnly(c.cmText[k]), F_S, RIGHT_EDGE - 8), F_S, C_WHITE);
    y += 44;
  }
}

// ── Top videos card: the last 10 uploads ranked by views per day ────────────
float perDay(const Channel::Recent &r);
void drawTopVideos() {
  Channel &c = ch[page];
  drawDetailHeader("Top videos");
  if (c.tw) { ftC(92, "Not available", F_M, C_GREY); ftC(118, "for Twitch channels", F_S, C_GREY); return; }
  if (!c.rvN) { ftC(92, lastRecentFetch ? "No videos yet" : "Loading...", F_M, C_GREY); ftC(118, "Checked every 6 hours", F_S, C_GREY); return; }
  int idx[RECENT_N]; for (int k = 0; k < c.rvN; k++) idx[k] = k;
  for (int a = 0; a < c.rvN; a++) for (int b = a + 1; b < c.rvN; b++)
    if (perDay(c.rv[idx[b]]) > perDay(c.rv[idx[a]])) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  ft(8, 48, "Last " + String(c.rvN) + " uploads, by views per day", F_XS2, C_GREY);
  int y = 72;
  for (int r = 0; r < min(4, c.rvN); r++) {
    Channel::Recent &v = c.rv[idx[r]];
    String pd = compact(lroundf(perDay(v))) + "/day";
    int pw = tw(pd, F_S);
    ft(8, y, String(r + 1) + ".", F_S, C_GOLD);
    ft(26, y, fit(asciiOnly(v.title), F_S, RIGHT_EDGE - 26 - pw - 10), F_S, C_WHITE);
    ftR(RIGHT_EDGE, y, pd, F_S, C_GREEN);
    y += 26;
  }
}

// ── Records card: personal bests ───────────────────────────────────────────
void drawRecords() {
  Channel &c = ch[page];
  drawDetailHeader("Records");
  if (!c.pbLoaded) pbLoad(page);
  if (!c.pbDay && !c.pbWeek && !c.pbVid) {
    if (!needData(c)) { ftC(92, "No records yet", F_M, C_GREY); ftC(118, "Your best day and week appear here", F_S, C_GREY); }
    return;
  }
  struct { const char *k; long v; uint32_t t; } r[] = {
    { "Best day", c.pbDay, c.pbDayT }, { "Best week", c.pbWeek, c.pbWeekT }, { "Best video, 1st day", c.pbVid, c.pbVidT } };
  int y = 60;
  for (int k = 0; k < 3; k++) {
    if (k == 2 && (page != 0 || !c.pbVid)) continue;
    if (!r[k].v) continue;
    String v = k == 2 ? compact(r[k].v) + " views" : signedNum(r[k].v);
    ft(8, y, r[k].k, F_S, C_GREY);
    if (r[k].t) ft(12 + tw(r[k].k, F_S), y, dateStr(r[k].t, "%d %b"), F_S, C_DKGREY);
    ftR(RIGHT_EDGE, y, v, F_M, C_GOLD);
    y += 32;
  }
  if (c.statsOk && c.pbDay > 0) {
    long pct = c.gainToday * 100 / c.pbDay;
    String t = "Today " + signedNum(c.gainToday) + (c.gainToday > 0 ? "  (" + String(pct) + "% of best)" : String(""));
    ftC(162, t, F_S, c.gainToday >= c.pbDay ? C_GREEN : C_WHITE);
  }
}

// ── Card 3: growth numbers ──────────────────────────────────────────────────
bool needData(Channel &c) {
  if (c.statsOk && c.histStart) return false;
  ftC(84, "Collecting data...", F_M, C_GREY);
  ftC(112, "Recorded every hour.", F_S, C_GREY);
  ftC(134, "Check back later today.", F_S, C_GREY);
  return true;
}

void drawGrowth() {
  Channel &c = ch[page];
  drawDetailHeader("Growth");
  if (needData(c)) return;
  struct { const char *k; long v; float span; float full; } g[] = {
    { "Today", c.gainToday, 1, 1 }, { "7 days", c.gain7, c.span7Days, 7 }, { "30 days", c.gain30, c.span30Days, 30 } };
  int y = 60;
  for (auto &r : g) {
    String k = r.k;
    if (r.full > 1 && r.span > 0 && r.span < r.full - 0.5f) k += " (" + String(r.span, r.span < 10 ? 1 : 0) + "d)";
    ft(8, y, k, F_M, C_GREY);
    ftR(RIGHT_EDGE, y, signedNum(r.v), F_M, r.v > 0 ? C_GREEN : (r.v < 0 ? C_RED : C_WHITE));
    y += 30;
  }
  if (c.ratePerDay != 0) ftC(160, "About " + signedNum(lroundf(c.ratePerDay)) + " a day", F_S, C_WHITE);
}

// ── Card 4: 7-day graph ─────────────────────────────────────────────────────
void drawGraph() {
  Channel &c = ch[page];
  drawDetailHeader("7 days");
  if (needData(c)) return;
  int gx = 8, gy = 38, gw = RIGHT_EDGE - 8, gh = 108;
  File f = LittleFS.open(histPath(page), "r");
  time_t now = nowT(), from = now - 7L * 86400;
  long mn = c.subs, mx = c.subs;
  Sample s;
  if (f) {
    while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s))
      if ((time_t)s.t >= from) { mn = min(mn, (long)s.s); mx = max(mx, (long)s.s); }
  }
  if (mx == mn) { mx += 1; mn -= 1; }
  for (int k = 1; k < 7; k++) gfx->drawFastVLine(gx + k * gw / 7, gy, gh, 0x18E3);  // day lines
  gfx->drawRect(gx, gy, gw, gh, C_DKGREY);
  if (f) {
    f.seek(0);
    int px = -1, py = -1;
    while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
      if ((time_t)s.t < from) continue;
      int x = gx + 1 + (int)((float)(s.t - from) / (7L * 86400) * (gw - 3));
      int y = gy + gh - 3 - (int)((float)(s.s - mn) / (mx - mn) * (gh - 6));
      if (px >= 0) { gfx->drawLine(px, py, x, y, C_GREEN); gfx->drawLine(px, py + 1, x, y + 1, C_GREEN); }
      px = x; py = y;
    }
    f.close();
  }
  ft(8, 168, compact(mn), F_S, C_GREY);
  ftR(RIGHT_EDGE, 168, compact(mx), F_S, C_WHITE);
  ftC(168, "low  -  high", F_S, C_DKGREY);
}

// ── Card 5: next milestone ──────────────────────────────────────────────────
long nextMilestone(long n) {
  long p = 10;
  while (true) {
    if (n < p) return p;
    if (n < 2 * p) return 2 * p;
    if (n < 5 * p) return 5 * p;
    p *= 10;
  }
}
long prevMilestone(long next) {
  long p = 1; while (p * 10 <= next) p *= 10;
  long lead = next / p;
  if (lead == 1) return p / 2 >= 1 ? p / 2 : 0;
  if (lead == 2) return p;
  return 2 * p;
}

void drawMilestone() {
  Channel &c = ch[page];
  drawDetailHeader("Next milestone");
  if (c.subs < 0) { ftC(100, "...", F_M, C_GREY); return; }
  long cur = estimateFor(page);
  long goal = nextMilestone(cur), prev = prevMilestone(goal);
  String g = withCommas(goal);
  ftC(76, g, numFont(g, RIGHT_EDGE - 10) == F_N42 ? F_N32 : F_N24, C_GOLD);
  int bx = 10, by = 88, bw = RIGHT_EDGE - 20, bh = 16;
  float frac = constrain((float)(cur - prev) / (float)(goal - prev), 0.0f, 1.0f);
  gfx->drawRoundRect(bx, by, bw, bh, 7, C_DKGREY);
  gfx->fillRoundRect(bx + 2, by + 2, max(8, (int)((bw - 4) * frac)), bh - 4, 5, C_GOLD);
  ftC(132, withCommas(goal - cur) + " to go", F_M, C_WHITE);
  String eta;
  if (c.ratePerDay > 0.01f && timeValid()) {
    float days = (goal - cur) / c.ratePerDay;
    if (days < 1) eta = "Expected today";
    else if (days > 3650) eta = "10+ years away";
    else eta = "Around " + dateStr(nowT() + (time_t)(days * 86400), days > 300 ? "%b %Y" : "%d %b");
  } else eta = c.ratePerDay < 0 ? "Losing subscribers" : "Date: need more data";
  ftC(162, eta, F_S, C_GREY);
}

// ── Leaderboard (5 rows per page; swipe left/right for more) ────────────────
int boardPage = 0;
#define BOARD_ROWS 5
void drawBoard() {
  gfx->fillScreen(C_BG);
  int pages = (numCh + BOARD_ROWS - 1) / BOARD_ROWS;
  if (boardPage >= pages) boardPage = 0;
  ft(8, 22, "Leaderboard", F_M, C_GOLD);
  if (pages > 1) ftR(gfx->width() - 8, 22, String(boardPage + 1) + "/" + String(pages), F_S, C_GREY);
  gfx->drawFastHLine(8, 31, gfx->width() - 16, C_DKGREY);
  int idx[MAX_CH]; for (int i = 0; i < numCh; i++) idx[i] = i;
  for (int a = 0; a < numCh; a++) for (int b = a + 1; b < numCh; b++)
    if (ch[idx[b]].subs > ch[idx[a]].subs) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  int y = 56;
  for (int r = boardPage * BOARD_ROWS; r < min(numCh, (boardPage + 1) * BOARD_ROWS); r++) {
    int i = idx[r];
    uint16_t col = (i == 0) ? C_GOLD : C_WHITE;   // first channel in your list = yours
    String num = String(r + 1);
    String subs = ch[i].subs >= 0 ? compact(ch[i].subs) : String("-");
    int sw = tw(subs, F_M);
    ft(8, y, num, F_S, C_GREY);
    int lw = ch[i].live ? tw("LIVE", F_XS) + 14 : 0;
    String nm = fit(nameOf(i), F_M, gfx->width() - 8 - sw - 40 - lw);
    ft(30, y, nm, F_M, col);
    if (ch[i].live) livePillAfter(30 + tw(nm, F_M), y - 3);
    ftR(gfx->width() - 8, y, subs, F_M, col);
    y += 26;
  }
  if (numCh < BOARD_ROWS) ftC(168, WiFi.localIP().toString(), F_S, C_DKGREY, gfx->width());
}

void drawView() {
  if (!numCh) { drawMain(); return; }
  if (board) { if (raceView) drawRace(); else drawBoard(); return; }
  switch (card) {
    case 1: drawOverview(); break;
    case 2: if (page == 0 && vt.active) drawVideoTracker(); else drawLatestVideo(); break;
    case 3: drawComments(); break;
    case 4: drawGrowth(); break;
    case 5: drawRecords(); break;
    case 6: drawTopVideos(); break;
    case 7: drawGraph(); break;
    case 8: drawMilestone(); break;
    default: drawMain(); break;
  }
}

void wipe(int dx, int dy) {
  int w = gfx->width(), h = gfx->height();
  for (int k = 0; k < 4; k++) {
    if (dx > 0) gfx->fillRect(w - (k + 1) * w / 4, 0, w / 4, h, C_BG);
    else if (dx < 0) gfx->fillRect(k * w / 4, 0, w / 4, h, C_BG);
    else if (dy > 0) gfx->fillRect(0, h - (k + 1) * h / 4, w, h / 4 + 1, C_BG);
    else gfx->fillRect(0, k * h / 4, w, h / 4 + 1, C_BG);
  }
}

void changePage(int dir) {
  if (board && raceView) return;
  if (board) {          // on the leaderboard, left/right pages through it
    int pages = (numCh + BOARD_ROWS - 1) / BOARD_ROWS;
    if (pages < 2) return;
    boardPage = (boardPage + dir + pages) % pages;
    wipe(dir, 0); drawBoard(); return;
  }
  if (numCh < 2) return;
  page = (page + dir + numCh) % numCh;
  wipe(dir, 0);
  drawView();
}

void changeCard(int dir) {   // dir +1 = deeper (swipe up)
  if (board) {
    if (raceView) { if (dir > 0) { raceView = false; wipe(0, 1); drawView(); } return; }       // race -> leaderboard
    if (dir > 0) { board = false; card = 0; wipe(0, 1); drawView(); }                         // leaderboard -> main
    else { raceView = true; wipe(0, -1); drawView(); }                                        // leaderboard -> race
    return;
  }
  if (dir < 0 && card == 0) { board = true; boardPage = 0; wipe(0, -1); drawView(); return; }
  int nc = constrain(card + dir, 0, NUM_CARDS - 1);
  if (nc == card) return;
  card = nc;
  wipe(0, dir);
  drawView();
}
