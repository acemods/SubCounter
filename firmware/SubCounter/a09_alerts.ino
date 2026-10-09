// ════════════════════════════════════════════════════════════════════════════
//  Celebrations, race, daily summary, night clock, tall leaderboard
// ════════════════════════════════════════════════════════════════════════════
// Bigger milestones get bigger parties: 1 = under 1K ... 4 = a million or more
int tierFor(long milestone) {
  if (milestone >= 1000000) return 4;
  if (milestone >= 100000) return 3;
  if (milestone >= 1000) return 2;
  return 1;
}

void confetti(int tier) {
  const uint16_t cols[] = { C_GOLD, C_RED, C_GREEN, C_BLUE, C_PURPLE, C_WHITE, 0xFD20, 0x07FF };
  const int MAXP = 120;
  struct P { float x, y, vx, vy; uint16_t c; uint8_t s; } p[MAXP];
  int n = 30 * tier; if (n > MAXP) n = MAXP;
  int W = gfx->width(), H = gfx->height();
  gfx->fillScreen(C_BG);
  for (int i = 0; i < n; i++) {
    bool burst = tier >= 3;                  // big ones explode from the middle, small ones rain down
    p[i].x = burst ? W / 2 + random(-20, 20) : random(0, W);
    p[i].y = burst ? H / 2 + random(-10, 10) : -random(0, H);
    float a = random(0, 628) / 100.0f, sp = random(20, 70) / 10.0f;
    p[i].vx = burst ? cosf(a) * sp : random(-10, 10) / 10.0f;
    p[i].vy = burst ? sinf(a) * sp - 2 : random(15, 40) / 10.0f;
    p[i].c = cols[random(0, 8)];
    p[i].s = random(2, 2 + tier + 1);
  }
  unsigned long until = millis() + 1500 + 700UL * tier;
  while (millis() < until) {
    for (int i = 0; i < n; i++) {
      gfx->fillRect((int)p[i].x, (int)p[i].y, p[i].s, p[i].s, C_BG);
      p[i].vy += 0.12f;
      p[i].x += p[i].vx; p[i].y += p[i].vy;
      if (p[i].y > H && tier < 3) { p[i].y = -5; p[i].x = random(0, W); p[i].vy = random(15, 40) / 10.0f; }
      gfx->fillRect((int)p[i].x, (int)p[i].y, p[i].s, p[i].s, p[i].c);
    }
    server.handleClient();
    delay(25);
  }
}

void sprinkle(int count) {
  const uint16_t cols[] = { C_GOLD, C_RED, C_GREEN, C_BLUE, C_PURPLE };
  for (int i = 0; i < count; i++) {
    int x = random(0, gfx->width()), y = random(0, gfx->height());
    if (y > 30 && y < 140 && x > 20 && x < gfx->width() - 20) continue;   // keep the middle clear
    gfx->fillRect(x, y, 3, 3, cols[random(0, 5)]);
  }
}

void waitShowing(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) { server.handleClient(); delay(20); }
}


void showAlert(const Alert &a) {
  if (a.type == A_MILESTONE) {
    int tier = tierFor(a.total);
    if (cfgCelebrate) confetti(tier);
    gfx->fillScreen(C_BG);
    if (cfgCelebrate) sprinkle(30 * tier);
    drawAvatar(a.idx, 6, 4, true);
    ft(58, 24, tier >= 4 ? "HUGE MILESTONE!" : "MILESTONE!", F_M, C_GOLD);
    ft(58, 46, fit(nameOf(a.idx), F_S, gfx->width() - 66), F_S, C_WHITE);
    String m = withCommas(a.total);
    ftC(110, m, numFont(m, gfx->width() - 20), C_GOLD, gfx->width());
    ftC(150, noun(a.idx), F_M, C_WHITE, gfx->width());
    waitShowing(ALERT_MS + 1000UL * tier);
    return;
  }
  if (a.type == A_RECORD) {
    for (int i = 0; i < 4; i++) { gfx->fillScreen(i % 2 ? C_BG : C_GREEN); delay(110); }
    gfx->fillScreen(C_BG);
    if (cfgCelebrate) sprinkle(60);
    drawAvatar(a.idx, 6, 4, true);
    ft(58, 24, "NEW RECORD!", F_M, C_GOLD);
    const char *what[] = { "Best day ever", "Best week ever", "Best video ever (1st day)" };
    ft(58, 46, what[constrain(a.idx2, 0, 2)], F_S, C_WHITE);
    String v = a.idx2 == 2 ? compact(a.total) : signedNum(a.total);
    ftC(112, v, numFont(v, gfx->width() - 20), C_GREEN, gfx->width());
    ftC(158, "previous best " + (a.idx2 == 2 ? compact(a.delta) : signedNum(a.delta)), F_S, C_GREY, gfx->width());
    waitShowing(ALERT_MS + 1000);
    return;
  }
  if (a.type == A_LIVE) {
    for (int i = 0; i < 4; i++) { gfx->fillScreen(i % 2 ? C_BG : C_RED); delay(120); }
    gfx->fillScreen(C_BG);
    Channel &c = ch[a.idx];
    if (!drawAvatar(a.idx, 6, 42, false)) gfx->fillCircle(50, 86, 44, C_DKGREY);
    int x = 106, w = gfx->width() - x - 8;
    livePill(x + 30, 40, true);
    ft(x, 66, fit(nameOf(a.idx), F_M, w), F_M, C_WHITE);
    ft(x, 88, "is live now", F_S, C_RED);
    wrapF(asciiOnly(c.vidTitle), x, 114, x + w, F_S, C_GREY, 2, 20);
    if (c.liveViewers >= 0) ft(x, 162, compact(c.liveViewers) + " watching", F_S, C_WHITE);
    waitShowing(ALERT_MS + 2000);
    jumpToCh = a.idx;                  // switch to the live channel afterwards
    return;
  }
  for (int i = 0; i < 4; i++) { gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD); delay(100); }
  gfx->fillScreen(C_BG);
  if (a.type == A_VIEWS) {
    ftC(30, "YOUR VIDEO HIT", F_M, C_GOLD, gfx->width());
    String m = withCommas(a.total);
    ftC(96, m, numFont(m, gfx->width() - 20), C_WHITE, gfx->width());
    ftC(126, "views!", F_M, C_GREEN, gfx->width());
    ftC(160, fit(asciiOnly(ch[0].vidTitle), F_S, gfx->width() - 16), F_S, C_GREY, gfx->width());
  } else if (a.type == A_OVERTAKE) {
    ftC(30, "OVERTAKE!", F_M, C_GOLD, gfx->width());
    ftC(70, fit(nameOf(a.idx), F_M, gfx->width() - 20), F_M, C_GREEN, gfx->width());
    ftC(98, "just passed", F_S, C_GREY, gfx->width());
    ftC(130, fit(nameOf(a.idx2), F_M, gfx->width() - 20), F_M, C_WHITE, gfx->width());
    ftC(160, withCommas(ch[a.idx].subs) + " vs " + withCommas(ch[a.idx2].subs), F_S, C_GREY, gfx->width());
  } else {
    drawAvatar(a.idx, 6, 4, true);
    ft(58, 24, ch[a.idx].tw ? "New followers!" : "New subscribers!", F_M, C_GOLD);
    ft(58, 46, fit(nameOf(a.idx), F_S, gfx->width() - 66), F_S, C_WHITE);
    String d = "+" + withCommas(a.delta);
    ftC(112, d, numFont(d, gfx->width() - 20), C_GREEN, gfx->width());
    ftC(158, "now " + withCommas(a.total), F_M, C_GREY, gfx->width());
  }
  waitShowing(ALERT_MS);
}

// ── Phone notifications (ntfy) ──────────────────────────────────────────────
String chLink(int i) {
  Channel &c = ch[i];
  if (c.tw) return "https://www.twitch.tv/" + c.twLogin;
  return c.id.length() ? "https://www.youtube.com/channel/" + c.id : String("");
}
void notifyAlert(const Alert &a) {
  String nm = ch[a.idx].title.length() ? ch[a.idx].title : ch[a.idx].handle;
  switch (a.type) {
    case A_MILESTONE:
      if (cfgNtfyMask & 1) queueNote(nm + " hit " + withCommas(a.total) + " " + noun(a.idx) + "!", "Milestone reached on " + nm + ".", "tada", chLink(a.idx));
      break;
    case A_GAIN:
      if ((cfgNtfyMask & 32) && a.idx == 0) queueNote(nm + " " + signedNum(a.delta), "Now " + withCommas(a.total) + " " + noun(a.idx) + ".", "chart_with_upwards_trend", chLink(a.idx));
      break;
    case A_LIVE: {
      Channel &c = ch[a.idx];
      String url = c.tw ? "https://www.twitch.tv/" + c.twLogin : (c.vidId.length() ? "https://youtu.be/" + c.vidId : chLink(a.idx));
      if (cfgNtfyMask & 2) queueNote(nm + " is live", c.vidTitle + (a.total >= 0 ? "\n" + withCommas(a.total) + " watching" : String("")), "red_circle", url);
      break; }
    case A_RECORD: {
      const char *what[] = { "Best day ever", "Best week ever", "Best video ever (first day)" };
      int k = constrain(a.idx2, 0, 2);
      String v = k == 2 ? withCommas(a.total) + " views" : signedNum(a.total);
      String p = k == 2 ? withCommas(a.delta) + " views" : signedNum(a.delta);
      if (cfgNtfyMask & 4) queueNote("New record: " + String(what[k]), nm + ": " + v + " (previous best " + p + ")", "trophy", chLink(a.idx));
      break; }
    case A_OVERTAKE: {
      String other = ch[a.idx2].title.length() ? ch[a.idx2].title : ch[a.idx2].handle;
      if (cfgNtfyMask & 8) queueNote(nm + " overtook " + other, withCommas(ch[a.idx].subs) + " vs " + withCommas(ch[a.idx2].subs), "checkered_flag", chLink(a.idx));
      break; }
    case A_VIEWS:
      if (cfgNtfyMask & 16) queueNote("Your video hit " + withCommas(a.total) + " views", ch[0].vidTitle, "eyes", ch[0].vidId.length() ? "https://youtu.be/" + ch[0].vidId : String(""));
      break;
  }
}
// Network task: send whatever is queued
void sendNotes() {
  while (noteN > 0 && cfgNtfy.length()) {
    Note n = noteQ[0];
    for (int k = 1; k < noteN; k++) noteQ[k - 1] = noteQ[k];
    noteN--;
    String url = cfgNtfyServer + "/" + cfgNtfy;
    int code;
    { NetIO io;
      WiFiClientSecure client; client.setInsecure();
      HTTPClient http; http.setTimeout(8000);
      if (!http.begin(client, url)) continue;
      http.addHeader("Title", asciiOnly(n.title));     // headers must be plain text; the message itself can be anything
      http.addHeader("Tags", n.tags);
      if (n.click.length()) http.addHeader("Click", n.click);
      code = http.POST(n.msg.length() ? n.msg : n.title);
      http.end(); }
    Serial.printf("ntfy %d\n", code);
  }
}

// ── Subscriber race ─────────────────────────────────────────────────────────
// Small YouTube / Twitch logo (top-left at x,y; about 17x12)
void platformIcon(int x, int y, bool twitch) {
  if (!twitch) {
    gfx->fillRoundRect(x, y, 17, 12, 3, C_RED);
    gfx->fillTriangle(x + 6, y + 3, x + 6, y + 9, x + 12, y + 6, C_WHITE);
  } else {
    gfx->fillRect(x + 1, y, 14, 10, C_TWITCH);
    gfx->fillTriangle(x + 15, y + 6, x + 15, y + 10, x + 11, y + 10, C_BG);   // cut corner
    gfx->fillTriangle(x + 4, y + 10, x + 8, y + 10, x + 4, y + 13, C_TWITCH); // speech tail
    gfx->fillRect(x + 6, y + 3, 2, 4, C_WHITE); gfx->fillRect(x + 10, y + 3, 2, 4, C_WHITE);
  }
}
bool raceSet() { return cfgRaceA >= 0 && cfgRaceB >= 0 && cfgRaceA < numCh && cfgRaceB < numCh && cfgRaceA != cfgRaceB; }

// Days until the chaser catches the leader (0 = not catching up)
float raceDays(int lead, int chase, float &closing) {
  closing = ch[chase].ratePerDay - ch[lead].ratePerDay;
  long gap = ch[lead].subs - ch[chase].subs;
  if (closing <= 0.01f || gap <= 0) return 0;
  return gap / closing;
}

void drawRace() {
  gfx->fillScreen(C_BG);
  ft(8, 22, "Race", F_M, C_GOLD);
  gfx->drawFastHLine(8, 31, gfx->width() - 16, C_DKGREY);
  if (!raceSet()) {
    ftC(84, "No race set up", F_M, C_GREY, gfx->width());
    ftC(116, "Pick two channels on", F_S, C_GREY, gfx->width());
    ftC(138, "the settings page", F_S, C_GREY, gfx->width());
    return;
  }
  int A = cfgRaceA, B = cfgRaceB;
  long sa = max(0L, estimateFor(A)), sb = max(0L, estimateFor(B));
  String ca = compact(sa), cb = compact(sb);
  platformIcon(8, 45, ch[A].tw);
  ft(30, 56, fit(nameOf(A), F_S, gfx->width() - tw(ca, F_M) - 48), F_S, C_RED);
  ftR(gfx->width() - 8, 56, ca, F_M, C_WHITE);
  platformIcon(8, 71, ch[B].tw);
  ft(30, 82, fit(nameOf(B), F_S, gfx->width() - tw(cb, F_M) - 48), F_S, C_BLUE);
  ftR(gfx->width() - 8, 82, cb, F_M, C_WHITE);
  // tug-of-war bar
  int bx = 8, by = 92, bw = gfx->width() - 16, bh = 14;
  float fa = (sa + sb) ? (float)sa / (sa + sb) : 0.5f;
  int wa = (int)(bw * fa);
  gfx->fillRect(bx, by, wa, bh, C_RED);
  gfx->fillRect(bx + wa, by, bw - wa, bh, C_BLUE);
  gfx->drawFastVLine(bx + bw / 2, by - 3, bh + 6, C_WHITE);
  int lead = sa >= sb ? A : B, chase = lead == A ? B : A;
  ftC(132, "Gap " + withCommas(labs(sa - sb)), F_M, C_WHITE, gfx->width());
  float closing;
  float days = raceDays(lead, chase, closing);
  String t;
  if (days > 0) t = "Catching up " + withCommas(lroundf(closing)) + "/day - pass in ~" + (days < 1 ? String("<1") : String((long)days)) + "d";
  else if (ch[A].statsOk && ch[B].statsOk && (ch[A].ratePerDay || ch[B].ratePerDay)) t = fit(nameOf(lead), F_S, 140) + " pulling away";
  else t = "Trend: need more data";
  ftC(160, fit(t, F_S, gfx->width() - 10), F_S, C_GREY, gfx->width());
}

// ── Daily summary (9 am) ────────────────────────────────────────────────────
// 1st of the month: a recap of last month instead of the usual summary
bool recapDue() {
  if (!timeValid()) return false;
  time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
  if (lt.tm_mday != 1) return false;
  for (int i = 0; i < numCh; i++) if (ch[i].lastMonthOk) return true;
  return false;
}
void drawRecap() {
  gfx->fillScreen(C_BG);
  time_t m1 = monthStart(1);
  ft(8, 22, dateStr(m1, "%B") + " recap", F_M, C_GOLD);
  ftR(gfx->width() - 8, 22, dateStr(m1, "%Y"), F_S, C_GREY);
  gfx->drawFastHLine(8, 31, gfx->width() - 16, C_DKGREY);
  int idx[MAX_CH]; int n = 0;
  for (int i = 0; i < numCh; i++) if (ch[i].lastMonthOk) idx[n++] = i;
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
    if (ch[idx[b]].gainLastMonth > ch[idx[a]].gainLastMonth) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  int rank = -1; for (int r = 0; r < n; r++) if (idx[r] == 0) rank = r;
  if (rank >= 0) {
    ft(8, 54, fit(nameOf(0), F_S, gfx->width() - 16), F_S, C_WHITE);
    String g = signedNum(ch[0].gainLastMonth);
    ftC(98, g, F_N42, ch[0].gainLastMonth > 0 ? C_GREEN : C_WHITE, gfx->width());
    ftC(122, noun(0), F_S, C_GREY, gfx->width());
    ftC(146, rank == 0 && n > 1 ? String("You grew the most!") : "#" + String(rank + 1) + " of " + String(n) + " for growth",
        F_M, rank == 0 ? C_GOLD : C_WHITE, gfx->width());
  }
  if (n && idx[0] != 0) ftC(168, fit("Top: " + nameOf(idx[0]) + " " + signedNum(ch[idx[0]].gainLastMonth), F_S, gfx->width() - 16), F_S, C_GREY, gfx->width());
  else if (rank < 0) ftC(100, "Not enough data for you yet", F_S, C_GREY, gfx->width());
}

void drawSummary() {
  if (recapDue()) { drawRecap(); return; }
  gfx->fillScreen(C_BG);
  int hr = 9; if (timeValid()) { time_t n = nowT(); struct tm lt; localtime_r(&n, &lt); hr = lt.tm_hour; }
  ft(8, 22, hr < 12 ? "Good morning!" : hr < 18 ? "Good afternoon!" : "Good evening!", F_M, C_GOLD);
  if (timeValid()) ftR(gfx->width() - 8, 22, dateStr(nowT(), "%a %d %b"), F_S, C_GREY);
  gfx->drawFastHLine(8, 31, gfx->width() - 16, C_DKGREY);
  int idx[MAX_CH]; int n = 0;
  for (int i = 0; i < numCh; i++) if (ch[i].statsOk) idx[n++] = i;
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
    if (ch[idx[b]].gain24 > ch[idx[a]].gain24) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  ft(8, 52, "Biggest growth since yesterday", F_S, C_GREY);
  int y = 76;
  if (!n) { ft(8, y, "Still collecting data...", F_M, C_GREY); y += 26; }
  for (int r = 0; r < min(n, 3); r++) {
    int i = idx[r];
    String g = signedNum(ch[i].gain24);
    ft(8, y, fit(nameOf(i), F_M, gfx->width() - tw(g, F_M) - 26), F_M, i == 0 ? C_GOLD : C_WHITE);
    ftR(gfx->width() - 8, y, g, F_M, ch[i].gain24 > 0 ? C_GREEN : C_GREY);
    y += 25;
  }
  int newVids = 0, newest = -1;
  for (int i = 0; i < numCh; i++)
    if (ch[i].vidPublished && timeValid() && nowT() - ch[i].vidPublished < 86400) {
      newVids++;
      if (newest < 0 || ch[i].vidPublished > ch[newest].vidPublished) newest = i;
    }
  String v = newVids == 0 ? String("No new videos in the last day") :
             String(newVids) + (newVids == 1 ? " new video, from " : " new videos, latest ") + nameOf(newest);
  ft(8, 166, fit(v, F_S, gfx->width() - 16), F_S, newVids ? C_BLUE : C_GREY);
}

// Save today's summary (at the summary time) so the dashboard can show it all day
void captureMorning() {
  JsonDocument d;
  d["at"] = (long)nowT();
  int idx[MAX_CH]; int n = 0;
  for (int i = 0; i < numCh; i++) if (ch[i].statsOk) idx[n++] = i;
  for (int a = 0; a < n; a++) for (int b = a + 1; b < n; b++)
    if (ch[idx[b]].gain24 > ch[idx[a]].gain24) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  JsonArray rows = d["rows"].to<JsonArray>();
  for (int r = 0; r < n; r++) {
    int i = idx[r]; JsonObject o = rows.add<JsonObject>();
    o["n"] = nameOf(i); o["tw"] = ch[i].tw; o["g"] = ch[i].gain24; o["s"] = ch[i].subs; o["me"] = (i == 0);
  }
  JsonArray vids = d["vids"].to<JsonArray>();
  for (int i = 0; i < numCh; i++)
    if (!ch[i].tw && ch[i].vidPublished && nowT() - ch[i].vidPublished < 86400) {
      JsonObject o = vids.add<JsonObject>(); o["n"] = nameOf(i); o["t"] = ch[i].vidTitle; o["id"] = ch[i].vidId; o["pub"] = (long)ch[i].vidPublished;
    }
  morningJson = ""; serializeJson(d, morningJson);
  if (fsOk) { File f = LittleFS.open("/summary.json", "w"); if (f) { f.print(morningJson); f.close(); } }
}
void loadMorning() {
  if (!fsOk || !LittleFS.exists("/summary.json")) return;
  File f = LittleFS.open("/summary.json", "r"); if (!f) return;
  morningJson = f.readString(); f.close();
}

// ── Night clock ─────────────────────────────────────────────────────────────
bool isNight() {
  if (cfgNightStart < 0 || cfgNightEnd < 0 || !timeValid()) return false;
  time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
  int h = lt.tm_hour;
  if (cfgNightStart == cfgNightEnd) return false;
  return cfgNightStart < cfgNightEnd ? (h >= cfgNightStart && h < cfgNightEnd)
                                     : (h >= cfgNightStart || h < cfgNightEnd);
}

void drawAmbient() {
  gfx->fillScreen(C_BG);
  if (!timeValid()) { ftC(90, "--:--", F_N42, C_DKGREY, gfx->width()); return; }
  ftC(82, dateStr(nowT(), "%H:%M"), F_N42, 0x7BEF, gfx->width());
  ftC(110, dateStr(nowT(), "%A %d %B"), F_S, C_DKGREY, gfx->width());
  if (numCh && ch[0].subs >= 0) {
    String s = withCommas(estimateFor(0));
    ftC(156, fit(nameOf(0), F_S, 150) + "   " + s, F_S, 0x6B4D, gfx->width());
  }
}

int blNow = 160;
void fadeBacklight(int target, int stepMs) {
  while (blNow != target) {
    blNow += (target > blNow) ? 1 : -1;
    ledcWrite(LCD_BL, blNow);
    delay(stepMs);
  }
}
void setBacklight(int v) { blNow = v; ledcWrite(LCD_BL, v); }

// ── Tall leaderboard (board turned on its side) ─────────────────────────────
void drawPortraitBoard() {
  gfx->fillScreen(C_BG);
  int W = gfx->width();     // 172
  ft(6, 22, "Leaderboard", F_M, C_GOLD);
  if (numCh > 0 && min(48, 284 / numCh) >= 44) ftR(W - 6, 22, "today", F_XS, C_GREEN);
  gfx->drawFastHLine(6, 31, W - 12, C_DKGREY);
  int idx[MAX_CH]; for (int i = 0; i < numCh; i++) idx[i] = i;
  for (int a = 0; a < numCh; a++) for (int b = a + 1; b < numCh; b++)
    if (ch[idx[b]].subs > ch[idx[a]].subs) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  int rowH = numCh > 0 ? min(48, 284 / numCh) : 28;
  bool icons = mixedPlatforms();
  int y = 34;
  for (int r = 0; r < numCh; r++) {
    int i = idx[r];
    uint16_t col = (i == 0) ? C_GOLD : C_WHITE;
    String subs = ch[i].subs >= 0 ? compact(ch[i].subs) : String("-");
    if (rowH >= 44) {     // roomy: name on one line, count + today underneath
      int lw = ch[i].live ? tw("LIVE", F_XS) + 14 : 0;
      String rk = String(r + 1) + " ";
      int nx = 6 + tw(rk, F_S);
      ft(6, y + 18, rk, F_S, col);
      if (icons) { platformIcon(nx, y + 7, ch[i].tw); nx += 21; }
      String nm = fit(nameOf(i), F_S, W - nx - 6 - lw);
      ft(nx, y + 18, nm, F_S, col);
      if (ch[i].live) livePillAfter(nx + tw(nm, F_S), y + 16);
      ft(6, y + 40, subs, F_M, col);
      if (ch[i].statsOk && ch[i].gainToday) ftR(W - 6, y + 40, signedNum(ch[i].gainToday), F_S, C_GREEN);
    } else {
      int sw = tw(subs, F_S);
      ft(6, y + 19, String(r + 1), F_S, C_GREY);
      int lw = ch[i].live ? tw("LIVE", F_XS) + 14 : 0;
      int nx = 24;
      if (icons) { platformIcon(nx, y + 8, ch[i].tw); nx += 21; }
      String nm = fit(nameOf(i), F_S, W - nx - 10 - sw - lw);
      ft(nx, y + 19, nm, F_S, col);
      if (ch[i].live) livePillAfter(nx + tw(nm, F_S), y + 17);
      ftR(W - 6, y + 19, subs, F_S, col);
    }
    y += rowH;
  }
}
