/*
 * SubCounter v8.2 — YouTube subscriber counter for Waveshare ESP32-C6-Touch-LCD-1.47
 *
 *  ON THE BOARD
 *    Swipe left / right ... next / previous channel   (BOOT short press = next)
 *    Swipe up ............. detail cards: Overview, Latest video, Growth, 7-day graph, Milestone
 *    Swipe down ........... back; from the main count: Leaderboard, then Race
 *    Shake ................ refresh now
 *    Face-down ............ screen off
 *    Stand on its side .... tall leaderboard
 *    Double-tap the desk .. next channel
 *    9 am ................. daily summary;  at night: dim clock mode
 *    Hold BOOT 3 s ........ setup mode
 *
 *  IN A BROWSER
 *    http://<board IP>/          dashboard (all channels, graphs, videos, race)
 *    http://<board IP>/settings  settings
 *
 *  Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *    Flash Size "8MB", Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
 *    Libraries: "GFX Library for Arduino" 1.6.x, "ArduinoJson" 7.x, "JPEGDEC" 1.8.x,
 *               "U8g2" (for its fonts)
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
#include <U8g2lib.h>   // readable fonts (U8g2 library)

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
#define NIGHT_IDLE_MS      60000UL
#define SUMMARY_SHOW_MS    (10UL * 60UL * 1000UL)
#define ALERT_MS           3500UL
#define WIFI_TIMEOUT_MS    20000UL
#define DHCP_EXTRA_MS      15000UL
#define HOLD_FOR_SETUP_MS  3000UL
#define SWIPE_MIN_PX       35
#define NUM_CARDS          6          // 0 main, 1 overview, 2 latest video, 3 growth, 4 graph, 5 milestone
#define HIST_MAX_AGE       (31L * 86400L)
#define TZ_UK              "GMT0BST,M3.5.0/1,M10.5.0/2"
#define BL_NORMAL          160
#define BL_NIGHT           18

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
  long gainToday = 0, gain24 = 0, gain7 = 0, gain30 = 0;
  float span7Days = 0, span30Days = 0, ratePerDay = 0;
  time_t histStart = 0, lastSampleT = 0; long lastSampleS = -1;
};
Channel ch[MAX_CH];
int numCh = 0;
int page = 0;              // channel on screen
int card = 0;              // 0..NUM_CARDS-1
bool board = false;        // leaderboard / race showing
bool raceView = false;     // (when board) showing the race instead of the leaderboard
long shownSubs = -1;

enum AlertType { A_GAIN, A_MILESTONE, A_OVERTAKE };
struct Alert { AlertType type; int idx; int idx2; long delta; long total; };
Alert alerts[MAX_CH * 2];
int numAlerts = 0;
int raceLeader = -1;

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

String cfgSsid, cfgPass, cfgChannels, cfgApiKey, cfgUser;
String cfgIp, cfgGw, cfgMask, cfgDns;
bool   cfgCompat = false, cfgAuto = false, cfgEst = true;
bool   cfgCelebrate = true, cfgSummary = true;
bool   cfgShake = true, cfgFaceDown = true, cfgPortrait = true, cfgFlipPortrait = false, cfgTap = true;
int    cfgTapSens = 2;
int    cfgRaceA = -1, cfgRaceB = -1;
int    cfgNightStart = 23, cfgNightEnd = 7;
float  cfgG0x = 0, cfgG0y = 0, cfgG0z = 1;       // motion calibration: "normal" gravity direction
bool   g0Saved = false;                          // false = use the position at power-up
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

// Forward declarations (functions used before they're defined)
bool raceSet();
void drawRace();
void drawBoard();
void handleSettings();
void setBacklight(int v);
long nextMilestone(long n);
long prevMilestone(long next);

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
  gfx->setFont((const GFXfont *)nullptr);
  gfx->setTextSize(size);
  gfx->setTextColor(colour, C_BG);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(txt.c_str(), 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(max(0, (int)(gfx->width() - w) / 2), y);
  gfx->print(txt);
}

void textAt(int x, int y, const String &t, uint8_t size, uint16_t colour) {
  gfx->setFont((const GFXfont *)nullptr);
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
  gfx->setFont((const GFXfont *)nullptr);
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
// Keep characters the fonts can draw (Latin-1, e.g. é ü ñ); drop emoji etc.
String asciiOnly(const String &s) {
  String o; o.reserve(s.length());
  for (size_t i = 0; i < s.length();) {
    uint8_t c = s[i];
    int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 1;
    if (len == 1) { if (c >= 32 && c < 127) o += (char)c; }
    else if (len == 2 && (c == 0xC2 || c == 0xC3) && i + 1 < s.length()) { o += (char)c; o += s[i + 1]; }
    else o += ' ';
    i += len;
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
  cfgCelebrate = prefs.getBool("celebrate", true);
  cfgSummary  = prefs.getBool("summary", true);
  cfgShake    = prefs.getBool("shake", true);
  cfgFaceDown = prefs.getBool("facedown", true);
  cfgPortrait = prefs.getBool("portrait", true);
  cfgFlipPortrait = prefs.getBool("flip", false);
  cfgTap      = prefs.getBool("tap", true);
  cfgTapSens  = prefs.getInt("tapsens", 2);
  cfgRaceA    = prefs.getInt("raceA", -1);
  cfgRaceB    = prefs.getInt("raceB", -1);
  cfgNightStart = prefs.getInt("nightS", 23);
  cfgNightEnd = prefs.getInt("nightE", 7);
  cfgG0x      = prefs.getFloat("g0x", 0);
  cfgG0y      = prefs.getFloat("g0y", 0);
  cfgG0z      = prefs.getFloat("g0z", 1);
  g0Saved     = prefs.isKey("g0z");
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
  prefs.putBool("celebrate", cfgCelebrate);
  prefs.putBool("summary", cfgSummary);
  prefs.putBool("shake", cfgShake);
  prefs.putBool("facedown", cfgFaceDown);
  prefs.putBool("portrait", cfgPortrait);
  prefs.putBool("flip", cfgFlipPortrait);
  prefs.putBool("tap", cfgTap);
  prefs.putInt("tapsens", cfgTapSens);
  prefs.putInt("raceA", cfgRaceA);
  prefs.putInt("raceB", cfgRaceB);
  prefs.putInt("nightS", cfgNightStart);
  prefs.putInt("nightE", cfgNightEnd);
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
  long baseToday = -1, base7 = -1, base30 = -1, base24 = -1; time_t t7 = 0, t30 = 0, tToday = 0;
  bool first = true;
  while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
    if (first) { c.histStart = s.t; first = false; }
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

// ── Readable fonts (U8g2) ───────────────────────────────────────────────────
#define F_S   u8g2_font_helvB14_tf      // smallest text used anywhere
#define F_M   u8g2_font_helvB18_tf      // normal text
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
  if (c.statsOk && c.gainToday != 0) ft(58, 46, signedNum(c.gainToday) + " today", F_S, c.gainToday > 0 ? C_GREEN : C_RED);
  else ft(58, 46, "subscribers", F_S, C_GREY);
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
void drawFooter() {
  if (board || card != 0) return;
  gfx->fillRect(0, 150, gfx->width() - 14, 22, C_BG);
  if (netError.length()) ft(8, 168, fit(netError, F_S, RIGHT_EDGE - 8), F_S, C_RED);
  else if (lastFetchOk && millis() - lastFetchOk > 10UL * 60000UL)
    ft(8, 168, "Not updated for " + String((millis() - lastFetchOk) / 60000UL) + " min", F_S, C_GOLD);
}

void drawMain() {
  gfx->fillScreen(C_BG);
  if (!numCh) { ftC(90, "No channels set", F_M, C_GREY); return; }
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
  drawDetailHeader("Overview");
  if (!drawAvatar(page, 6, 42, false)) gfx->fillCircle(50, 86, 44, C_DKGREY);
  int x = 104, y = 60;
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
  drawDetailHeader(c.live ? "LIVE" : "Latest video");
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
    ft(30, y, fit(nameOf(i), F_M, gfx->width() - 8 - sw - 40), F_M, col);
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
    case 2: drawLatestVideo(); break;
    case 3: drawGrowth(); break;
    case 4: drawGraph(); break;
    case 5: drawMilestone(); break;
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
//  Motion sensor (QMI8658) — shake, face-down, portrait, desk double-tap
// ════════════════════════════════════════════════════════════════════════════
uint8_t imuAddr = 0;
bool imuOk = false;

bool imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(imuAddr); Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}
bool imuRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(imuAddr); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(imuAddr, n) != n) return false;
  Wire.readBytes(buf, n);
  return true;
}

void imuInit() {
  const uint8_t addrs[] = { 0x6B, 0x6A };
  for (uint8_t a : addrs) {
    imuAddr = a;
    uint8_t who = 0;
    if (imuRead(0x00, &who, 1) && who == 0x05) { imuOk = true; break; }
  }
  if (!imuOk) { Serial.println("Motion sensor NOT found"); return; }
  imuWrite(0x60, 0xB0);  delay(30);    // soft reset
  imuWrite(0x02, 0x40);                // CTRL1: register auto-increment, little endian
  imuWrite(0x03, 0x15);                // CTRL2: accel ±4 g, 250 Hz
  imuWrite(0x08, 0x01);                // CTRL7: accelerometer on
  delay(20);
  Serial.printf("Motion sensor found at 0x%02X\n", imuAddr);
}

bool imuAccel(float &x, float &y, float &z) {
  uint8_t d[6];
  if (!imuRead(0x35, d, 6)) return false;
  x = (int16_t)(d[0] | (d[1] << 8)) / 8192.0f;
  y = (int16_t)(d[2] | (d[3] << 8)) / 8192.0f;
  z = (int16_t)(d[4] | (d[5] << 8)) / 8192.0f;
  return true;
}

enum Orient { O_NORMAL, O_FACEDOWN, O_PORTRAIT_A, O_PORTRAIT_B };
Orient orient = O_NORMAL;
float gX = 0, gY = 0, gZ = 1;            // smoothed gravity (for orientation)
float fX = 0, fY = 0, fZ = 1;            // fast low-pass (for taps)
bool imuPrimed = false;
unsigned long lastImu = 0, orientSince = 0, lastTouchActivity = 0;
Orient orientCandidate = O_NORMAL;
// shake
unsigned long shakeTimes[6]; int shakeN = 0; unsigned long lastShake = 0;
// taps
unsigned long tapBurstStart = 0, tapLast = 0, tapFirst = 0, tapSecond = 0, quietBefore = 0;
int tapCount = 0;
bool evShake = false, evTap = false;

float tapThreshold() { return cfgTapSens >= 3 ? 0.06f : (cfgTapSens == 1 ? 0.25f : 0.12f); }

// Called often from loop(); sets evShake / evTap and updates `orient`.
void imuUpdate() {
  if (!imuOk || millis() - lastImu < 10) return;
  lastImu = millis();
  float x, y, z;
  if (!imuAccel(x, y, z)) return;
  static unsigned long primedAt = 0;
  if (!imuPrimed) { gX = fX = x; gY = fY = y; gZ = fZ = z; imuPrimed = true; primedAt = millis(); return; }
  static bool autoCal = false;
  if (!g0Saved && !autoCal && millis() - primedAt > 1500) {   // not calibrated yet: power-up position = normal
    float m = sqrtf(gX * gX + gY * gY + gZ * gZ);
    if (m > 0.5f) { cfgG0x = gX / m; cfgG0y = gY / m; cfgG0z = gZ / m; }
    autoCal = true;
  }
  if (!g0Saved && !autoCal) { gX += (x - gX) * 0.04f; gY += (y - gY) * 0.04f; gZ += (z - gZ) * 0.04f; return; }
  fX += (x - fX) * 0.15f; fY += (y - fY) * 0.15f; fZ += (z - fZ) * 0.15f;
  gX += (x - gX) * 0.04f; gY += (y - gY) * 0.04f; gZ += (z - gZ) * 0.04f;
  float hx = x - fX, hy = y - fY, hz = z - fZ;
  float hp = sqrtf(hx * hx + hy * hy + hz * hz);        // sudden movement, in g
  unsigned long now = millis();

  // ── shake: 4+ big jolts within a second ──
  if (hp > 0.8f) {
    if (shakeN == 0 || now - shakeTimes[shakeN - 1] > 60) {
      if (shakeN == 6) { memmove(shakeTimes, shakeTimes + 1, 5 * sizeof(unsigned long)); shakeN = 5; }
      shakeTimes[shakeN++] = now;
    }
    int recent = 0;
    for (int i = 0; i < shakeN; i++) if (now - shakeTimes[i] < 1000) recent++;
    if (recent >= 4 && now - lastShake > 2500) { lastShake = now; shakeN = 0; evShake = true; }
  }

  // ── desk double-tap: exactly two small knocks 120-600 ms apart, quiet around them ──
  bool tapsAllowed = now - lastTouchActivity > 1500 && now - lastShake > 1500 && orient == O_NORMAL;
  if (tapsAllowed && hp > tapThreshold() && hp < 0.8f) {
    if (now - tapLast > 100) {                  // start of a new knock (not the ringing of the last one)
      if (tapCount == 0) { tapFirst = now; tapBurstStart = now; }
      if (tapCount == 1) tapSecond = now;
      tapCount++;
    }
    tapLast = now;
  }
  if (tapCount && now - tapLast > 450) {        // burst finished
    unsigned long gap = tapSecond - tapFirst;
    if (tapCount == 2 && gap >= 120 && gap <= 600 && tapFirst - quietBefore > 700) evTap = true;
    quietBefore = tapLast;
    tapCount = 0;
  }
  if (!tapCount && hp > tapThreshold()) quietBefore = now;   // any stray knock resets the quiet timer

  // ── orientation (relative to the calibrated "normal" position) ──
  float n = sqrtf(gX * gX + gY * gY + gZ * gZ);
  if (n < 0.5f) return;
  float ux = gX / n, uy = gY / n, uz = gZ / n;
  float zUp = (fabsf(cfgG0z) > 0.3f) ? (cfgG0z > 0 ? 1 : -1) : 1;   // which way the screen faces
  Orient o = O_NORMAL;
  if (uz * zUp < -0.75f) o = O_FACEDOWN;
  // Found on the real board: gravity runs along the sensor's Y axis when the board stands
  // in portrait, and along X when it stands in landscape. The sign of Y says which end is down.
  else if (fabsf(uy) > 0.75f) o = (uy > 0) ? O_PORTRAIT_A : O_PORTRAIT_B;
  if (o != orientCandidate) { orientCandidate = o; orientSince = now; }
  if (orientCandidate != orient && now - orientSince > 700) orient = orientCandidate;
}

// Store the current position as "normal" (from the web settings page)
void calibrateMotion() {
  float n = sqrtf(gX * gX + gY * gY + gZ * gZ);
  if (n < 0.5f) return;
  cfgG0x = gX / n; cfgG0y = gY / n; cfgG0z = gZ / n;
  prefs.begin("subcounter", false);
  prefs.putFloat("g0x", cfgG0x); prefs.putFloat("g0y", cfgG0y); prefs.putFloat("g0z", cfgG0z);
  prefs.end();
  g0Saved = true;
}

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
    ftC(150, "subscribers", F_M, C_WHITE, gfx->width());
    waitShowing(ALERT_MS + 1000UL * tier);
    return;
  }
  for (int i = 0; i < 4; i++) { gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD); delay(100); }
  gfx->fillScreen(C_BG);
  if (a.type == A_OVERTAKE) {
    ftC(30, "OVERTAKE!", F_M, C_GOLD, gfx->width());
    ftC(70, fit(nameOf(a.idx), F_M, gfx->width() - 20), F_M, C_GREEN, gfx->width());
    ftC(98, "just passed", F_S, C_GREY, gfx->width());
    ftC(130, fit(nameOf(a.idx2), F_M, gfx->width() - 20), F_M, C_WHITE, gfx->width());
    ftC(160, withCommas(ch[a.idx].subs) + " vs " + withCommas(ch[a.idx2].subs), F_S, C_GREY, gfx->width());
  } else {
    drawAvatar(a.idx, 6, 4, true);
    ft(58, 24, "New subscribers!", F_M, C_GOLD);
    ft(58, 46, fit(nameOf(a.idx), F_S, gfx->width() - 66), F_S, C_WHITE);
    String d = "+" + withCommas(a.delta);
    ftC(112, d, numFont(d, gfx->width() - 20), C_GREEN, gfx->width());
    ftC(158, "now " + withCommas(a.total), F_M, C_GREY, gfx->width());
  }
  waitShowing(ALERT_MS);
}

// ── Subscriber race ─────────────────────────────────────────────────────────
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
  ft(8, 56, fit(nameOf(A), F_S, gfx->width() - tw(ca, F_M) - 26), F_S, C_RED);
  ftR(gfx->width() - 8, 56, ca, F_M, C_WHITE);
  ft(8, 82, fit(nameOf(B), F_S, gfx->width() - tw(cb, F_M) - 26), F_S, C_BLUE);
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
void drawSummary() {
  gfx->fillScreen(C_BG);
  ft(8, 22, "Good morning!", F_M, C_GOLD);
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
  gfx->drawFastHLine(6, 31, W - 12, C_DKGREY);
  int idx[MAX_CH]; for (int i = 0; i < numCh; i++) idx[i] = i;
  for (int a = 0; a < numCh; a++) for (int b = a + 1; b < numCh; b++)
    if (ch[idx[b]].subs > ch[idx[a]].subs) { int t = idx[a]; idx[a] = idx[b]; idx[b] = t; }
  int rowH = numCh > 0 ? min(48, 284 / numCh) : 28;
  int y = 34;
  for (int r = 0; r < numCh; r++) {
    int i = idx[r];
    uint16_t col = (i == 0) ? C_GOLD : C_WHITE;
    String subs = ch[i].subs >= 0 ? compact(ch[i].subs) : String("-");
    if (rowH >= 44) {     // roomy: name on one line, count + today underneath
      ft(6, y + 18, String(r + 1) + " " + fit(nameOf(i), F_S, W - 30), F_S, col);
      ft(6, y + 40, subs, F_M, col);
      if (ch[i].statsOk && ch[i].gainToday) ftR(W - 6, y + 40, signedNum(ch[i].gainToday), F_S, C_GREEN);
    } else {
      int sw = tw(subs, F_S);
      ft(6, y + 19, String(r + 1), F_S, C_GREY);
      ft(24, y + 19, fit(nameOf(i), F_S, W - 34 - sw), F_S, col);
      ftR(W - 6, y + 19, subs, F_S, col);
    }
    y += rowH;
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  Web: dashboard (/), data API, settings (/settings)
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


const char DASH_HTML[] PROGMEM = R"HTML(<!doctype html><html lang="en"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter</title>
<style>
:root{--bg:#0d0d0f;--card:#18181b;--line:#2a2a2e;--text:#f2f2f3;--muted:#8d8d95;--red:#ff3b30;--gold:#ffc53d;--green:#30d158;--blue:#4da3ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:15px/1.4 -apple-system,system-ui,Segoe UI,Roboto,sans-serif}
a{color:inherit;text-decoration:none}
header{display:flex;align-items:center;gap:12px;padding:16px 20px;border-bottom:1px solid var(--line);position:sticky;top:0;background:rgba(13,13,15,.92);backdrop-filter:blur(8px);z-index:2}
.logo{width:34px;height:24px;border-radius:7px;background:var(--red);display:grid;place-items:center}
.logo:after{content:"";border-left:10px solid #fff;border-top:6px solid transparent;border-bottom:6px solid transparent;margin-left:3px}
header h1{font-size:18px;margin:0;flex:1}header .meta{color:var(--muted);font-size:13px}
.btn{border:1px solid var(--line);border-radius:9px;padding:7px 12px;font-size:13px;color:var(--text);background:var(--card);cursor:pointer}
main{max-width:1200px;margin:0 auto;padding:20px;display:grid;gap:16px}
.row{display:grid;gap:16px;grid-template-columns:repeat(auto-fit,minmax(320px,1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:18px}
.card h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--gold);margin:0 0 12px}
.err{background:#3a1210;border-color:#e62117}
.av{width:44px;height:44px;border-radius:50%;object-fit:cover;background:#333;flex:none}
.av.lg{width:64px;height:64px}
.top{display:flex;gap:12px;align-items:center}.top .nm{font-weight:700;font-size:17px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.sub{color:var(--muted);font-size:13px}
.big{font-size:44px;font-weight:800;letter-spacing:-.02em;margin:10px 0 2px;font-variant-numeric:tabular-nums}
.est{font-size:12px;color:var(--muted);font-weight:500;margin-left:6px;letter-spacing:0}
.chips{display:flex;gap:8px;flex-wrap:wrap;margin:10px 0}
.chip{background:#222226;border-radius:9px;padding:6px 10px;font-size:13px}.chip b{font-size:15px}
.pos{color:var(--green)}.neg{color:var(--red)}
.bar{height:10px;border-radius:6px;background:#2a2a2e;overflow:hidden;margin:8px 0 4px}.bar i{display:block;height:100%;background:var(--gold);border-radius:6px}
.vid{display:flex;gap:12px;margin-top:14px;padding-top:14px;border-top:1px solid var(--line)}
.vid img{width:128px;aspect-ratio:16/9;object-fit:cover;border-radius:8px;flex:none;background:#333}
.vid .t{font-weight:600;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.live{background:var(--red);color:#fff;border-radius:5px;padding:1px 6px;font-size:11px;font-weight:800;margin-right:6px}
svg.g{width:100%;height:120px;display:block;margin-top:12px}
.tabs{display:flex;gap:6px;margin-top:12px}.tabs button{border:0;border-radius:7px;padding:3px 9px;font-size:12px;background:#222226;color:var(--muted);cursor:pointer}.tabs button.on{background:#3a3a40;color:var(--text)}
table{width:100%;border-collapse:collapse}td{padding:8px 4px;border-top:1px solid var(--line)}td.n{text-align:right;font-variant-numeric:tabular-nums}
tr.me td{color:var(--gold)}
.race .side{display:flex;justify-content:space-between;align-items:center;margin:8px 0}.race .side span{display:flex;align-items:center;gap:10px;min-width:0}
td .av{width:32px;height:32px}
.tug{display:flex;height:14px;border-radius:7px;overflow:hidden;margin:10px 0}.tug .a{background:var(--red)}.tug .b{background:var(--blue)}
.list div{display:flex;justify-content:space-between;padding:5px 0}
</style></head><body>
<header><div class="logo"></div><h1>SubCounter</h1><span class="meta" id="meta"></span><a class="btn" href="/settings">Settings</a></header>
<main id="app"><div class="card">Loading…</div></main>
<script>
const $=s=>document.querySelector(s);
function h(t,a,...k){const e=document.createElement(t);for(const x in a||{}){if(x=='class')e.className=a[x];else if(x=='text')e.textContent=a[x];else if(x.startsWith('on'))e[x]=a[x];else e.setAttribute(x,a[x])}for(const c of k.flat())if(c!=null)e.append(c.nodeType?c:document.createTextNode(c));return e}
const fmt=n=>n==null||n<0?'-':Number(n).toLocaleString('en-GB');
const cmp=n=>{if(n==null||n<0)return'-';if(n<10000)return fmt(n);const u=['K','M','B'];let i=-1;while(n>=1000&&i<2){n/=1000;i++}return(n>=100?n.toFixed(0):n>=10?n.toFixed(1):n.toFixed(2))+u[i]};
const sg=n=>(n>=0?'+':'')+fmt(n);
const cls=n=>n>0?'pos':n<0?'neg':'';
function ago(t){if(!t)return'';let d=Date.now()/1000-t;if(d<3600)return Math.max(1,d/60|0)+' min ago';if(d<86400)return(d/3600|0)+' h ago';if(d<2592000)return(d/86400|0)+' days ago';return(d/2592000|0)+' months ago'}
function dur(s){if(!s)return'';const p=x=>String(x).padStart(2,'0');return s>=3600?`${s/3600|0}:${p((s/60|0)%60)}:${p(s%60)}`:`${s/60|0}:${p(s%60)}`}
let D=null,graphs={};
function est(c){if(!D.est||!c.rate||c.rate<=0||c.step<=1||!c.stepAt)return c.subs;return c.subs+Math.min(c.step-1,Math.max(0,Math.floor(c.rate*(Date.now()/1000-c.stepAt)/86400)))}
function eta(c){const e=est(c);if(!(c.rate>0.01))return c.rate<0?'Losing subscribers':'Need more data for a date';const d=(c.next-e)/c.rate;if(d<1)return'Expected today';if(d>3650)return'10+ years away';return'Expected around '+new Date(Date.now()+d*864e5).toLocaleDateString('en-GB',{day:'numeric',month:'short',year:d>300?'numeric':undefined})}
function av(c,lg){return c.avatar?h('img',{class:'av'+(lg?' lg':''),src:c.avatar,alt:''}):h('div',{class:'av'+(lg?' lg':'')})}
function name(c){return c.title||c.handle}
async function graph(c,days,box){box.textContent='';const r=await fetch('/api/history?i='+c.i+'&days='+days);const pts=await r.json();if(pts.length<2){box.append(h('div',{class:'sub',text:'Collecting data – recorded every hour'}));return}
pts.push([Date.now()/1000|0,c.subs]);const W=600,H=120,t0=pts[0][0],t1=pts[pts.length-1][0];let mn=Math.min(...pts.map(p=>p[1])),mx=Math.max(...pts.map(p=>p[1]));if(mx==mn){mx++;mn--}
const X=t=>(t-t0)/(t1-t0||1)*(W-4)+2,Y=v=>H-6-(v-mn)/(mx-mn)*(H-24);const d=pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg');svg.setAttribute('viewBox',`0 0 ${W} ${H}`);svg.setAttribute('class','g');svg.setAttribute('preserveAspectRatio','none');
const area=document.createElementNS(ns,'path');area.setAttribute('d',d+` L ${X(t1)} ${H} L ${X(t0)} ${H} Z`);area.setAttribute('fill','rgba(48,209,88,.12)');svg.append(area);
const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#30d158');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');svg.append(ln);
box.append(svg,h('div',{class:'sub',text:`${cmp(mn)} – ${cmp(mx)}  ·  ${new Date(t0*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'})} to now`}))}
function channelCard(c){const card=h('div',{class:'card'+(c.err&&c.subs<0?' err':'')});
const link=c.id?'https://www.youtube.com/channel/'+c.id:'#';
card.append(h('a',{class:'top',href:link,target:'_blank'},av(c,true),h('div',{style:'min-width:0'},h('div',{class:'nm',text:name(c)}),h('div',{class:'sub',text:[c.handle,c.country,c.joined?'since '+new Date(c.joined*1000).getFullYear():''].filter(Boolean).join(' · ')}))));
if(c.err&&c.subs<0){card.append(h('p',{text:c.err}));return card}
const e=est(c);card.append(h('div',{class:'big'},h('span',{'data-est':c.i,text:fmt(e)}),(D.est&&c.step>1&&c.rate>0)?h('span',{class:'est',text:'est.'}):null));
card.append(h('div',{class:'sub',text:`${cmp(c.views)} views · ${fmt(c.videos)} videos · ${c.videos>0?cmp(Math.round(c.views/c.videos)):'-'} avg`}));
if(c.statsOk)card.append(h('div',{class:'chips'},[['Today',c.today],['24 h',c.d1],['7 days',c.d7],['30 days',c.d30]].map(([k,v])=>h('div',{class:'chip'},k+' ',h('b',{class:cls(v),text:sg(v)}))),c.rate?h('div',{class:'chip'},'≈ ',h('b',{text:sg(Math.round(c.rate))}),'/day'):null));
else card.append(h('div',{class:'chips'},h('div',{class:'chip',text:'Growth: collecting data'})));
const f=Math.max(0,Math.min(1,(e-c.prev)/(c.next-c.prev||1)));
card.append(h('div',{style:'margin-top:6px;display:flex;justify-content:space-between'},h('span',{class:'sub',text:'Next milestone '}),h('b',{text:fmt(c.next)})),h('div',{class:'bar'},h('i',{style:`width:${(f*100).toFixed(1)}%`})),h('div',{class:'sub',text:`${fmt(c.next-e)} to go · ${eta(c)}`}));
const tabs=h('div',{class:'tabs'}),gbox=h('div');let cur=graphs[c.i]||7;
for(const dd of [7,30]){const b=h('button',{text:dd+' days',class:dd==cur?'on':'',onclick:()=>{graphs[c.i]=dd;[...tabs.children].forEach(x=>x.className='');b.className='on';graph(c,dd,gbox)}});tabs.append(b)}
card.append(tabs,gbox);graph(c,cur,gbox);
if(c.vid){const v=c.vid;card.append(h('a',{class:'vid',href:'https://youtu.be/'+v.id,target:'_blank'},h('img',{src:`https://i.ytimg.com/vi/${v.id}/mqdefault.jpg`,alt:''}),h('div',{style:'min-width:0'},h('div',{class:'t'},v.live?h('span',{class:'live',text:'LIVE'}):null,v.title),h('div',{class:'sub',text:v.live?`${fmt(v.viewers)} watching now`:`${ago(v.pub)} · ${dur(v.dur)}`}),h('div',{class:'sub',text:`${cmp(v.views)} views · ${v.likes>=0?cmp(v.likes)+' likes':'likes hidden'} · ${v.comments>=0?cmp(v.comments)+' comments':'comments off'}`}))))}
return card}
function render(){const app=$('#app');app.textContent='';const C=D.channels;
$('#meta').textContent=D.err?D.err:(D.updatedAgo>=0?'Updated '+(D.updatedAgo<60?'just now':(D.updatedAgo/60|0)+' min ago'):'');
const row=h('div',{class:'row'});
// summary
const ok=C.filter(c=>c.statsOk).sort((a,b)=>b.d1-a.d1);const nv=C.filter(c=>c.vid&&Date.now()/1000-c.vid.pub<86400);
row.append(h('div',{class:'card'},h('h2',{text:'Last 24 hours'}),h('div',{class:'list'},ok.length?ok.slice(0,5).map(c=>h('div',{},h('span',{text:name(c)}),h('b',{class:cls(c.d1),text:sg(c.d1)}))):h('div',{class:'sub',text:'Collecting data – check back in a few hours'})),h('div',{class:'sub',style:'margin-top:8px',text:nv.length?`${nv.length} new video${nv.length>1?'s':''} today`:'No new videos in the last day'})));
// race
if(D.race){const A=C[D.race[0]],B=C[D.race[1]],ea=est(A),eb=est(B),fa=ea+eb?ea/(ea+eb):.5;const lead=ea>=eb?A:B,ch=lead===A?B:A,closing=(ch.rate||0)-(lead.rate||0),gap=Math.abs(ea-eb);
row.append(h('div',{class:'card race'},h('h2',{text:'Race'}),h('div',{class:'side'},h('span',{},av(A),h('b',{style:'color:var(--red)',text:name(A)})),h('b',{text:fmt(ea)})),h('div',{class:'side'},h('span',{},av(B),h('b',{style:'color:var(--blue)',text:name(B)})),h('b',{text:fmt(eb)})),h('div',{class:'tug'},h('div',{class:'a',style:`width:${fa*100}%`}),h('div',{class:'b',style:`width:${(1-fa)*100}%`})),h('div',{text:`Gap ${fmt(gap)}`}),h('div',{class:'sub',text:closing>0.01?`${name(ch)} is catching up by ${fmt(Math.round(closing))}/day – could pass in about ${Math.max(1,Math.round(gap/closing))} days`:(lead.rate||ch.rate)?`${name(lead)} is pulling away`:'Trend: need more data'})))}
// leaderboard
const lb=[...C].filter(c=>c.subs>=0).sort((a,b)=>b.subs-a.subs);
row.append(h('div',{class:'card'},h('h2',{text:'Leaderboard'}),h('table',{},lb.map((c,k)=>h('tr',{class:c.i==0?'me':''},h('td',{text:k+1+'.'}),h('td',{},av(c)),h('td',{text:name(c)}),h('td',{class:'n',text:cmp(c.subs)}),h('td',{class:'n '+cls(c.today),text:c.statsOk?sg(c.today):''}))))));
app.append(row);
const grid=h('div',{class:'row'});C.forEach(c=>grid.append(channelCard(c)));app.append(grid)}
async function load(){try{const r=await fetch('/api/data');D=await r.json();render()}catch(e){$('#meta').textContent='Board not reachable'}}
setInterval(()=>{if(!D)return;document.querySelectorAll('[data-est]').forEach(el=>{const c=D.channels[el.dataset.est];el.textContent=fmt(est(c))})},1000);
load();setInterval(load,60000);
</script></body></html>)HTML";

void handleDashboard() {
  if (portalMode) { handleSettings(); return; }
  server.send_P(200, "text/html", DASH_HTML);
}

void handleApiData() {
  JsonDocument doc;
  doc["now"] = (long)nowT();
  doc["updatedAgo"] = lastFetchOk ? (long)((millis() - lastFetchOk) / 1000) : -1;
  doc["err"] = netError;
  doc["est"] = cfgEst;
  doc["ip"] = WiFi.localIP().toString();
  if (raceSet()) { JsonArray r = doc["race"].to<JsonArray>(); r.add(cfgRaceA); r.add(cfgRaceB); }
  else doc["race"] = nullptr;
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    JsonObject o = arr.add<JsonObject>();
    o["i"] = i; o["handle"] = c.handle; o["id"] = c.id; o["title"] = c.title;
    o["subs"] = c.subs; o["step"] = c.subs >= 0 ? stepFor(c.subs) : 1;
    o["rate"] = c.ratePerDay; o["stepAt"] = (long)c.stepChangedAt;
    o["views"] = c.views; o["videos"] = c.videos; o["joined"] = (long)c.joined;
    o["country"] = c.country; o["avatar"] = c.avatarUrl; o["err"] = c.err;
    o["statsOk"] = c.statsOk; o["today"] = c.gainToday; o["d1"] = c.gain24;
    o["d7"] = c.gain7; o["d30"] = c.gain30; o["histStart"] = (long)c.histStart;
    long e = c.subs >= 0 ? estimateFor(i) : 0;
    long nx = nextMilestone(max(0L, e));
    o["next"] = nx; o["prev"] = prevMilestone(nx);
    if (c.vidId.length() && c.vidTitle.length()) {
      JsonObject v = o["vid"].to<JsonObject>();
      v["id"] = c.vidId; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished; v["dur"] = c.vidDuration;
      v["views"] = c.vidViews; v["likes"] = c.vidLikes; v["comments"] = c.vidComments;
      v["live"] = c.live; v["viewers"] = c.liveViewers;
    }
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// [[time, subs], ...] for one channel, streamed in chunks
void handleApiHistory() {
  int i = server.arg("i").toInt();
  int days = constrain(server.arg("days").toInt(), 1, 31);
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");
  server.sendContent("[");
  if (i >= 0 && i < numCh && fsOk && ch[i].id.length()) {
    File f = LittleFS.open(histPath(i), "r");
    if (f) {
      time_t from = nowT() - (time_t)days * 86400;
      String chunk; bool first = true; Sample s;
      while (f.read((uint8_t *)&s, sizeof(s)) == sizeof(s)) {
        if ((time_t)s.t < from) continue;
        chunk += (first ? "[" : ",[") + String(s.t) + "," + String(s.s) + "]";
        first = false;
        if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
      }
      f.close();
      if (chunk.length()) server.sendContent(chunk);
    }
  }
  server.sendContent("]");
  server.sendContent("");
}

String checkbox(const char *name, bool on, const char *label) {
  return String("<label><input type='checkbox' name='") + name + "' value='1' style='width:auto'" + (on ? " checked" : "") + "> " + label + "</label>";
}

String hourSelect(const char *name, int val) {
  String s = String("<select name='") + name + "'><option value='-1'" + (val < 0 ? " selected" : "") + ">Off</option>";
  for (int h = 0; h < 24; h++) {
    char b[8]; snprintf(b, sizeof(b), "%02d:00", h);
    s += "<option value='" + String(h) + "'" + (val == h ? " selected" : "") + ">" + b + "</option>";
  }
  return s + "</select>";
}

String channelSelect(const char *name, int val) {
  String s = String("<select name='") + name + "'><option value='-1'>None</option>";
  for (int i = 0; i < numCh; i++)
    s += "<option value='" + String(i) + "'" + (val == i ? " selected" : "") + ">" + htmlEscape(ch[i].title.length() ? ch[i].title : ch[i].handle) + "</option>";
  return s + "</select>";
}

void handleSettings() {
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter settings</h1><p>Saved on the board only.";
  if (!portalMode) h += " <a href='/'>&larr; Dashboard</a>";
  h += "</p>";
  if (wifiFailReason.length()) {
    h += "<div style='background:#3a1210;border:1px solid #e62117;border-radius:10px;padding:12px;margin-bottom:8px;font-size:14px'>"
         "<b>Last attempt to join " + htmlEscape(cfgSsid) + " failed:</b><br>" + htmlEscape(wifiFailReason) + "</div>";
  }
  h += "<form method='POST' action='/save'>";
  h += "<label>YouTube channels (up to 10, one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@yourchannel&#10;@mkbhd&#10;@veritasium' required>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>@handles, UC… channel IDs or channel links. Put your own channel first – it's highlighted and shown on the night clock.</small>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' autocapitalize='off' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'>";

  h += "<h1 style='font-size:17px;margin-top:26px'>Display</h1>";
  h += checkbox("auto", cfgAuto, "Switch channels automatically every 10 seconds");
  h += checkbox("est", cfgEst, "Estimated live counts between YouTube's rounded steps (“est.”)");
  h += checkbox("celebrate", cfgCelebrate, "Confetti for milestones (bigger milestones, bigger party)");
  h += checkbox("summary", cfgSummary, "Daily summary on screen at 9 am");
  if (numCh >= 2) {
    h += "<label>Subscriber race</label><div style='display:flex;gap:8px'>" + channelSelect("raceA", cfgRaceA) +
         "<span style='align-self:center'>vs</span>" + channelSelect("raceB", cfgRaceB) + "</div>";
    h += "<small>Swipe down twice from the main count to see it. You get an alert if one overtakes the other.</small>";
  }
  h += "<label>Night clock (dims and shows the time)</label><div style='display:flex;gap:8px'>" +
       hourSelect("nightStart", cfgNightStart) + "<span style='align-self:center'>to</span>" + hourSelect("nightEnd", cfgNightEnd) + "</div>";

  h += "<h1 style='font-size:17px;margin-top:26px'>Motion</h1>";
  if (!imuOk && !portalMode) h += "<small style='color:#e62117'>Motion sensor not detected.</small>";
  h += checkbox("shake", cfgShake, "Shake to refresh");
  h += checkbox("facedown", cfgFaceDown, "Face-down turns the screen off");
  h += checkbox("portrait", cfgPortrait, "Stand it on its side for a tall leaderboard");
  h += checkbox("flip", cfgFlipPortrait, "Tall leaderboard is upside down? Tick to flip it");
  h += checkbox("tap", cfgTap, "Double-tap the desk for the next channel");
  h += "<label>Desk tap sensitivity</label><select name='tapsens'>";
  const char *sens[] = { "", "Low (firm knocks)", "Medium", "High (light taps)" };
  for (int k = 1; k <= 3; k++) h += "<option value='" + String(k) + "'" + (cfgTapSens == k ? " selected" : "") + ">" + sens[k] + "</option>";
  h += "</select>";

  h += "<h1 style='font-size:17px;margin-top:26px'>Wi-Fi</h1>";
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
  h += checkbox("compat", cfgCompat, "Compatibility mode (Wi-Fi 4 instead of Wi-Fi 6)");
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

  if (!portalMode && imuOk) {
    h += "<form method='POST' action='/calibrate' style='margin-top:22px'>"
         "<label>Motion calibration</label><small>Put the board in the position you normally use it (on its stand or flat), then press:</small>"
         "<button type='submit' style='background:#333'>Set this as the normal position</button></form>";
  }
  h += "<small style='margin-top:16px'>Board Wi-Fi MAC address: " + boardMac() + "</small></div></body></html>";
  server.send(200, "text/html", h);
}

void sendMessage(int code, const String &title, const String &body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>" + title + "</h1><p>" + body +
              "</p><a href='/settings'>Go back</a></div></body></html>");
}

void handleCalibrate() {
  calibrateMotion();
  sendMessage(200, "Calibrated &#10003;", "This is now the board's normal position.");
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
  cfgCelebrate = server.arg("celebrate") == "1";
  cfgSummary = server.arg("summary") == "1";
  cfgShake = server.arg("shake") == "1";
  cfgFaceDown = server.arg("facedown") == "1";
  cfgPortrait = server.arg("portrait") == "1";
  cfgFlipPortrait = server.arg("flip") == "1";
  cfgTap = server.arg("tap") == "1";
  if (server.hasArg("tapsens")) cfgTapSens = constrain(server.arg("tapsens").toInt(), 1, 3);
  if (server.hasArg("raceA")) { cfgRaceA = server.arg("raceA").toInt(); cfgRaceB = server.arg("raceB").toInt(); }
  if (server.hasArg("nightStart")) { cfgNightStart = server.arg("nightStart").toInt(); cfgNightEnd = server.arg("nightEnd").toInt(); }
  saveSettings();
  sendMessage(200, "Saved &#10003;", "The board is restarting and will join <b>" + htmlEscape(cfgSsid) + "</b>.");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

void handleNotFound() {
  if (!portalMode) { server.send(404, "text/plain", "Not found"); return; }
  server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/", true);
  server.send(302, "text/plain", "");
}

void registerRoutes() {
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/settings", HTTP_GET, handleSettings);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/calibrate", HTTP_POST, handleCalibrate);
  server.on("/api/data", HTTP_GET, handleApiData);
  server.on("/api/history", HTTP_GET, handleApiHistory);
  server.onNotFound(handleNotFound);
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

bool routesRegistered = false;
void startPortal() {
  portalMode = true;
  setBacklight(160);
  drawStatus("Setup mode", "Scanning Wi-Fi...", C_GOLD);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  buildScanOptions();
  WiFi.softAP(AP_NAME);
  delay(200);
  dns.start(53, "*", WiFi.softAPIP());
  if (!routesRegistered) { registerRoutes(); routesRegistered = true; }
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
  if (alertOnGain && c.subs >= 0 && n > c.subs && numAlerts < MAX_CH * 2) {
    long m = nextMilestone(c.subs);
    if (n >= m) alerts[numAlerts++] = { A_MILESTONE, i, -1, n - c.subs, m };   // crossed a milestone: party
    else        alerts[numAlerts++] = { A_GAIN, i, -1, n - c.subs, n };
  }
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
  // race: who's ahead? alert when that changes
  if (raceSet() && ch[cfgRaceA].subs >= 0 && ch[cfgRaceB].subs >= 0) {
    int lead = ch[cfgRaceA].subs >= ch[cfgRaceB].subs ? cfgRaceA : cfgRaceB;
    if (raceLeader >= 0 && lead != raceLeader && ch[cfgRaceA].subs != ch[cfgRaceB].subs && numAlerts < MAX_CH * 2)
      alerts[numAlerts++] = { A_OVERTAKE, lead, raceLeader, 0, ch[lead].subs };
    raceLeader = lead;
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
enum Mode { M_NORMAL, M_SLEEP, M_PORTRAIT, M_SUMMARY, M_AMBIENT };
Mode mode = M_NORMAL;
bool summaryActive = false;
unsigned long summaryShownAt = 0;
int lastSummaryDay = -1;
int portraitRot = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SubCounter v8.2 starting");
  setenv("TZ", TZ_UK, 1); tzset();

  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  gfx->begin();
  lcdRegInit();
  gfx->setRotation(1);
  gfx->setTextWrap(false);
  gfx->setUTF8Print(true);
  gfx->fillScreen(C_BG);
  ledcAttach(LCD_BL, 5000, 8);
  setBacklight(BL_NORMAL);
  touchInit();
  imuInit();
  fsOk = LittleFS.begin(true);
  Serial.printf("History storage %s\n", fsOk ? "ready" : "unavailable");
  randomSeed(esp_random());

  WiFi.onEvent(onWiFiEvent);
  loadSettings();

  if (cfgSsid.length() == 0 || cfgApiKey.length() == 0 || numCh == 0) { startPortal(); return; }
  if (!connectWiFi()) { drawWifiFailScreen(); delay(8000); startPortal(); return; }

  configTime(0, 0, "pool.ntp.org", "time.google.com");   // backup: clock is also set from Google's replies
  setenv("TZ", TZ_UK, 1); tzset();
  registerRoutes(); routesRegistered = true;
  server.begin();
  drawStatus("Loading...", String(numCh) + (numCh == 1 ? " channel" : " channels"), C_WHITE);
  fetchAll();
  lastFetch = millis();
  drawStatus("Loading...", "pictures & videos", C_WHITE);
  fetchAvatars();
  fetchLatestVideos();
  lastVideoFetch = millis();
  lastInteract = millis();
  numAlerts = 0;                       // nothing to celebrate on start-up
  drawMain();
  if (ch[page].subs > 0) { shownSubs = max(0L, estimateFor(page) - 30); drawMainNumber(shownSubs); }
  Serial.print("Dashboard: http://"); Serial.println(WiFi.localIP());
}

// Redraw whatever the current mode shows
void drawMode() {
  switch (mode) {
    case M_SLEEP: break;
    case M_PORTRAIT: drawPortraitBoard(); break;
    case M_SUMMARY: drawSummary(); break;
    case M_AMBIENT: drawAmbient(); break;
    default: drawView(); break;
  }
}

void enterMode(Mode m) {
  if (m == mode) return;
  Mode old = mode;
  mode = m;
  if (old == M_PORTRAIT) gfx->setRotation(1);
  switch (m) {
    case M_SLEEP:    fadeBacklight(0, 2); gfx->fillScreen(C_BG); break;
    case M_PORTRAIT: setBacklight(BL_NORMAL); gfx->setRotation(portraitRot); drawPortraitBoard(); break;
    case M_SUMMARY:  setBacklight(BL_NORMAL); drawSummary(); break;
    case M_AMBIENT:  drawAmbient(); fadeBacklight(BL_NIGHT, 25); break;
    default:
      gfx->fillScreen(C_BG);
      drawView();
      if (blNow != BL_NORMAL) fadeBacklight(BL_NORMAL, 2);
      break;
  }
}

// Something the user did: wake from night / summary
void userActivity() {
  lastInteract = millis();
  if (mode == M_SUMMARY) summaryActive = false;
}

void loop() {
  server.handleClient();
  if (portalMode) { dns.processNextRequest(); delay(2); return; }

  // ── inputs ────────────────────────────────────────────────────────────────
  imuUpdate();
  bool shake = evShake; evShake = false;
  bool tap = evTap; evTap = false;
  char swipe = pollSwipe();
  if (touching) lastTouchActivity = millis();

  bool bootShort = false;
  if (digitalRead(BOOT_BTN) == LOW) {
    if (!btnDownAt) btnDownAt = millis();
    if (millis() - btnDownAt > HOLD_FOR_SETUP_MS) { server.stop(); startPortal(); btnDownAt = 0; return; }
  } else if (btnDownAt) {
    if (millis() - btnDownAt > 40) bootShort = true;
    btnDownAt = 0;
  }

  // ── 9 am summary ──────────────────────────────────────────────────────────
  if (cfgSummary && timeValid()) {
    time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
    if (lt.tm_hour == 9 && lt.tm_min < 30 && lt.tm_yday != lastSummaryDay) {
      lastSummaryDay = lt.tm_yday;
      summaryActive = true; summaryShownAt = millis();
    }
  }
  if (summaryActive && millis() - summaryShownAt > SUMMARY_SHOW_MS) summaryActive = false;

  // ── decide the mode ───────────────────────────────────────────────────────
  bool anyInput = swipe || tap || shake || bootShort || touching;
  if (anyInput && (mode == M_AMBIENT || mode == M_SUMMARY)) {
    bool wasOverlay = true;
    userActivity();
    swipe = 0; tap = false; bootShort = false;   // this input just wakes it up
    (void)wasOverlay;
  }
  Mode want = M_NORMAL;
  if (cfgFaceDown && orient == O_FACEDOWN) want = M_SLEEP;
  else if (cfgPortrait && (orient == O_PORTRAIT_A || orient == O_PORTRAIT_B)) {
    want = M_PORTRAIT;
    int r = (orient == O_PORTRAIT_A) ? 0 : 2;   // tick "flip" in settings if this is upside down
    if (cfgFlipPortrait) r = 2 - r;
    if (mode == M_PORTRAIT && r != portraitRot) { portraitRot = r; gfx->setRotation(r); drawPortraitBoard(); }
    portraitRot = r;
  }
  else if (summaryActive) want = M_SUMMARY;
  else if (isNight() && millis() - lastInteract > NIGHT_IDLE_MS) want = M_AMBIENT;
  if (want != mode) {
    if (mode == M_SLEEP || mode == M_PORTRAIT) lastInteract = millis();   // picked up again
    enterMode(want);
  }

  // ── actions in the normal view ────────────────────────────────────────────
  if (mode == M_NORMAL) {
    switch (swipe) {
      case 'L': changePage(+1); lastInteract = millis(); break;
      case 'R': changePage(-1); lastInteract = millis(); break;
      case 'U': changeCard(+1); lastInteract = millis(); break;
      case 'D': changeCard(-1); lastInteract = millis(); break;
    }
    if (bootShort) { changePage(+1); lastInteract = millis(); }
    if (tap && cfgTap && !board) { changePage(+1); lastInteract = millis(); }
    if (shake && cfgShake) {
      lastInteract = millis();
      if (!board && card == 0) { gfx->fillRect(0, 150, gfx->width() - 14, 22, C_BG); ft(8, 168, "Refreshing...", F_S, C_GOLD); }
      lastFetch = 0;                                           // fetch counts now
      if (millis() - lastVideoFetch > 3UL * 60000UL) lastVideoFetch = 0;   // and videos, if not done recently
    }
    // back to the main count after a while untouched
    if ((card != 0 || board) && millis() - lastInteract > IDLE_RETURN_MS) {
      card = 0; board = false; raceView = false; drawView(); lastInteract = millis();
    }
    // auto-switch channels (main count only, pauses 30 s after you touch it)
    if (cfgAuto && numCh > 1 && card == 0 && !board && millis() - lastInteract > 30000UL) {
      static unsigned long lastAuto = 0;
      if (millis() - lastAuto > AUTO_SWITCH_MS) { lastAuto = millis(); changePage(+1); }
    }
  }

  // ── network ───────────────────────────────────────────────────────────────
  if (WiFi.status() != WL_CONNECTED) {
    netError = "Wi-Fi lost, reconnecting...";
    if (mode == M_NORMAL) drawFooter();
    WiFi.reconnect();
    delay(3000);
    return;
  }

  bool refreshed = false;
  if (lastFetch == 0 || millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    fetchAll();
    fetchAvatars();          // picks up any that failed earlier
    refreshed = true;
  }
  if (lastVideoFetch == 0 || millis() - lastVideoFetch > VIDEO_REFRESH_MS) {
    lastVideoFetch = millis();
    fetchLatestVideos();
    refreshed = true;
  }
  if (refreshed) {
    // celebrate only when someone can see it (not face-down, side-on or at night)
    if (numAlerts && (mode == M_NORMAL || mode == M_SUMMARY)) {
      setBacklight(BL_NORMAL);
      for (int i = 0; i < numAlerts; i++) showAlert(alerts[i]);
      gfx->fillScreen(C_BG);
    }
    numAlerts = 0;
    if (mode == M_NORMAL && card == 0 && !board) { long keep = shownSubs; drawMain(); shownSubs = keep; drawMainNumber(shownSubs); }
    else drawMode();
  }

  // ── animation / periodic redraws ──────────────────────────────────────────
  static unsigned long lastAnim = 0;
  if (mode == M_NORMAL && numCh && card == 0 && !board && ch[page].subs >= 0 && millis() - lastAnim > 30) {
    lastAnim = millis();
    long target = estimateFor(page);
    if (shownSubs != target) {
      if (shownSubs < 0 || shownSubs > target) shownSubs = target;
      else { long gap = target - shownSubs; shownSubs += (gap > 20) ? gap / 8 : 1; }
      drawMainNumber(shownSubs);
    }
  }
  static unsigned long lastSlow = 0;
  if (millis() - lastSlow > 30000UL) {
    lastSlow = millis();
    if (mode == M_AMBIENT) drawAmbient();
    else if (mode == M_NORMAL) drawFooter();
  }

  delay(5);
}
