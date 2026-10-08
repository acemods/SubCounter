/*
 * SubCounter v10.1 — YouTube subscriber counter for Waveshare ESP32-C6-Touch-LCD-1.47
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
 *    Not used for 3 min ... big clock (time can be changed in settings; pick it up to go back)
 *    Hold BOOT 3 s ........ setup mode
 *
 *  APPS: long-press the screen for the home menu (YouTube, Weather, Spotify)
 *    Weather: swipe up/down for Now / Next hours / Tomorrow
 *    Spotify: tap = play/pause, swipe left/right = next/previous, up/down = volume
 *
 *  IN A BROWSER
 *    http://<board IP>/          dashboard (all channels, graphs, videos, race)
 *    http://<board IP>/settings  settings
 *    http://<board IP>/update    wireless firmware update (APP-ONLY .bin)
 *
 *  Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *    Flash Size "8MB", Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
 *    Libraries: "GFX Library for Arduino" 1.6.x, "ArduinoJson" 7.x, "JPEGDEC" 1.8.x,
 *               "U8g2" (for its fonts)
 */

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <JPEGDEC.h>
#include <PNGdec.h>      // Twitch profile pictures are usually PNG
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
#include <Update.h>    // wireless firmware updates
#include <ESPmDNS.h>   // http://subcounter.local
#define HOSTNAME "subcounter"
#define FW_VERSION "10.1"     // shown on start-up, home menu, settings, update page and dashboard

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
#define VIDEO_REFRESH_MS   (10UL * 60UL * 1000UL)   // latest videos: channels+1 units
#define AUTO_SWITCH_MS     10000UL
#define IDLE_RETURN_MS     120000UL
#define NIGHT_IDLE_MS      60000UL
#define SUMMARY_SHOW_MS    (10UL * 60UL * 1000UL)
#define ALERT_MS           3500UL
#define WIFI_TIMEOUT_MS    20000UL
#define DHCP_EXTRA_MS      15000UL
#define HOLD_FOR_SETUP_MS  3000UL
#define SWIPE_MIN_PX       35
#define NUM_CARDS          8          // 0 main, 1 overview, 2 latest video, 3 comments, 4 growth, 5 records, 6 graph, 7 milestone
#define COMMENT_REFRESH_MS (30UL * 60UL * 1000UL)   // newest comments: 1 unit per channel
#define HIST_MAX_AGE       (40L * 86400L)        // a little over a month, for the monthly recap
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
#define C_TWITCH  0x923F   // Twitch purple #9146FF
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
  // monthly
  long gainMonth = 0, gainLastMonth = 0; bool lastMonthOk = false;
  // newest comments on the latest video
  String cmAuthor[3], cmText[3]; time_t cmT[3] = {0, 0, 0}; long cmLikes[3] = {0, 0, 0};
  int cmN = 0; bool cmOff = false;
  // personal bests (saved on the board)
  int32_t pbDay = 0, pbWeek = 0, pbVid = 0; uint32_t pbDayT = 0, pbWeekT = 0, pbVidT = 0; String pbVidTitle;
  bool pbLoaded = false;
  // Twitch channels ("twitch.tv/name" in the list)
  bool tw = false; String twLogin, twId, twGame, twThumb;
  uint16_t *av565 = nullptr;          // decoded 88x88 picture (Twitch)
};
Channel ch[MAX_CH];
int numCh = 0;
int page = 0;              // channel on screen
int card = 0;              // 0..NUM_CARDS-1
bool board = false;        // leaderboard / race showing
bool raceView = false;     // (when board) showing the race instead of the leaderboard
long shownSubs = -1;

enum AlertType { A_GAIN, A_MILESTONE, A_OVERTAKE, A_VIEWS, A_LIVE, A_RECORD };
struct Alert { AlertType type; int idx; int idx2; long delta; long total; };
Alert alerts[MAX_CH * 2];
int numAlerts = 0;
int jumpToCh = -1;
int raceLeader = -1;

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

String cfgSsid, cfgPass, cfgChannels, cfgApiKey, cfgUser;   // cfgSsid... = the network being tried / in use
String cfgIp, cfgGw, cfgMask, cfgDns;
// Saved Wi-Fi networks (e.g. work + home); the board joins whichever is in range
#define MAX_NETS 5
struct Net { String ssid, pass, user, ip, gw, mask, dns; bool compat = false; };
Net nets[MAX_NETS];
int numNets = 0, curNet = -1;
int cfgPreferred = -1;          // saved network to try first when it's in range (-1 = strongest wins)
int pendingSwitch = -1;         // "Connect now" request from the settings page
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

enum Mode { M_NORMAL, M_SLEEP, M_PORTRAIT, M_SUMMARY, M_AMBIENT, M_CLOCK };
int cfgIdleClock = 3;          // minutes without use before the clock appears (0 = off)
Mode mode = M_NORMAL;
float  cfgWxLat = 55.861f, cfgWxLon = -4.250f;   // weather location (default Glasgow)
String cfgWxName = "Glasgow";
String cfgSpId, cfgSpSecret, cfgSpRefresh;       // Spotify app + saved login
String cfgPin;
String cfgTwId, cfgTwSecret;                     // Twitch app (dev.twitch.tv/console)                                   // settings PIN (blank = none)
bool   cfgLiveAlert = true;                      // alert when a channel goes live
String cfgSpRedirect;                            // https address registered with Spotify (e.g. GitHub Pages relay)

#define VT_MAX 110
struct VideoTrack {
  String vid; time_t pub = 0; bool active = false;
  int n = 0; uint32_t t[VT_MAX]; uint32_t v[VT_MAX];
  long typical = -1;               // median lifetime views of your recent uploads
  long base1h = -1, base24h = -1;  // your usual views after 1 hour / 1 day (from past uploads)
  long at1h = -1, at24h = -1;      // this video
  long nextMilestone = 100;
} vt;
unsigned long lastVtFetch = 0, lastTypicalFetch = 0, lastCommentFetch = 0;

// Forward declarations (functions used before they're defined)
void drawMode();
long vtViews();
void drawVideoTracker();
extern const char PAGE_HEAD[];
int ytGet(const String &endpoint, const String &query, JsonDocument &doc);
void twitchFetch();
extern String twError;
void raceCheck();
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

// Start of this month (back = 0) or an earlier one (back = 1 is last month), local time
time_t monthStart(int back) {
  time_t n = nowT();
  struct tm lt; localtime_r(&n, &lt);
  lt.tm_mday = 1; lt.tm_hour = 0; lt.tm_min = 0; lt.tm_sec = 0; lt.tm_isdst = -1;
  lt.tm_mon -= back;
  while (lt.tm_mon < 0) { lt.tm_mon += 12; lt.tm_year--; }
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
    String lo = h; lo.toLowerCase();
    int tp = lo.indexOf("twitch.tv/");
    if (tp >= 0 || lo.startsWith("twitch:")) {             // Twitch channel
      String login = tp >= 0 ? lo.substring(tp + 10) : lo.substring(7);
      login.trim();
      int e = 0; while (e < (int)login.length() && (isalnum((unsigned char)login[e]) || login[e] == '_')) e++;
      login = login.substring(0, e);
      if (!login.length()) continue;
      ch[numCh] = Channel();
      ch[numCh].tw = true; ch[numCh].twLogin = login;
      ch[numCh].handle = login; ch[numCh].id = "tw_" + login;   // id is only used for history file names
      numCh++;
      continue;
    }
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
  numNets = 0;
  cfgPreferred = prefs.getInt("prefnet", -1);
  if (prefs.isKey("nnets")) {
    numNets = constrain(prefs.getInt("nnets", 0), 0, MAX_NETS);
    for (int k = 0; k < numNets; k++) {
      String p = "n" + String(k);
      nets[k].ssid = prefs.getString((p + "ssid").c_str(), "");
      nets[k].pass = prefs.getString((p + "pass").c_str(), "");
      nets[k].user = prefs.getString((p + "user").c_str(), "");
      nets[k].ip   = prefs.getString((p + "ip").c_str(), "");
      nets[k].gw   = prefs.getString((p + "gw").c_str(), "");
      nets[k].mask = prefs.getString((p + "mask").c_str(), "");
      nets[k].dns  = prefs.getString((p + "dns").c_str(), "");
      nets[k].compat = prefs.getBool((p + "compat").c_str(), false);
    }
  } else if (prefs.getString("ssid", "").length()) {      // upgrade from the single-network versions
    Net &n = nets[0];
    n.ssid = prefs.getString("ssid", ""); n.pass = prefs.getString("pass", ""); n.user = prefs.getString("user", "");
    n.ip = prefs.getString("ip", ""); n.gw = prefs.getString("gw", ""); n.mask = prefs.getString("mask", "");
    n.dns = prefs.getString("dns", ""); n.compat = prefs.getBool("compat", false);
    numNets = 1;
  }
  cfgChannels = prefs.getString("channels", prefs.getString("channel", ""));
  cfgApiKey   = prefs.getString("apikey", "");
  cfgAuto     = prefs.getBool("auto", false);
  cfgEst      = prefs.getBool("est", true);
  cfgCelebrate = prefs.getBool("celebrate", true);
  cfgPin = prefs.getString("pin", "");
  cfgTwId = prefs.getString("twid", "");
  cfgTwSecret = prefs.getString("twsec", "");
  cfgLiveAlert = prefs.getBool("livealert", true);
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
  cfgWxLat    = prefs.getFloat("wxlat", 55.861f);
  cfgWxLon    = prefs.getFloat("wxlon", -4.250f);
  cfgWxName   = prefs.getString("wxname", "Glasgow");
  cfgSpId     = prefs.getString("spid", "");
  cfgSpSecret = prefs.getString("spsec", "");
  cfgSpRefresh = prefs.getString("spref", "");
  cfgSpRedirect = prefs.getString("spredir", "");
  cfgIdleClock = prefs.getInt("idleclk", 3);
  g0Saved     = prefs.isKey("g0z");
  prefs.end();
  parseChannels();
}

void saveSettings() {
  prefs.begin("subcounter", false);
  prefs.putInt("nnets", numNets);
  prefs.putInt("prefnet", cfgPreferred);
  for (int k = 0; k < numNets; k++) {
    String p = "n" + String(k);
    prefs.putString((p + "ssid").c_str(), nets[k].ssid);
    prefs.putString((p + "pass").c_str(), nets[k].pass);
    prefs.putString((p + "user").c_str(), nets[k].user);
    prefs.putString((p + "ip").c_str(), nets[k].ip);
    prefs.putString((p + "gw").c_str(), nets[k].gw);
    prefs.putString((p + "mask").c_str(), nets[k].mask);
    prefs.putString((p + "dns").c_str(), nets[k].dns);
    prefs.putBool((p + "compat").c_str(), nets[k].compat);
  }
  prefs.putString("channels", cfgChannels);
  prefs.putString("apikey", cfgApiKey);
  prefs.putBool("auto", cfgAuto);
  prefs.putBool("est", cfgEst);
  prefs.putBool("celebrate", cfgCelebrate);
  prefs.putString("pin", cfgPin);
  prefs.putString("twid", cfgTwId);
  prefs.putString("twsec", cfgTwSecret);
  prefs.putBool("livealert", cfgLiveAlert);
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
  prefs.putFloat("wxlat", cfgWxLat);
  prefs.putFloat("wxlon", cfgWxLon);
  prefs.putString("wxname", cfgWxName);
  prefs.putString("spid", cfgSpId);
  prefs.putString("spsec", cfgSpSecret);
  prefs.putString("spref", cfgSpRefresh);
  prefs.putString("spredir", cfgSpRedirect);
  prefs.putInt("idleclk", cfgIdleClock);
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
  if (numAlerts < MAX_CH * 2) alerts[numAlerts++] = { A_RECORD, i, kind, prev, now };
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

int livePill(int cx, int by, bool big);
// Draw a channel's picture at (x,y), full 88px or half size 44px
bool drawAvatar(int i, int x, int y, bool half) {
  Channel &c = ch[i];
  if (c.av565) {                       // Twitch: pre-decoded 88x88 bitmap, circle-cropped here
    int w = half ? 44 : 88, st = half ? 2 : 1;
    avCx = x + w / 2; avCy = y + w / 2; avR = w / 2;
    static uint16_t line[88];
    for (int r = 0; r < w; r++) {
      int dy = r - avR; int hw = (int)sqrtf((float)(avR * avR - dy * dy));
      int x0 = avR - hw, x1 = min(w - 1, avR + hw);
      for (int k = x0; k <= x1; k++) line[k - x0] = c.av565[(r * st) * 88 + k * st];
      gfx->draw16bitRGBBitmap(x + x0, y + r, line, x1 - x0 + 1, 1);
    }
  } else {
  if (!c.avatar) return false;
  if (!jpeg.openRAM(c.avatar, c.avatarLen, jpegDraw)) return false;
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  int w = jpeg.getWidth() / (half ? 2 : 1);
  avCx = x + w / 2; avCy = y + w / 2; avR = w / 2;
  jpeg.decode(x, y, half ? JPEG_SCALE_HALF : 0);
  jpeg.close();
  }
  if (c.live) {                       // YouTube-style live ring + pill
    for (int k = 1; k <= (half ? 2 : 3); k++) gfx->drawCircle(avCx, avCy, avR + k, C_RED);
    livePill(avCx, avCy + avR + (half ? 5 : 8), !half);
  }
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
    case 6: drawGraph(); break;
    case 7: drawMilestone(); break;
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

// Returns 'L','R','U','D' for a swipe, 'T' for a tap (position in tapX/tapY),
// 'H' once when a finger is held still for 0.7 s (long-press), 0 otherwise.
int tapX = 0, tapY = 0;
bool holdFired = false;
char pollSwipe() {
  if (!touchOk || millis() - lastTouchPoll < 20) return 0;
  lastTouchPoll = millis();
  int x, y;
  if (touchRead(x, y)) {
    if (!touching) { touching = true; holdFired = false; tStartX = x; tStartY = y; tStartAt = millis(); }
    tLastX = x; tLastY = y;
    if (!holdFired && millis() - tStartAt > 700 && abs(x - tStartX) < 15 && abs(y - tStartY) < 15) { holdFired = true; return 'H'; }
    return 0;
  }
  if (!touching) return 0;
  touching = false;
  if (holdFired) return 0;
  int dx = tLastX - tStartX, dy = tLastY - tStartY;
  if (abs(dx) < 15 && abs(dy) < 15 && millis() - tStartAt < 500) { tapX = tLastX; tapY = tLastY; return 'T'; }
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
bool evShake = false, evTap = false, evMoved = false;
unsigned long lastMoveEvent = 0;

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
  // picked up / nudged (typing on the desk stays well below this)
  if (hp > 0.2f && now - lastMoveEvent > 500) { lastMoveEvent = now; evMoved = true; }

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

// After joining Wi-Fi: show where the dashboard is for a few seconds
void showAddress() {
  gfx->fillScreen(C_BG);
  ftC(40, "Connected to " + WiFi.SSID(), F_S, C_GREEN, gfx->width());
  ftC(84, WiFi.localIP().toString(), F_M, C_WHITE, gfx->width());
  ftC(118, "http://" HOSTNAME ".local", F_S, C_GOLD, gfx->width());
  ftC(150, "Open either in a browser", F_S, C_DKGREY, gfx->width());
  ftC(168, "SubCounter v" FW_VERSION, F_S, C_DKGREY, gfx->width());
  waitShowing(5000);
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
  if (numCh > 0 && min(48, 284 / numCh) >= 44) ftR(W - 6, 22, "today", F_XS, C_GREEN);
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
      int lw = ch[i].live ? tw("LIVE", F_XS) + 14 : 0;
      String nm = String(r + 1) + " " + fit(nameOf(i), F_S, W - 30 - lw);
      ft(6, y + 18, nm, F_S, col);
      if (ch[i].live) livePillAfter(6 + tw(nm, F_S), y + 16);
      ft(6, y + 40, subs, F_M, col);
      if (ch[i].statsOk && ch[i].gainToday) ftR(W - 6, y + 40, signedNum(ch[i].gainToday), F_S, C_GREEN);
    } else {
      int sw = tw(subs, F_S);
      ft(6, y + 19, String(r + 1), F_S, C_GREY);
      int lw = ch[i].live ? tw("LIVE", F_XS) + 14 : 0;
      String nm = fit(nameOf(i), F_S, W - 34 - sw - lw);
      ft(24, y + 19, nm, F_S, col);
      if (ch[i].live) livePillAfter(24 + tw(nm, F_S), y + 17);
      ftR(W - 6, y + 19, subs, F_S, col);
    }
    y += rowH;
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  Wireless updates (upload a new .bin from the settings page)
// ════════════════════════════════════════════════════════════════════════════
bool otaOk = false;
String otaErr = "";
size_t otaBytes = 0;

const char UPDATE_BODY[] PROGMEM = R"HTML(
<h1>&#11014; Update firmware</h1>
<p>Choose the <b>APP-ONLY</b> .bin file (not the FULL one). Your settings and history are kept. Takes about 30 seconds.</p>
<input type="file" id="f" accept=".bin">
<button id="go" onclick="up()">Upload &amp; install</button>
<div class="bar" style="height:12px;border-radius:7px;background:#2a2a2e;overflow:hidden;margin-top:16px;display:none" id="pb"><i id="pi" style="display:block;height:100%;width:0;background:#30d158"></i></div>
<p id="msg" style="margin-top:12px"></p>
<p><a href="/settings">&larr; Back to settings</a></p>
</div><script>
function up(){const f=document.getElementById('f').files[0],m=document.getElementById('msg');
if(!f){m.textContent='Pick a .bin file first.';return}
if(/FULL/i.test(f.name)){m.textContent='That is the FULL image (for brand-new boards). Use the APP-ONLY file here.';return}
const fd=new FormData();fd.append('firmware',f,f.name);const x=new XMLHttpRequest();
document.getElementById('pb').style.display='block';document.getElementById('go').disabled=true;
x.upload.onprogress=e=>{if(e.lengthComputable){const p=e.loaded/e.total*100;document.getElementById('pi').style.width=p+'%';m.textContent='Uploading '+p.toFixed(0)+'%'}};
x.onload=()=>{m.innerHTML=x.responseText;if(x.status==200){setTimeout(()=>location.href='/',15000)}else document.getElementById('go').disabled=false};
x.onerror=()=>{m.textContent='Upload failed - check the board is still on Wi-Fi and try again.';document.getElementById('go').disabled=false};
x.open('POST','/update');x.send(fd)}
</script></body></html>)HTML";

// Settings PIN: browsers ask for it (user name "admin"). Not needed in setup
// mode, which needs a hand on the BOOT button, so a forgotten PIN can be cleared there.
bool pinOk() { return portalMode || !cfgPin.length() || server.authenticate("admin", cfgPin.c_str()); }
bool authed() {
  if (pinOk()) return true;
  server.requestAuthentication(BASIC_AUTH, "SubCounter settings - user name: admin, password: your PIN",
                               "<h3>PIN needed</h3>Use the user name <b>admin</b> and your SubCounter PIN. "
                               "Forgotten it? Hold BOOT for 3 seconds and clear it in setup mode.");
  return false;
}

void handleUpdatePage() {
  if (!authed()) return;
  String b = FPSTR(UPDATE_BODY);
  b.replace("<h1>&#11014; Update firmware</h1>", "<h1>&#11014; Update firmware</h1><p>Currently running <b>v" FW_VERSION "</b></p>");
  server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + b);
}

void drawOtaProgress(const String &line2) {
  gfx->fillRect(0, 96, gfx->width(), 40, C_BG);
  centreText(line2, 104, 2, C_GREY);
}

void handleUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    otaOk = false; otaErr = ""; otaBytes = 0;
    if (!pinOk()) { otaErr = "PIN required"; return; }
    setBacklight(BL_NORMAL);
    if (mode == M_PORTRAIT) gfx->setRotation(1);
    drawStatus("Updating...", "receiving", C_GOLD);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) otaErr = String("Can't start: ") + Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaErr.length()) return;
    if (otaBytes == 0) {
      // An app image carries the ESP-IDF app description (magic 0xABCD5432) at byte 32;
      // the FULL image starts with the bootloader instead, which must not go here.
      bool isApp = up.currentSize > 36 && up.buf[0] == 0xE9 &&
                   up.buf[32] == 0x32 && up.buf[33] == 0x54 && up.buf[34] == 0xCD && up.buf[35] == 0xAB;
      if (!isApp) { otaErr = "That isn't an APP-ONLY firmware file."; Update.abort(); return; }
    }
    if (Update.write(up.buf, up.currentSize) != up.currentSize) { otaErr = String("Write failed: ") + Update.errorString(); return; }
    otaBytes += up.currentSize;
    static size_t lastDrawn = 0;
    if (otaBytes - lastDrawn > 64 * 1024 || otaBytes < lastDrawn) { lastDrawn = otaBytes; drawOtaProgress(String(otaBytes / 1024) + " KB"); }
  } else if (up.status == UPLOAD_FILE_END) {
    if (otaErr.length()) return;
    if (Update.end(true)) otaOk = true;
    else otaErr = String("Check failed: ") + Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaErr = "Upload was interrupted.";
  }
}

void handleUpdateDone() {
  if (!authed()) return;
  if (otaOk) {
    server.send(200, "text/html", "<b style='color:#30d158'>Installed &#10003;</b> The board is restarting &ndash; this page will go back to the dashboard in a few seconds.");
    drawStatus("Updated!", "Restarting...", C_GREEN);
    delay(1200);
    ESP.restart();
  } else {
    server.send(400, "text/html", "<b style='color:#ff3b30'>Not installed:</b> " + htmlEscape(otaErr.length() ? otaErr : String("no file received")) +
                " The board is still running the old version.");
    drawStatus("Update failed", otaErr.substring(0, 26), C_RED);
    delay(3000);
    gfx->fillScreen(C_BG);
    drawMode();
  }
}

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
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, url)) return;
  int code = http.GET();
  String body = code == 200 ? http.getString() : String("");
  http.end();
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
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000);
  if (!http.begin(client, "https://geocoding-api.open-meteo.com/v1/search?count=1&language=en&name=" + urlEncode(name))) return false;
  int code = http.GET();
  String body = code == 200 ? http.getString() : String("");
  http.end();
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
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, "https://accounts.spotify.com/api/token")) { err = "Can't reach Spotify"; return false; }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Authorization", "Basic " + b64(cfgSpId + ":" + cfgSpSecret));
  int code = http.POST(body);
  String resp = http.getString();
  http.end();
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
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000);
  if (!http.begin(client, "https://api.spotify.com/v1" + path)) return -2;
  http.addHeader("Authorization", "Bearer " + sp.access);
  int code;
  if (!strcmp(method, "GET")) code = http.GET();
  else { http.addHeader("Content-Length", "0"); code = http.sendRequest(method, (uint8_t *)nullptr, 0); }
  if (resp && code == 200) *resp = http.getString();
  http.end();
  if (code == 401) { sp.access = ""; }
  return code;
}

void spFetchArt() {
  if (sp.art) { free(sp.art); sp.art = nullptr; sp.artLen = 0; }
  if (!sp.artUrl.length()) return;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000);
  if (!http.begin(client, sp.artUrl)) return;
  if (http.GET() == 200) {
    int len = http.getSize();
    int cap = (len > 0 && len < 90000) ? len : 90000;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (buf) {
      WiFiClient *st = http.getStreamPtr();
      int got = 0; unsigned long t0 = millis();
      while (got < cap && millis() - t0 < 8000) {
        int a = st->available();
        if (a > 0) got += st->readBytes(buf + got, min(a, cap - got));
        else if (!st->connected()) break;
        else delay(3);
      }
      bool ok = (len > 0 ? got == len : got > 4) && buf[0] == 0xFF && buf[1] == 0xD8;
      if (ok) { sp.art = buf; sp.artLen = got; } else free(buf);
    }
  }
  http.end();
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
    ftC(108, sp.err.length() ? sp.err : String("Start something on Spotify"), F_S, sp.err.length() ? C_RED : C_DKGREY, gfx->width());
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

void spCommand(char what) {
  int code = 0;
  if (what == 'T') code = spCall("PUT", sp.playing ? "/me/player/pause" : "/me/player/play");
  else if (what == 'L') code = spCall("POST", "/me/player/next");
  else if (what == 'R') code = spCall("POST", "/me/player/previous");
  else if (what == 'U' || what == 'D') {
    int v = sp.volume < 0 ? 50 : sp.volume;
    v = constrain(v + (what == 'U' ? 10 : -10), 0, 100);
    code = spCall("PUT", "/me/player/volume?volume_percent=" + String(v));
    if (code >= 200 && code < 300) { sp.volume = v; spToast("Volume " + String(v) + "%"); delay(600); }
  }
  if (code == 403) { spToast("Needs Spotify Premium"); delay(1500); }
  else if (code == 404) { spToast("No active device"); delay(1500); }
  if (what == 'T' && code >= 200 && code < 300) sp.playing = !sp.playing;
  delay(250);
  spPoll();
  drawSpotify();
  lastSpPoll = millis();
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
  if (a == APP_YT) { gfx->fillRoundRect(cx - 30, cy - 21, 60, 42, 12, C_RED); gfx->fillTriangle(cx - 8, cy - 11, cx - 8, cy + 11, cx + 12, cy, C_WHITE); }
  if (a == APP_WX) { drawWxIcon(2, cx, cy, 30, true); }
  if (a == APP_SP) {
    gfx->fillCircle(cx, cy, 26, 0x1EE9);
    for (int k = 0; k < 3; k++) { gfx->drawFastHLine(cx - 13 + k * 2, cy - 8 + k * 8, 26 - k * 4, C_BG); gfx->drawFastHLine(cx - 13 + k * 2, cy - 7 + k * 8, 26 - k * 4, C_BG); }
  }
}

void drawMenu() {
  gfx->fillScreen(C_BG);
  ftR(gfx->width() - 6, 14, "v" FW_VERSION, F_XS, C_DKGREY);
  const char *names[NUM_APPS] = { "YouTube", "Weather", "Spotify" };
  int w = gfx->width() / NUM_APPS;
  for (int a = 0; a < NUM_APPS; a++) {
    int cx = a * w + w / 2;
    if (a == app) gfx->drawRoundRect(a * w + 6, 22, w - 12, 128, 12, C_DKGREY);
    drawAppIcon(a, cx, 70);
    ft(cx - tw(names[a], F_S) / 2, 130, names[a], F_S, a == app ? C_WHITE : C_GREY);
  }
  if (WiFi.status() == WL_CONNECTED) ftC(168, WiFi.localIP().toString() + "  Â·  " HOSTNAME ".local", F_S, C_GREY, gfx->width());
  else ftC(168, "Tap an app", F_S, C_DKGREY, gfx->width());
}

void drawApp() {
  if (menuOpen) { drawMenu(); return; }
  if (app == APP_WX) drawWeather();
  else if (app == APP_SP) drawSpotify();
  else drawView();
}

void openApp(int a) {
  app = a; menuOpen = false;
  prefs.begin("subcounter", false); prefs.putInt("app", app); prefs.end();
  if (app == APP_SP) { lastSpPoll = 0; }
  if (app == APP_WX && !wx.ok) lastWxFetch = 0;
  gfx->fillScreen(C_BG);
  drawApp();
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
h2{font-size:17px;margin:30px 0 4px;padding-top:16px;border-top:1px solid #2a2a2e;scroll-margin-top:12px}
.jump{display:flex;flex-wrap:wrap;gap:6px;margin:0 0 14px}.jump a{font-size:13px;background:#2a2a2e;color:#ddd;border-radius:999px;padding:5px 10px;text-decoration:none}
summary{margin-top:14px;cursor:pointer;color:#aaa;font-size:14px}
.row{display:flex;gap:8px;align-items:center}.row span{color:#aaa}
.b2{background:#2a2a2e;font-size:15px;padding:12px;margin-top:10px}
.mini{width:auto;margin:0;padding:5px 10px;font-size:13px;background:#2a2a2e}
.btnlink{display:block;text-align:center;margin-top:10px;padding:12px;border-radius:10px;background:#1db954;color:#fff;font-weight:600;text-decoration:none}
.saved{display:flex;flex-wrap:wrap;gap:6px 14px;align-items:center;padding:8px 0;border-top:1px solid #333}.saved b{flex:1 1 100%;word-break:break-word;color:#fff}
.ok{color:#30d158;margin:6px 0}
.savebar{position:sticky;bottom:0;background:#1c1c1e;padding:6px 0 10px;margin-top:22px;box-shadow:0 -10px 14px #1c1c1e}.savebar button{margin-top:6px}
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
main{max-width:1500px;margin:0 auto;padding:20px}
.layout{display:grid;grid-template-columns:340px minmax(0,1fr);gap:16px;align-items:start}
.side{display:grid;gap:16px;align-content:start}
.grid{display:grid;gap:16px;grid-template-columns:repeat(auto-fill,minmax(340px,1fr));align-items:start}
@media(max-width:1000px){.layout{grid-template-columns:1fr}.side{grid-template-columns:repeat(auto-fit,minmax(280px,1fr));align-items:start}}
@media(max-width:600px){main{padding:12px}.side,.grid{grid-template-columns:1fr}.big{font-size:38px}.vid img{width:104px}header{padding:12px}header .meta{white-space:nowrap;font-size:12px}}
.card{background:var(--card);border:1px solid var(--line);border-radius:16px;padding:18px}
.card h2{font-size:13px;letter-spacing:.06em;text-transform:uppercase;color:var(--gold);margin:0 0 12px}
.err{background:#3a1210;border-color:#e62117}
.av{width:44px;height:44px;border-radius:50%;object-fit:cover;background:#333;flex:none}
.av.lg{width:64px;height:64px}
.avw{position:relative;display:inline-flex;flex:none;border-radius:50%}
.avw.on{box-shadow:0 0 0 2px var(--card),0 0 0 4px var(--red)}.avw.on.lg{box-shadow:0 0 0 3px var(--card),0 0 0 6px var(--red)}
.avw .lv{position:absolute;left:50%;bottom:-7px;transform:translateX(-50%);background:var(--red);color:#fff;border:2px solid var(--card);border-radius:5px;padding:0 4px;font-size:9px;line-height:13px;font-weight:800;letter-spacing:.03em}
.avw.lg .lv{font-size:11px;line-height:15px;padding:0 6px;bottom:-9px}
td .avw .lv{font-size:7px;line-height:10px;padding:0 3px;bottom:-5px;border-width:1px}td .avw.on{box-shadow:0 0 0 1px var(--card),0 0 0 3px var(--red)}
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
.tabs{display:flex;gap:6px;margin-top:12px}.tabs button{border:0;border-radius:8px;padding:5px 12px;font-size:13px;background:#222226;color:var(--muted);cursor:pointer}.tabs button.on{background:#3a3a40;color:var(--text)}
table{width:100%;border-collapse:collapse}td{padding:8px 4px;border-top:1px solid var(--line)}td.n{text-align:right;font-variant-numeric:tabular-nums}
tr.me td{color:var(--gold)}
th{font-size:11px;font-weight:600;letter-spacing:.05em;text-transform:uppercase;color:var(--muted);padding:0 4px 6px}th.n{text-align:right}tr.hd+tr td{border-top:0}
.race .rs{display:flex;justify-content:space-between;align-items:center;margin:8px 0}.race .rs span{display:flex;align-items:center;gap:10px;min-width:0}
td .av{width:28px;height:28px;display:block}td{padding:6px 4px}td.nm2{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:150px}
.tug{display:flex;height:14px;border-radius:7px;overflow:hidden;margin:10px 0}.tug .a{background:var(--red)}.tug .b{background:var(--blue)}
.list div{display:flex;justify-content:space-between;padding:5px 0}
.wide{grid-column:1/-1}
.twb{background:#9146ff;color:#fff;border-radius:5px;font-size:10px;font-weight:800;padding:1px 5px;margin-left:8px;vertical-align:3px;letter-spacing:.03em}
.ctl{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:12px}
.tg{display:inline-flex;align-items:center;gap:6px;border:1px solid var(--line);border-radius:999px;padding:4px 10px;font-size:13px;background:none;color:var(--muted);cursor:pointer}
.tg.on{color:var(--text);border-color:#4a4a52;background:#222226}.tg i{width:10px;height:10px;border-radius:50%;display:inline-block}
.seg{display:inline-flex;background:#222226;border-radius:9px;padding:2px}.seg button{border:0;background:none;color:var(--muted);padding:4px 10px;border-radius:7px;font-size:13px;cursor:pointer}.seg button.on{background:#3a3a40;color:var(--text)}
.cmp{position:relative}.cmp svg{width:100%;height:260px;display:block}
.tip{position:absolute;pointer-events:none;background:#0d0d0f;border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:12px;white-space:nowrap;display:none;z-index:1}
.tip div{display:flex;gap:8px;align-items:center}.tip i{width:8px;height:8px;border-radius:50%;display:inline-block}
.small{font-size:12px;color:var(--muted);text-decoration:underline;margin-left:auto}
.cm{margin-top:14px;padding-top:12px;border-top:1px solid var(--line)}.cm h3{font-size:12px;letter-spacing:.06em;text-transform:uppercase;color:var(--muted);margin:0 0 6px}
.cm .c{padding:6px 0}.cm .c b{font-size:13px}.cm .c p{margin:2px 0 0;font-size:14px;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.pb{display:flex;flex-wrap:wrap;gap:6px 14px;font-size:13px;color:var(--muted);margin-top:10px}.pb b{color:var(--gold)}
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
function ago(t){if(!t)return'';let d=Date.now()/1000-t;const p=(n,w)=>n+' '+w+(n==1?'':'s')+' ago';if(d<3600)return Math.max(1,d/60|0)+' min ago';if(d<86400)return(d/3600|0)+' h ago';if(d<2592000)return p(d/86400|0,'day');return p(d/2592000|0,'month')}
function dur(s){if(!s)return'';const p=x=>String(x).padStart(2,'0');return s>=3600?`${s/3600|0}:${p((s/60|0)%60)}:${p(s%60)}`:`${s/60|0}:${p(s%60)}`}
let D=null,graphs={};
function est(c){if(!D.est||!c.rate||c.rate<=0||c.step<=1||!c.stepAt)return c.subs;return c.subs+Math.min(c.step-1,Math.max(0,Math.floor(c.rate*(Date.now()/1000-c.stepAt)/86400)))}
function eta(c){const e=est(c);if(!(c.rate>0.01))return c.rate<0?'Losing subscribers':'Need more data for a date';const d=(c.next-e)/c.rate;if(d<1)return'Expected today';if(d>3650)return'10+ years away';return'Expected around '+new Date(Date.now()+d*864e5).toLocaleDateString('en-GB',{day:'numeric',month:'short',year:d>300?'numeric':undefined})}
function av(c,lg){const i=c.avatar?h('img',{class:'av'+(lg?' lg':''),src:c.avatar,alt:''}):h('div',{class:'av'+(lg?' lg':'')});const L=isLive(c);return h('span',{class:'avw'+(L?' on':'')+(lg?' lg':''),title:L?name(c)+' is live now':''},i,L?h('span',{class:'lv',text:'LIVE'}):null)}
function isLive(c){return !!(c.vid&&c.vid.live)}
function name(c){return c.title||c.handle}
async function graph(c,days,box){box.textContent='';const r=await fetch('/api/history?i='+c.i+'&days='+days);const pts=await r.json();if(pts.length<2){box.append(h('div',{class:'sub',text:'Collecting data – recorded every hour'}));return}
pts.push([Date.now()/1000|0,c.subs]);const W=600,H=120,t0=pts[0][0],t1=pts[pts.length-1][0];let mn=Math.min(...pts.map(p=>p[1])),mx=Math.max(...pts.map(p=>p[1]));if(mx==mn){mx++;mn--}
const X=t=>(t-t0)/(t1-t0||1)*(W-4)+2,Y=v=>H-6-(v-mn)/(mx-mn)*(H-24);const d=pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
const ns='http://www.w3.org/2000/svg',svg=document.createElementNS(ns,'svg');svg.setAttribute('viewBox',`0 0 ${W} ${H}`);svg.setAttribute('class','g');svg.setAttribute('preserveAspectRatio','none');
const area=document.createElementNS(ns,'path');area.setAttribute('d',d+` L ${X(t1)} ${H} L ${X(t0)} ${H} Z`);area.setAttribute('fill','rgba(48,209,88,.12)');svg.append(area);
const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#30d158');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');svg.append(ln);
box.append(svg,h('div',{class:'sub',text:`${cmp(mn)} – ${cmp(mx)}  ·  ${new Date(t0*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'})} to now`}))}
function channelCard(c){const card=h('div',{class:'card'+(c.err&&c.subs<0?' err':'')});
const link=c.tw?'https://www.twitch.tv/'+c.tw:isLive(c)?'https://youtu.be/'+c.vid.id:c.id?'https://www.youtube.com/channel/'+c.id:'#';
card.append(h('a',{class:'top',href:link,target:'_blank'},av(c,true),h('div',{style:'min-width:0'},h('div',{class:'nm'},name(c),c.tw?h('span',{class:'twb',text:'Twitch'}):null),h('div',{class:'sub',text:[c.tw?'twitch.tv/'+c.tw:c.handle,c.country,c.joined?'since '+new Date(c.joined*1000).getFullYear():''].filter(Boolean).join(' · ')}))));
if(c.err&&c.subs<0){card.append(h('p',{text:c.err}));return card}
const e=est(c);card.append(h('div',{class:'big'},h('span',{'data-est':c.i,text:fmt(e)}),(D.est&&c.step>1&&c.rate>0)?h('span',{class:'est',text:'est.'}):null));
card.append(h('div',{class:'sub',text:c.tw?(isLive(c)?'followers · live now':'followers · offline'):`${cmp(c.views)} views · ${fmt(c.videos)} videos · ${c.videos>0?cmp(Math.round(c.views/c.videos)):'-'} avg`}));
if(c.statsOk)card.append(h('div',{class:'chips'},[['Today',c.today],['Last 24 h',c.d1],['7 days',c.d7],['30 days',c.d30]].map(([k,v])=>h('div',{class:'chip'},k+' ',h('b',{class:cls(v),text:sg(v)}))),c.rate?h('div',{class:'chip'},'≈ ',h('b',{text:sg(Math.round(c.rate))}),'/day'):null));
else card.append(h('div',{class:'chips'},h('div',{class:'chip',text:'Growth: collecting data'})));
const f=Math.max(0,Math.min(1,(e-c.prev)/(c.next-c.prev||1)));
card.append(h('div',{style:'margin-top:6px;display:flex;justify-content:space-between'},h('span',{class:'sub',text:'Next milestone '}),h('b',{text:fmt(c.next)})),h('div',{class:'bar'},h('i',{style:`width:${(f*100).toFixed(1)}%`})),(c.next-e>0&&c.next-e<=Math.max(10,(c.next-c.prev)/10))?h('div',{style:'color:var(--gold);font-weight:700',text:`Almost there: ${fmt(c.next-e)} to go! · ${eta(c)}`}):h('div',{class:'sub',text:`${fmt(c.next-e)} to go · ${eta(c)}`}));
const tabs=h('div',{class:'tabs'}),gbox=h('div');let cur=graphs[c.i]||7;
for(const dd of [7,30]){const b=h('button',{text:dd+' days',class:dd==cur?'on':'',onclick:()=>{graphs[c.i]=dd;[...tabs.children].forEach(x=>x.className='');b.className='on';graph(c,dd,gbox)}});tabs.append(b)}
tabs.append(h('a',{class:'small',href:'/api/csv?i='+c.i,text:'Download CSV'}));card.append(tabs,gbox);graph(c,cur,gbox);
if(c.vt){const t=c.vt,ag=t.ageMin<60?t.ageMin+' min':(t.ageMin/60|0)+'h '+(t.ageMin%60)+'m';const box=h('div',{style:'margin-top:14px;padding:12px;border-radius:12px;background:#1f2a1f'},h('div',{class:'sub',style:'color:var(--gold)',text:'NEW VIDEO TRACKER'}),h('div',{style:'font-size:26px;font-weight:800',text:fmt(t.views)+' views'}),h('div',{class:'sub',text:`in ${ag}`+(t.rate>=0?` · ${cmp(t.rate)}/hour now`:'')}),t.cmp?h('div',{style:'margin-top:4px',class:t.cmp.includes('+')?'pos':t.cmp.includes('-')?'neg':'',text:t.cmp}):null);
if(t.pts&&t.pts.length>1){const W=600,H=70,p=t.pts,t0=p[0][0],t1=p[p.length-1][0],mn=p[0][1],mx=Math.max(p[p.length-1][1],mn+1);const d=p.map((q,k)=>(k?'L':'M')+((q[0]-t0)/(t1-t0||1)*(W-4)+2).toFixed(1)+' '+(H-4-(q[1]-mn)/(mx-mn)*(H-10)).toFixed(1)).join(' ');const ns='http://www.w3.org/2000/svg',sv=document.createElementNS(ns,'svg');sv.setAttribute('viewBox',`0 0 ${W} ${H}`);sv.setAttribute('preserveAspectRatio','none');sv.setAttribute('class','g');sv.style.height='70px';const ln=document.createElementNS(ns,'path');ln.setAttribute('d',d);ln.setAttribute('fill','none');ln.setAttribute('stroke','#ffc53d');ln.setAttribute('stroke-width','2.5');ln.setAttribute('vector-effect','non-scaling-stroke');sv.append(ln);box.append(sv)}
card.append(box)}
if(c.vid){const v=c.vid;card.append(h('a',{class:'vid',href:c.tw?'https://www.twitch.tv/'+c.tw:'https://youtu.be/'+v.id,target:'_blank'},h('img',{src:v.thumb||`https://i.ytimg.com/vi/${v.id}/mqdefault.jpg`,alt:''}),h('div',{style:'min-width:0'},h('div',{class:'t'},v.live?h('span',{class:'live',text:'LIVE'}):null,v.title),h('div',{class:'sub',text:v.live?`${fmt(v.viewers)} watching now`:`${ago(v.pub)} · ${dur(v.dur)}`}),c.tw?h('div',{class:'sub',text:v.game||''}):h('div',{class:'sub',text:`${cmp(v.views)} views · ${v.likes>=0?cmp(v.likes)+' likes':'likes hidden'} · ${v.comments>=0?cmp(v.comments)+' comments':'comments off'}`}))))}
if(c.pb){const b=c.pb,dd=t=>t?new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'}):'';const it=[];
if(b.day)it.push(h('span',{},'Best day ',h('b',{text:sg(b.day)}),' '+dd(b.dayT)));if(b.week)it.push(h('span',{},'Best week ',h('b',{text:sg(b.week)}),' '+dd(b.weekT)));if(b.vid)it.push(h('span',{title:b.vidTitle||''},'Best video, 1st day ',h('b',{text:cmp(b.vid)+' views'})));
if(it.length)card.append(h('div',{class:'pb'},h('span',{text:'Records:'}),it))}
if(c.vid&&(c.cm||c.cmOff)){const box=h('div',{class:'cm'},h('h3',{text:'Latest comments'}));
if(c.cmOff)box.append(h('div',{class:'sub',text:'Comments are off on this video'}));
else c.cm.forEach(m=>box.append(h('div',{class:'c'},h('b',{text:m.a}),h('span',{class:'sub',text:'  '+ago(m.ts)+(m.l>0?' · '+cmp(m.l)+' likes':'')}),h('p',{text:m.t}))));card.append(box)}
return card}
const COLS=['#3987e5','#d95926','#199e70','#c98500','#d55181','#008300','#9085e9','#e66767'];
const colFor=i=>i<COLS.length?COLS[i]:'#8d8d95';
let CMP={sel:null,days:7,mode:'gain'},HC={};
async function hist(i,days){const k=i+':'+days;if(!HC[k]||Date.now()-HC[k].at>120000){const r=await fetch('/api/history?i='+i+'&days='+days);HC[k]={at:Date.now(),p:await r.json()}}return HC[k].p}
function compareCard(){const C=D.channels.filter(c=>c.subs>=0);
if(!CMP.sel){const o=[...C].filter(c=>c.i!=0).sort((a,b)=>(b.d7||0)-(a.d7||0));CMP.sel=[0,...o.slice(0,2).map(c=>c.i)].filter(i=>C.some(c=>c.i==i))}
const card=h('div',{class:'card wide'}),box=h('div',{class:'cmp'}),ctl=h('div',{class:'ctl'});
card.append(h('h2',{text:'Compare'}),h('div',{class:'sub',style:'margin:-8px 0 12px',text:'Subscribers gained over the period, so big and small channels fit on one chart. Pick up to 4.'}),ctl,box,h('div',{style:'display:flex;margin-top:8px'},h('a',{class:'small',href:'/api/csv',text:'Download all history (CSV)'})));
const draw=()=>{ctl.textContent='';
C.forEach(c=>{const on=CMP.sel.includes(c.i);ctl.append(h('button',{class:'tg'+(on?' on':''),onclick:()=>{if(on)CMP.sel=CMP.sel.filter(x=>x!=c.i);else if(CMP.sel.length<4)CMP.sel.push(c.i);draw()}},h('i',{style:'background:'+(on?colFor(c.i):'transparent')+';border:1px solid '+colFor(c.i)}),name(c)))});
const seg=(opts,key)=>h('div',{class:'seg'},opts.map(([v,t])=>h('button',{class:CMP[key]==v?'on':'',text:t,onclick:()=>{CMP[key]=v;draw()}})));
ctl.append(h('span',{style:'flex:1'}),seg([[7,'7 days'],[30,'30 days']],'days'),seg([['gain','Gained'],['pct','% growth']],'mode'));
plot(box,C)};draw();return card}
async function plot(box,C){const sel=CMP.sel.slice(),days=CMP.days,mode=CMP.mode;
const ser=[];for(const i of sel){const c=C.find(x=>x.i==i);if(!c)continue;const p=(await hist(i,days)).slice();p.push([Date.now()/1000|0,c.subs]);if(p.length<2)continue;const b=p[0][1];
ser.push({c,pts:p.map(([t,v])=>[t,mode=='pct'?(b>0?(v-b)/b*100:0):v-b])})}
const BW=box.clientWidth||900;box.textContent='';if(!ser.length){box.append(h('div',{class:'sub',style:'padding:40px 0;text-align:center',text:sel.length?'Not enough history yet':'Pick a channel above'}));return}
const W=Math.max(280,BW),narrow=W<600,H=260,L=narrow?46:56,R=narrow?10:150,T=12,B=28,ns='http://www.w3.org/2000/svg',mk=(t,a)=>{const e=document.createElementNS(ns,t);for(const k in a)e.setAttribute(k,a[k]);return e};
const now=Date.now()/1000,t0=now-days*86400;let mn=0,mx=0;ser.forEach(s=>s.pts.forEach(([,v])=>{mn=Math.min(mn,v);mx=Math.max(mx,v)}));if(mx==mn)mx=mn+1;
const st=(()=>{const r=(mx-mn)/4,m=Math.pow(10,Math.floor(Math.log10(r))),f=r/m;return (f<=1?1:f<=2?2:f<=2.5?2.5:f<=5?5:10)*m})();mn=Math.floor(mn/st)*st;mx=Math.ceil(mx/st)*st;const nT=Math.round((mx-mn)/st);
const X=t=>L+(Math.max(t,t0)-t0)/(now-t0)*(W-L-R),Y=v=>T+(mx-v)/(mx-mn)*(H-T-B);
const fv=v=>mode=='pct'?(v>=0?'+':'')+v.toFixed(Math.abs(mx)<1?2:1)+'%':(v>=0?'+':'')+cmp(Math.round(v));
const svg=mk('svg',{viewBox:`0 0 ${W} ${H}`});
for(let k=0;k<=nT;k++){const v=mn+st*k,y=Y(v);svg.append(mk('line',{x1:L,x2:W-R,y1:y,y2:y,stroke:'#2a2a2e','stroke-width':1}));const tx=mk('text',{x:L-8,y:y+4,'text-anchor':'end','font-size':11,fill:'#8d8d95'});tx.textContent=fv(v);svg.append(tx)}
[0,.5,1].forEach(f=>{const t=t0+(now-t0)*f,tx=mk('text',{x:L+(W-L-R)*f,y:H-8,'text-anchor':f==0?'start':f==1?'end':'middle','font-size':11,fill:'#8d8d95'});tx.textContent=f==1?'now':new Date(t*1000).toLocaleDateString('en-GB',{day:'numeric',month:'short'});svg.append(tx)});
const ends=[];ser.forEach(s=>{const d=s.pts.map((p,k)=>(k?'L':'M')+X(p[0]).toFixed(1)+' '+Y(p[1]).toFixed(1)).join(' ');
svg.append(mk('path',{d,fill:'none',stroke:colFor(s.c.i),'stroke-width':2,'stroke-linejoin':'round','vector-effect':'non-scaling-stroke','stroke-dasharray':s.c.i<COLS.length?'':'5 4'}));
const last=s.pts[s.pts.length-1];ends.push({s,y:Y(last[1]),v:last[1]})});
ends.sort((a,b)=>a.y-b.y);for(let k=1;k<ends.length;k++)if(ends[k].y-ends[k-1].y<16)ends[k].y=ends[k-1].y+16;
ends.forEach(e=>{svg.append(mk('circle',{cx:W-R,cy:Y(e.s.pts[e.s.pts.length-1][1]),r:4,fill:colFor(e.s.c.i),stroke:'#18181b','stroke-width':2}));if(narrow)return;const g=mk('text',{x:W-R+8,y:e.y+4,'font-size':12,fill:'#f2f2f3'});g.textContent=name(e.s.c).slice(0,14)+' '+fv(e.v);svg.append(g)});
const cross=mk('line',{y1:T,y2:H-B,stroke:'#8d8d95','stroke-width':1,visibility:'hidden'});svg.append(cross);
const tip=h('div',{class:'tip'});box.append(svg,tip);
svg.onmousemove=ev=>{const r=svg.getBoundingClientRect(),sx=(ev.clientX-r.left)/r.width*W;if(sx<L||sx>W-R){svg.onmouseleave();return}
const t=t0+(sx-L)/(W-L-R)*(now-t0);cross.setAttribute('x1',sx);cross.setAttribute('x2',sx);cross.setAttribute('visibility','visible');
tip.textContent='';tip.append(h('div',{class:'sub',text:new Date(t*1000).toLocaleString('en-GB',{day:'numeric',month:'short',hour:'2-digit',minute:'2-digit'})}));
ser.forEach(s=>{let v=s.pts[0][1];for(const p of s.pts){if(p[0]>t)break;v=p[1]}tip.append(h('div',{},h('i',{style:'background:'+colFor(s.c.i)}),name(s.c)+' ',h('b',{text:fv(v)})))});
tip.style.display='block';const px=ev.clientX-r.left;tip.style.left=Math.min(px+12,r.width-tip.offsetWidth-4)+'px';tip.style.top='10px'};
svg.onmouseleave=()=>{cross.setAttribute('visibility','hidden');tip.style.display='none'}}
function render(){const app=$('#app');app.textContent='';const C=D.channels;
$('#meta').textContent=(D.err?D.err:(D.updatedAgo>=0?'Updated '+(D.updatedAgo<60?'just now':(D.updatedAgo/60|0)+' min ago'):''))+(D.ver?'  ·  v'+D.ver:'');
const row=h('aside',{class:'side'});
// summary
const ok=C.filter(c=>c.statsOk).sort((a,b)=>b.d1-a.d1);const nv=C.filter(c=>c.vid&&!c.tw&&Date.now()/1000-c.vid.pub<86400);
row.append(h('div',{class:'card'},h('h2',{text:'Last 24 hours'}),h('div',{class:'sub',style:'margin:-8px 0 8px',text:'Rolling: gains since this time yesterday'}),h('div',{class:'list'},ok.length?ok.slice(0,5).map(c=>h('div',{},h('span',{text:name(c)}),h('b',{class:cls(c.d1),text:sg(c.d1)}))):h('div',{class:'sub',text:'Collecting data – check back in a few hours'})),h('div',{class:'sub',style:'margin-top:8px',text:nv.length?`${nv.length} new video${nv.length>1?'s':''} today`:'No new videos in the last day'})));
// weather
if(D.wx){const w=D.wx;row.append(h('div',{class:'card'},h('h2',{text:'Weather · '+w.place}),h('div',{class:'big',text:w.temp+'°'}),h('div',{text:w.text+' · feels '+w.feels+'°'}),h('div',{class:'sub',text:`High ${w.hi}° · Low ${w.lo}° · Wind ${w.wind} mph`}),w.rainHour>=0?h('div',{style:'margin-top:8px;color:var(--blue)',text:`Rain likely around ${String(w.rainHour).padStart(2,'0')}:00 (${w.rainPct}%)`}):null))}
// race
if(D.race){const A=C[D.race[0]],B=C[D.race[1]],ea=est(A),eb=est(B),fa=ea+eb?ea/(ea+eb):.5;const lead=ea>=eb?A:B,ch=lead===A?B:A,closing=(ch.rate||0)-(lead.rate||0),gap=Math.abs(ea-eb);
row.append(h('div',{class:'card race'},h('h2',{text:'Race'}),h('div',{class:'rs'},h('span',{},av(A),h('b',{style:'color:var(--red)',text:name(A)})),h('b',{text:fmt(ea)})),h('div',{class:'rs'},h('span',{},av(B),h('b',{style:'color:var(--blue)',text:name(B)})),h('b',{text:fmt(eb)})),h('div',{class:'tug'},h('div',{class:'a',style:`width:${fa*100}%`}),h('div',{class:'b',style:`width:${(1-fa)*100}%`})),h('div',{text:`Gap ${fmt(gap)}`}),h('div',{class:'sub',text:closing>0.01?`${name(ch)} is catching up by ${fmt(Math.round(closing))}/day – could pass in about ${Math.max(1,Math.round(gap/closing))} days`:(lead.rate||ch.rate)?`${name(lead)} is pulling away`:'Trend: need more data'})))}
// leaderboard
const lb=[...C].filter(c=>c.subs>=0).sort((a,b)=>b.subs-a.subs);
row.append(h('div',{class:'card'},h('h2',{text:'Leaderboard'}),h('table',{},h('tr',{class:'hd'},h('th',{colspan:'3'}),h('th',{class:'n',text:C.some(c=>c.tw)?'Total':'Subs'}),h('th',{class:'n',text:'Today',title:'Since midnight'})),lb.map((c,k)=>h('tr',{class:c.i==0?'me':''},h('td',{text:k+1+'.'}),h('td',{},av(c)),h('td',{class:'nm2',text:name(c)}),h('td',{class:'n',text:cmp(c.subs)}),h('td',{class:'n '+cls(c.today),text:c.statsOk?sg(c.today):''}))))));
{const mon=t=>new Date(t*1000).toLocaleDateString('en-GB',{month:'short'});const ms=[...C].filter(c=>c.subs>=0).sort((a,b)=>(b.mo||0)-(a.mo||0));
if(ms.length&&C.some(c=>c.statsOk))row.append(h('div',{class:'card'},h('h2',{text:'Monthly'}),h('table',{},h('tr',{class:'hd'},h('th',{}),h('th',{class:'n',text:mon(D.m1)}),h('th',{class:'n',text:mon(D.m0)+' so far'})),ms.map(c=>h('tr',{class:c.i==0?'me':''},h('td',{class:'nm2',text:name(c)}),h('td',{class:'n '+(c.lm!=null?cls(c.lm):''),text:c.lm!=null?sg(c.lm):'–'}),h('td',{class:'n '+cls(c.mo),text:c.statsOk?sg(c.mo):'–'}))))))}
const grid=h('section',{class:'grid'});C.forEach(c=>grid.append(channelCard(c)));if(C.filter(c=>c.subs>=0).length>1)grid.append(compareCard());app.append(h('div',{class:'layout'},row,grid))}
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
  doc["ver"] = FW_VERSION;
  doc["m0"] = (long)monthStart(0); doc["m1"] = (long)monthStart(1);
  if (raceSet()) { JsonArray r = doc["race"].to<JsonArray>(); r.add(cfgRaceA); r.add(cfgRaceB); }
  else doc["race"] = nullptr;
  if (wx.ok) {
    JsonObject w = doc["wx"].to<JsonObject>();
    w["place"] = cfgWxName; w["temp"] = lroundf(wx.temp); w["feels"] = lroundf(wx.feels); w["text"] = wxText(wx.code);
    w["hi"] = wx.hi; w["lo"] = wx.lo; w["rainHour"] = wx.rainHour; w["rainPct"] = wx.rainPct; w["wind"] = lroundf(wx.wind);
  }
  JsonArray arr = doc["channels"].to<JsonArray>();
  for (int i = 0; i < numCh; i++) {
    Channel &c = ch[i];
    JsonObject o = arr.add<JsonObject>();
    o["i"] = i; o["handle"] = c.handle; o["id"] = c.id; o["title"] = c.title;
    o["subs"] = c.subs; o["step"] = (c.subs >= 0 && !c.tw) ? stepFor(c.subs) : 1;
    o["rate"] = c.ratePerDay; o["stepAt"] = (long)c.stepChangedAt;
    o["views"] = c.views; o["videos"] = c.videos; o["joined"] = (long)c.joined;
    o["country"] = c.country; o["avatar"] = c.avatarUrl; o["err"] = c.err;
    o["statsOk"] = c.statsOk; o["today"] = c.gainToday; o["d1"] = c.gain24;
    o["d7"] = c.gain7; o["d30"] = c.gain30; o["histStart"] = (long)c.histStart;
    long e = c.subs >= 0 ? estimateFor(i) : 0;
    long nx = nextMilestone(max(0L, e));
    o["next"] = nx; o["prev"] = prevMilestone(nx);
    if (i == 0 && vt.active && vt.n) {
      JsonObject t = o["vt"].to<JsonObject>();
      t["views"] = vtViews(); t["ageMin"] = vtAgeMin(); t["rate"] = vtRate(); t["typical"] = vt.typical;
      t["at1h"] = vt.at1h; t["base1h"] = vt.base1h; t["at24h"] = vt.at24h; t["base24h"] = vt.base24h; t["cmp"] = vtCompare();
      JsonArray pts = t["pts"].to<JsonArray>();
      for (int k = 0; k < vt.n; k++) { JsonArray pp = pts.add<JsonArray>(); pp.add(vt.t[k]); pp.add(vt.v[k]); }
    }
    if (c.tw) {
      o["tw"] = c.twLogin;
      if (c.live) { JsonObject v = o["vid"].to<JsonObject>(); v["id"] = ""; v["title"] = c.vidTitle; v["pub"] = (long)c.vidPublished;
                    v["live"] = true; v["viewers"] = c.liveViewers; v["thumb"] = c.twThumb; v["game"] = c.twGame; }
    }
    o["mo"] = c.gainMonth;
    if (c.lastMonthOk) o["lm"] = c.gainLastMonth;
    if (!c.pbLoaded) pbLoad(i);
    if (c.pbDay || c.pbWeek || c.pbVid) {
      JsonObject b = o["pb"].to<JsonObject>();
      b["day"] = c.pbDay; b["dayT"] = c.pbDayT; b["week"] = c.pbWeek; b["weekT"] = c.pbWeekT;
      if (c.pbVid) { b["vid"] = c.pbVid; b["vidT"] = c.pbVidT; b["vidTitle"] = c.pbVidTitle; }
    }
    if (c.cmOff) o["cmOff"] = true;
    if (c.cmN) {
      JsonArray cm = o["cm"].to<JsonArray>();
      for (int k = 0; k < c.cmN; k++) {
        JsonObject x = cm.add<JsonObject>();
        x["a"] = c.cmAuthor[k]; x["t"] = c.cmText[k]; x["ts"] = (long)c.cmT[k]; x["l"] = c.cmLikes[k];
      }
    }
    if (!c.tw && c.vidId.length() && c.vidTitle.length()) {
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

// History as CSV: /api/csv?i=2 for one channel, /api/csv for all of them
void handleApiCsv() {
  int only = server.hasArg("i") ? server.arg("i").toInt() : -1;
  String fname = only >= 0 && only < numCh ? nameOf(only) : String("all-channels");
  String safe; for (char ch2 : fname) safe += isalnum((unsigned char)ch2) ? ch2 : '-';
  server.sendHeader("Content-Disposition", "attachment; filename=\"subcounter-" + safe + ".csv\"");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/csv", "");
  server.sendContent("channel,time_utc,subscribers\n");
  for (int i = 0; i < numCh; i++) {
    if (only >= 0 && i != only) continue;
    if (!fsOk || !ch[i].id.length()) continue;
    File f = LittleFS.open(histPath(i), "r");
    if (!f) continue;
    String nm = nameOf(i); nm.replace("\"", "'");
    String chunk; Sample sm;
    while (f.read((uint8_t *)&sm, sizeof(sm)) == sizeof(sm)) {
      char ts[24]; time_t t = sm.t; struct tm g; gmtime_r(&t, &g);
      strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &g);
      chunk += "\"" + nm + "\"," + ts + "," + String(sm.s) + "\n";
      if (chunk.length() > 1200) { server.sendContent(chunk); chunk = ""; }
    }
    f.close();
    if (chunk.length()) server.sendContent(chunk);
  }
  server.sendContent("");
}

const char SCAN_JS[] PROGMEM = R"JS(<style>
.net{display:flex;align-items:center;gap:10px;width:100%;margin:0;padding:11px 12px;border:1px solid #333;border-radius:10px;background:#111;color:#fff;font-size:15px;font-weight:400;text-align:left;cursor:pointer;margin-top:6px}
.net:hover{border-color:#e62117}.net b{flex:1;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.net small{margin:0;color:#888}.bars{display:inline-flex;gap:2px;align-items:flex-end;height:14px}.bars i{width:4px;background:#555;border-radius:1px}.bars i.on{background:#30d158}
</style><script>
async function scan(){
  const box=document.getElementById('nets'),btn=document.getElementById('scanBtn');
  btn.disabled=true;btn.textContent='Scanning… (a few seconds)';box.textContent='';
  try{
    const r=await fetch('/api/scan');const list=await r.json();
    if(!list.length){box.innerHTML='<small>No networks found. Is the hotspot on? (iPhone: keep the Personal Hotspot screen open and turn on Maximise Compatibility)</small>'}
    list.forEach(n=>{
      const b=document.createElement('button');b.type='button';b.className='net';
      const bars=document.createElement('span');bars.className='bars';
      const lvl=n.rssi>-55?4:n.rssi>-67?3:n.rssi>-78?2:1;
      for(let k=1;k<=4;k++){const i=document.createElement('i');i.style.height=(k*3+2)+'px';if(k<=lvl)i.className='on';bars.append(i)}
      const nm=document.createElement('b');nm.textContent=n.ssid;
      const info=document.createElement('small');
      info.textContent=(n.saved?'saved · ':'')+(n.ent?'needs username':(n.open?'open':'🔒'));
      b.append(bars,nm,info);
      b.onclick=()=>{document.getElementById('s').value=n.ssid;document.querySelectorAll('.net').forEach(x=>x.style.borderColor='');b.style.borderColor='#30d158';
        const pw=document.getElementById('pw');pw.value='';pw.focus();pw.scrollIntoView({block:'center',behavior:'smooth'})};
      box.append(b)});
  }catch(e){box.innerHTML='<small>Scan failed – try again.</small>'}
  btn.disabled=false;btn.textContent='\u{1F4F6} Scan again';
}
</script>)JS";

// Nearby networks for the settings page: [{ssid, rssi, open, ent, saved}, ...] strongest first
void handleApiScan() {
  if (!authed()) return;
  int n = WiFi.scanNetworks();
  struct Found { String ssid; int rssi; wifi_auth_mode_t auth; } f[25]; int cnt = 0;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (!s.length()) continue;                                  // hidden networks
    int dup = -1;
    for (int k = 0; k < cnt; k++) if (f[k].ssid == s) dup = k;
    if (dup >= 0) { if (WiFi.RSSI(i) > f[dup].rssi) f[dup].rssi = WiFi.RSSI(i); continue; }
    if (cnt < 25) { f[cnt].ssid = s; f[cnt].rssi = WiFi.RSSI(i); f[cnt].auth = WiFi.encryptionType(i); cnt++; }
  }
  WiFi.scanDelete();
  for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++)      // strongest first
    if (f[b].rssi > f[a].rssi) { Found t = f[a]; f[a] = f[b]; f[b] = t; }
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int k = 0; k < cnt; k++) {
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = f[k].ssid;
    o["rssi"] = f[k].rssi;
    o["open"] = (f[k].auth == WIFI_AUTH_OPEN);
    o["ent"] = (f[k].auth == WIFI_AUTH_WPA2_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA3_ENTERPRISE || f[k].auth == WIFI_AUTH_WPA2_WPA3_ENTERPRISE);
    bool saved = false;
    for (int j = 0; j < numNets; j++) if (nets[j].ssid == f[k].ssid) saved = true;
    o["saved"] = saved;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
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

// Section heading with an anchor for the jump links at the top
String sec(const char *id, const char *title) {
  return String("<h2 id='") + id + "'>" + title + "</h2>";
}

void handleSettings() {
  if (!authed()) return;
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter settings</h1><p>Saved on the board only. v" FW_VERSION;
  if (!portalMode) h += " &middot; <a href='/'>&larr; Dashboard</a>";
  h += "</p><nav class='jump'><a href='#channels'>Channels</a><a href='#display'>Display</a><a href='#alerts'>Alerts</a>"
       "<a href='#weather'>Weather</a><a href='#spotify'>Spotify</a><a href='#motion'>Motion</a><a href='#wifi'>Wi-Fi</a>"
       "<a href='#security'>Security</a>" + String(portalMode ? "" : "<a href='#firmware'>Firmware</a>") + "</nav>";
  if (wifiFailReason.length()) {
    h += "<div style='background:#3a1210;border:1px solid #e62117;border-radius:10px;padding:12px;margin-bottom:8px;font-size:14px'>"
         "<b>Couldn't connect to Wi-Fi:</b><br>" + htmlEscape(wifiFailReason) + "</div>";
  }
  // Everything is inside ONE form so a single Save sends it all. Extra actions
  // (Connect now, Spotify, calibrate) are buttons with their own formaction.
  h += "<form method='POST' action='/save'>";

  // ── Channels ──
  h += sec("channels", "Channels");
  h += "<label>Channels to follow (up to 10, one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@yourchannel&#10;@mkbhd&#10;twitch.tv/somestreamer' required>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>YouTube: @handles, UC… channel IDs or channel links. Twitch: <b>twitch.tv/name</b>. "
       "Put your own channel first – it's highlighted and shown on the night clock.</small>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' autocapitalize='off' autocomplete='off' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'><small>Only needed for YouTube channels.</small>";
  h += "<details" + String(cfgTwId.length() || cfgChannels.indexOf("twitch") >= 0 ? " open" : "") + "><summary>Twitch app (only for Twitch channels)</summary>";
  h += "<small>Create a free app at <a href='https://dev.twitch.tv/console/apps' target='_blank'>dev.twitch.tv/console</a> "
       "(Category: Other, Client type: Confidential, OAuth Redirect URL: <code>https://localhost</code> – any https address works, it isn't used), then copy its Client ID and a new Secret here.</small>";
  h += "<label>Twitch Client ID</label><input name='twid' autocapitalize='off' autocomplete='off' value='" + htmlEscape(cfgTwId) + "'>";
  h += "<label>Twitch Client Secret</label><input name='twsec' type='password' autocapitalize='off' autocomplete='new-password' placeholder='";
  h += cfgTwSecret.length() ? "(saved — leave blank to keep)" : "";
  h += "'>";
  if (twError.length() && !portalMode) h += "<small style='color:#e62117'>" + htmlEscape(twError) + "</small>";
  h += "</details>";

  // ── Display ──
  h += sec("display", "Display");
  h += checkbox("auto", cfgAuto, "Switch channels automatically every 10 seconds");
  h += checkbox("est", cfgEst, "Estimated live counts between YouTube's rounded steps (“est.”)");
  h += "<label>Show the big clock after this long without use</label><select name='idleclk'>";
  { const int opts[] = { 0, 1, 2, 3, 5, 10 };
    for (int o : opts) h += "<option value='" + String(o) + "'" + (cfgIdleClock == o ? " selected" : "") + ">" +
                            (o == 0 ? String("Never") : String(o) + (o == 1 ? " minute" : " minutes")) + "</option>"; }
  h += "</select><small>Touch, press BOOT or pick the board up to go back.</small>";
  h += "<label>Night clock (dims and shows the time)</label><div class='row'>" +
       hourSelect("nightStart", cfgNightStart) + "<span>to</span>" + hourSelect("nightEnd", cfgNightEnd) + "</div>";
  if (numCh >= 2) {
    h += "<label>Subscriber race</label><div class='row'>" + channelSelect("raceA", cfgRaceA) +
         "<span>vs</span>" + channelSelect("raceB", cfgRaceB) + "</div>";
    h += "<small>Swipe down twice from the main count to see it.</small>";
  }

  // ── Alerts ──
  h += sec("alerts", "Alerts");
  h += checkbox("celebrate", cfgCelebrate, "Confetti for milestones (bigger milestones, bigger party)");
  h += checkbox("livealert", cfgLiveAlert, "Alert when a channel goes live (and switch to it)");
  h += checkbox("summary", cfgSummary, "Daily summary at 9 am (a monthly recap on the 1st)");
  h += "<small>New-subscriber, overtake and record alerts are always on. Alerts are skipped at night, "
       "when the board is face-down, and while Spotify is playing.</small>";

  // ── Weather ──
  h += sec("weather", "Weather");
  h += "<label>Town or city</label><input name='wxtown' value='" + htmlEscape(cfgWxName) + "' placeholder='e.g. Glasgow'>";
  h += "<small>Long-press the board's screen and pick Weather. Forecasts from Open-Meteo.</small>";

  // ── Spotify ──
  h += sec("spotify", "Spotify");
  if (cfgSpRefresh.length()) h += "<p class='ok'>Connected &#10003;</p>";
  h += "<details" + String(cfgSpId.length() ? "" : " open") + "><summary>Spotify app details (one-time setup, needs Premium)</summary>";
  h += "<small>Put the relay page on GitHub Pages (or any https address you own), then at "
       "<b>developer.spotify.com/dashboard</b> &rarr; Create app &rarr; add that https address as the Redirect URI &rarr; tick <b>Web API</b> &rarr; Save. "
       "Copy the Client ID, Client secret and the same Redirect URI here, and save.</small>";
  h += "<label>Redirect URI (exactly as entered at Spotify)</label><input name='spredir' value='" + htmlEscape(cfgSpRedirect) +
       "' autocapitalize='off' placeholder='https://yourname.github.io/spotify-callback/'>";
  h += "<label>Client ID</label><input name='spid' value='" + htmlEscape(cfgSpId) + "' autocapitalize='off' autocomplete='off'>";
  h += "<label>Client secret</label><input name='spsecret' type='password' autocapitalize='off' autocomplete='new-password' placeholder='" +
       String(cfgSpSecret.length() ? "(saved — leave blank to keep)" : "") + "'>";
  h += "</details>";
  if (!portalMode && cfgSpId.length() && cfgSpSecret.length() && cfgSpRedirect.length()) {
    if (!cfgSpRefresh.length()) {
      h += "<a class='btnlink' href='" + htmlEscape(spAuthUrl()) + "'>Connect Spotify</a>"
           "<small>Press Agree on Spotify's page and you'll be brought straight back here.</small>"
           "<details><summary>The relay page showed a code instead?</summary>"
           "<label>Paste the code or that page's full address</label><input name='url' placeholder='code or full address' autocapitalize='off'>"
           "<button type='submit' class='b2' formaction='/spotify/code' formnovalidate>Use this code</button></details>";
    } else {
      h += "<button type='submit' class='b2' formaction='/spotify/disconnect' formnovalidate>Disconnect Spotify</button>";
    }
  } else if (!portalMode) {
    h += "<small>Save the app details first; a <b>Connect Spotify</b> button then appears here.</small>";
  }

  // ── Motion ──
  h += sec("motion", "Motion");
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
  if (!portalMode && imuOk) {
    h += "<label>Calibration</label><small>Put the board in the position you normally use it (on its stand or flat), then press:</small>"
         "<button type='submit' class='b2' formaction='/calibrate' formnovalidate>Set this as the normal position</button>";
  }

  // ── Wi-Fi: saved networks + add/edit one ──
  int edit = server.hasArg("edit") ? server.arg("edit").toInt() : -1;
  if (edit >= numNets) edit = -1;
  Net blank; Net &e = edit >= 0 ? nets[edit] : blank;
  h += sec("wifi", "Wi-Fi networks");
  if (numNets) {
    h += "<small>The board joins your <b>Preferred</b> network when it's in range, otherwise the strongest saved one. "
         "<b>Connect now</b> switches straight away.</small><div class='st' style='margin:10px 0'>";
    for (int k = 0; k < numNets; k++) {
      h += "<div class='saved'><b>" + htmlEscape(nets[k].ssid) + "</b>";
      if (k == curNet && !portalMode) h += "<span style='color:#30d158'>connected</span>";
      else if (!portalMode) h += "<button type='submit' formaction='/wifi/connect?n=" + String(k) + "' formnovalidate class='mini'>Connect now</button>";
      if (nets[k].ip.length()) h += "<span>fixed IP</span>";
      h += "<label style='margin:0'><input type='radio' name='pref' value='" + String(k) + "' style='width:auto'" +
           String(cfgPreferred == k ? " checked" : "") + "> Preferred</label>";
      h += "<a href='/settings?edit=" + String(k) + "#wifi'>Edit</a>";
      h += "<label style='margin:0'><input type='checkbox' name='rm" + String(k) + "' value='1' style='width:auto'> Remove</label></div>";
    }
    h += "<label style='margin:4px 0 0'><input type='radio' name='pref' value='-1' style='width:auto'" +
         String(cfgPreferred < 0 ? " checked" : "") + "> No preference (strongest signal wins)</label>";
    h += "</div>";
  }
  h += "<label>" + String(edit >= 0 ? "Editing: " + htmlEscape(e.ssid) : (numNets ? String("Add another network (e.g. home)") : String("Wi-Fi network"))) + "</label>";
  h += "<button type='button' id='scanBtn' onclick='scan()' class='b2' style='margin-top:6px'>&#128246; Scan for networks</button>";
  h += "<div id='nets' style='margin:8px 0'></div>";
  h += "<input id='s' name='ssid' value='" + htmlEscape(e.ssid) + "' placeholder='Network name (tap one above, or type it)'" + String(numNets ? "" : " required") + ">";
  if (numNets && edit < 0) h += "<small>Leave blank if you're not adding a network.</small>";
  h += "<label>Wi-Fi password</label>";
  h += "<input id='pw' name='pass' type='password' autocomplete='off' autocapitalize='off' placeholder='";
  h += e.pass.length() ? "(saved — leave blank to keep)" : "Password";
  h += "'><small><label style='display:inline;margin:0'><input type='checkbox' style='width:auto' "
       "onclick=\"document.getElementById('pw').type=this.checked?'text':'password'\"> Show password</label></small>";
  h += "<label>Username (only for work Wi-Fi that asks for one)</label>";
  h += "<input name='user' value='" + htmlEscape(e.user) + "' autocapitalize='off' placeholder='Leave blank for normal Wi-Fi'>";
  h += "<details" + String(e.ip.length() || e.compat ? " open" : "") + "><summary>Advanced settings for this network</summary>";
  h += checkbox("compat", e.compat, "Compatibility mode (Wi-Fi 4 instead of Wi-Fi 6)");
  h += "<label>Fixed IP address (blank = automatic)</label>";
  h += "<input name='ip' value='" + htmlEscape(e.ip) + "' placeholder='e.g. 192.168.1.250' inputmode='decimal'>";
  h += "<label>Gateway (router) address</label>";
  h += "<input name='gw' value='" + htmlEscape(e.gw) + "' placeholder='e.g. 192.168.1.1' inputmode='decimal'>";
  h += "<label>Subnet mask</label>";
  h += "<input name='mask' value='" + htmlEscape(e.mask) + "' placeholder='255.255.255.0' inputmode='decimal'>";
  h += "<label>DNS server</label>";
  h += "<input name='dns' value='" + htmlEscape(e.dns) + "' placeholder='e.g. 8.8.8.8' inputmode='decimal'>";
  h += "<small>If this one doesn't answer, the board also tries 8.8.8.8.</small>";
  h += "</details>";

  // ── Security ──
  h += sec("security", "Security");
  if (cfgPin.length()) h += "<p class='ok'>PIN is on &#10003;</p>";
  else h += "<small>No PIN set: anyone on your Wi-Fi can open this page and change things.</small>";
  h += "<label>" + String(cfgPin.length() ? "New PIN" : "Choose a PIN (4–12 characters)") + "</label>"
       "<input name='pin' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12' placeholder='" +
       String(cfgPin.length() ? "Leave blank to keep the current PIN" : "e.g. 4 digits") + "'>";
  h += "<label>Type it again</label><input name='pin2' type='password' inputmode='numeric' autocomplete='new-password' maxlength='12'>";
  if (cfgPin.length()) h += checkbox("nopin", false, "Remove the PIN");
  h += "<small>After you save, your browser asks for it whenever you open Settings or Update: "
       "user name <b>admin</b>, password = your PIN. The dashboard stays open to view. "
       "Forgotten it? Hold BOOT for 3 seconds: setup mode doesn't ask for it, so you can change it there.</small>";

  // ── Save ──
  h += "<div class='savebar'><button type='submit'>Save &amp; restart</button></div>";
  h += "</form>";

  // ── Firmware (links only) ──
  if (!portalMode) {
    h += sec("firmware", "Firmware");
    h += "<p style='margin:0'>Running <b>v" FW_VERSION "</b> &middot; <a href='/update'>&#11014; Update wirelessly</a></p>";
  }
  h += "<small style='margin-top:16px'>Board Wi-Fi MAC address: " + boardMac() + "</small></div>";
  h += FPSTR(SCAN_JS);
  if (portalMode) h += "<script>scan()</script>";
  h += "</body></html>";
  server.send(200, "text/html", h);
}

void sendMessage(int code, const String &title, const String &body) {
  server.send(code, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>" + title + "</h1><p>" + body +
              "</p><a href='/settings'>Go back</a></div></body></html>");
}

void handleCalibrate() {
  if (!authed()) return;
  calibrateMotion();
  sendMessage(200, "Calibrated &#10003;", "This is now the board's normal position.");
}

void handleSave() {
  if (!authed()) return;
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (pin.length() && server.arg("nopin") != "1") {
      if (pin != pin2) { sendMessage(400, "PINs don't match", "Type the same PIN in both boxes. Nothing was saved."); return; }
      if (pin.length() < 4) { sendMessage(400, "PIN too short", "Use at least 4 characters. Nothing was saved."); return; }
    } }
  String ssid = server.arg("ssid");
  String pass = server.arg("pass");
  String user = server.arg("user");     user.trim();
  String chs  = server.arg("channels"); chs.trim();
  String key  = server.arg("apikey");   key.trim();
  while (pass.length() && (pass.endsWith("\n") || pass.endsWith("\r") || pass.endsWith(" ") || pass.endsWith("\t")))
    pass.remove(pass.length() - 1);
  while (pass.length() && (pass[0] == ' ' || pass[0] == '\n' || pass[0] == '\r' || pass[0] == '\t'))
    pass.remove(0, 1);
  bool ytListed = false;
  { String l = chs + "\n"; l.replace(",", "\n"); int st = 0;
    while (true) { int nl = l.indexOf('\n', st); if (nl < 0) break; String x = l.substring(st, nl); x.trim(); x.toLowerCase(); st = nl + 1;
                   if (x.length() && x.indexOf("twitch.tv/") < 0 && !x.startsWith("twitch:")) ytListed = true; } }
  if (chs.length() == 0 || (ytListed && key.length() == 0 && cfgApiKey.length() == 0)) {
    sendMessage(400, "Missing details", "At least one channel is needed, and the YouTube API key for YouTube channels.");
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
  // Wi-Fi: remove ticked networks, then add / update the one in the form
  Net keep[MAX_NETS]; int nk = 0;
  int prefOld = server.hasArg("pref") ? server.arg("pref").toInt() : cfgPreferred;
  int prefNew = -1;
  for (int k = 0; k < numNets; k++) if (server.arg("rm" + String(k)) != "1") { if (k == prefOld) prefNew = nk; keep[nk++] = nets[k]; }
  if (ssid.length()) {
    int found = -1;
    for (int k = 0; k < nk; k++) if (keep[k].ssid == ssid) found = k;
    if (found < 0) {
      if (nk >= MAX_NETS) { sendMessage(400, "Too many networks", "You can save up to 5. Remove one first."); return; }
      found = nk++;
      keep[found] = Net();
      keep[found].ssid = ssid;
    }
    Net &n = keep[found];
    if (pass.length()) n.pass = pass;
    n.user = user;
    n.ip = ip; n.gw = ip.length() ? gw : ""; n.mask = ip.length() ? mask : ""; n.dns = dnsS;
    n.compat = server.arg("compat") == "1";
  }
  if (nk == 0) { sendMessage(400, "No Wi-Fi network", "Add at least one Wi-Fi network."); return; }
  for (int k = 0; k < nk; k++) nets[k] = keep[k];
  numNets = nk;
  cfgPreferred = prefNew;
  cfgChannels = chs;
  if (key.length()) cfgApiKey = key;
  cfgAuto = server.arg("auto") == "1";
  cfgEst = server.arg("est") == "1";
  cfgCelebrate = server.arg("celebrate") == "1";
  cfgLiveAlert = server.arg("livealert") == "1";
  { String v = server.arg("twid"); v.trim(); if (server.hasArg("twid")) cfgTwId = v;
    v = server.arg("twsec"); v.trim(); if (v.length()) { cfgTwSecret = v; } }
  { String pin = server.arg("pin"), pin2 = server.arg("pin2"); pin.trim(); pin2.trim();
    if (server.arg("nopin") == "1") cfgPin = "";
    else if (pin.length()) cfgPin = pin; }
  cfgSummary = server.arg("summary") == "1";
  cfgShake = server.arg("shake") == "1";
  cfgFaceDown = server.arg("facedown") == "1";
  cfgPortrait = server.arg("portrait") == "1";
  cfgFlipPortrait = server.arg("flip") == "1";
  cfgTap = server.arg("tap") == "1";
  if (server.hasArg("idleclk")) cfgIdleClock = constrain(server.arg("idleclk").toInt(), 0, 60);
  String town = server.arg("wxtown"); town.trim();
  if (town.length() && town != cfgWxName && !portalMode) {
    float la, lo; String label;
    if (geocode(town, la, lo, label)) { cfgWxLat = la; cfgWxLon = lo; cfgWxName = label; }
    else { sendMessage(400, "Town not found", "Couldn't find \"" + htmlEscape(town) + "\". Try a nearby town or add the country, e.g. \"Paisley, UK\"."); return; }
  } else if (town.length() && portalMode && town != cfgWxName) { cfgWxName = town; cfgWxLat = cfgWxLon = 0; }   // looked up once online
  String spid = server.arg("spid"); spid.trim();
  String spsec = server.arg("spsecret"); spsec.trim();
  if (spid != cfgSpId) { cfgSpId = spid; cfgSpRefresh = ""; }
  if (spsec.length()) cfgSpSecret = spsec;
  String spredir = server.arg("spredir"); spredir.trim();
  if (spredir != cfgSpRedirect) { cfgSpRedirect = spredir; cfgSpRefresh = ""; }
  if (server.hasArg("tapsens")) cfgTapSens = constrain(server.arg("tapsens").toInt(), 1, 3);
  if (server.hasArg("raceA")) { cfgRaceA = server.arg("raceA").toInt(); cfgRaceB = server.arg("raceB").toInt(); }
  if (server.hasArg("nightStart")) { cfgNightStart = server.arg("nightStart").toInt(); cfgNightEnd = server.arg("nightEnd").toInt(); }
  saveSettings();
  sendMessage(200, "Saved &#10003;", "The board is restarting and will join whichever saved network is in range.");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

// "Connect now": answer the browser first (we're about to leave this network), then switch in loop()
void handleWifiConnect() {
  if (!authed()) return;
  int k = server.arg("n").toInt();
  if (k < 0 || k >= numNets) { sendMessage(400, "Unknown network", "That network isn't saved."); return; }
  sendMessage(200, "Switching to " + htmlEscape(nets[k].ssid) + "&hellip;",
              "The board is leaving this network now, so this page will stop updating. "
              "Join <b>" + htmlEscape(nets[k].ssid) + "</b> yourself and use the new address shown on the board's leaderboard. "
              "If it can't connect, it goes back to the best network it can find.");
  pendingSwitch = k;
}

void handleSpotifyCode() {
  if (!authed()) return;
  String err;
  if (spConnectWithCode(server.arg("url"), err)) {
    saveSettings();
    sendMessage(200, "Spotify connected &#10003;", "Long-press the board's screen and choose Spotify.");
    if (app == APP_SP && !menuOpen) { lastSpPoll = 0; }
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Codes only work once and expire after a few minutes &ndash; open the Spotify link again and paste the new address.");
}

// The relay page sends the browser here with ?code=... after you press Agree on Spotify
void handleSpotifyCallback() {
  if (!authed()) return;
  if (server.hasArg("error")) { sendMessage(400, "Spotify not connected", "Spotify said: " + htmlEscape(server.arg("error"))); return; }
  String err;
  if (spConnectWithCode(server.arg("code"), err)) {
    saveSettings();
    if (app == APP_SP && !menuOpen) lastSpPoll = 0;
    server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>Spotify connected &#10003;</h1><p>Long-press the board's screen and choose Spotify.</p>"
                "<a href='/'>Go to the dashboard</a></div></body></html>");
  } else sendMessage(400, "Couldn't connect Spotify", htmlEscape(err) + ". Press Connect Spotify on the settings page to try again.");
}

void handleSpotifyDisconnect() {
  if (!authed()) return;
  cfgSpRefresh = ""; sp = SpotifyState();
  saveSettings();
  sendMessage(200, "Spotify disconnected", "You can connect it again any time.");
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
  server.on("/api/csv", HTTP_GET, handleApiCsv);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/wifi/connect", HTTP_POST, handleWifiConnect);
  server.on("/update", HTTP_GET, handleUpdatePage);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/spotify/code", HTTP_POST, handleSpotifyCode);
  server.on("/spotify/callback", HTTP_GET, handleSpotifyCallback);
  server.on("/spotify/disconnect", HTTP_POST, handleSpotifyDisconnect);
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
  WiFi.setHostname(HOSTNAME);           // name shown in the router's device list
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

void useNet(int k) {
  Net &n = nets[k];
  cfgSsid = n.ssid; cfgPass = n.pass; cfgUser = n.user;
  cfgIp = n.ip; cfgGw = n.gw; cfgMask = n.mask; cfgDns = n.dns; cfgCompat = n.compat;
}

bool connectOne(int k);
bool connectWiFi() {
  drawStatus("Looking for Wi-Fi...", String(numNets) + (numNets == 1 ? " saved network" : " saved networks"), C_WHITE);
  WiFi.disconnect(true);
  delay(100);
  WiFi.setHostname(HOSTNAME);           // name shown in the router's device list
  WiFi.mode(WIFI_STA);
  int n = WiFi.scanNetworks();
  int order[MAX_NETS]; int rssi[MAX_NETS]; int cnt = 0;
  for (int k = 0; k < numNets; k++) {
    int best = -999;
    for (int i = 0; i < n; i++) if (WiFi.SSID(i) == nets[k].ssid) best = max(best, (int)WiFi.RSSI(i));
    if (best > -999) { order[cnt] = k; rssi[cnt] = best; cnt++; }
  }
  WiFi.scanDelete();
  for (int c = 0; c < cnt; c++) if (order[c] == cfgPreferred) rssi[c] += 1000;   // preferred network jumps the queue
  for (int a = 0; a < cnt; a++) for (int b = a + 1; b < cnt; b++)
    if (rssi[b] > rssi[a]) { int t = order[a]; order[a] = order[b]; order[b] = t; t = rssi[a]; rssi[a] = rssi[b]; rssi[b] = t; }
  String reasons;
  for (int c = 0; c < cnt; c++) {
    if (connectOne(order[c])) { curNet = order[c]; return true; }
    reasons += (reasons.length() ? " | " : "") + nets[order[c]].ssid + ": " + wifiFailReason;
  }
  if (cnt == 0) {
    // nothing seen in the scan (could be a hidden network) - try them all anyway
    for (int k = 0; k < numNets; k++) {
      if (connectOne(k)) { curNet = k; return true; }
      reasons += (reasons.length() ? " | " : "") + nets[k].ssid + ": " + wifiFailReason;
    }
    String names;
    for (int k = 0; k < numNets; k++) names += (names.length() ? ", " : "") + nets[k].ssid;
    wifiFailReason = "None of your saved networks are in range (" + names + "). Add this one in setup.";
    return false;
  }
  wifiFailReason = reasons;
  return false;
}

bool connectOne(int k) {
  useNet(k);
  bool ok = connectAttempt(cfgCompat);
  if (!ok && staAssociated && !cfgCompat && wifiFailReason.indexOf("no IP") >= 0) {
    ok = connectAttempt(true);
    if (ok) { cfgCompat = true; nets[k].compat = true; saveSettings(); }
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
    if (ch[i].tw || ch[i].id.length()) continue;
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
  for (int i = 0; i < numCh; i++) if (!ch[i].tw && ch[i].id.length()) ids += (ids.length() ? "," : "") + ch[i].id;
  if (!ids.length()) { twitchFetch(); raceCheck(); return; }
  JsonDocument doc;
  int code = ytGet("channels", "part=statistics,snippet,contentDetails&fields=" + String(CH_FIELDS) + "&id=" + ids, doc);
  if (code != 200) { netError = apiErrorText(code, doc); return; }
  netError = "";
  lastFetchOk = millis();
  for (int i = 0; i < numCh; i++) {
    if (ch[i].tw || !ch[i].id.length()) continue;
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
    updateRecords(i);
  }
  twitchFetch();
  raceCheck();
}

// race: who's ahead? alert when that changes
void raceCheck() {
  if (raceSet() && ch[cfgRaceA].subs >= 0 && ch[cfgRaceB].subs >= 0) {
    int lead = ch[cfgRaceA].subs >= ch[cfgRaceB].subs ? cfgRaceA : cfgRaceB;
    if (raceLeader >= 0 && lead != raceLeader && ch[cfgRaceA].subs != ch[cfgRaceB].subs && numAlerts < MAX_CH * 2)
      alerts[numAlerts++] = { A_OVERTAKE, lead, raceLeader, 0, ch[lead].subs };
    raceLeader = lead;
  }
}

// ════════════════════════════════════════════════════════════════════════════
//  Twitch: followers, live status and pictures (Helix API, app access token)
// ════════════════════════════════════════════════════════════════════════════
String twToken; unsigned long twTokenUntil = 0;
bool twLoaded = false;                 // no "went live" alerts for streams already running at start-up
String twError;

bool twGetToken() {
  if (twToken.length() && millis() < twTokenUntil) return true;
  if (!cfgTwId.length() || !cfgTwSecret.length()) { twError = "Twitch: add your Client ID and Secret in settings"; return false; }
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, "https://id.twitch.tv/oauth2/token")) return false;
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("client_id=" + urlEncode(cfgTwId) + "&client_secret=" + urlEncode(cfgTwSecret) + "&grant_type=client_credentials");
  String body = http.getString(); http.end();
  if (code != 200) { twError = code == 400 || code == 403 ? "Twitch: Client ID or Secret is wrong" : "Twitch: can't sign in (" + String(code) + ")"; return false; }
  JsonDocument doc; deserializeJson(doc, body);
  twToken = doc["access_token"] | "";
  long exp = doc["expires_in"] | 3600;
  twTokenUntil = millis() + (unsigned long)max(60L, exp - 300) * 1000UL;
  return twToken.length() > 0;
}

int twGet(const String &path, JsonDocument &doc) {
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, "https://api.twitch.tv/helix/" + path)) return -100;
  http.addHeader("Client-Id", cfgTwId);
  http.addHeader("Authorization", "Bearer " + twToken);
  int code = http.GET();
  if (code == 200) deserializeJson(doc, http.getStream());
  http.end();
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
  uint16_t *dst = (uint16_t *)malloc(88 * 88 * 2);
  if (!dst) return;
  memset(dst, 0, 88 * 88 * 2);
  twDst = dst; bool ok = false;
  if (len > 8 && buf[0] == 0x89 && buf[1] == 'P') {
    PNG *png = new PNG();
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
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000);
  if (!http.begin(client, url)) return;
  if (http.GET() == 200) {
    int len = http.getSize(), cap = (len > 0 && len < 90000) ? len : 90000;
    uint8_t *buf = (uint8_t *)malloc(cap);
    if (buf) {
      WiFiClient *st = http.getStreamPtr(); int got = 0; unsigned long t0 = millis();
      while (got < cap && millis() - t0 < 8000) {
        int a = st->available();
        if (a > 0) got += st->readBytes(buf + got, min(a, cap - got));
        else if (!st->connected()) break; else delay(5);
      }
      if (len <= 0 || got == len) twDecodeAvatar(c, buf, got);
      free(buf);
    }
  }
  http.end();
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
      alerts[numAlerts++] = n >= m ? Alert{ A_MILESTONE, i, -1, n - c.subs, m } : Alert{ A_GAIN, i, -1, n - c.subs, n };
    }
    c.subs = n; c.err = "";
    recordSample(i); computeStats(i); updateRecords(i);
    server.handleClient();
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
          alerts[numAlerts++] = { A_LIVE, i, -1, 0, c.liveViewers };
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
    alerts[numAlerts++] = { A_LIVE, (int)(&c - ch), -1, 0, c.liveViewers };
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
bool summaryActive = false;
unsigned long summaryShownAt = 0;
int lastSummaryDay = -1;
int portraitRot = 0;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SubCounter v" FW_VERSION " starting");
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
  prefs.begin("subcounter", true); app = constrain(prefs.getInt("app", APP_YT), 0, NUM_APPS - 1); prefs.end();

  bool needKey = false; for (int i = 0; i < numCh; i++) if (!ch[i].tw) needKey = true;
  if (numNets == 0 || (needKey && cfgApiKey.length() == 0) || numCh == 0) { startPortal(); return; }
  if (!connectWiFi()) { drawWifiFailScreen(); delay(8000); startPortal(); return; }

  configTime(0, 0, "pool.ntp.org", "time.google.com");   // backup: clock is also set from Google's replies
  setenv("TZ", TZ_UK, 1); tzset();
  registerRoutes(); routesRegistered = true;
  server.begin();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
  showAddress();
  if (cfgWxLat == 0 && cfgWxLon == 0 && cfgWxName.length()) {          // town entered in setup mode
    float la, lo; String label;
    if (geocode(cfgWxName, la, lo, label)) { cfgWxLat = la; cfgWxLon = lo; cfgWxName = label; saveSettings(); }
  }
  drawStatus("Loading...", String(numCh) + (numCh == 1 ? " channel" : " channels"), C_WHITE);
  fetchAll();
  lastFetch = millis();
  drawStatus("Loading...", "pictures & videos", C_WHITE);
  fetchAvatars();
  fetchLatestVideos();
  vtCheckNewUpload();
  lastVideoFetch = millis();
  fetchWeather(); lastWxFetch = millis();
  lastInteract = millis();
  numAlerts = 0;                       // nothing to celebrate on start-up
  gfx->fillScreen(C_BG);
  drawApp();
  if (app == APP_YT && ch[page].subs > 0) { shownSubs = max(0L, estimateFor(page) - 30); drawMainNumber(shownSubs); }
  Serial.print("Dashboard: http://"); Serial.print(WiFi.localIP()); Serial.println("  or  http://" HOSTNAME ".local");
}

// Redraw whatever the current mode shows
void drawMode() {
  switch (mode) {
    case M_SLEEP: break;
    case M_PORTRAIT: drawPortraitBoard(); break;
    case M_SUMMARY: drawSummary(); break;
    case M_AMBIENT: drawAmbient(); break;
    case M_CLOCK: drawClock(); break;
    default: drawApp(); break;
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
    case M_CLOCK:    if (blNow != BL_NORMAL) setBacklight(BL_NORMAL); drawClock(); break;
    default:
      gfx->fillScreen(C_BG);
      drawApp();
      if (blNow != BL_NORMAL) fadeBacklight(BL_NORMAL, 2);
      break;
  }
}

// Something the user did: wake from night / summary
void userActivity() {
  lastInteract = millis();
  if (mode == M_SUMMARY) summaryActive = false;
}

bool ytVisible() { return mode == M_NORMAL && !menuOpen && app == APP_YT; }

void loop() {
  server.handleClient();
  if (portalMode) {
    dns.processNextRequest();
    // In setup mode because no saved network was found? Every 3 minutes, if nobody is using
    // the setup page, check again whether one of them has come into range.
    static unsigned long lastRetry = millis();
    if (numNets > 0 && cfgApiKey.length() && millis() - lastRetry > 180000UL && WiFi.softAPgetStationNum() == 0) {
      lastRetry = millis();
      int n = WiFi.scanNetworks();
      bool seen = false;
      for (int i = 0; i < n; i++) for (int k = 0; k < numNets; k++) if (WiFi.SSID(i) == nets[k].ssid) seen = true;
      WiFi.scanDelete();
      if (seen) { drawStatus("Network found!", "Restarting...", C_GREEN); delay(1000); ESP.restart(); }
    }
    delay(2);
    return;
  }

  // ── inputs ────────────────────────────────────────────────────────────────
  imuUpdate();
  bool shake = evShake; evShake = false;
  bool moved = evMoved; evMoved = false;
  if (moved) lastInteract = millis();                      // picking it up counts as using it
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
  bool anyInput = swipe || tap || shake || bootShort || touching || moved;
  if (anyInput && (mode == M_AMBIENT || mode == M_SUMMARY || mode == M_CLOCK)) {
    userActivity();
    swipe = 0; tap = false; bootShort = false;   // this input just wakes it up
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
  else if (cfgIdleClock > 0 && millis() - lastInteract > (unsigned long)cfgIdleClock * 60000UL &&
           !(app == APP_SP && sp.playing) && !menuOpen) want = M_CLOCK;
  if (want != mode) {
    if (mode == M_SLEEP || mode == M_PORTRAIT) lastInteract = millis();   // picked up again
    enterMode(want);
  }

  // ── actions in the normal view ────────────────────────────────────────────
  if (mode == M_NORMAL) {
    if (swipe) lastInteract = millis();
    if (swipe == 'H') {                                     // long-press: home menu (or close it)
      menuOpen = !menuOpen;
      gfx->fillScreen(C_BG); drawApp();
      swipe = 0;
    }
    if (menuOpen) {
      if (swipe == 'T') {
        int a = constrain(tapX * NUM_APPS / gfx->width(), 0, NUM_APPS - 1);
        openApp(a);
      } else if (bootShort) openApp(app);
      if (millis() - lastInteract > 20000UL) openApp(app);  // menu left open: go back
    }
    else if (app == APP_YT) {
      switch (swipe) {
        case 'L': changePage(+1); break;
        case 'R': changePage(-1); break;
        case 'U': changeCard(+1); break;
        case 'D': changeCard(-1); break;
      }
      if (bootShort) { changePage(+1); lastInteract = millis(); }
      if (tap && cfgTap && !board) { changePage(+1); lastInteract = millis(); }
      if (shake && cfgShake) {
        lastInteract = millis();
        if (!board && card == 0) { gfx->fillRect(0, 150, gfx->width() - 14, 22, C_BG); ft(8, 168, "Refreshing...", F_S, C_GOLD); }
        lastFetch = 0;
        if (millis() - lastVideoFetch > 3UL * 60000UL) lastVideoFetch = 0;
      }
      if ((card != 0 || board) && millis() - lastInteract > IDLE_RETURN_MS) {
        card = 0; board = false; raceView = false; drawView(); lastInteract = millis();
      }
      if (cfgAuto && numCh > 1 && card == 0 && !board && millis() - lastInteract > 30000UL) {
        static unsigned long lastAuto = 0;
        if (millis() - lastAuto > AUTO_SWITCH_MS) { lastAuto = millis(); changePage(+1); }
      }
    }
    else if (app == APP_WX) {
      int old = wxCard;
      if (swipe == 'U') wxCard = min(wxCard + 1, WX_CARDS - 1);
      if (swipe == 'D') wxCard = max(wxCard - 1, 0);
      if (bootShort) wxCard = (wxCard + 1) % WX_CARDS;
      if (shake && cfgShake) { lastWxFetch = 0; }
      if (wxCard != old) { wipe(0, wxCard > old ? 1 : -1); drawWeather(); }
      if (wxCard != 0 && millis() - lastInteract > IDLE_RETURN_MS) { wxCard = 0; drawWeather(); }
    }
    else if (app == APP_SP) {
      if (cfgSpRefresh.length() && (swipe == 'T' || swipe == 'L' || swipe == 'R' || swipe == 'U' || swipe == 'D')) spCommand(swipe);
      if (bootShort && cfgSpRefresh.length()) spCommand('T');
    }
  }

  // ── network ───────────────────────────────────────────────────────────────
  if (pendingSwitch >= 0) {
    int k = pendingSwitch; pendingSwitch = -1;
    delay(500);                                   // let the browser get its reply
    if (connectOne(k)) curNet = k;
    else if (!connectWiFi()) { drawWifiFailScreen(); delay(5000); }
    netError = ""; lastFetch = 0;
    gfx->fillScreen(C_BG); drawMode();
  }
  static unsigned long wifiLostAt = 0;
  if (WiFi.status() != WL_CONNECTED) {
    if (!wifiLostAt) wifiLostAt = millis();
    netError = "Wi-Fi lost, reconnecting...";
    if (ytVisible()) drawFooter();
    if (millis() - wifiLostAt > 60000UL && numNets > 0) {
      // been gone a minute: maybe we've moved (work -> home). Look for any saved network.
      if (connectWiFi()) { wifiLostAt = 0; netError = ""; lastFetch = 0; gfx->fillScreen(C_BG); drawMode(); }
      else wifiLostAt = millis();
      return;
    }
    WiFi.reconnect();
    delay(3000);
    return;
  }
  wifiLostAt = 0;

  bool refreshed = false;
  if (lastFetch == 0 || millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    fetchAll();
    refreshLiveVideos();     // keeps LIVE badges and viewer counts current
    fetchAvatars();          // picks up any that failed earlier
    refreshed = true;
  }
  if (lastVideoFetch == 0 || millis() - lastVideoFetch > VIDEO_REFRESH_MS) {
    lastVideoFetch = millis();
    fetchLatestVideos();
    vtCheckNewUpload();
    refreshed = true;
  }
  if (lastCommentFetch == 0 || millis() - lastCommentFetch > COMMENT_REFRESH_MS) {
    lastCommentFetch = millis();
    fetchComments();
  }
  // your new upload: its own numbers every 5 minutes, typical views once a day
  if (vt.active && (lastVtFetch == 0 || millis() - lastVtFetch > 5UL * 60000UL)) {
    lastVtFetch = millis();
    vtPoll();
    refreshed = true;
  }
  if (vt.active && (lastTypicalFetch == 0 || millis() - lastTypicalFetch > 24UL * 3600000UL)) {
    lastTypicalFetch = millis();
    vtFetchTypical();
  }
  bool wxRefreshed = false;
  if (lastWxFetch == 0 || millis() - lastWxFetch > 15UL * 60000UL) {
    lastWxFetch = millis();
    fetchWeather();
    wxRefreshed = true;
  }
  // Spotify: every 3 s while it's on screen
  if (mode == M_NORMAL && !menuOpen && app == APP_SP && cfgSpRefresh.length() &&
      (lastSpPoll == 0 || millis() - lastSpPoll > 3000UL)) {
    lastSpPoll = millis();
    bool wasPlaying = sp.playing, had = sp.hasTrack;
    bool changed = spPoll();
    if (changed || wasPlaying != sp.playing || had != sp.hasTrack) drawSpotify();
  }

  if (refreshed) {
    // celebrate only when someone can see it (not face-down, side-on or at night)
    if (numAlerts && (mode == M_NORMAL || mode == M_SUMMARY || mode == M_CLOCK) && !(app == APP_SP && !menuOpen && sp.playing)) {
      setBacklight(BL_NORMAL);
      for (int i = 0; i < numAlerts; i++) showAlert(alerts[i]);
      if (jumpToCh >= 0 && app == APP_YT && !menuOpen) { page = jumpToCh; card = 0; board = false; }
      jumpToCh = -1;
      gfx->fillScreen(C_BG);
      drawMode();
    }
    numAlerts = 0;
    if (ytVisible() && card == 0 && !board) { long keep = shownSubs; drawMain(); shownSubs = keep; drawMainNumber(shownSubs); }
    else if (ytVisible() || mode != M_NORMAL) drawMode();
  }
  if (wxRefreshed && mode == M_NORMAL && !menuOpen && app == APP_WX) drawWeather();

  // ── animation / periodic redraws ──────────────────────────────────────────
  static unsigned long lastAnim = 0;
  if (ytVisible() && numCh && card == 0 && !board && ch[page].subs >= 0 && millis() - lastAnim > 30) {
    lastAnim = millis();
    long target = estimateFor(page);
    if (shownSubs != target) {
      if (shownSubs < 0 || shownSubs > target) shownSubs = target;
      else { long gap = target - shownSubs; shownSubs += (gap > 20) ? gap / 8 : 1; }
      drawMainNumber(shownSubs);
    }
  }
  if (mode == M_NORMAL && !menuOpen && app == APP_SP && sp.hasTrack) { mqTick(mqTitle); mqTick(mqArtist); }
  static unsigned long lastProg = 0;
  if (mode == M_NORMAL && !menuOpen && app == APP_SP && sp.hasTrack && millis() - lastProg > 1000) {
    lastProg = millis(); drawSpProgress();
  }
  static unsigned long lastSlow = 0;
  static int lastMinute = -1;
  if (millis() - lastSlow > 30000UL) {
    lastSlow = millis();
    if (mode == M_AMBIENT) drawAmbient();
    else if (ytVisible()) drawFooter();
  }
  static int lastClockMin = -1;
  if (mode == M_CLOCK && timeValid()) {
    time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
    if (lt.tm_min != lastClockMin) { lastClockMin = lt.tm_min; drawClock(); }
  }
  if (mode == M_NORMAL && !menuOpen && app == APP_WX && wxCard == 0 && timeValid()) {   // keep the clock ticking
    time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
    if (lt.tm_min != lastMinute) { lastMinute = lt.tm_min; if (wx.ok) drawWeather(); }
  }

  delay(5);
}
