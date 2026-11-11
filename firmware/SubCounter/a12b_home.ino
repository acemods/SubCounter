// ════════════════════════════════════════════════════════════════════════════
//  Home: room temperature and humidity from Home Assistant
// ════════════════════════════════════════════════════════════════════════════
// The board asks Home Assistant's REST API for each chosen sensor once a minute:
//   GET <address>/api/states/sensor.xxx   with   Authorization: Bearer <long-lived token>
// Any sensor HA knows (Matter, Zigbee, Thread, ESPHome...) works the same way.
int homeCard = 0;                                // page of rooms (3 a page)
#define HA_PER_PAGE 3

int homePages() { return max(1, (haN + HA_PER_PAGE - 1) / HA_PER_PAGE); }

// "192.168.1.20:8123/" -> "http://192.168.1.20:8123"
String haBase(String u) {
  u.trim();
  if (u.length() && u.indexOf("://") < 0) u = "http://" + u;
  while (u.endsWith("/")) u.remove(u.length() - 1);
  return u;
}
// true if the address names a port ("http://x:8123")
bool haHasPort(const String &base) {
  int s = base.indexOf("://"); s = s < 0 ? 0 : s + 3;
  int slash = base.indexOf('/', s);
  String host = slash < 0 ? base.substring(s) : base.substring(s, slash);
  return host.indexOf(':') >= 0;
}
String haCodeText(int code) {
  if (code == 401 || code == 403) return "Home Assistant didn't accept the token (" + String(code) + ")";
  if (code == 404) return "That address answered, but not as Home Assistant (404)";
  if (code < 0) return "Can't reach Home Assistant";
  return "Home Assistant error " + String(code);
}
float haNum(const char *s) {                      // "21.4" -> 21.4; "unavailable" / "unknown" -> NAN
  if (!s || !*s) return NAN;
  char *e; float v = strtof(s, &e);
  return e == s ? NAN : v;
}

volatile bool netHaRefreshed = false;
void haPoll() {
  if (!haOn()) return;
  String base = haBase(cfgHaUrl), auth = "Bearer " + cfgHaToken;
  bool tls = base.startsWith("https://");
  String ent[HA_MAX * 2]; float val[HA_MAX * 2]; String unit[HA_MAX * 2]; int code[HA_MAX * 2]; int nj = 0;
  for (int i = 0; i < haN; i++) { ent[nj++] = haRoom[i].tEnt; ent[nj++] = haRoom[i].hEnt; }
  for (int j = 0; j < nj; j++) { val[j] = NAN; code[j] = 0; }
  JsonDocument filter; filter["state"] = true; filter["attributes"]["unit_of_measurement"] = true;
  {
    NetIO io;                                    // the screen and web pages keep going while we wait
    WiFiClient plain; WiFiClientSecure sec; if (tls) sec.setInsecure();
    HTTPClient http; http.setReuse(true); http.setTimeout(4000); http.setConnectTimeout(3000);
    for (int j = 0; j < nj; j++) {
      if (!ent[j].length()) continue;
      if (!http.begin(tls ? (NetworkClient &)sec : (NetworkClient &)plain, base + "/api/states/" + ent[j])) { code[j] = -1; break; }
      http.addHeader("Authorization", auth);
      code[j] = http.GET();
      if (code[j] == 200) {
        String body = http.getString();
        JsonDocument d;
        if (!deserializeJson(d, body, DeserializationOption::Filter(filter))) {
          val[j] = haNum(d["state"] | "");
          unit[j] = String((const char *)(d["attributes"]["unit_of_measurement"] | ""));
        }
      }
      http.end();
      if (code[j] < 0 || code[j] == 401 || code[j] == 403) break;   // no point asking for the rest
    }
  }
  // back holding the data lock: store the results
  String err; bool any = false;
  for (int i = 0; i < haN; i++) {
    for (int w = 0; w < 2; w++) {
      int j = 2 * i + w;
      if (!ent[j].length()) continue;
      if (code[j] == 200) {
        any = true; haRoom[i].at = millis();
        if (w == 0) { haRoom[i].t = val[j]; haRoom[i].unit = unit[j]; } else haRoom[i].h = val[j];
      } else if (!err.length() && code[j] != 0) err = code[j] == 404 ? "Sensor not found: " + ent[j] : haCodeText(code[j]);
    }
  }
  haErr = err;
  if (any) haOkAt = millis();
  netHaRefreshed = true;
}

// ── On the screen ───────────────────────────────────────────────────────────
void drawHaIcon(int cx, int cy) {                // a house with a thermometer in it
  gfx->fillTriangle(cx - 30, cy - 4, cx, cy - 30, cx + 30, cy - 4, C_HA);
  gfx->fillRect(cx - 22, cy - 6, 44, 32, C_HA);
  gfx->fillRoundRect(cx - 4, cy - 18, 8, 28, 4, C_WHITE);
  gfx->fillCircle(cx, cy + 14, 7, C_WHITE);
  gfx->fillCircle(cx, cy + 14, 4, C_RED);
  gfx->fillRect(cx - 1, cy - 6, 3, 18, C_RED);
}
void drawDrop(int cx, int cy, int r, uint16_t col) {
  gfx->fillCircle(cx, cy + r / 2, r, col);
  gfx->fillTriangle(cx - r, cy + r / 2, cx + r, cy + r / 2, cx, cy - r * 3 / 2, col);
}
String haTemp(float t) { if (isnan(t)) return "--"; char b[12]; snprintf(b, sizeof(b), "%.1f", t); return b; }
String haHum(float h) { if (isnan(h)) return "--%"; return String(lroundf(h)) + "%"; }
bool haStale(const HaRoom &r) { return !r.at || millis() - r.at > 15UL * 60000UL; }

void drawHome() {
  gfx->fillScreen(C_BG);
  if (!cfgHaUrl.length() || !cfgHaToken.length() || !haN) {
    drawHaIcon(160, 56);
    ftC(116, "Home", F_M, C_WHITE, gfx->width());
    ftC(140, "Add Home Assistant on the", F_S, C_GREY, gfx->width());
    ftC(160, "settings page", F_S, C_GREY, gfx->width());
    return;
  }
  homeCard = constrain(homeCard, 0, homePages() - 1);
  ft(8, 22, "Home", F_M, C_GOLD);
  if (wx.ok) ftR(RIGHT_EDGE, 22, "Outside " + String(lroundf(wx.temp)) + "\xC2\xB0", F_S, C_GREY);
  gfx->drawFastHLine(8, 31, RIGHT_EDGE - 8, C_DKGREY);
  bool anyData = false; for (int i = 0; i < haN; i++) if (haRoom[i].at) anyData = true;
  if (!anyData) {
    ftC(92, haErr.length() ? "Can't get the readings" : "Asking Home Assistant...", F_M, haErr.length() ? C_RED : C_GREY);
    if (haErr.length()) { ftC(122, fit(haErr, F_S, RIGHT_EDGE - 4), F_S, C_GREY); ftC(146, "Check Home Assistant in settings", F_S, C_DKGREY); }
    return;
  }
  int first = homeCard * HA_PER_PAGE, n = min(HA_PER_PAGE, haN - first);
  bool big = haN <= 2;                                   // one or two rooms: bigger numbers
  int rowH = big ? 66 : 45, y0 = 34 + (big && haN == 1 ? 33 : 0);
  for (int k = 0; k < n; k++) {
    HaRoom &r = haRoom[first + k];
    int y = y0 + k * rowH;
    bool old = haStale(r);
    uint16_t tc = old ? C_DKGREY : C_WHITE, hc = old ? C_DKGREY : C_BLUE;
    int base = y + (big ? 44 : 32);
    ft(10, base - (big ? 6 : 0), fit(r.name, F_M, big ? 122 : 128), F_M, old ? C_GREY : C_WHITE);
    // humidity on the right, temperature just left of it
    String hs = r.hEnt.length() ? haHum(r.h) : String("");
    int hx = RIGHT_EDGE;
    if (hs.length()) { ftR(hx, base, hs, F_M, hc); int hw = tw(hs, F_M); drawDrop(hx - hw - 9, base - 9, 5, hc); hx -= hw + 22; }
    if (r.tEnt.length()) {
      String ts = haTemp(r.t);
      const uint8_t *nf = big ? F_N32 : F_N24;
      int degW = tw("\xC2\xB0", F_M);
      ft(hx - degW, base - (big ? 20 : 12), "\xC2\xB0", F_M, tc);
      ftR(hx - degW - 1, base, ts, nf, tc);
    }
    if (k < n - 1) gfx->drawFastHLine(10, y + rowH - 1, RIGHT_EDGE - 10, 0x2104);
  }
  if (homePages() > 1) {                                  // page dots on the right, like Weather
    int x = gfx->width() - 7, yd = 100 - (homePages() - 1) * 7;
    for (int p = 0; p < homePages(); p++)
      if (p == homeCard) gfx->fillCircle(x, yd + p * 14, 3, C_WHITE); else gfx->drawCircle(x, yd + p * 14, 3, C_DKGREY);
  }
  if (haErr.length()) { gfx->fillRect(0, 158, RIGHT_EDGE + 4, 14, C_BG); ft(10, 170, fit(haErr, F_XS2, RIGHT_EDGE - 10), F_XS2, C_RED); }
}

// The idle clock's bottom line, split into groups that each fit the screen: they take turns
// every few seconds, so every room gets shown however many there are.
int haClockGroups(String *out, int maxOut, int width) {
  if (!cfgHaClock || !haOn()) return 0;
  int n = 0; String cur;
  for (int i = 0; i < haN && n < maxOut; i++) {
    HaRoom &r = haRoom[i];
    if (!r.at || haStale(r)) continue;
    String one = r.name;
    if (r.tEnt.length() && !isnan(r.t)) one += " " + haTemp(r.t) + "\xC2\xB0";
    if (r.hEnt.length() && !isnan(r.h)) one += " " + haHum(r.h);
    String joined = cur.length() ? cur + "  -  " + one : one;
    if (cur.length() && tw(joined, F_S) > width) { out[n++] = cur; cur = one; }
    else cur = joined;
  }
  if (cur.length() && n < maxOut) out[n++] = cur;
  return n;
}

// One line for the idle clock: "Lounge 21.4° 48%  ·  Bedroom 19.0° 55%"
String haClockLine() {
  if (!cfgHaClock || !haOn()) return "";
  String s;
  for (int i = 0; i < haN; i++) {
    HaRoom &r = haRoom[i];
    if (!r.at || haStale(r)) continue;
    String one = r.name;
    if (r.tEnt.length() && !isnan(r.t)) one += " " + haTemp(r.t) + "\xC2\xB0";
    if (r.hEnt.length() && !isnan(r.h)) one += " " + haHum(r.h);
    s += (s.length() ? "  -  " : "") + one;
  }
  return s;
}

// ── Settings: "Find sensors" ────────────────────────────────────────────────
// Reads /api/states one entity at a time (a big house can have thousands of entities, far
// more than fits in memory at once) and keeps only temperature and humidity sensors.
int haFindAt(const String &base, const String &token, JsonArray out, int &nT, int &nH) {
  bool tls = base.startsWith("https://");
  JsonDocument filter;
  filter["entity_id"] = true; filter["state"] = true;
  filter["attributes"]["device_class"] = true; filter["attributes"]["friendly_name"] = true; filter["attributes"]["unit_of_measurement"] = true;
  NetIO io;
  WiFiClient plain; WiFiClientSecure sec; if (tls) sec.setInsecure();
  HTTPClient http; http.useHTTP10(true);          // no chunked replies, so the stream is plain JSON
  http.setTimeout(6000); http.setConnectTimeout(4000);
  if (!http.begin(tls ? (NetworkClient &)sec : (NetworkClient &)plain, base + "/api/states")) return -1;
  http.addHeader("Authorization", "Bearer " + token);
  int code = http.GET();
  if (code == 200) {
    Stream &s = http.getStream();
    if (s.find("[")) {
      do {
        JsonDocument e;
        if (deserializeJson(e, s, DeserializationOption::Filter(filter))) break;
        String id = e["entity_id"] | "";
        String dc = e["attributes"]["device_class"] | "";
        if (!id.startsWith("sensor.") || (dc != "temperature" && dc != "humidity")) continue;
        if (dc == "temperature") nT++; else nH++;
        if (out.size() >= 40) continue;
        JsonObject o = out.add<JsonObject>();
        o["e"] = id; o["c"] = dc == "temperature" ? "t" : "h";
        o["n"] = String((const char *)(e["attributes"]["friendly_name"] | id.c_str()));
        o["v"] = String((const char *)(e["state"] | "")); o["u"] = String((const char *)(e["attributes"]["unit_of_measurement"] | ""));
      } while (s.findUntil(",", "]"));
    }
  }
  http.end();
  return code;
}

void handleHaFind() {
  if (!authed()) return;
  String base = haBase(server.arg("url"));
  String token = server.arg("token"); token.trim();
  if (!token.length()) token = cfgHaToken;
  JsonDocument doc;
  if (!base.length() || !token.length()) { doc["err"] = "Type the Home Assistant address and paste a token first."; }
  else {
    JsonArray list = doc["s"].to<JsonArray>();
    int nT = 0, nH = 0;
    int code = haFindAt(base, token, list, nT, nH);
    if (code < 0 && !haHasPort(base)) {           // Home Assistant normally listens on port 8123
      String alt = base + ":8123";
      int c2 = haFindAt(alt, token, list, nT, nH);
      if (c2 > 0) { code = c2; base = alt; }
    }
    doc["url"] = base; doc["code"] = code; doc["nT"] = nT; doc["nH"] = nH;
    if (code != 200) doc["err"] = haCodeText(code) + (code < 0 ? " at " + base + ". Is the address right? Home Assistant usually uses port 8123, e.g. http://192.168.1.20:8123" : "");
  }
  String out; serializeJson(doc, out);
  server.send(200, "application/json", out);
}
