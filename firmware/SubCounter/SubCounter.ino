/*
 * SubCounter v11.6 — YouTube / Twitch subscriber counter and desk hub for the
 * Waveshare ESP32-C6-Touch-LCD-1.47.  https://github.com/acemods/SubCounter
 *
 *  ON THE BOARD
 *    Swipe left / right ... next / previous channel   (BOOT short press = next)
 *    Swipe up ............. detail cards: Overview, Latest video, Comments, Growth,
 *                           Records, 7-day graph, Milestone
 *    Swipe down ........... back; from the main count: Leaderboard, then Race
 *    Long-press, or hold BOOT ~1 s ... home menu (YouTube, Weather, Spotify; swipe for Device)
 *    Shake / face-down / on its side / double-tap the desk ... refresh / off / tall board / next
 *    Hold BOOT 3 s ........ setup mode
 *
 *  IN A BROWSER: http://subcounter.local/  (dashboard), /settings, /update
 *
 *  FILES (Arduino joins them in this order):
 *    SubCounter.ino  settings, shared data, background networking + connection helpers
 *    a01_panel       display start-up        a10_ota       wireless updates
 *    a02_helpers     text, time, URLs         a11_tracker   new video tracker
 *    a03_settings    load / save settings     a12_apps      home menu, Weather, Spotify
 *    a04_history     history, records         a13_web       dashboard, settings page, API
 *    a05_avatars     profile pictures         a14_portal    setup mode
 *    a06_screens     YouTube cards            a15_wifi      Wi-Fi, roaming
 *    a07_touch       touch screen             a16_youtube   YouTube API
 *    a08_motion      motion sensor            a17_twitch    Twitch API
 *    a09_alerts      alerts, race, clocks     a18_main      setup(), loop(), network task
 *
 *  Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *    Flash Size "8MB", Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
 *    Libraries: GFX Library for Arduino 1.6.x, ArduinoJson 7.x, JPEGDEC 1.8.x, PNGdec 1.1.x, U8g2
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
#include <qrcode.h>    // built into the ESP32 core (esp_qrcode_*)
#include <nvs.h>       // settings backup: list every saved setting
#include <ESPmDNS.h>   // http://subcounter.local
#define HOSTNAME "subcounter"
#define FW_VERSION "13.0"     // shown on start-up, home menu, settings, update page and dashboard

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
#define NUM_CARDS          9          // 0 main, 1 overview, 2 latest video, 3 comments, 4 growth, 5 records, 6 top videos, 7 graph, 8 milestone
#define RECENT_N           10         // recent uploads ranked on the Top videos card
#define RECENT_REFRESH_MS  (6UL * 3600000UL)   // 2 units per channel
#define COMMENT_REFRESH_MS (30UL * 60UL * 1000UL)   // newest comments: 1 unit per channel
#define HIST_MAX_AGE       (40L * 86400L)        // a little over a month, for the monthly recap
#define TZ_UK              "GMT0BST,M3.5.0/1,M10.5.0/2"
int BL_NORMAL = 160;                 // screen brightness (0-255), set from the brightness setting
int BL_NIGHT  = 18;                  // night clock brightness

// ── Display ─────────────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, false, 172, 320, 34, 0, 34, 0);
JPEGDEC jpeg;

#define C_BG      0x0000
#define C_RED     0xF800
#define C_WHITE   0xFFFF
uint16_t C_GREY   = 0x8410;          // these three change with the colour theme
uint16_t C_DKGREY = 0x39E7;
uint16_t C_GOLD   = 0xFEA0;          // accent colour (headings, highlights)
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
  // channel views over time (YouTube): saved on the board, about once an hour
  bool vOk = false; long long vToday = 0, v24 = 0, v7 = 0, v30 = 0;
  time_t lastViewT = 0; long long lastViewV = -1; time_t vHistStart = 0;
  // latest video's views over time (every channel; from the 10-minute latest-video check)
  String lvVid; uint32_t lvT[48]; uint32_t lvV[48]; int lvN = 0;   // hourly, 48 h
  // newest comments on the latest video
  String cmAuthor[3], cmText[3]; time_t cmT[3] = {0, 0, 0}; long cmLikes[3] = {0, 0, 0};
  int cmN = 0; bool cmOff = false;
  // personal bests (saved on the board)
  int32_t pbDay = 0, pbWeek = 0, pbVid = 0; uint32_t pbDayT = 0, pbWeekT = 0, pbVidT = 0; String pbVidTitle;
  bool pbLoaded = false;
  // recent uploads, for the Top videos card / dashboard table
  struct Recent { String id, title; time_t pub = 0; long long views = -1; long likes = -1, comments = -1; } rv[RECENT_N];
  int rvN = 0;
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
// Phone notifications (ntfy): alerts are also queued here and sent by the network task
String cfgNtfy;                                   // ntfy topic ("" = off)
String cfgNtfyServer = "https://ntfy.sh";
int    cfgNtfyMask = 1 | 2 | 4 | 8 | 16;          // 1 milestones, 2 went live, 4 records, 8 overtakes, 16 video views, 32 every new subscriber (your channel)
struct Note { String title, msg, tags, click; };
Note noteQ[8]; int noteN = 0;
void queueNote(const String &title, const String &msg, const char *tags, const String &click = "") {
  if (noteN < 8) noteQ[noteN++] = { title, msg, tags, click };
}
void notifyAlert(const Alert &a);
void addAlert(const Alert &a) {
  if (numAlerts < MAX_CH * 2) alerts[numAlerts++] = a;
  if (cfgNtfy.length()) notifyAlert(a);
}
int raceLeader = -1;

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

// ════════════════════════════════════════════════════════════════════════════
//  Background networking
//  All internet requests (YouTube, Twitch, Spotify, weather) run in their own
//  task, so the screen, touch and web pages keep working while we wait for a
//  slow server. Shared data is protected by one lock: each task holds it while
//  it reads or changes data, and the network task lets go of it while it waits
//  for an answer (see NetIO).
// ════════════════════════════════════════════════════════════════════════════
SemaphoreHandle_t dataLock = nullptr;
TaskHandle_t netTaskH = nullptr;
volatile bool netRunning = false;
volatile bool netPause = false;          // during wireless updates and Wi-Fi switches
volatile bool netRefreshed = false, netWxRefreshed = false, netSpRedraw = false;
volatile bool netBusy = false;           // the network task is in the middle of a cycle
// ── Diagnostics (/debug): what the network task is doing, and any moment the screen froze ──
volatile const char *netStage = "starting";
volatile bool netHolding = false;          // the network task has the data right now (not waiting on the internet)
struct Stall { unsigned long at, ms; char what[48]; };
Stall stalls[16]; int stallN = 0, stallPos = 0;
unsigned long worstStall = 0;
bool lastLongIsWeb = false;              // this loop's slow moment was a web page (already logged)
void noteStall(unsigned long ms, const String &what) {
  Stall &s = stalls[stallPos]; s.at = millis(); s.ms = ms;
  strncpy(s.what, what.c_str(), sizeof(s.what) - 1); s.what[sizeof(s.what) - 1] = 0;
  stallPos = (stallPos + 1) % 16; if (stallN < 16) stallN++;
  if (ms > worstStall) worstStall = ms;
  Serial.printf("STALL %lu ms: %s\n", ms, s.what);
}
bool onNetTask() { return netRunning && xTaskGetCurrentTaskHandle() == netTaskH; }
void lockData()   { if (dataLock) xSemaphoreTakeRecursive(dataLock, portMAX_DELAY); }
void unlockData() { if (dataLock) xSemaphoreGiveRecursive(dataLock); }
struct DataGuard { DataGuard() { lockData(); } ~DataGuard() { unlockData(); } };
// Put one of these in scope around a slow network call: it releases the lock
// (however many times this task holds it) and takes it back afterwards.
struct NetIO {
  int n = 0;
  NetIO() {
    if (!dataLock) return;
    while (xSemaphoreGetMutexHolder(dataLock) == xTaskGetCurrentTaskHandle()) { xSemaphoreGiveRecursive(dataLock); n++; }
    if (n && onNetTask()) netHolding = false;
  }
  ~NetIO() { for (int k = 0; k < n; k++) xSemaphoreTakeRecursive(dataLock, portMAX_DELAY); if (n && onNetTask()) netHolding = true; }
};
// Before switching Wi-Fi: stop new network jobs and wait (up to 10 s) for the current one to finish
void netQuiesce() {
  netPause = true;
  if (!netRunning || onNetTask()) return;
  NetIO io;
  unsigned long t = millis();
  while (netBusy && millis() - t < 10000) delay(20);
}
// Keep the web pages responsive during long jobs (main task only); on the network
// task, briefly hand the data back so the screen and touch don't stall
void pump() { if (!onNetTask()) server.handleClient(); else { NetIO io; vTaskDelay(1); } }

// ── Re-usable HTTPS connections ─────────────────────────────────────────────
// Setting up a secure connection takes 1-2 s; keeping it open for the next
// request to the same server makes refreshes much quicker. Idle ones are closed
// after 20 s to give the memory back.
struct Conn { WiFiClientSecure *cli = nullptr; HTTPClient *http = nullptr; unsigned long lastUse = 0; bool temp = false; };
Conn connYT, connTW, connSP, connGH;
// Each secure connection needs ~40 KB in one piece. Before opening a new one, close any
// other kept-open connections that aren't in use if memory is getting tight.
void connFreeOthers(Conn *keep) {
  Conn *all[] = { &connYT, &connTW, &connSP, &connGH };
  for (Conn *c : all) if (c != keep && c->cli) { c->cli->stop(); delete c->http; delete c->cli; c->http = nullptr; c->cli = nullptr; }
}
bool memTight() { return ESP.getMaxAllocHeap() < 52000 || ESP.getFreeHeap() < 70000; }
// Diagnostics: the last few failed internet requests (shown on /debug)
struct NetErr { unsigned long at; int code; uint32_t freeMem, block; char host[28]; };
NetErr netErrs[12]; int netErrN = 0, netErrPos = 0; unsigned long netErrTotal = 0;
int netFailRun = 0;                        // failed requests in a row
volatile bool memRestartDue = false;       // memory too broken up to connect: restart when nobody's using the board
String bootWhy;                            // why the board last restarted (for /debug)
void noteNetErr(const String &url, int code) {
  if (code < 0 && ++netFailRun >= 6 && ESP.getMaxAllocHeap() < 40000) memRestartDue = true;
  NetErr &e = netErrs[netErrPos]; e.at = millis(); e.code = code; e.freeMem = ESP.getFreeHeap(); e.block = ESP.getMaxAllocHeap();
  int a = url.indexOf("://"); a = a < 0 ? 0 : a + 3; int b = url.indexOf('/', a); if (b < 0) b = url.length();
  String h = url.substring(a, b); strncpy(e.host, h.c_str(), sizeof(e.host) - 1); e.host[sizeof(e.host) - 1] = 0;
  netErrPos = (netErrPos + 1) % 12; if (netErrN < 12) netErrN++; netErrTotal++;
  Serial.printf("NET ERR %d %s heap %u block %u\n", code, e.host, e.freeMem, e.block);
}
HTTPClient *connBegin(Conn &c, const String &url, Conn &tmp) {
  Conn *u = &c;
  if (!onNetTask() && netRunning) u = &tmp;            // other tasks get a one-off connection
  if (ESP.getFreeHeap() < 50000 && u->cli && !u->temp) { delete u->http; delete u->cli; u->http = nullptr; u->cli = nullptr; }
  if (!u->cli && memTight() && (onNetTask() || !netRunning)) connFreeOthers(&c);   // make room for the new connection
  if (!u->cli) {
    u->cli = new (std::nothrow) WiFiClientSecure(); u->http = new (std::nothrow) HTTPClient();
    if (!u->cli || !u->http) { delete u->cli; delete u->http; u->cli = nullptr; u->http = nullptr; return nullptr; }
    u->cli->setInsecure(); u->temp = (u == &tmp);
  }
  u->http->setReuse(u != &tmp);
  u->http->setTimeout(10000); u->http->setConnectTimeout(6000);
  if (!u->http->begin(*u->cli, url)) return nullptr;
  return u->http;
}
void connEnd(Conn &c, Conn &tmp) {
  Conn *u = (tmp.cli) ? &tmp : &c;
  if (u->http) u->http->end();
  u->lastUse = millis();
  if (u == &tmp || ESP.getFreeHeap() < 50000) { if (u->cli) u->cli->stop(); delete u->http; delete u->cli; u->http = nullptr; u->cli = nullptr; }
}
void connClose(Conn &c) { if (c.cli) { c.cli->stop(); delete c.http; delete c.cli; c.http = nullptr; c.cli = nullptr; } }
struct Hdr { const char *k; String v; };
// One HTTPS request on a re-usable connection. Lets go of the data lock while waiting.
int httpsCall(Conn &c, const char *method, const String &url, const Hdr *hs, int nh, String *resp,
              const char *collect = nullptr, String *collected = nullptr, int timeoutMs = 10000) {
  int code = -1;
  NetIO io;
  for (int attempt = 0; attempt < 2; attempt++) {
    Conn tmp;
    bool hadOpen = c.cli != nullptr && (onNetTask() || !netRunning);   // re-using a kept-open connection?
    HTTPClient *h = connBegin(c, url, tmp);
    if (!h) { connEnd(c, tmp); return -100; }
    h->setTimeout(timeoutMs);
    for (int k = 0; k < nh; k++) h->addHeader(hs[k].k, hs[k].v);
    const char *cl[] = { collect };
    if (collect) h->collectHeaders(cl, 1);
    if (!strcmp(method, "GET")) code = h->GET();
    else { h->addHeader("Content-Length", "0"); code = h->sendRequest(method, (uint8_t *)nullptr, 0); }
    if (code > 0) {
      if (collect && collected) *collected = h->header(collect);
      if (resp) *resp = h->getString();
    }
    connEnd(c, tmp);
    // A kept-open connection the server had quietly closed fails straight away (refused / not
    // connected / lost). Retry those once on a fresh connection - only for GETs, so a Spotify
    // "next track" is never sent twice, and never after a read timeout (-11).
    bool staleConn = code == HTTPC_ERROR_CONNECTION_REFUSED || code == HTTPC_ERROR_SEND_HEADER_FAILED ||
                     code == HTTPC_ERROR_NOT_CONNECTED || code == HTTPC_ERROR_CONNECTION_LOST;
    if (attempt == 0 && hadOpen && staleConn && !strcmp(method, "GET")) { connClose(c); continue; }
    // Couldn't connect at all (weak Wi-Fi, or not enough memory for a secure connection):
    // free up memory and try once more on a fresh connection. GETs only.
    if (attempt == 0 && code < 0 && code != HTTPC_ERROR_READ_TIMEOUT && !strcmp(method, "GET")) {
      noteNetErr(url, code);
      connClose(c); if (onNetTask() || !netRunning) connFreeOthers(&c);
      delay(400); continue;
    }
    break;
  }
  if (code < 0 || code >= 500) noteNetErr(url, code); else { netFailRun = 0; memRestartDue = false; }
  return code;
}
// Download a picture into a new buffer (caller frees). Lets go of the data lock while waiting.
uint8_t *httpDownload(const String &url, int &outLen, int maxLen, uint8_t *into = nullptr) {
  outLen = 0;
  if (!into && ESP.getFreeHeap() < maxLen + 70000) { connClose(connYT); connClose(connTW); connClose(connSP); }   // make room
  NetIO io;
  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, url)) return nullptr;
  uint8_t *buf = nullptr;
  int dcode = http.GET();
  if (dcode != 200) noteNetErr(url, dcode);
  if (dcode == 200) {
    int len = http.getSize();
    int cap = (len > 0 && len < maxLen) ? len : maxLen;
    if (into && len > maxLen) { http.end(); return nullptr; }          // too big for the fixed buffer
    buf = into ? into : (uint8_t *)malloc(cap);
    if (buf) {
      WiFiClient *st = http.getStreamPtr();
      int got = 0; unsigned long t0 = millis();
      while (got < cap && millis() - t0 < 8000) {
        int a = st->available();
        if (a > 0) got += st->readBytes(buf + got, min(a, cap - got));
        else if (!st->connected()) break;
        else delay(3);
      }
      bool complete = (len > 0) ? (got == len) : (got > 4 && got < cap);
      if (complete) outLen = got; else { if (!into) free(buf); buf = nullptr; }
    }
  }
  http.end();
  return buf;
}
void connCloseIdle(Conn &c, unsigned long idleMs = 20000) {
  if (c.cli && millis() - c.lastUse > idleMs) { c.cli->stop(); delete c.http; delete c.cli; c.http = nullptr; c.cli = nullptr; }
}

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
int    cfgSumHour = 9;                          // daily summary time (hour)
String morningJson;                             // today's summary, kept for the dashboard
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
String cfgTwitch;                                // Twitch channel names, one per line
String cfgTwId, cfgTwSecret;                     // Twitch app (dev.twitch.tv/console)                                   // settings PIN (blank = none)
bool   cfgLiveAlert = true;                      // alert when a channel goes live
int    cfgTheme = 0;                              // colour theme (see THEMES)
int    cfgBright = 63, cfgNightBright = 7;        // brightness in %
bool   cfgBootUpd = true;                        // check GitHub on the start-up screen (and wait 20 s if there's one)
bool   cfgAutoUpd = false;                       // install new versions from GitHub by itself (3 am)
String cfgUpdUrl = "https://raw.githubusercontent.com/acemods/SubCounter/main/builds/latest/";   // where to look for updates
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
unsigned long lastVtFetch = 0, lastTypicalFetch = 0, lastCommentFetch = 0, lastRecentFetch = 0;

// Forward declarations (functions used before they're defined)
void drawMode();
long vtViews();
void drawVideoTracker();
extern const char PAGE_HEAD[];
int ytGet(const String &endpoint, const String &query, JsonDocument &doc);
void twitchFetch();
void startNetTask();
extern String twError;
void raceCheck();
bool raceSet();
void drawRace();
void drawBoard();
void handleSettings();
void setBacklight(int v);
long nextMilestone(long n);
long prevMilestone(long next);
