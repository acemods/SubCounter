/*
 * SubCounter v6 — YouTube subscriber counter for Waveshare ESP32-C6-Touch-LCD-1.47
 *
 *  NAVIGATION
 *    Swipe left / right ... next / previous channel   (BOOT short press = next)
 *    Swipe up ............. more detail for this channel:
 *                             1 Overview (profile picture, views, videos, joined)
 *                             2 Latest video (views, likes, comments, LIVE badge)
 *                             3 Growth (today / 7 days / 30 days + graph)
 *                             4 Next milestone (progress + predicted date)
 *    Swipe down ........... back up; from the main count it opens the Leaderboard
 *    Hold BOOT 3 s ........ setup mode
 *    After 2 minutes untouched it returns to the main count.
 *
 *  Subscriber history is stored on the board (LittleFS) so growth survives restarts.
 *  "est." numbers are estimates between YouTube's rounded steps (can be turned off).
 *
 *  Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *    Flash Size "8MB", Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
 *    Libraries: "GFX Library for Arduino" 1.6.x, "ArduinoJson" 7.x, "JPEGDEC" 1.8.x
 */

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <JPEGDEC.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <LittleFS.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <time.h>
#include <sys/time.h>

// ── Pins (ESP32-C6 version of the board) ────────────────────────────────────
#define LCD_SCK   1
#define LCD_MOSI  2
#define LCD_CS    14
#define LCD_DC    15
#define LCD_RST   22
#define LCD_BL    23
#define SD_CS     4
#define BOOT_BTN  9
#define TP_SDA    18
#define TP_SCL    19
#define TP_RST    20
#define TP_ADDR   0x63

// ── Settings ────────────────────────────────────────────────────────────────
#define AP_NAME            "SubCounter-Setup"
#define MAX_CH             10
#define REFRESH_MS         (2UL * 60UL * 1000UL)    // channel stats: 1 API unit
#define VIDEO_REFRESH_MS   (15UL * 60UL * 1000UL)   // latest videos: channels+1 units
#define AUTO_SWITCH_MS     10000UL
#define IDLE_RETURN_MS     120000UL
#define ALERT_MS           3500UL
#define WIFI_TIMEOUT_MS    20000UL
#define DHCP_EXTRA_MS      15000UL
#define HOLD_FOR_SETUP_MS  3000UL
#define SWIPE_MIN_PX       35
#define NUM_CARDS          5          // 0 main, 1 overview, 2 latest video, 3 growth, 4 milestone
#define HIST_MAX_AGE       (31L * 86400L)
#define TZ_UK              "GMT0BST,M3.5.0/1,M10.5.0/2"

// ── Display ─────────────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, false, 172, 320, 34, 0, 34, 0);
JPEGDEC jpeg;

#define C_BG      0x0000
#define C_RED     0xF800
#define C_WHITE   0xFFFF
#define C_GREY    0x8410
#define C_DKGREY  0x39E7
#define C_GOLD    0xFEA0
#define C_PURPLE  0x901F
#define C_GREEN   0x07E0
#define C_BLUE    0x4D7F

// ── Channels ────────────────────────────────────────────────────────────────
struct Channel {
  String handle, id, title, country, uploads, avatarUrl;
  long   subs = -1;
  long long views = -1;
  long   videos = -1;
  time_t joined = 0;
  String err;
  uint8_t *avatar = nullptr; int avatarLen = 0;
  // latest video
  String vidId, vidTitle;
  time_t vidPublished = 0;
  long vidDuration = 0;
  long long vidViews = -1; long vidLikes = -1, vidComments = -1;
  bool live = false; long liveViewers = -1;
  // history-derived
  time_t stepChangedAt = 0;
  bool statsOk = false;
  long gainToday = 0, gain7 = 0, gain30 = 0;
  float span7Days = 0, span30Days = 0, ratePerDay = 0;
  time_t histStart = 0, lastSampleT = 0; long lastSampleS = -1;
};
Channel ch[MAX_CH];
int numCh = 0;
int page = 0;              // channel on screen
int card = 0;              // 0..NUM_CARDS-1
bool board = false;        // leaderboard showing
long shownSubs = -1;

struct Alert { int idx; long delta; long total; };
Alert alerts[MAX_CH];
int numAlerts = 0;

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

String cfgSsid, cfgPass, cfgChannels, cfgApiKey, cfgUser;
String cfgIp, cfgGw, cfgMask, cfgDns;
bool   cfgCompat = false, cfgAuto = false, cfgEst = true;
bool   usedCompatThisBoot = false;
String wifiFailReason = "";
volatile int lastDiscReason = 0;
volatile bool staAssociated = false;
bool portalMode = false;
String scanOptions;
bool fsOk = false;

String netError = "";
unsigned long lastFetch = 0, lastVideoFetch = 0, lastFetchOk = 0, lastInteract = 0;
unsigned long btnDownAt = 0;

// ════════════════════════════════════════════════════════════════════════════
//  Panel init (JD9853 register setup)
// ════════════════════════════════════════════════════════════════════════════
void lcdRegInit() {
  static const uint8_t ops[] = {
    BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE, DELAY, 120,
    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8,  0xB2, 0x23,
    WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
    WRITE_COMMAND_8, 0xBB, WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8,  0xC1, 0x16,
    WRITE_COMMAND_8, 0xC3, WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    WRITE_COMMAND_8, 0xC4, WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A,
                                            0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    WRITE_COMMAND_8, 0xC8, WRITE_BYTES, 32,
      0x3F,0x32,0x29,0x29,0x27,0x2B,0x27,0x28,0x28,0x26,0x25,0x17,0x12,0x0D,0x04,0x00,
      0x3F,0x32,0x29,0x29,0x27,0x2B,0x27,0x28,0x28,0x26,0x25,0x17,0x12,0x0D,0x04,0x00,
    WRITE_COMMAND_8, 0xD0, WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8,  0xE6, 0x14,
    WRITE_C8_D8,  0xDE, 0x01,
    WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    WRITE_COMMAND_8, 0xC1, WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8,  0xBE, 0x00,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x01, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x35, 0x00,
    WRITE_C8_D8,  0x3A, 0x05,
    WRITE_COMMAND_8, 0x2A, WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
    WRITE_COMMAND_8, 0x2B, WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE, DELAY, 10,
    BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE
  };
  bus->batchOperation(ops, sizeof(ops));
}


// ════════════════════════════════════════════════════════════════════════════
//  Small helpers
// ════════════════════════════════════════════════════════════════════════════
void centreText(const String &txt, int y, uint8_t size, uint16_t colour) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour, C_BG);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(txt.c_str(), 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(max(0, (int)(gfx->width() - w) / 2), y);
  gfx->print(txt);
}

void textAt(int x, int y, const String &t, uint8_t size, uint16_t colour) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour, C_BG);
  gfx->setCursor(x, y);
  gfx->print(t);
}

void textRight(int xRight, int y, const String &t, uint8_t size, uint16_t colour) {
  textAt(xRight - (int)t.length() * 6 * size, y, t, size, colour);
}

String withCommas(long long n) {
  bool neg = n < 0; if (neg) n = -n;
  String raw = String((unsigned long long)n), out = neg ? "-" : "";
  int len = raw.length();
  for (int i = 0; i < len; i++) {
    out += raw[i];
    int left = len - i - 1;
    if (left > 0 && left % 3 == 0) out += ',';
  }
  return out;
}

String compact(long long n) {
  if (n < 0) return "-";
  if (n < 10000) return withCommas(n);
  const char *suf[] = {"K", "M", "B"};
  double v = n; int i = -1;
  while (v >= 1000 && i < 2) { v /= 1000; i++; }
  char buf[16];
  if (v >= 100) snprintf(buf, sizeof(buf), "%.0f%s", v, suf[i]);
  else if (v >= 10) snprintf(buf, sizeof(buf), "%.1f%s", v, suf[i]);
  else snprintf(buf, sizeof(buf), "%.2f%s", v, suf[i]);
  return String(buf);
}

String signedNum(long n) { return (n >= 0 ? "+" : "") + withCommas(n); }

uint8_t sizeForText(const String &s, int maxSize, int width) {
  for (int sz = maxSize; sz > 1; sz--)
    if ((int)s.length() * 6 * sz <= width) return sz;
  return 1;
}

// Word-wrapped text; returns the y after the last line. maxLines 0 = unlimited.
int wrapText(const String &msg, int x0, int y, int xMax, uint8_t ts, uint16_t colour, int maxLines = 0) {
  int cw = 6 * ts, lh = 8 * ts + 3, x = x0, lines = 1;
  gfx->setTextSize(ts);
  gfx->setTextColor(colour, C_BG);
  String word, text = msg + " ";
  for (size_t i = 0; i < text.length(); i++) {
    if (text[i] == ' ') {
      if (x + (int)word.length() * cw > xMax && x > x0) {
        if (maxLines && lines >= maxLines) { gfx->setCursor(x, y); gfx->print("..."); return y + lh; }
        x = x0; y += lh; lines++;
      }
      while ((int)word.length() * cw > xMax - x0) word.remove(word.length() - 1);   // very long word
      gfx->setCursor(x, y); gfx->print(word); x += (word.length() + 1) * cw; word = "";
    } else word += text[i];
  }
  return y + lh;
}

String boardMac() {
  uint8_t m[6];
  esp_read_mac(m, ESP_MAC_WIFI_STA);
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(buf);
}

String htmlEscape(const String &s) {
  String o; o.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;"; else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;"; else o += c;
  }
  return o;
}

String urlEncode(const String &s) {
  String o; const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else { o += '%'; o += hex[(c >> 4) & 0xF]; o += hex[c & 0xF]; }
  }
  return o;
}

bool looksLikeId(const String &s) { return s.startsWith("UC") && s.length() == 24; }

// Strip characters the built-in font can't show (emoji etc.)
String asciiOnly(const String &s) {
  String o; o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (c >= 32 && c < 127) o += (char)c;
    else if (c >= 0xC0) { o += ' '; }    // start of a multi-byte character
  }
  o.trim();
  while (o.indexOf("  ") >= 0) o.replace("  ", " ");
  return o;
}

// ── Time ────────────────────────────────────────────────────────────────────
static long daysFromCivil(int y, unsigned m, unsigned d) {
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const unsigned yoe = (unsigned)(y - era * 400);
  const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long)doe - 719468;
}

time_t parseIso(const String &s) {     // 2026-10-06T07:12:33Z
  int Y, M, D, h = 0, mi = 0, se = 0;
  if (sscanf(s.c_str(), "%d-%d-%dT%d:%d:%d", &Y, &M, &D, &h, &mi, &se) < 3) return 0;
  return (time_t)daysFromCivil(Y, M, D) * 86400 + h * 3600 + mi * 60 + se;
}

time_t parseHttpDate(const String &s) { // Tue, 06 Oct 2026 08:39:00 GMT
  int d, y, h, mi, se; char mon[4] = {0};
  if (sscanf(s.c_str(), "%*3s, %d %3s %d %d:%d:%d", &d, mon, &y, &h, &mi, &se) != 6) return 0;
  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  const char *p = strstr(months, mon);
  if (!p) return 0;
  return (time_t)daysFromCivil(y, (p - months) / 3 + 1, d) * 86400 + h * 3600 + mi * 60 + se;
}

bool timeValid() { return time(nullptr) > 1700000000; }
time_t nowT() { return time(nullptr); }

void setClock(time_t t) {
  if (t < 1700000000) return;
  if (abs((long)(t - nowT())) < 30) return;
  struct timeval tv = { t, 0 };
  settimeofday(&tv, nullptr);
  Serial.println("Clock set from server time");
}

time_t localMidnight() {
  time_t n = nowT();
  struct tm lt; localtime_r(&n, &lt);
  lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0;
  return mktime(&lt);
}

String ago(time_t t) {
  if (!timeValid() || !t) return "";
  long d = nowT() - t;
  if (d < 0) d = 0;
  if (d < 3600) return String(max(1L, d / 60)) + " min ago";
  if (d < 86400) return String(d / 3600) + (d / 3600 == 1 ? " hour ago" : " hours ago");
  if (d < 86400L * 30) return String(d / 86400) + (d / 86400 == 1 ? " day ago" : " days ago");
  if (d < 86400L * 365) return String(d / (86400L * 30)) + " months ago";
  return String(d / (86400L * 365)) + " years ago";
}

String dateStr(time_t t, const char *fmt) {
  struct tm lt; localtime_r(&t, &lt);
  char buf[24]; strftime(buf, sizeof(buf), fmt, &lt);
  return String(buf);
}

long parseDuration(const String &s) {   // PT1H2M3S
  long total = 0, num = 0;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isdigit((unsigned char)c)) num = num * 10 + (c - '0');
    else { if (c == 'H') total += num * 3600; else if (c == 'M') total += num * 60; else if (c == 'S') total += num; else if (c == 'D') total += num * 86400; num = 0; }
  }
  return total;
}

String durationStr(long s) {
  char buf[16];
  if (s >= 3600) snprintf(buf, sizeof(buf), "%ld:%02ld:%02ld", s / 3600, (s / 60) % 60, s % 60);
  else snprintf(buf, sizeof(buf), "%ld:%02ld", s / 60, s % 60);
  return String(buf);
}

// YouTube shows 3 significant figures: step between possible public values
long stepFor(long n) {
  if (n < 1000) return 1;
  long step = 1; long v = n;
  while (v >= 1000) { v /= 10; step *= 10; }
  return step;
}

// ════════════════════════════════════════════════════════════════════════════
//  Settings
// ════════════════════════════════════════════════════════════════════════════
void parseChannels() {
  numCh = 0;
  String list = cfgChannels + "\n";
  list.replace(",", "\n");
  list.replace("\r", "");
  int start = 0;
  while (numCh < MAX_CH) {
    int nl = list.indexOf('\n', start);
    if (nl < 0) break;
    String h = list.substring(start, nl); h.trim();
    start = nl + 1;
    if (!h.length()) continue;
    int at = h.indexOf("/@");          if (at >= 0) h = h.substring(at + 1);
    int cp = h.indexOf("/channel/");   if (cp >= 0) h = h.substring(cp + 9);
    int sl = h.indexOf('/');           if (sl > 0) h = h.substring(0, sl);
    int q  = h.indexOf('?');           if (q > 0)  h = h.substring(0, q);
    if (!looksLikeId(h) && !h.startsWith("@")) h = "@" + h;
    ch[numCh] = Channel();
    ch[numCh].handle = h;
    if (looksLikeId(h)) ch[numCh].id = h;
    numCh++;
  }
  if (page >= numCh) page = 0;
}

void loadSettings() {
  prefs.begin("subcounter", true);
  cfgSsid     = prefs.getString("ssid", "");
  cfgPass     = prefs.getString("pass", "");
  cfgChannels = prefs.getString("channels", prefs.getString("channel", ""));
  cfgApiKey   = prefs.getString("apikey", "");
  cfgUser     = prefs.getString("user", "");
  cfgIp       = prefs.getString("ip", "");
  cfgGw       = prefs.getString("gw", "");
  cfgMask     = prefs.getString("mask", "");
  cfgDns      = prefs.getString("dns", "");
  cfgCompat   = prefs.getBool("compat", false);
  cfgAuto     = prefs.getBool("auto", false);
  cfgEst      = prefs.getBool("est", true);
  prefs.end();
  parseChannels();
}

void saveSettings() {
  prefs.begin("subcounter", false);
  prefs.putString("ssid", cfgSsid);
  prefs.putString("pass", cfgPass);
  prefs.putString("channels", cfgChannels);
  prefs.putString("apikey", cfgApiKey);
  prefs.putString("user", cfgUser);
  prefs.putString("ip", cfgIp);
  prefs.putString("gw", cfgGw);
  prefs.putString("mask", cfgMask);
  prefs.putString("dns", cfgDns);
  prefs.putBool("compat", cfgCompat);
  prefs.putBool("auto", cfgAuto);
  prefs.putBool("est", cfgEst);
  prefs.end();
}

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
  long baseToday = -1, base7 = -1, base30 = -1; time_t t7 = 0, t30 = 0, tToday = 0;
  bool first = true;
  while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
    if (first) { c.histStart = s.t; first = false; }
    if ((time_t)s.t < mid) beforeMid = s;
    else if (!haveToday) { haveToday = true; baseToday = beforeMid.s >= 0 ? beforeMid.s : s.s; tToday = beforeMid.s >= 0 ? beforeMid.t : s.t; }
    if (!have30 && (time_t)s.t >= now - 30L * 86400) { have30 = true; base30 = s.s; t30 = s.t; }
    if (!have7 && (time_t)s.t >= now - 7L * 86400)   { have7 = true;  base7 = s.s;  t7 = s.t; }
  }
  f.close();
  if (first) return;
  if (!haveToday) { baseToday = beforeMid.s >= 0 ? beforeMid.s : c.subs; tToday = beforeMid.t; }
  c.gainToday = c.subs - baseToday;
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
  // keep files small: drop samples older than 31 days once the file passes ~1000 samples
  if (size > 1000 * sizeof(Sample)) {
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
  }
}

// Estimated live count between YouTube's rounded steps
long estimateFor(int i) {
  Channel &c = ch[i];
  if (!cfgEst || c.subs < 0 || !timeValid() || c.ratePerDay <= 0 || !c.stepChangedAt) return c.subs;
  long step = stepFor(c.subs);
  if (step <= 1) return c.subs;
  long add = (long)(c.ratePerDay * (nowT() - c.stepChangedAt) / 86400.0f);
  if (add > step - 1) add = step - 1;
  if (add < 0) add = 0;
  return c.subs + add;
}
bool isEstimated(int i) { return estimateFor(i) != ch[i].subs || (cfgEst && stepFor(ch[i].subs) > 1 && ch[i].ratePerDay > 0); }

// ════════════════════════════════════════════════════════════════════════════
//  Avatar drawing (JPEG, round-cropped)
// ════════════════════════════════════════════════════════════════════════════
int avCx, avCy, avR;
int jpegDraw(JPEGDRAW *p) {
  for (int row = 0; row < p->iHeight; row++) {
    int y = p->y + row;
    int dy = y - avCy;
    if (dy * dy > avR * avR) continue;
    int half = (int)sqrtf((float)(avR * avR - dy * dy));
    int x0 = max(p->x, avCx - half), x1 = min(p->x + p->iWidthUsed - 1, avCx + half);
    if (x1 < x0) continue;
    gfx->draw16bitRGBBitmap(x0, y, p->pPixels + row * p->iWidth + (x0 - p->x), x1 - x0 + 1, 1);
  }
  return 1;
}

// Draw a channel's picture at (x,y), full 88px or half size 44px
bool drawAvatar(int i, int x, int y, bool half) {
  Channel &c = ch[i];
  if (!c.avatar) return false;
  if (!jpeg.openRAM(c.avatar, c.avatarLen, jpegDraw)) return false;
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  int w = jpeg.getWidth() / (half ? 2 : 1);
  avCx = x + w / 2; avCy = y + w / 2; avR = w / 2;
  jpeg.decode(x, y, half ? JPEG_SCALE_HALF : 0);
  jpeg.close();
  return true;
}

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

String nameOf(int i) { String t = ch[i].title.length() ? ch[i].title : ch[i].handle; return asciiOnly(t); }

// Vertical card position dots on the right edge
void drawCardDots() {
  int x = gfx->width() - 6, y0 = 86 - (NUM_CARDS - 1) * 6;
  for (int k = 0; k < NUM_CARDS; k++)
    if (k == card) gfx->fillCircle(x, y0 + k * 12, 2, C_WHITE);
    else gfx->drawCircle(x, y0 + k * 12, 2, C_DKGREY);
}

// Header used by the detail cards
void drawDetailHeader(const char *label) {
  gfx->fillScreen(C_BG);
  String t = nameOf(page);
  if (t.length() > 22) t = t.substring(0, 21) + ".";
  textAt(10, 6, t, 2, C_WHITE);
  textAt(10, 26, label, 1, C_GOLD);
  gfx->drawFastHLine(10, 37, gfx->width() - 26, C_DKGREY);
  drawCardDots();
}

// ── Card 0: main count ──────────────────────────────────────────────────────
void drawMainHeader() {
  gfx->fillRect(0, 0, gfx->width() - 12, 54, C_BG);
  if (!numCh) return;
  if (!drawAvatar(page, 10, 5, true)) {
    gfx->fillRoundRect(12, 12, 40, 28, 8, C_RED);
    gfx->fillTriangle(26, 18, 26, 34, 40, 26, C_WHITE);
  }
  Channel &c = ch[page];
  String t = nameOf(page);
  if (t.length() > 20) t = t.substring(0, 19) + ".";
  textAt(62, 13, t, 2, C_WHITE);
  String sub = "subscribers";
  textAt(62, 34, sub, 1, C_GREY);
  if (c.statsOk && c.gainToday != 0) textAt(62 + 12 * 6, 34, signedNum(c.gainToday) + " today", 1, c.gainToday > 0 ? C_GREEN : C_RED);
  if (numCh > 1) textRight(gfx->width() - 14, 34, String(page + 1) + "/" + String(numCh), 1, C_GREY);
  gfx->drawFastHLine(12, 52, gfx->width() - 26, C_DKGREY);
}

void drawMainNumber(long n) {
  gfx->fillRect(0, 56, gfx->width() - 12, 78, C_BG);
  if (!numCh) return;
  Channel &c = ch[page];
  if (c.err.length() && c.subs < 0) { wrapText(c.err, 14, 74, gfx->width() - 20, 2, C_RED); return; }
  if (n < 0) { centreText("...", 78, 5, C_GREY); return; }
  String s = withCommas(n);
  uint8_t sz = sizeForText(s, 7, gfx->width() - 30);
  centreText(s, 96 - (sz * 8) / 2, sz, C_WHITE);
  if (isEstimated(page)) textRight(gfx->width() - 16, 124, "est.", 1, C_GREY);
}

void drawChannelDots() {
  gfx->fillRect(0, 136, gfx->width() - 12, 10, C_BG);
  if (numCh < 2) return;
  int gap = 12, x = (gfx->width() - (numCh - 1) * gap) / 2;
  for (int i = 0; i < numCh; i++)
    if (i == page) gfx->fillCircle(x + i * gap, 141, 3, C_WHITE);
    else gfx->drawCircle(x + i * gap, 141, 3, C_GREY);
}

void drawFooter() {
  if (board || card != 0) return;
  gfx->fillRect(0, 150, gfx->width() - 12, 22, C_BG);
  gfx->setCursor(12, 158);
  if (netError.length()) { textAt(12, 158, netError.substring(0, 48), 1, C_RED); return; }
  if (lastFetchOk) {
    unsigned long mins = (millis() - lastFetchOk) / 60000UL;
    textAt(12, 158, mins == 0 ? String("Updated just now") : "Updated " + String(mins) + " min ago", 1, C_GREY);
  }
  String ip = WiFi.localIP().toString() + (usedCompatThisBoot ? " (W4)" : "");
  textRight(gfx->width() - 14, 158, ip, 1, C_GREY);
}

void drawMain() {
  gfx->fillScreen(C_BG);
  if (!numCh) { centreText("No channels set", 70, 2, C_GREY); return; }
  shownSubs = estimateFor(page);
  drawMainHeader();
  drawMainNumber(shownSubs);
  drawChannelDots();
  drawFooter();
  drawCardDots();
}

// ── Card 1: overview ────────────────────────────────────────────────────────
void drawOverview() {
  Channel &c = ch[page];
  drawDetailHeader("OVERVIEW");
  if (!drawAvatar(page, 10, 46, false)) gfx->fillCircle(54, 90, 44, C_DKGREY);
  int x = 110, xr = gfx->width() - 16, y = 46;
  struct { const char *k; String v; } rows[] = {
    { "Subscribers", c.subs >= 0 ? compact(c.subs) : String("-") },
    { "Total views", compact(c.views) },
    { "Videos",      c.videos >= 0 ? withCommas(c.videos) : String("-") },
    { "Avg views",   (c.videos > 0 && c.views >= 0) ? compact(c.views / c.videos) : String("-") },
  };
  for (auto &r : rows) {
    textAt(x, y + 4, r.k, 1, C_GREY);
    textRight(xr, y, r.v, 2, C_WHITE);
    y += 22;
  }
  String joined = c.joined ? "Joined " + dateStr(c.joined, "%b %Y") : "";
  if (c.country.length()) joined += (joined.length() ? "  -  " : "") + c.country;
  textAt(x, 140, joined, 1, C_GREY);
  if (c.joined && timeValid()) {
    long yrs = (nowT() - c.joined) / (365L * 86400);
    textAt(x, 154, yrs >= 1 ? String(yrs) + (yrs == 1 ? " year" : " years") + " on YouTube" : String("New this year"), 1, C_DKGREY);
  }
}

// ── Card 2: latest video ────────────────────────────────────────────────────
void drawLatestVideo() {
  Channel &c = ch[page];
  drawDetailHeader(c.live ? "LIVE NOW" : "LATEST VIDEO");
  if (!c.vidId.length()) { centreText(c.uploads.length() ? "Loading..." : "No videos", 80, 2, C_GREY); return; }
  int y = 44;
  if (c.live) {
    gfx->fillRoundRect(10, y, 50, 18, 4, C_RED);
    textAt(17, y + 2, "LIVE", 2, C_WHITE);
    if (c.liveViewers >= 0) textAt(68, y + 2, withCommas(c.liveViewers) + " watching", 2, C_WHITE);
    y += 24;
  }
  y = wrapText(asciiOnly(c.vidTitle), 10, y, gfx->width() - 18, 2, C_WHITE, c.live ? 1 : 2);
  String meta = ago(c.vidPublished);
  if (c.vidDuration > 0 && !c.live) meta += (meta.length() ? "  -  " : "") + durationStr(c.vidDuration);
  textAt(10, y, meta, 1, C_GREY);
  // stats row
  int colW = (gfx->width() - 26) / 3, sy = 128;
  struct { const char *k; String v; uint16_t col; } st[] = {
    { "views",    compact(c.vidViews), C_WHITE },
    { "likes",    c.vidLikes >= 0 ? compact(c.vidLikes) : String("hidden"), C_GREEN },
    { "comments", c.vidComments >= 0 ? compact(c.vidComments) : String("off"), C_BLUE },
  };
  for (int k = 0; k < 3; k++) {
    int cx = 10 + k * colW;
    textAt(cx, sy, st[k].v, 2, st[k].col);
    textAt(cx, sy + 20, st[k].k, 1, C_GREY);
  }
}

// ── Card 3: growth ──────────────────────────────────────────────────────────
void drawGrowth() {
  Channel &c = ch[page];
  drawDetailHeader("GROWTH");
  if (!c.statsOk || !c.histStart) {
    centreText("Collecting data...", 70, 2, C_GREY);
    centreText("The board records the count every hour.", 100, 1, C_DKGREY);
    centreText("Check back later today.", 114, 1, C_DKGREY);
    return;
  }
  int colW = (gfx->width() - 26) / 3;
  struct { const char *k; long v; float span; } g[] = {
    { "today", c.gainToday, 1 }, { "7 days", c.gain7, c.span7Days }, { "30 days", c.gain30, c.span30Days } };
  for (int k = 0; k < 3; k++) {
    int cx = 10 + k * colW;
    textAt(cx, 44, signedNum(g[k].v), 2, g[k].v > 0 ? C_GREEN : (g[k].v < 0 ? C_RED : C_WHITE));
    String lbl = g[k].k;
    if (k > 0 && g[k].span < (k == 1 ? 6.5f : 29.5f) && g[k].span > 0) lbl += " (" + String(g[k].span, 1) + "d)";
    textAt(cx, 63, lbl, 1, C_GREY);
  }
  // 7-day graph
  int gx = 10, gy = 78, gw = gfx->width() - 28, gh = 66;
  gfx->drawRect(gx, gy, gw, gh, C_DKGREY);
  File f = LittleFS.open(histPath(page), "r");
  time_t now = nowT(), from = now - 7L * 86400;
  long mn = LONG_MAX, mx = LONG_MIN;
  Sample s;
  if (f) {
    while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s))
      if ((time_t)s.t >= from) { mn = min(mn, (long)s.s); mx = max(mx, (long)s.s); }
    mn = min(mn, c.subs); mx = max(mx, c.subs);
    if (mx == mn) { mx += 1; mn -= 1; }
    f.seek(0);
    int px = -1, py = -1;
    while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
      if ((time_t)s.t < from) continue;
      int x = gx + 1 + (int)((float)(s.t - from) / (7L * 86400) * (gw - 3));
      int y = gy + gh - 2 - (int)((float)(s.s - mn) / (mx - mn) * (gh - 4));
      if (px >= 0) gfx->drawLine(px, py, x, y, C_GREEN); else gfx->fillCircle(x, y, 1, C_GREEN);
      px = x; py = y;
    }
    f.close();
  }
  textAt(gx + 3, gy + 3, compact(mx), 1, C_DKGREY);
  textAt(gx + 3, gy + gh - 11, compact(mn), 1, C_DKGREY);
  String rate = c.ratePerDay != 0 ? String("~") + signedNum(lroundf(c.ratePerDay)) + "/day" : String("");
  textAt(10, 150, "Last 7 days", 1, C_GREY);
  textRight(gfx->width() - 18, 150, rate, 1, C_WHITE);
  textAt(10, 162, "Tracking since " + dateStr(c.histStart, "%d %b %H:%M"), 1, C_DKGREY);
}

// ── Card 4: next milestone ──────────────────────────────────────────────────
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
  return 2 * p;   // lead 5
}

void drawMilestone() {
  Channel &c = ch[page];
  drawDetailHeader("NEXT MILESTONE");
  if (c.subs < 0) { centreText("...", 80, 3, C_GREY); return; }
  long cur = estimateFor(page);
  long goal = nextMilestone(cur), prev = prevMilestone(goal);
  String g = withCommas(goal);
  centreText(g, 46, sizeForText(g, 5, gfx->width() - 30), C_GOLD);
  // progress bar from previous milestone to next
  int bx = 14, by = 92, bw = gfx->width() - 34, bh = 14;
  float frac = (float)(cur - prev) / (float)(goal - prev);
  frac = constrain(frac, 0.0f, 1.0f);
  gfx->drawRoundRect(bx, by, bw, bh, 6, C_DKGREY);
  gfx->fillRoundRect(bx + 2, by + 2, max(6, (int)((bw - 4) * frac)), bh - 4, 4, C_GOLD);
  textAt(bx, by + 18, compact(prev), 1, C_GREY);
  textRight(bx + bw, by + 18, compact(goal), 1, C_GREY);
  centreText(withCommas(goal - cur) + " to go", 124, 2, C_WHITE);
  String eta;
  if (c.ratePerDay > 0.01f && timeValid()) {
    float days = (goal - cur) / c.ratePerDay;
    if (days < 1) eta = "Expected today";
    else if (days > 3650) eta = "Expected in 10+ years";
    else eta = "Expected around " + dateStr(nowT() + (time_t)(days * 86400), days > 300 ? "%b %Y" : "%d %b");
  } else eta = c.ratePerDay < 0 ? "Losing subscribers right now" : "Need more data to predict a date";
  centreText(eta, 150, 1, C_GREY);
  if (isEstimated(page)) textRight(gfx->width() - 16, 162, "est.", 1, C_DKGREY);
}

// ── Leaderboard ─────────────────────────────────────────────────────────────
void drawBoard() {
  gfx->fillScreen(C_BG);
  textAt(10, 6, "LEADERBOARD", 2, C_GOLD);
  textRight(gfx->width() - 10, 10, "today", 1, C_GREY);
  int idx[MAX_CH]; for (int i = 0; i < numCh; i++) idx[i] = i;
  for (int a = 0; a < numCh; a++) for (int b = a + 1; b < numCh; b++)
    if (ch[idx[b]].subs > ch[idx[a]].subs) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  int rows = numCh, lh = rows > 7 ? 14 : 18, y = 28;
  for (int r = 0; r < rows; r++) {
    int i = idx[r];
    bool mine = (i == 0);   // first channel in the list = yours
    if (i == page) gfx->fillRect(6, y - 2, gfx->width() - 12, lh - 2, 0x18E3);
    uint16_t col = mine ? C_GOLD : C_WHITE;
    textAt(10, y + 2, String(r + 1) + ".", 1, C_GREY);
    String n = nameOf(i); if (n.length() > 22) n = n.substring(0, 21) + ".";
    textAt(30, y + 2, n, 1, col);
    textRight(gfx->width() - 70, y + 2, ch[i].subs >= 0 ? compact(ch[i].subs) : String("-"), 1, col);
    if (ch[i].statsOk) textRight(gfx->width() - 10, y + 2, signedNum(ch[i].gainToday), 1, ch[i].gainToday > 0 ? C_GREEN : C_GREY);
    y += lh;
  }
  textAt(10, 162, "Swipe up to go back", 1, C_DKGREY);
}

void drawView() {
  if (!numCh) { drawMain(); return; }
  if (board) { drawBoard(); return; }
  switch (card) {
    case 1: drawOverview(); break;
    case 2: drawLatestVideo(); break;
    case 3: drawGrowth(); break;
    case 4: drawMilestone(); break;
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
  if (numCh < 2) return;
  page = (page + dir + numCh) % numCh;
  wipe(dir, 0);
  drawView();
}

void changeCard(int dir) {   // dir +1 = deeper (swipe up)
  if (board) { if (dir > 0) { board = false; card = 0; wipe(0, 1); drawView(); } return; }
  if (dir < 0 && card == 0) { board = true; wipe(0, -1); drawView(); return; }
  int nc = constrain(card + dir, 0, NUM_CARDS - 1);
  if (nc == card) return;
  card = nc;
  wipe(0, dir);
  drawView();
}

void showAlert(const Alert &a) {
  for (int i = 0; i < 4; i++) { gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD); delay(100); }
  gfx->fillScreen(C_BG);
  drawAvatar(a.idx, 10, 8, true);
  textAt(62, 12, "NEW SUBSCRIBERS!", 2, C_GOLD);
  String name = nameOf(a.idx);
  textAt(62, 34, name.length() > 40 ? name.substring(0, 39) + "." : name, 1, C_WHITE);
  String d = "+" + withCommas(a.delta);
  centreText(d, 66, sizeForText(d, 5, gfx->width() - 20), C_GREEN);
  centreText("now " + withCommas(a.total), 132, 2, C_GREY);
  unsigned long start = millis();
  while (millis() - start < ALERT_MS) { server.handleClient(); delay(20); }
}

// ════════════════════════════════════════════════════════════════════════════
//  Touch (AXS5106L) — polled; swipe detection in 4 directions
// ════════════════════════════════════════════════════════════════════════════
bool touchOk = false, touching = false;
int tStartX, tStartY, tLastX, tLastY;
unsigned long tStartAt = 0, lastTouchPoll = 0;

void touchInit() {
  Wire.begin(TP_SDA, TP_SCL);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(200);
  digitalWrite(TP_RST, HIGH); delay(300);
  Wire.beginTransmission(TP_ADDR);
  touchOk = (Wire.endTransmission() == 0);
  Serial.printf("Touch controller %s\n", touchOk ? "found" : "NOT found");
}

bool touchRead(int &x, int &y) {
  uint8_t d[14] = {0};
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)14) != 14) return false;
  Wire.readBytes(d, 14);
  if (d[1] == 0 || d[1] > 5) return false;
  uint16_t rawX = ((uint16_t)(d[2] & 0x0F) << 8) | d[3];
  uint16_t rawY = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
  x = rawY;   // landscape (rotation 1): screen X = panel Y, screen Y = panel X
  y = rawX;
  return true;
}

// Returns 'L','R','U','D' for a completed swipe, 0 otherwise.
char pollSwipe() {
  if (!touchOk || millis() - lastTouchPoll < 20) return 0;
  lastTouchPoll = millis();
  int x, y;
  if (touchRead(x, y)) {
    if (!touching) { touching = true; tStartX = x; tStartY = y; tStartAt = millis(); }
    tLastX = x; tLastY = y;
    return 0;
  }
  if (!touching) return 0;
  touching = false;
  int dx = tLastX - tStartX, dy = tLastY - tStartY;
  if (millis() - tStartAt > 1500) return 0;
  if (abs(dx) >= abs(dy) && abs(dx) >= SWIPE_MIN_PX) return dx < 0 ? 'L' : 'R';
  if (abs(dy) > abs(dx) && abs(dy) >= SWIPE_MIN_PX * 2 / 3) return dy < 0 ? 'U' : 'D';
  return 0;
}

// ════════════════════════════════════════════════════════════════════════════
//  Web pages
// ════════════════════════════════════════════════════════════════════════════
const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter setup</title><style>
body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{max-width:440px;margin:0 auto;background:#1c1c1e;border-radius:14px;padding:22px}
h1{font-size:22px;margin:0 0 4px}p{color:#999;font-size:14px;margin:0 0 18px}
label{display:block;font-size:13px;color:#aaa;margin:14px 0 6px}
input,select,textarea{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #333;background:#000;color:#fff;font-size:16px;font-family:inherit}
textarea{min-height:150px}
button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:10px;background:#e62117;color:#fff;font-size:17px;font-weight:600}
small{display:block;color:#777;font-size:12px;margin-top:6px}a{color:#4ea1ff}
.st{font-size:13px;color:#aaa;margin-top:6px}.st b{color:#fff}
</style></head><body><div class="card">)HTML";


void handleRoot() {
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter setup</h1><p>Saved on the board only.</p>";
  if (wifiFailReason.length()) {
    h += "<div style='background:#3a1210;border:1px solid #e62117;border-radius:10px;padding:12px;margin-bottom:8px;font-size:14px'>"
         "<b>Last attempt to join " + htmlEscape(cfgSsid) + " failed:</b><br>" + htmlEscape(wifiFailReason) + "</div>";
  }
  if (!portalMode && numCh) {
    h += "<div class='st'>";
    for (int i = 0; i < numCh; i++)
      h += "<div>" + htmlEscape(ch[i].handle) + " &rarr; <b>" +
           (ch[i].subs >= 0 ? withCommas(ch[i].subs) : String("…")) + "</b> " + htmlEscape(ch[i].err) + "</div>";
    h += "</div>";
  }
  h += "<form method='POST' action='/save'>";
  h += "<label>YouTube channels (up to 10, one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@yourchannel&#10;@mkbhd&#10;@veritasium' required>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>Use @handles, UC… channel IDs or paste channel links. Put your own channel first – it's highlighted on the leaderboard.</small>";
  h += "<label><input type='checkbox' name='auto' value='1' style='width:auto'" + String(cfgAuto ? " checked" : "") +
       "> Switch channels automatically every 10 seconds</label>";
  h += "<label><input type='checkbox' name='est' value='1' style='width:auto'" + String(cfgEst ? " checked" : "") +
       "> Estimated live counts between YouTube's rounded steps (shown as “est.”)</label>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' autocapitalize='off' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'>";
  h += "<label>Wi-Fi network</label>";
  if (scanOptions.length()) {
    h += "<select onchange=\"document.getElementById('s').value=this.value\">";
    h += "<option value=''>Choose a network…</option>" + scanOptions + "</select><small>Or type it below:</small>";
  }
  h += "<input id='s' name='ssid' value='" + htmlEscape(cfgSsid) + "' placeholder='Network name' required>";
  h += "<label>Wi-Fi password</label>";
  h += "<input id='pw' name='pass' type='password' autocomplete='off' autocapitalize='off' placeholder='";
  h += cfgPass.length() ? "(saved — leave blank to keep)" : "Password";
  h += "'><small><label style='display:inline;margin:0'><input type='checkbox' style='width:auto' "
       "onclick=\"document.getElementById('pw').type=this.checked?'text':'password'\"> Show password</label></small>";
  h += "<label>Username (only for work Wi-Fi that asks for one)</label>";
  h += "<input name='user' value='" + htmlEscape(cfgUser) + "' autocapitalize='off' placeholder='Leave blank for normal Wi-Fi'>";
  h += "<details style='margin-top:18px'" + String(cfgIp.length() || cfgCompat ? " open" : "") +
       "><summary style='color:#aaa;font-size:14px'>Advanced network settings</summary>";
  h += "<label><input type='checkbox' name='compat' value='1' style='width:auto'" + String(cfgCompat ? " checked" : "") +
       "> Compatibility mode (Wi-Fi 4 instead of Wi-Fi 6)</label>";
  h += "<label>Fixed IP address (blank = automatic)</label>";
  h += "<input name='ip' value='" + htmlEscape(cfgIp) + "' placeholder='e.g. 192.168.1.250' inputmode='decimal'>";
  h += "<label>Gateway (router) address</label>";
  h += "<input name='gw' value='" + htmlEscape(cfgGw) + "' placeholder='e.g. 192.168.1.1' inputmode='decimal'>";
  h += "<label>Subnet mask</label>";
  h += "<input name='mask' value='" + htmlEscape(cfgMask) + "' placeholder='255.255.255.0' inputmode='decimal'>";
  h += "<label>DNS server</label>";
  h += "<input name='dns' value='" + htmlEscape(cfgDns) + "' placeholder='e.g. 8.8.8.8' inputmode='decimal'>";
  h += "<small>If this one doesn't answer, the board also tries 8.8.8.8.</small>";
  h += "</details>";
  h += "<button type='submit'>Save &amp; restart</button></form>";
  h += "<small style='margin-top:16px'>Board Wi-Fi MAC address: " + boardMac() + "</small></div></body></html>";
  server.send(200, "text/html", h);
}

void sendMessage(int code, const String &title, const String &body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>" + title + "</h1><p>" + body +
              "</p><a href='/'>Go back</a></div></body></html>");
}

void handleSave() {
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String user = server.arg("user");     user.trim();
  String chs  = server.arg("channels"); chs.trim();
  String key  = server.arg("apikey");   key.trim();
  while (pass.length() && (pass.endsWith("\n") || pass.endsWith("\r") || pass.endsWith(" ") || pass.endsWith("\t")))
    pass.remove(pass.length() - 1);
  while (pass.length() && (pass[0] == ' ' || pass[0] == '\n' || pass[0] == '\r' || pass[0] == '\t'))
    pass.remove(0, 1);
  if (ssid.length() == 0 || chs.length() == 0 || (key.length() == 0 && cfgApiKey.length() == 0)) {
    sendMessage(400, "Missing details", "Wi-Fi name, at least one channel and the API key are all needed.");
    return;
  }
  String ip = server.arg("ip"), gw = server.arg("gw"), mask = server.arg("mask"), dnsS = server.arg("dns");
  ip.trim(); gw.trim(); mask.trim(); dnsS.trim();
  IPAddress t;
  if ((ip.length() && (!t.fromString(ip) || !t.fromString(gw) || (mask.length() && !t.fromString(mask)))) ||
      (dnsS.length() && !t.fromString(dnsS))) {
    sendMessage(400, "Check the network settings", "A fixed IP needs a valid IP and gateway, e.g. 192.168.1.250 and 192.168.1.1.");
    return;
  }
  if (ssid != cfgSsid || pass.length()) cfgPass = pass;
  cfgSsid = ssid; cfgUser = user; cfgChannels = chs;
  if (key.length()) cfgApiKey = key;
  cfgIp = ip; cfgGw = ip.length() ? gw : ""; cfgMask = ip.length() ? mask : "";
  cfgDns = dnsS;
  cfgCompat = server.arg("compat") == "1";
  cfgAuto = server.arg("auto") == "1";
  cfgEst = server.arg("est") == "1";
  saveSettings();
  sendMessage(200, "Saved &#10003;", "The board is restarting and will join <b>" + htmlEscape(cfgSsid) + "</b>.");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

void handleNotFound() {
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

// ════════════════════════════════════════════════════════════════════════════
//  Setup portal
// ════════════════════════════════════════════════════════════════════════════
void buildScanOptions() {
  scanOptions = "";
  int n = WiFi.scanNetworks();
  for (int i = 0; i < n && i < 20; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0 || scanOptions.indexOf("value=\"" + htmlEscape(s) + "\"") >= 0) continue;
    wifi_auth_mode_t a = WiFi.encryptionType(i);
    String note = (a == WIFI_AUTH_OPEN) ? ", open" :
                  (a == WIFI_AUTH_WPA2_ENTERPRISE || a == WIFI_AUTH_WPA3_ENTERPRISE || a == WIFI_AUTH_WPA2_WPA3_ENTERPRISE)
                  ? ", needs username" : "";
    scanOptions += "<option value=\"" + htmlEscape(s) + "\">" + htmlEscape(s) +
                   " (" + String(WiFi.RSSI(i)) + " dBm" + note + ")</option>";
  }
  WiFi.scanDelete();
}

void startPortal() {
  portalMode = true;
  drawStatus("Setup mode", "Scanning Wi-Fi...", C_GOLD);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  buildScanOptions();
  WiFi.softAP(AP_NAME);
  delay(200);
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound(handleNotFound);
  server.begin();
  drawPortalScreen();
}


// ════════════════════════════════════════════════════════════════════════════
//  Wi-Fi
// ════════════════════════════════════════════════════════════════════════════
String reasonText(int r, bool associated) {
  switch (r) {
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_MIC_FAILURE:
      return "Wrong password (the network rejected it)";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "Username or password rejected (work/enterprise Wi-Fi)";
    case WIFI_REASON_NO_AP_FOUND:
      return "Network not found (out of range or 5 GHz only?)";
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
      return "Network found but its security type isn't supported";
    case WIFI_REASON_ASSOC_FAIL:
    case WIFI_REASON_ASSOC_TOOMANY:
    case WIFI_REASON_ASSOC_EXPIRE:
    case WIFI_REASON_NOT_AUTHED:
    case WIFI_REASON_NOT_ASSOCED:
      return "Access point refused the board (MAC filter or client limit?)";
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_BEACON_TIMEOUT:
      return "Signal too weak or access point stopped responding";
    case 0:
      return associated ? "Joined Wi-Fi but got no IP address (DHCP / device approval?)"
                        : "Timed out with no reply from the network";
  }
  return String("Wi-Fi error ") + r + " (" + WiFi.disconnectReasonName((wifi_err_reason_t)r) + ")";
}

void onWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) lastDiscReason = info.wifi_sta_disconnected.reason;
  if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED)    staAssociated = true;
}

bool connectAttempt(bool compat) {
  usedCompatThisBoot = compat;
  drawStatus("Connecting...", cfgSsid + (compat ? " (compat)" : ""), C_WHITE);
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  esp_wifi_set_protocol(WIFI_IF_STA, compat
      ? (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N)
      : (WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX));
  IPAddress google(8, 8, 8, 8);
  if (cfgIp.length()) {
    IPAddress ip, gw, mask(255, 255, 255, 0), dns1;
    ip.fromString(cfgIp); gw.fromString(cfgGw);
    if (cfgMask.length()) mask.fromString(cfgMask);
    if (!cfgDns.length() || !dns1.fromString(cfgDns)) dns1 = google;
    WiFi.config(ip, gw, mask, dns1, google);     // 8.8.8.8 as backup DNS
  } else {
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
  }
  lastDiscReason = 0;
  staAssociated = false;
  wifiFailReason = "";

  if (cfgUser.length()) WiFi.begin(cfgSsid.c_str(), WPA2_AUTH_PEAP, cfgUser.c_str(), cfgUser.c_str(), cfgPass.c_str());
  else                  WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());

  unsigned long start = millis();
  int lastSeenReason = 0;
  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < WIFI_TIMEOUT_MS + (staAssociated ? DHCP_EXTRA_MS : 0)) {
    delay(250);
    if (lastDiscReason) lastSeenReason = lastDiscReason;
    if (lastSeenReason && !staAssociated && millis() - start > 6000) break;
    if (digitalRead(BOOT_BTN) == LOW) { wifiFailReason = "Cancelled with BOOT button"; return false; }
  }
  if (WiFi.status() == WL_CONNECTED) {
    // With DHCP, add 8.8.8.8 as a backup DNS server too
    if (!cfgIp.length())
      WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), WiFi.dnsIP(0), google);
    WiFi.setAutoReconnect(true);
    return true;
  }
  wifiFailReason = reasonText(lastSeenReason ? lastSeenReason : lastDiscReason, staAssociated);
  WiFi.disconnect(true);
  return false;
}

bool connectWiFi() {
  bool ok = connectAttempt(cfgCompat);
  if (!ok && staAssociated && !cfgCompat && wifiFailReason.indexOf("no IP") >= 0) {
    ok = connectAttempt(true);
    if (ok) { cfgCompat = true; prefs.begin("subcounter", false); prefs.putBool("compat", true); prefs.end(); }
    else if (wifiFailReason.indexOf("no IP") >= 0)
      wifiFailReason = "Joined Wi-Fi but got no IP address in both Wi-Fi 6 and compatibility mode. "
                       "Try a fixed IP in Advanced settings, or allow MAC " + boardMac() + " on the router/firewall.";
  }
  return ok;
}

void drawWifiFailScreen() {
  gfx->fillScreen(C_BG);
  centreText("Wi-Fi failed", 10, 3, C_RED);
  wrapText(wifiFailReason, 10, 48, gfx->width() - 10, wifiFailReason.length() > 100 ? 1 : 2, C_WHITE);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BG);
  gfx->setCursor(10, 132); gfx->print("Network: " + cfgSsid);
  gfx->setCursor(10, 146); gfx->print("Board MAC: " + boardMac());
  gfx->setCursor(10, 160); gfx->print("Opening setup in a few seconds...");
}


// ════════════════════════════════════════════════════════════════════════════
//  YouTube API
// ════════════════════════════════════════════════════════════════════════════
int ytGet(const String &endpoint, const String &query, JsonDocument &doc) {
  String url = "https://www.googleapis.com/youtube/v3/" + endpoint + "?key=" + urlEncode(cfgApiKey) + "&" + query;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(12000);
  const char *hdrs[] = { "Date" };
  if (!http.begin(client, url)) return -100;
  http.collectHeaders(hdrs, 1);
  int code = http.GET();
  if (code > 0) {
    setClock(parseHttpDate(http.header("Date")));
    String body = http.getString();
    deserializeJson(doc, body);
  }
  http.end();
  Serial.printf("YouTube %s HTTP %d\n", endpoint.c_str(), code);
  return code;
}

String apiErrorText(int code, JsonDocument &doc) {
  if (code < 0) return "Can't reach YouTube - check DNS/firewall (" + String(code) + ")";
  String msg = doc["error"]["message"] | "";
  if (msg.indexOf("API key not valid") >= 0) return "API key not valid";
  if (msg.indexOf("has not been used") >= 0 || msg.indexOf("disabled") >= 0) return "YouTube Data API not enabled for this key";
  if (msg.indexOf("quota") >= 0) return "Daily API quota used up";
  return "API error " + String(code) + (msg.length() ? ": " + msg : "");
}

const char *CH_FIELDS = "items(id,snippet(title,publishedAt,country,thumbnails/default/url),"
                        "statistics(subscriberCount,hiddenSubscriberCount,viewCount,videoCount),"
                        "contentDetails/relatedPlaylists/uploads)";

void applyItem(JsonObject item, int i, bool alertOnGain) {
  Channel &c = ch[i];
  c.id = item["id"] | c.id;
  c.title = item["snippet"]["title"] | c.title;
  c.country = item["snippet"]["country"] | c.country;
  c.avatarUrl = item["snippet"]["thumbnails"]["default"]["url"] | c.avatarUrl;
  c.uploads = item["contentDetails"]["relatedPlaylists"]["uploads"] | c.uploads;
  String pub = item["snippet"]["publishedAt"] | "";
  if (pub.length()) c.joined = parseIso(pub);
  c.views = atoll(item["statistics"]["viewCount"] | "-1");
  c.videos = String(item["statistics"]["videoCount"] | "-1").toInt();
  if (item["statistics"]["hiddenSubscriberCount"] | false) { c.err = "Subscriber count is hidden"; return; }
  long n = String(item["statistics"]["subscriberCount"] | "-1").toInt();
  if (n < 0) return;
  if (n != c.subs) c.stepChangedAt = timeValid() ? nowT() : 0;
  if (alertOnGain && c.subs >= 0 && n > c.subs && numAlerts < MAX_CH) alerts[numAlerts++] = { i, n - c.subs, n };
  c.subs = n;
  c.err = "";
}

void resolveHandles() {
  for (int i = 0; i < numCh; i++) {
    if (ch[i].id.length()) continue;
    JsonDocument doc;
    int code = ytGet("channels", "part=statistics,snippet,contentDetails&fields=" + String(CH_FIELDS) +
                     "&forHandle=" + urlEncode(ch[i].handle), doc);
    if (code != 200) { netError = apiErrorText(code, doc); if (code < 0) return; continue; }
    JsonArray items = doc["items"];
    if (items.size() == 0) { ch[i].err = "Channel not found"; continue; }
    applyItem(items[0], i, false);
    loadLastSample(i);
    netError = "";
  }
}

void fetchAll() {
  resolveHandles();
  String ids;
  for (int i = 0; i < numCh; i++) if (ch[i].id.length()) ids += (ids.length() ? "," : "") + ch[i].id;
  if (!ids.length()) return;
  JsonDocument doc;
  int code = ytGet("channels", "part=statistics,snippet,contentDetails&fields=" + String(CH_FIELDS) + "&id=" + ids, doc);
  if (code != 200) { netError = apiErrorText(code, doc); return; }
  netError = "";
  lastFetchOk = millis();
  for (int i = 0; i < numCh; i++) {
    if (!ch[i].id.length()) continue;
    bool found = false;
    for (JsonObject item : doc["items"].as<JsonArray>()) {
      if (ch[i].id == (const char *)(item["id"] | "")) {
        if (!ch[i].lastSampleT) loadLastSample(i);
        applyItem(item, i, true); found = true; break;
      }
    }
    if (!found) ch[i].err = "Channel not found";
    recordSample(i);
    computeStats(i);
  }
}

// Latest upload for every channel: 1 unit each + 1 unit for all video details
void fetchLatestVideos() {
  String vids;
  for (int i = 0; i < numCh; i++) {
    if (!ch[i].uploads.length()) continue;
    JsonDocument doc;
    int code = ytGet("playlistItems", "part=contentDetails&maxResults=1&fields=items/contentDetails/videoId&playlistId=" +
                     ch[i].uploads, doc);
    if (code != 200) continue;
    String v = doc["items"][0]["contentDetails"]["videoId"] | "";
    if (v.length()) { ch[i].vidId = v; vids += (vids.length() ? "," : "") + v; }
  }
  if (!vids.length()) return;
  JsonDocument doc;
  int code = ytGet("videos", "part=snippet,statistics,contentDetails,liveStreamingDetails"
                   "&fields=items(id,snippet(title,publishedAt,liveBroadcastContent),statistics(viewCount,likeCount,commentCount),"
                   "contentDetails/duration,liveStreamingDetails/concurrentViewers)&id=" + vids, doc);
  if (code != 200) return;
  for (JsonObject it : doc["items"].as<JsonArray>()) {
    String id = it["id"] | "";
    for (int i = 0; i < numCh; i++) {
      if (ch[i].vidId != id) continue;
      Channel &c = ch[i];
      c.vidTitle = it["snippet"]["title"] | "";
      c.vidPublished = parseIso(it["snippet"]["publishedAt"] | "");
      c.live = String(it["snippet"]["liveBroadcastContent"] | "none") == "live";
      c.vidDuration = parseDuration(it["contentDetails"]["duration"] | "");
      c.vidViews = atoll(it["statistics"]["viewCount"] | "-1");
      c.vidLikes = String(it["statistics"]["likeCount"] | "-1").toInt();
      c.vidComments = String(it["statistics"]["commentCount"] | "-1").toInt();
      c.liveViewers = String(it["liveStreamingDetails"]["concurrentViewers"] | "-1").toInt();
    }
  }
}

// Profile pictures (not API quota — plain image downloads), once per boot
void fetchAvatars() {
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    if (c.avatar || !c.avatarUrl.length()) continue;
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(10000);
    if (!http.begin(client, c.avatarUrl)) continue;
    if (http.GET() == 200) {
      int len = http.getSize();                 // -1 if the server doesn't say
      int cap = (len > 0 && len < 40000) ? len : 40000;
      uint8_t *buf = (uint8_t *)malloc(cap);
      if (buf) {
        WiFiClient *st = http.getStreamPtr();
        int got = 0; unsigned long t0 = millis();
        while (got < cap && millis() - t0 < 8000) {
          int avail = st->available();
          if (avail > 0) got += st->readBytes(buf + got, min(avail, cap - got));
          else if (!st->connected()) break;
          else delay(5);
        }
        bool complete = (len > 0) ? (got == len) : (got > 4 && got < cap);
        if (complete && buf[0] == 0xFF && buf[1] == 0xD8) { c.avatar = buf; c.avatarLen = got; }
        else free(buf);
      }
    }
    http.end();
    server.handleClient();
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  Setup & loop
// ════════════════════════════════════════════════════════════════════════════
void startSettingsServer() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SubCounter v6 starting");
  setenv("TZ", TZ_UK, 1); tzset();

  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  gfx->begin();
  lcdRegInit();
  gfx->setRotation(1);
  gfx->fillScreen(C_BG);
  ledcAttach(LCD_BL, 5000, 8);
  ledcWrite(LCD_BL, 160);
  touchInit();
  fsOk = LittleFS.begin(true);
  Serial.printf("History storage %s\n", fsOk ? "ready" : "unavailable");

  WiFi.onEvent(onWiFiEvent);
  loadSettings();

  if (cfgSsid.length() == 0 || cfgApiKey.length() == 0 || numCh == 0) { startPortal(); return; }
  if (!connectWiFi()) { drawWifiFailScreen(); delay(8000); startPortal(); return; }

  configTime(0, 0, "pool.ntp.org", "time.google.com");   // backup: clock is also set from Google's replies
  setenv("TZ", TZ_UK, 1); tzset();
  startSettingsServer();
  drawStatus("Loading...", String(numCh) + (numCh == 1 ? " channel" : " channels"), C_WHITE);
  fetchAll();
  lastFetch = millis();
  drawStatus("Loading...", "pictures & videos", C_WHITE);
  fetchAvatars();
  fetchLatestVideos();
  lastVideoFetch = millis();
  lastInteract = millis();
  drawMain();
  if (ch[page].subs > 0) { shownSubs = max(0L, estimateFor(page) - 30); drawMainNumber(shownSubs); }
}

void loop() {
  server.handleClient();
  if (portalMode) { dns.processNextRequest(); delay(2); return; }

  // BOOT: short press = next channel, hold 3 s = setup mode
  if (digitalRead(BOOT_BTN) == LOW) {
    if (!btnDownAt) btnDownAt = millis();
    if (millis() - btnDownAt > HOLD_FOR_SETUP_MS) { server.stop(); startPortal(); btnDownAt = 0; return; }
  } else if (btnDownAt) {
    if (millis() - btnDownAt > 40) { changePage(+1); lastInteract = millis(); }
    btnDownAt = 0;
  }

  // Swipes
  switch (pollSwipe()) {
    case 'L': changePage(+1); lastInteract = millis(); break;
    case 'R': changePage(-1); lastInteract = millis(); break;
    case 'U': changeCard(+1); lastInteract = millis(); break;
    case 'D': changeCard(-1); lastInteract = millis(); break;
  }

  // Back to the main count after a while untouched
  if ((card != 0 || board) && millis() - lastInteract > IDLE_RETURN_MS) {
    card = 0; board = false; drawView(); lastInteract = millis();
  }

  // Auto-switch channels (main count only, pauses 30 s after a swipe)
  if (cfgAuto && numCh > 1 && card == 0 && !board && millis() - lastInteract > 30000UL) {
    static unsigned long lastAuto = 0;
    if (millis() - lastAuto > AUTO_SWITCH_MS) { lastAuto = millis(); changePage(+1); }
  }

  if (WiFi.status() != WL_CONNECTED) {
    netError = "Wi-Fi lost, reconnecting...";
    drawFooter();
    WiFi.reconnect();
    delay(3000);
    return;
  }

  bool refreshed = false;
  if (millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    fetchAll();
    fetchAvatars();          // picks up any that failed earlier
    refreshed = true;
  }
  if (millis() - lastVideoFetch > VIDEO_REFRESH_MS) {
    lastVideoFetch = millis();
    fetchLatestVideos();
    refreshed = true;
  }
  if (refreshed) {
    if (numAlerts) { for (int i = 0; i < numAlerts; i++) showAlert(alerts[i]); numAlerts = 0; }
    if (card == 0 && !board) { long keep = shownSubs; drawMain(); shownSubs = keep; drawMainNumber(shownSubs); }
    else drawView();
  }

  // Count-up / estimate animation on the main card
  static unsigned long lastAnim = 0;
  if (numCh && card == 0 && !board && ch[page].subs >= 0 && millis() - lastAnim > 30) {
    lastAnim = millis();
    long target = estimateFor(page);
    if (shownSubs != target) {
      if (shownSubs < 0 || shownSubs > target) shownSubs = target;
      else { long gap = target - shownSubs; shownSubs += (gap > 20) ? gap / 8 : 1; }
      drawMainNumber(shownSubs);
    }
  }

  static unsigned long lastFooter = 0;
  if (millis() - lastFooter > 60000UL) { lastFooter = millis(); drawFooter(); }

  delay(5);
}
