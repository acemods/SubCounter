// ════════════════════════════════════════════════════════════════════════════
//  Apps: home menu, Weather, Spotify
// ════════════════════════════════════════════════════════════════════════════
enum AppId { APP_YT = 0, APP_WX = 1, APP_SP = 2, NUM_APPS = 3 };
int app = APP_YT;
bool menuOpen = false;
int wxCard = 0;              // 0 now, 1 next hours, 2 tomorrow
#define WX_CARDS 3

// ── Weather data (Open-Meteo: free, no key) ─────────────────────────────────
struct Weather {
  bool ok = false;
  float temp = 0, feels = 0, wind = 0;
  int code = 0; bool isDay = true;
  int hi = 0, lo = 0;
  int hHour[12]; int hTemp[12]; int hCode[12]; int hRain[12]; int hN = 0;
  int tHi = 0, tLo = 0, tCode = 0, tRain = 0;
  String sunrise, sunset, tSunrise, tSunset;
  int rainHour = -1, rainPct = 0;       // rain warning in the next 3 hours
} wx;
unsigned long lastWxFetch = 0;

String wxText(int c) {
  if (c == 0) return "Clear";
  if (c <= 2) return "Partly cloudy";
  if (c == 3) return "Cloudy";
  if (c == 45 || c == 48) return "Fog";
  if (c >= 51 && c <= 57) return "Drizzle";
  if (c >= 61 && c <= 67) return c >= 65 ? "Heavy rain" : "Rain";
  if (c >= 71 && c <= 77) return "Snow";
  if (c >= 80 && c <= 82) return "Showers";
  if (c == 85 || c == 86) return "Snow showers";
  if (c >= 95) return "Thunderstorm";
  return "";
}

void drawCloud(int cx, int cy, int r, uint16_t col) {
  gfx->fillCircle(cx - r * 6 / 10, cy + r / 6, r * 5 / 10, col);
  gfx->fillCircle(cx, cy - r / 6, r * 7 / 10, col);
  gfx->fillCircle(cx + r * 6 / 10, cy + r / 6, r * 5 / 10, col);
  gfx->fillRoundRect(cx - r * 11 / 10, cy + r / 6, r * 22 / 10, r * 5 / 10, r / 4, col);
}

void drawSun(int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r * 5 / 10, C_GOLD);
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4;
    gfx->drawLine(cx + cosf(a) * r * 0.65f, cy + sinf(a) * r * 0.65f, cx + cosf(a) * r * 0.9f, cy + sinf(a) * r * 0.9f, C_GOLD);
  }
}

void drawMoon(int cx, int cy, int r) {
  gfx->fillCircle(cx, cy, r * 5 / 10, 0xDEFB);
  gfx->fillCircle(cx + r * 3 / 10, cy - r * 2 / 10, r * 4 / 10, C_BG);
}

// A simple weather picture centred at (cx,cy), about 2r wide
void drawWxIcon(int code, int cx, int cy, int r, bool day) {
  uint16_t grey = 0xBDF7, dark = 0x7BEF;
  if (code == 0) { if (day) drawSun(cx, cy, r); else drawMoon(cx, cy, r); return; }
  if (code <= 2) {
    if (day) drawSun(cx - r / 3, cy - r / 3, r * 7 / 10); else drawMoon(cx - r / 3, cy - r / 3, r * 7 / 10);
    drawCloud(cx + r / 6, cy + r / 5, r * 7 / 10, grey); return;
  }
  bool heavy = code == 3 || code >= 61;
  drawCloud(cx, cy - r / 5, r * 8 / 10, heavy ? dark : grey);
  if (code == 45 || code == 48) { for (int k = 0; k < 3; k++) gfx->drawFastHLine(cx - r * 7 / 10, cy + r / 3 + k * r / 5, r * 14 / 10, grey); return; }
  bool snow = (code >= 71 && code <= 77) || code == 85 || code == 86;
  bool rain = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || code >= 95;
  if (rain) for (int k = -1; k <= 1; k++) gfx->drawLine(cx + k * r / 3, cy + r * 4 / 10, cx + k * r / 3 - r / 8, cy + r * 8 / 10, C_BLUE);
  if (snow) for (int k = -1; k <= 1; k++) gfx->fillCircle(cx + k * r / 3, cy + r * 6 / 10, max(1, r / 10), C_WHITE);
  if (code >= 95) gfx->fillTriangle(cx, cy + r * 2 / 10, cx - r / 4, cy + r * 7 / 10, cx + r / 8, cy + r * 5 / 10, C_GOLD);
}

void fetchWeather() {
  if (cfgWxLat == 0 && cfgWxLon == 0) return;
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(cfgWxLat, 3) + "&longitude=" + String(cfgWxLon, 3) +
               "&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m,is_day"
               "&hourly=temperature_2m,precipitation_probability,weather_code"
               "&daily=temperature_2m_max,temperature_2m_min,weather_code,precipitation_probability_max,sunrise,sunset"
               "&timezone=auto&forecast_days=2&wind_speed_unit=mph";
  int code; String body;
  { NetIO io;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(10000);
    if (!http.begin(client, url)) return;
    code = http.GET();
    body = code == 200 ? http.getString() : String("");
    http.end(); }
  if (code != 200) { Serial.printf("Weather HTTP %d\n", code); return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) return;
  JsonObject cur = doc["current"];
  wx.temp = cur["temperature_2m"] | 0.0f;
  wx.feels = cur["apparent_temperature"] | 0.0f;
  wx.code = cur["weather_code"] | 0;
  wx.wind = cur["wind_speed_10m"] | 0.0f;
  wx.isDay = (cur["is_day"] | 1) == 1;
  String nowHour = String((const char *)(cur["time"] | "")).substring(0, 13);
  JsonObject d = doc["daily"];
  wx.hi = lroundf(d["temperature_2m_max"][0] | 0.0f); wx.lo = lroundf(d["temperature_2m_min"][0] | 0.0f);
  wx.tHi = lroundf(d["temperature_2m_max"][1] | 0.0f); wx.tLo = lroundf(d["temperature_2m_min"][1] | 0.0f);
  wx.tCode = d["weather_code"][1] | 0; wx.tRain = d["precipitation_probability_max"][1] | 0;
  wx.sunrise = String((const char *)(d["sunrise"][0] | "")).substring(11, 16);
  wx.sunset = String((const char *)(d["sunset"][0] | "")).substring(11, 16);
  wx.tSunrise = String((const char *)(d["sunrise"][1] | "")).substring(11, 16);
  wx.tSunset = String((const char *)(d["sunset"][1] | "")).substring(11, 16);
  JsonObject h = doc["hourly"];
  JsonArray times = h["time"];
  int start = 0;
  for (int i = 0; i < (int)times.size(); i++) if (String((const char *)times[i]).substring(0, 13) == nowHour) { start = i; break; }
  wx.hN = 0; wx.rainHour = -1; wx.rainPct = 0;
  for (int i = start; i < (int)times.size() && wx.hN < 12; i++) {
    int k = wx.hN++;
    wx.hHour[k] = String((const char *)times[i]).substring(11, 13).toInt();
    wx.hTemp[k] = lroundf(h["temperature_2m"][i] | 0.0f);
    wx.hCode[k] = h["weather_code"][i] | 0;
    wx.hRain[k] = h["precipitation_probability"][i] | 0;
    if (k <= 3 && wx.hRain[k] >= 50 && wx.rainHour < 0) { wx.rainHour = wx.hHour[k]; wx.rainPct = wx.hRain[k]; }
  }
  wx.ok = true;
}

String deg(int t) { return String(t) + "\xC2\xB0"; }   // e.g. 12°

void drawWxDots() {
  int x = gfx->width() - 7, y0 = 86 - (WX_CARDS - 1) * 7;
  for (int k = 0; k < WX_CARDS; k++)
    if (k == wxCard) gfx->fillCircle(x, y0 + k * 14, 3, C_WHITE); else gfx->drawCircle(x, y0 + k * 14, 3, C_DKGREY);
}

void drawWeather() {
  gfx->fillScreen(C_BG);
  if (cfgWxLat == 0 && cfgWxLon == 0) {
    ftC(80, "Weather", F_M, C_GOLD); ftC(112, "Set your town on the", F_S, C_GREY); ftC(134, "settings page", F_S, C_GREY); return;
  }
  if (!wx.ok) { ftC(96, "Loading weather...", F_M, C_GREY); return; }
  drawWxDots();
  if (wxCard == 0) {
    // clock + now
    if (timeValid()) {
      ft(10, 58, dateStr(nowT(), "%H:%M"), F_N42, C_WHITE);
      ft(12, 84, dateStr(nowT(), "%a %d %b"), F_S, C_GREY);
    }
    drawWxIcon(wx.code, 232, 40, 30, wx.isDay);
    ftR(RIGHT_EDGE, 104, deg(lroundf(wx.temp)), F_M, C_WHITE);
    ft(12, 112, fit(cfgWxName + "  -  " + wxText(wx.code), F_S, 190), F_S, C_WHITE);
    ft(12, 134, "High " + deg(wx.hi) + "   Low " + deg(wx.lo) + "   Feels " + deg(lroundf(wx.feels)), F_S, C_GREY);
    if (wx.rainHour >= 0) {
      gfx->fillRoundRect(6, 144, RIGHT_EDGE - 4, 26, 6, 0x10A2);
      char b[48]; snprintf(b, sizeof(b), "Rain likely around %02d:00 (%d%%)", wx.rainHour, wx.rainPct);
      ft(14, 163, b, F_S, C_BLUE);
    } else ft(12, 162, "Wind " + String(lroundf(wx.wind)) + " mph   Sunset " + wx.sunset, F_S, C_GREY);
  } else if (wxCard == 1) {
    ft(8, 22, "Next hours", F_M, C_GOLD);
    ftR(RIGHT_EDGE, 22, cfgWxName, F_S, C_GREY);
    gfx->drawFastHLine(8, 31, RIGHT_EDGE - 8, C_DKGREY);
    int cols = 6, cw = (RIGHT_EDGE - 4) / cols;
    for (int c = 0; c < cols; c++) {
      int k = c * 2; if (k >= wx.hN) break;
      int cx = 6 + c * cw + cw / 2;
      char hh[4]; snprintf(hh, sizeof(hh), "%02d", wx.hHour[k]);
      ft(cx - tw(hh, F_S) / 2, 54, hh, F_S, C_GREY);
      drawWxIcon(wx.hCode[k], cx, 82, 17, wx.hHour[k] >= 7 && wx.hHour[k] < 19);
      String t = deg(wx.hTemp[k]);
      ft(cx - tw(t, F_M) / 2, 126, t, F_M, C_WHITE);
      String r = String(wx.hRain[k]) + "%";
      ft(cx - tw(r, F_S) / 2, 150, r, F_S, wx.hRain[k] >= 50 ? C_BLUE : C_DKGREY);
    }
    ft(8, 170, "rain chance", F_S, C_DKGREY);
  } else {
    ft(8, 22, "Tomorrow", F_M, C_GOLD);
    ftR(RIGHT_EDGE, 22, cfgWxName, F_S, C_GREY);
    gfx->drawFastHLine(8, 31, RIGHT_EDGE - 8, C_DKGREY);
    drawWxIcon(wx.tCode, 58, 90, 40, true);
    ft(120, 66, wxText(wx.tCode), F_M, C_WHITE);
    ft(120, 96, "High " + deg(wx.tHi) + "  Low " + deg(wx.tLo), F_S, C_WHITE);
    ft(120, 120, "Rain chance " + String(wx.tRain) + "%", F_S, wx.tRain >= 50 ? C_BLUE : C_GREY);
    ft(120, 144, "Sunrise " + wx.tSunrise, F_S, C_GREY);
    ft(120, 166, "Sunset " + wx.tSunset, F_S, C_GREY);
  }
}

// Look up a town name -> coordinates (Open-Meteo geocoding)
bool geocode(const String &name, float &lat, float &lon, String &label) {
  int code; String body;
  { NetIO io;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(8000);
    if (!http.begin(client, "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&name=" + urlEncode(name))) return false;
    code = http.GET();
    body = code == 200 ? http.getString() : String("");
    http.end(); }
  if (code != 200) return false;
  JsonDocument doc;
  if (deserializeJson(doc, body)) return false;
  JsonObject r = doc["results"][0];
  if (r.isNull()) return false;
  lat = r["latitude"] | 0.0f; lon = r["longitude"] | 0.0f;
  label = String((const char *)(r["name"] | name.c_str()));
  return true;
}

// ── Spotify ─────────────────────────────────────────────────────────────────
struct SpotifyState {
  String access; unsigned long accessUntil = 0;
  bool connected = false, playing = false, hasTrack = false;
  String trackId, title, artist, album, device, artUrl, err;
  long progress = 0, duration = 0; unsigned long progressAt = 0;
  int volume = -1;
  uint8_t *art = nullptr; int artLen = 0;
} sp;
unsigned long lastSpPoll = 0;
unsigned long spWait = 3000;        // time until the next poll: 3 s playing, 6 s paused, longer after errors
int spFails = 0;

String b64(const String &s) {
  static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String o; int i = 0; uint32_t v;
  while (i + 2 < (int)s.length()) { v = ((uint8_t)s[i] << 16) | ((uint8_t)s[i + 1] << 8) | (uint8_t)s[i + 2];
    o += tbl[v >> 18]; o += tbl[(v >> 12) & 63]; o += tbl[(v >> 6) & 63]; o += tbl[v & 63]; i += 3; }
  int rem = s.length() - i;
  if (rem == 1) { v = (uint8_t)s[i] << 16; o += tbl[v >> 18]; o += tbl[(v >> 12) & 63]; o += "=="; }
  else if (rem == 2) { v = ((uint8_t)s[i] << 16) | ((uint8_t)s[i + 1] << 8); o += tbl[v >> 18]; o += tbl[(v >> 12) & 63]; o += tbl[(v >> 6) & 63]; o += '='; }
  return o;
}

String spAuthUrl() {
  return "https://accounts.spotify.com/authorize?response_type=code&client_id=" + urlEncode(cfgSpId) +
         "&scope=" + urlEncode("user-read-playback-state user-modify-playback-state user-read-currently-playing") +
         "&redirect_uri=" + urlEncode(cfgSpRedirect) +
         "&state=" + urlEncode("http://" + WiFi.localIP().toString());   // tells the relay page where the board is
}

// POST to Spotify's token endpoint; stores the access token (and a new refresh token if given)
bool spToken(const String &body, String &err) {
  int code; String resp;
  String auth = "Basic " + b64(cfgSpId + ":" + cfgSpSecret);   // built while we still hold the data lock
  { NetIO io;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(6000); http.setConnectTimeout(5000);
    if (!http.begin(client, "https://accounts.spotify.com/api/token")) { err = "Can't reach Spotify"; return false; }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Authorization", auth);
    code = http.POST(body);
    resp = http.getString();
    http.end(); }
  JsonDocument doc; deserializeJson(doc, resp);
  if (code != 200) {
    err = String((const char *)(doc["error_description"] | doc["error"] | "error")) + " (" + code + ")";
    return false;
  }
  sp.access = String((const char *)(doc["access_token"] | ""));
  sp.accessUntil = millis() + ((long)(doc["expires_in"] | 3600) - 60) * 1000UL;
  String rt = doc["refresh_token"] | "";
  if (rt.length() && rt != cfgSpRefresh) {
    cfgSpRefresh = rt;
    prefs.begin("subcounter", false); prefs.putString("spref", cfgSpRefresh); prefs.end();
  }
  sp.connected = sp.access.length() > 0;
  return sp.connected;
}

bool spEnsureToken() {
  if (!cfgSpRefresh.length() || !cfgSpId.length()) return false;
  if (sp.access.length() && (long)(sp.accessUntil - millis()) > 0) return true;
  String err;
  if (!spToken("grant_type=refresh_token&refresh_token=" + urlEncode(cfgSpRefresh), err)) { sp.err = err; return false; }
  return true;
}

// Exchange the code from the pasted address for tokens
bool spConnectWithCode(String pasted, String &err) {
  int i = pasted.indexOf("code=");
  String code = i >= 0 ? pasted.substring(i + 5) : pasted;
  int amp = code.indexOf('&'); if (amp >= 0) code = code.substring(0, amp);
  code.trim();
  if (!code.length()) { err = "No code found in what you pasted"; return false; }
  cfgSpRefresh = "";
  return spToken("grant_type=authorization_code&code=" + urlEncode(code) + "&redirect_uri=" + urlEncode(cfgSpRedirect), err);
}

int spCall(const char *method, const String &path, String *resp = nullptr) {
  if (!spEnsureToken()) return -1;
  Hdr hs[] = { { "Authorization", "Bearer " + sp.access } };
  String body;
  int code = httpsCall(connSP, method, "https://api.spotify.com/v1" + path, hs, 1, &body, nullptr, nullptr, 5000);
  if (resp && code == 200) *resp = body;
  if (code == 401) { sp.access = ""; }
  return code;
}

void spFetchArt() {
  if (sp.art) { free(sp.art); sp.art = nullptr; sp.artLen = 0; }
  if (!sp.artUrl.length()) return;
  String url = sp.artUrl; int len;
  uint8_t *buf = httpDownload(url, len, 90000);
  if (!buf) return;
  if (len > 4 && buf[0] == 0xFF && buf[1] == 0xD8 && url == sp.artUrl) {
    if (sp.art) free(sp.art);
    sp.art = buf; sp.artLen = len;
  } else free(buf);
}

// Returns true if the track changed (needs a full redraw)
bool spPoll() {
  String body;
  int code = spCall("GET", "/me/player?market=from_token", &body);
  if (code == 204 || code == 202) { bool was = sp.hasTrack; sp.hasTrack = false; sp.playing = false; sp.err = ""; return was; }
  if (code != 200) {
    if (code == -1) sp.err = cfgSpRefresh.length() ? sp.err : String("Not connected");
    else sp.err = "Spotify error " + String(code);
    return false;
  }
  JsonDocument filter;
  filter["is_playing"] = true; filter["progress_ms"] = true;
  filter["device"]["name"] = true; filter["device"]["volume_percent"] = true;
  filter["item"]["id"] = true; filter["item"]["name"] = true; filter["item"]["duration_ms"] = true;
  filter["item"]["artists"][0]["name"] = true; filter["item"]["show"]["name"] = true;
  filter["item"]["album"]["name"] = true; filter["item"]["album"]["images"][0]["url"] = true; filter["item"]["album"]["images"][0]["width"] = true;
  filter["item"]["images"][0]["url"] = true; filter["item"]["images"][0]["width"] = true;
  JsonDocument doc;
  if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;
  sp.err = "";
  sp.playing = doc["is_playing"] | false;
  sp.progress = doc["progress_ms"] | 0; sp.progressAt = millis();
  sp.device = String((const char *)(doc["device"]["name"] | ""));
  sp.volume = doc["device"]["volume_percent"] | -1;
  JsonObject it = doc["item"];
  if (it.isNull()) { bool was = sp.hasTrack; sp.hasTrack = false; return was; }
  String id = it["id"] | "";
  sp.duration = it["duration_ms"] | 0;
  bool changed = !sp.hasTrack || id != sp.trackId;
  sp.hasTrack = true;
  if (changed) {
    sp.trackId = id;
    sp.title = String((const char *)(it["name"] | ""));
    sp.artist = String((const char *)(it["artists"][0]["name"] | (it["show"]["name"] | "")));
    sp.album = String((const char *)(it["album"]["name"] | ""));
    // pick the ~300 px picture (half-size on screen = 150 px)
    JsonArray imgs = it["album"]["images"].isNull() ? it["images"].as<JsonArray>() : it["album"]["images"].as<JsonArray>();
    sp.artUrl = "";
    for (JsonObject im : imgs) { int w = im["width"] | 0; if (w >= 250 && w <= 400) sp.artUrl = String((const char *)(im["url"] | "")); }
    if (!sp.artUrl.length() && imgs.size()) sp.artUrl = String((const char *)(imgs[imgs.size() > 1 ? 1 : 0]["url"] | ""));
    spFetchArt();
  }
  return changed;
}

bool drawArt(int x, int y) {
  if (!sp.art) return false;
  if (!jpeg.openRAM(sp.art, sp.artLen, jpegDraw)) return false;
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  int w = jpeg.getWidth();
  int opt = w >= 560 ? JPEG_SCALE_QUARTER : (w >= 280 ? JPEG_SCALE_HALF : 0);
  int sw = w / (opt == JPEG_SCALE_QUARTER ? 4 : (opt == JPEG_SCALE_HALF ? 2 : 1));
  avCx = x + sw / 2; avCy = y + sw / 2; avR = 10000;            // no round crop
  jpeg.decode(x, y, opt);
  jpeg.close();
  return true;
}

String mmss(long ms) { char b[12]; long s = ms / 1000; snprintf(b, sizeof(b), "%ld:%02ld", s / 60, s % 60); return String(b); }

void drawSpProgress() {
  if (!sp.hasTrack) return;
  long p = sp.progress + (sp.playing ? (long)(millis() - sp.progressAt) : 0);
  if (sp.duration && p > sp.duration) p = sp.duration;
  int x = 170, w = RIGHT_EDGE - 170, y = 122;
  gfx->fillRect(x, y - 2, w + 4, 30, C_BG);
  gfx->fillRoundRect(x, y, w, 6, 3, C_DKGREY);
  if (sp.duration) gfx->fillRoundRect(x, y, max(6, (int)(w * (float)p / sp.duration)), 6, 3, 0x1EE9);
  ft(x, y + 24, mmss(p), F_S, C_GREY);
  ftR(x + w, y + 24, mmss(sp.duration), F_S, C_GREY);
}

// ── Scrolling text (ticker) for long titles / artists ──
// Each line is drawn into a small off-screen canvas, then copied to the display in one go (no flicker).
#define MQ_GAP 48
struct Marquee {
  Arduino_Canvas *cv = nullptr;
  int x = 0, y = 0, w = 0, h = 0, baseline = 0;
  const uint8_t *font = nullptr; uint16_t col = C_WHITE;
  String text; int textW = 0, off = 0;
  unsigned long pauseUntil = 0, last = 0;
};
Marquee mqTitle, mqArtist;

void mqInit(Marquee &m, int x, int y, int w, int h, int baseline, const uint8_t *font, uint16_t col) {
  m.x = x; m.y = y; m.w = w; m.h = h; m.baseline = baseline; m.font = font; m.col = col;
  if (!m.cv) {
    m.cv = new Arduino_Canvas(w, h, gfx, x, y);
    if (!m.cv->begin(GFX_SKIP_OUTPUT_BEGIN)) { delete m.cv; m.cv = nullptr; return; }
    m.cv->setTextWrap(false);
    m.cv->setUTF8Print(true);
  }
}

void mqRender(Marquee &m) {
  if (!m.cv) { ft(m.x, m.y + m.baseline, fit(m.text, m.font, m.w), m.font, m.col); return; }   // no memory: plain text
  m.cv->fillScreen(C_BG);
  m.cv->setFont(m.font); m.cv->setTextSize(1); m.cv->setTextColor(m.col);
  m.cv->setCursor(-m.off, m.baseline); m.cv->print(m.text);
  if (m.textW > m.w) { m.cv->setCursor(-m.off + m.textW + MQ_GAP, m.baseline); m.cv->print(m.text); }
  m.cv->flush();
}

void mqSet(Marquee &m, const String &text) {
  m.text = text; m.textW = tw(text, m.font); m.off = 0;
  m.pauseUntil = millis() + 2000;
  mqRender(m);
}

// Call often: scrolls ~33 px a second when the text doesn't fit, pausing at the start of each loop
void mqTick(Marquee &m) {
  if (!m.cv || m.textW <= m.w || millis() < m.pauseUntil || millis() - m.last < 30) return;
  m.last = millis();
  m.off++;
  if (m.off >= m.textW + MQ_GAP) { m.off = 0; m.pauseUntil = millis() + 2000; }
  mqRender(m);
}

void drawSpotify() {
  gfx->fillScreen(C_BG);
  if (!cfgSpId.length() || !cfgSpRefresh.length()) {
    gfx->fillCircle(160, 52, 24, 0x1EE9);
    for (int k = 0; k < 3; k++) gfx->drawFastHLine(148 + k * 2, 44 + k * 8, 24 - k * 4, C_BG);
    ftC(108, "Spotify", F_M, C_WHITE, gfx->width());
    ftC(132, "Connect it on the", F_S, C_GREY, gfx->width());
    ftC(152, "settings page", F_S, C_GREY, gfx->width());
    return;
  }
  if (!sp.hasTrack) {
    ftC(80, "Nothing playing", F_M, C_GREY, gfx->width());
    ftC(108, fit(sp.err.length() ? sp.err : String("Start something on Spotify"), F_S, gfx->width() - 10), F_S, sp.err.length() ? C_RED : C_DKGREY, gfx->width());
    if (sp.err.length()) { ftC(136, "Check Spotify in settings", F_S, C_GREY, gfx->width()); ftC(158, "Hold BOOT 1 s for the menu", F_S, C_DKGREY, gfx->width()); }
    return;
  }
  if (!drawArt(8, 11)) gfx->fillRoundRect(8, 11, 150, 150, 8, C_DKGREY);
  int x = 170;
  mqInit(mqTitle, x, 16, RIGHT_EDGE + 10 - x, 30, 22, F_M, C_WHITE);       // title: one line, scrolls if long
  mqInit(mqArtist, x, 50, RIGHT_EDGE + 10 - x, 26, 18, F_S, 0x1EE9);       // artist: one line, scrolls if long
  mqSet(mqTitle, asciiOnly(sp.title));
  mqSet(mqArtist, asciiOnly(sp.artist));
  // play / pause symbol
  if (sp.playing) { gfx->fillRect(x, 92, 6, 18, C_WHITE); gfx->fillRect(x + 11, 92, 6, 18, C_WHITE); }
  else gfx->fillTriangle(x, 92, x, 110, x + 16, 101, C_WHITE);
  if (sp.device.length()) ft(x + 26, 108, fit(asciiOnly(sp.device), F_S, RIGHT_EDGE + 10 - x - 26), F_S, C_DKGREY);
  drawSpProgress();
}

void spToast(const String &t) {
  gfx->fillRoundRect(60, 66, 200, 40, 8, 0x2104);
  ftC(93, t, F_M, C_WHITE, gfx->width());
}

// Touch / BOOT on the Spotify screen queue a command; the network task sends it
volatile char spPendingCmd = 0;
String spToastMsg;
void spCommandNet(char what) {
  int code = 0; String toast;
  if (what == 'T') code = spCall("PUT", sp.playing ? "/me/player/pause" : "/me/player/play");
  else if (what == 'L') code = spCall("POST", "/me/player/next");
  else if (what == 'R') code = spCall("POST", "/me/player/previous");
  else if (what == 'U' || what == 'D') {
    int v = sp.volume < 0 ? 50 : sp.volume;
    v = constrain(v + (what == 'U' ? 10 : -10), 0, 100);
    code = spCall("PUT", "/me/player/volume?volume_percent=" + String(v));
    if (code >= 200 && code < 300) { sp.volume = v; toast = "Volume " + String(v) + "%"; }
  }
  if (code == 403) toast = "Needs Spotify Premium";
  else if (code == 404) toast = "No active device";
  if (what == 'T' && code >= 200 && code < 300) sp.playing = !sp.playing;
  { NetIO io; delay(250); }                      // give Spotify a moment before asking what's playing
  spPoll();
  lastSpPoll = millis();
  spToastMsg = toast;
  netSpRedraw = true;
}

// Idle clock: big and bright, with date, weather and your channel's count
void drawClock() {
  gfx->fillScreen(C_BG);
  if (!timeValid()) { ftC(100, "--:--", F_N42, C_DKGREY, gfx->width()); return; }
  ftC(76, dateStr(nowT(), "%H:%M"), u8g2_font_logisoso58_tn, C_WHITE, gfx->width());
  ftC(106, dateStr(nowT(), "%A %d %B"), F_M, C_GREY, gfx->width());
  int y = 148;
  String bottom;
  if (wx.ok) bottom = String(lroundf(wx.temp)) + "\xC2\xB0 " + wxText(wx.code);
  if (numCh && ch[0].subs >= 0) bottom += (bottom.length() ? "   -   " : "") + compact(estimateFor(0)) + " subs";
  if (bottom.length()) ftC(y, fit(bottom, F_S, gfx->width() - 16), F_S, C_GREY, gfx->width());
  if (wx.ok && wx.rainHour >= 0) {
    char b[40]; snprintf(b, sizeof(b), "Rain around %02d:00", wx.rainHour);
    ftC(168, b, F_S, C_BLUE, gfx->width());
  }
}

// ── Home menu ───────────────────────────────────────────────────────────────
void drawAppIcon(int a, int cx, int cy) {
  if (a == APP_YT) {                                                   // "Creators": half YouTube, half Twitch
    gfx->fillRoundRect(cx - 30, cy - 21, 60, 42, 12, C_TWITCH);           // purple badge, rounded all round
    gfx->fillRoundRect(cx - 30, cy - 21, 44, 42, 12, C_RED);              // red over the left side...
    gfx->fillRect(cx, cy - 21, 15, 42, C_TWITCH);                         // ...cut straight down the middle
    gfx->fillTriangle(cx - 17, cy - 9, cx - 17, cy + 9, cx - 4, cy, C_WHITE);   // play
    gfx->fillRect(cx + 8, cy - 8, 4, 11, C_WHITE); gfx->fillRect(cx + 16, cy - 8, 4, 11, C_WHITE); // Twitch bars
  }
  if (a == APP_WX) { drawWxIcon(2, cx, cy, 30, true); }
  if (a == APP_SP) {
    gfx->fillCircle(cx, cy, 26, 0x1EE9);
    for (int k = 0; k < 3; k++) { gfx->drawFastHLine(cx - 13 + k * 2, cy - 8 + k * 8, 26 - k * 4, C_BG); gfx->drawFastHLine(cx - 13 + k * 2, cy - 7 + k * 8, 26 - k * 4, C_BG); }
  }
}

#define MENU_TILES 4            // the three apps + Device (more tiles can be added later)
#define MENU_PER_PAGE 3         // three big tiles per page; swipe left/right for the next page
int menuPage = 0;
int menuPages() { return (MENU_TILES + MENU_PER_PAGE - 1) / MENU_PER_PAGE; }
void drawDeviceIcon(int cx, int cy, uint16_t col) {
  gfx->drawRoundRect(cx - 17, cy - 26, 34, 52, 6, col);              // a phone...
  gfx->drawRoundRect(cx - 16, cy - 25, 32, 50, 5, col);
  for (int k = 0; k < 3; k++) {                                       // ...showing a QR code
    int x = cx - 11 + (k == 1 ? 13 : 0), y = cy - 17 + (k == 2 ? 13 : 0);
    gfx->drawRect(x, y, 9, 9, col); gfx->fillRect(x + 3, y + 3, 3, 3, col);
  }
  gfx->fillRect(cx + 3, cy - 3, 3, 3, col); gfx->fillRect(cx + 7, cy + 1, 3, 3, col); gfx->fillRect(cx + 3, cy + 5, 3, 3, col);
}
void drawMenu() {
  gfx->fillScreen(C_BG);
  ftR(gfx->width() - 6, 14, "v" FW_VERSION, F_XS, C_DKGREY);
  if (updAvail) ft(6, 14, "v" + updVer + " available - swipe to Device", F_XS, C_GOLD);
  const char *names[MENU_TILES] = { "Creators", "Weather", "Spotify", "Device" };
  menuPage = constrain(menuPage, 0, menuPages() - 1);
  int w = gfx->width() / MENU_PER_PAGE;
  for (int s = 0; s < MENU_PER_PAGE; s++) {
    int a = menuPage * MENU_PER_PAGE + s;
    if (a >= MENU_TILES) break;
    int cx = s * w + w / 2;
    if (a == app) gfx->drawRoundRect(s * w + 6, 22, w - 12, 116, 12, C_DKGREY);
    if (a < NUM_APPS) drawAppIcon(a, cx, 70);
    else { drawDeviceIcon(cx, 70, updAvail ? C_GOLD : C_WHITE); if (updAvail) gfx->fillCircle(cx + 18, 44, 6, C_RED); }
    ft(cx - tw(names[a], F_S) / 2, 130, names[a], F_S, a == app ? C_WHITE : C_GREY);
  }
  // page dots (a red one if an update is waiting on a page you're not on)
  int np = menuPages();
  if (np > 1) {
    int x0 = gfx->width() / 2 - (np - 1) * 7;
    for (int p = 0; p < np; p++) {
      bool hasUpd = updAvail && p != menuPage && p == (NUM_APPS / MENU_PER_PAGE);
      if (p == menuPage) gfx->fillCircle(x0 + p * 14, 146, 3, C_WHITE);
      else gfx->fillCircle(x0 + p * 14, 146, 3, hasUpd ? C_RED : C_DKGREY);
    }
  }
  if (WiFi.status() == WL_CONNECTED) ftC(168, WiFi.localIP().toString() + "  ·  " HOSTNAME ".local", F_S, C_GREY, gfx->width());
  else ftC(168, "Tap an app", F_S, C_DKGREY, gfx->width());
}

// ── Device screen: QR code for the dashboard, address, version, updates ─────
bool deviceOpen = false, devChecking = false;
time_t devShownCheck = 0;
int qrX, qrY, qrScale;
void qrDraw(esp_qrcode_handle_t q) {
  int n = esp_qrcode_get_size(q);
  qrScale = max(2, min(5, 150 / (n + 4)));
  int box = (n + 4) * qrScale;
  gfx->fillRoundRect(qrX, qrY, box, box, 6, C_WHITE);
  for (int y = 0; y < n; y++)
    for (int x = 0; x < n; x++)
      if (esp_qrcode_get_module(q, x, y)) gfx->fillRect(qrX + (x + 2) * qrScale, qrY + (y + 2) * qrScale, qrScale, qrScale, C_BG);
}
void drawDevice() {
  gfx->fillScreen(C_BG);
  bool online = WiFi.status() == WL_CONNECTED;
  String url = "http://" + WiFi.localIP().toString() + "/";
  qrX = 8; qrY = 11;
  if (online) {
    esp_qrcode_config_t cfg = { qrDraw, 5, ESP_QRCODE_ECC_MED };
    esp_qrcode_generate(&cfg, url.c_str());
  } else { gfx->drawRoundRect(8, 11, 150, 150, 6, C_DKGREY); ftC(92, "Offline", F_S, C_GREY, 166); }
  int x = 172, w = gfx->width() - x - 6;
  ft(x, 22, "Scan to open", F_S, C_GREY);
  ft(x, 40, "the dashboard", F_S, C_GREY);
  ft(x, 66, online ? WiFi.localIP().toString() : String("No Wi-Fi"), F_S, C_WHITE);
  ft(x, 84, HOSTNAME ".local", F_XS2, C_GREY);
  ft(x, 104, "Running v" FW_VERSION, F_XS2, C_GREY);
  String st; uint16_t sc = C_GREY;
  if (devChecking) st = "Checking...";
  else if (updAvail) { st = "v" + updVer + " available!"; sc = C_GREEN; }
  else if (updErr.length()) { st = updErr; sc = C_RED; }
  else if (updCheckedAt) st = "Up to date";
  ft(x, 122, fit(st, F_XS2, w), F_XS2, sc);
  // the button
  String b = updAvail ? "Install v" + updVer : String("Check for update");
  gfx->fillRoundRect(x, 130, w, 34, 8, updAvail ? 0x2589 : 0x2104);
  ft(x + (w - tw(b, F_XS2)) / 2, 152, b, F_XS2, C_WHITE);
}
void openDevice() { deviceOpen = true; menuOpen = true; devShownCheck = updCheckedAt; gfx->fillScreen(C_BG); drawDevice(); }

void drawApp() {
  if (deviceOpen) { drawDevice(); return; }
  if (menuOpen) { drawMenu(); return; }
  if (app == APP_WX) drawWeather();
  else if (app == APP_SP) drawSpotify();
  else drawView();
}

void openApp(int a) {
  app = a; menuOpen = false; deviceOpen = false;
  prefs.begin("subcounter", false); prefs.putInt("app", app); prefs.end();
  if (app == APP_SP) { lastSpPoll = 0; spFails = 0; spWait = 3000; }
  if (app == APP_WX && !wx.ok) lastWxFetch = 0;
  gfx->fillScreen(C_BG);
  drawApp();
}
