/*
 * SubCounter v5 — YouTube subscriber counter for Waveshare ESP32-C6-Touch-LCD-1.47
 *
 *  - Track up to 10 YouTube channels (@handles or UC… channel IDs)
 *  - Swipe left/right on the screen (or tap BOOT) to change channel
 *  - Pop-up alert whenever any tracked channel gains subscribers
 *  - Optional auto-switch between channels every 10 seconds
 *
 * SETUP
 *   First run (or hold BOOT for 3 s): join Wi-Fi "SubCounter-Setup" and open
 *   http://192.168.4.1. While running you can also open http://<board IP>.
 *
 * Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *   Flash Size "8MB", Partition Scheme "8M with spiffs (3MB APP/1.5MB SPIFFS)".
 *   Libraries: "GFX Library for Arduino" 1.6.x, "ArduinoJson" 7.x
 *
 * Settings are stored in the board's own flash memory (NVS), nowhere else.
 */

#include <Arduino_GFX_Library.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include <esp_wifi.h>
#include <esp_mac.h>

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
#define TP_INT    21
#define TP_ADDR   0x63      // AXS5106L touch controller

// ── Settings ────────────────────────────────────────────────────────────────
#define AP_NAME            "SubCounter-Setup"
#define MAX_CH             10
#define REFRESH_MS         (2UL * 60UL * 1000UL)   // all channels in one API call every 2 min
#define AUTO_SWITCH_MS     10000UL
#define ALERT_MS           3500UL
#define WIFI_TIMEOUT_MS    20000UL
#define DHCP_EXTRA_MS      15000UL
#define HOLD_FOR_SETUP_MS  3000UL
#define SWIPE_MIN_PX       40

// ── Display ─────────────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, LCD_RST, 0, false, 172, 320, 34, 0, 34, 0);

#define C_BG      0x0000
#define C_RED     0xF800
#define C_WHITE   0xFFFF
#define C_GREY    0x8410
#define C_DKGREY  0x39E7
#define C_GOLD    0xFEA0
#define C_PURPLE  0x901F
#define C_GREEN   0x07E0

// ── Channels ────────────────────────────────────────────────────────────────
struct Channel {
  String handle;        // what the user typed (@name or UC…)
  String id;            // resolved UC… channel ID
  String title;         // channel name from YouTube
  long   subs = -1;     // latest count (-1 = unknown)
  String err;           // per-channel problem, e.g. "Not found"
};
Channel ch[MAX_CH];
int numCh = 0;
int page = 0;           // which channel is on screen
long shownSubs = -1;    // animated number on screen

struct Alert { int idx; long delta; long total; };
Alert alerts[MAX_CH];
int numAlerts = 0;

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

String cfgSsid, cfgPass, cfgChannels, cfgApiKey, cfgUser;
String cfgIp, cfgGw, cfgMask, cfgDns;
bool   cfgCompat = false, cfgAuto = false;
bool   usedCompatThisBoot = false;
String wifiFailReason = "";
volatile int lastDiscReason = 0;
volatile bool staAssociated = false;
bool portalMode = false;
String scanOptions;

String netError = "";            // problem reaching YouTube at all
unsigned long lastFetch = 0, lastFetchOk = 0, lastInteract = 0;
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

String withCommas(long n) {
  String raw = String(n), out = "";
  int len = raw.length();
  for (int i = 0; i < len; i++) {
    out += raw[i];
    int left = len - i - 1;
    if (left > 0 && left % 3 == 0) out += ',';
  }
  return out;
}

uint8_t sizeForText(const String &s, int maxSize, int margin) {
  for (int sz = maxSize; sz > 1; sz--)
    if ((int)s.length() * 6 * sz <= gfx->width() - margin) return sz;
  return 1;
}

// Word-wrapped text block
void wrapText(const String &msg, int x0, int y, uint8_t ts, uint16_t colour) {
  int cw = 6 * ts, lh = 8 * ts + 3, x = x0;
  gfx->setTextSize(ts);
  gfx->setTextColor(colour, C_BG);
  String word, text = msg + " ";
  for (size_t i = 0; i < text.length(); i++) {
    if (text[i] == ' ') {
      if (x + (int)word.length() * cw > gfx->width() - x0) { x = x0; y += lh; }
      gfx->setCursor(x, y); gfx->print(word); x += (word.length() + 1) * cw; word = "";
    } else word += text[i];
  }
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
    // accept pasted links like youtube.com/@name or youtube.com/channel/UC…
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
  cfgChannels = prefs.getString("channels", prefs.getString("channel", ""));  // migrate v4
  cfgApiKey   = prefs.getString("apikey", "");
  cfgUser     = prefs.getString("user", "");
  cfgIp       = prefs.getString("ip", "");
  cfgGw       = prefs.getString("gw", "");
  cfgMask     = prefs.getString("mask", "");
  cfgDns      = prefs.getString("dns", "");
  cfgCompat   = prefs.getBool("compat", false);
  cfgAuto     = prefs.getBool("auto", false);
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
  prefs.end();
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

void drawHeader() {
  gfx->fillRect(0, 0, gfx->width(), 54, C_BG);
  gfx->fillRoundRect(12, 12, 40, 28, 8, C_RED);
  gfx->fillTriangle(26, 18, 26, 34, 40, 26, C_WHITE);
  if (!numCh) return;
  Channel &c = ch[page];
  String t = c.title.length() ? c.title : c.handle;
  if (t.length() > 21) t = t.substring(0, 20) + ".";
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE, C_BG);
  gfx->setCursor(62, 13);
  gfx->print(t);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BG);
  gfx->setCursor(62, 34);
  gfx->print("subscribers");
  if (numCh > 1) {
    String pos = String(page + 1) + "/" + String(numCh);
    gfx->setCursor(gfx->width() - 12 - pos.length() * 6, 34);
    gfx->print(pos);
  }
  gfx->drawFastHLine(12, 52, gfx->width() - 24, C_DKGREY);
}

void drawNumber(long n) {
  gfx->fillRect(0, 56, gfx->width(), 78, C_BG);
  if (!numCh) return;
  Channel &c = ch[page];
  if (c.err.length() && c.subs < 0) { wrapText(c.err, 14, 74, 2, C_RED); return; }
  if (n < 0) { centreText("...", 78, 5, C_GREY); return; }
  String s = withCommas(n);
  uint8_t sz = sizeForText(s, 7, 16);
  centreText(s, 96 - (sz * 8) / 2, sz, C_WHITE);
}

void drawDots() {
  gfx->fillRect(0, 136, gfx->width(), 10, C_BG);
  if (numCh < 2) return;
  int gap = 12, x = (gfx->width() - (numCh - 1) * gap) / 2;
  for (int i = 0; i < numCh; i++)
    if (i == page) gfx->fillCircle(x + i * gap, 141, 3, C_WHITE);
    else gfx->drawCircle(x + i * gap, 141, 3, C_GREY);
}

void drawFooter() {
  gfx->fillRect(0, 150, gfx->width(), 22, C_BG);
  gfx->setTextSize(1);
  gfx->setCursor(12, 158);
  if (netError.length()) {
    gfx->setTextColor(C_RED, C_BG);
    gfx->print(netError.substring(0, 50));
    return;
  }
  gfx->setTextColor(C_GREY, C_BG);
  if (lastFetchOk) {
    unsigned long mins = (millis() - lastFetchOk) / 60000UL;
    gfx->print(mins == 0 ? String("Updated just now") : "Updated " + String(mins) + " min ago");
  }
  String ip = WiFi.localIP().toString() + (usedCompatThisBoot ? " (W4)" : "");
  gfx->setCursor(gfx->width() - 12 - ip.length() * 6, 158);
  gfx->print(ip);
}

void drawPage() {
  gfx->fillScreen(C_BG);
  if (!numCh) { centreText("No channels set", 70, 2, C_GREY); return; }
  shownSubs = ch[page].subs;
  drawHeader();
  drawNumber(shownSubs);
  drawDots();
  drawFooter();
}

// Slide the old page out: quick wipe in the swipe direction
void changePage(int dir) {
  if (numCh < 2) return;
  page = (page + dir + numCh) % numCh;
  int w = gfx->width();
  for (int i = 0; i < 4; i++) {
    int x = dir > 0 ? w - (i + 1) * w / 4 : i * w / 4;
    gfx->fillRect(x, 0, w / 4, gfx->height(), C_BG);
  }
  drawPage();
}

void showAlert(const Alert &a) {
  for (int i = 0; i < 4; i++) {
    gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD);
    delay(100);
  }
  gfx->fillScreen(C_BG);
  Channel &c = ch[a.idx];
  String name = c.title.length() ? c.title : c.handle;
  centreText("NEW SUBSCRIBERS!", 10, 2, C_GOLD);
  centreText(name.length() > 26 ? name.substring(0, 25) + "." : name, 38, 2, C_WHITE);
  String d = "+" + withCommas(a.delta);
  centreText(d, 66, sizeForText(d, 5, 20), C_GREEN);
  centreText("now " + withCommas(a.total), 128, 2, C_GREY);
  unsigned long start = millis();
  while (millis() - start < ALERT_MS) { server.handleClient(); delay(20); }
}

// ════════════════════════════════════════════════════════════════════════════
//  Touch (AXS5106L) — polled, swipe detection only
// ════════════════════════════════════════════════════════════════════════════
bool touchOk = false, touching = false;
int touchStartX = 0, touchLastX = 0;
unsigned long touchStartAt = 0, lastTouchPoll = 0;

void touchInit() {
  Wire.begin(TP_SDA, TP_SCL);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(200);
  digitalWrite(TP_RST, HIGH); delay(300);
  Wire.beginTransmission(TP_ADDR);
  touchOk = (Wire.endTransmission() == 0);
  Serial.printf("Touch controller %s\n", touchOk ? "found" : "NOT found");
}

// Returns true while a finger is down; x is in screen (landscape) pixels.
bool touchRead(int &x) {
  uint8_t d[14] = {0};
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)14) != 14) return false;
  Wire.readBytes(d, 14);
  if (d[1] == 0 || d[1] > 5) return false;
  uint16_t rawY = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
  x = rawY;   // rotation 1 (landscape): screen X = panel Y
  return true;
}

// Returns -1 / +1 for a completed swipe, 0 otherwise.
int pollSwipe() {
  if (!touchOk || millis() - lastTouchPoll < 20) return 0;
  lastTouchPoll = millis();
  int x;
  bool down = touchRead(x);
  if (down) {
    if (!touching) { touching = true; touchStartX = x; touchStartAt = millis(); }
    touchLastX = x;
    return 0;
  }
  if (!touching) return 0;
  touching = false;
  int dx = touchLastX - touchStartX;
  if (millis() - touchStartAt > 1500 || abs(dx) < SWIPE_MIN_PX) return 0;
  return dx < 0 ? +1 : -1;   // swipe left → next channel, swipe right → previous
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
  // Live status of each channel (when running)
  if (!portalMode && numCh) {
    h += "<div class='st'>";
    for (int i = 0; i < numCh; i++) {
      h += "<div>" + htmlEscape(ch[i].handle) + " &rarr; <b>" +
           (ch[i].subs >= 0 ? withCommas(ch[i].subs) : String("…")) + "</b> " +
           htmlEscape(ch[i].err) + "</div>";
    }
    h += "</div>";
  }
  h += "<form method='POST' action='/save'>";
  h += "<label>YouTube channels (up to 10, one per line)</label>";
  h += "<textarea name='channels' autocapitalize='off' autocorrect='off' spellcheck='false' placeholder='@mkbhd&#10;@veritasium&#10;@yourchannel' required>" +
       htmlEscape(cfgChannels) + "</textarea>";
  h += "<small>Use @handles, UC… channel IDs or paste channel links. The first one shows first.</small>";
  h += "<label><input type='checkbox' name='auto' value='1' style='width:auto'" + String(cfgAuto ? " checked" : "") +
       "> Switch channels automatically every 10 seconds</label>";
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
  wrapText(wifiFailReason, 10, 48, wifiFailReason.length() > 100 ? 1 : 2, C_WHITE);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BG);
  gfx->setCursor(10, 132); gfx->print("Network: " + cfgSsid);
  gfx->setCursor(10, 146); gfx->print("Board MAC: " + boardMac());
  gfx->setCursor(10, 160); gfx->print("Opening setup in a few seconds...");
}

// ════════════════════════════════════════════════════════════════════════════
//  YouTube API
// ════════════════════════════════════════════════════════════════════════════
// GET a YouTube API URL into a JSON document. Returns HTTP code (negative = network problem).
int ytGet(const String &query, JsonDocument &doc) {
  String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics,snippet"
               "&fields=items(id,snippet/title,statistics(subscriberCount,hiddenSubscriberCount))"
               "&key=" + urlEncode(cfgApiKey) + "&" + query;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(12000);
  if (!http.begin(client, url)) return -100;
  int code = http.GET();
  if (code > 0) {
    String body = http.getString();
    deserializeJson(doc, body);
  }
  http.end();
  Serial.printf("YouTube API HTTP %d\n", code);
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

void applyItem(JsonObject item, int i, bool alertOnGain) {
  ch[i].id = item["id"] | ch[i].id;
  ch[i].title = item["snippet"]["title"] | ch[i].title;
  if (item["statistics"]["hiddenSubscriberCount"] | false) { ch[i].err = "Subscriber count is hidden"; return; }
  long n = String(item["statistics"]["subscriberCount"] | "-1").toInt();
  if (n < 0) return;
  if (alertOnGain && ch[i].subs >= 0 && n > ch[i].subs && numAlerts < MAX_CH)
    alerts[numAlerts++] = { i, n - ch[i].subs, n };
  ch[i].subs = n;
  ch[i].err = "";
}

// Look up @handles once to get their channel IDs (1 API unit each).
void resolveHandles() {
  for (int i = 0; i < numCh; i++) {
    if (ch[i].id.length()) continue;
    JsonDocument doc;
    int code = ytGet("forHandle=" + urlEncode(ch[i].handle), doc);
    if (code != 200) { netError = apiErrorText(code, doc); if (code < 0) return; continue; }
    JsonArray items = doc["items"];
    if (items.size() == 0) { ch[i].err = "Channel not found"; continue; }
    applyItem(items[0], i, false);
    netError = "";
  }
}

// Refresh every channel in ONE request (1 API unit total).
void fetchAll() {
  resolveHandles();                     // retries any handle that failed earlier
  String ids;
  for (int i = 0; i < numCh; i++) if (ch[i].id.length()) ids += (ids.length() ? "," : "") + ch[i].id;
  if (!ids.length()) return;
  JsonDocument doc;
  int code = ytGet("id=" + ids, doc);
  if (code != 200) { netError = apiErrorText(code, doc); return; }
  netError = "";
  lastFetchOk = millis();
  for (int i = 0; i < numCh; i++) {
    if (!ch[i].id.length()) continue;
    bool found = false;
    for (JsonObject item : doc["items"].as<JsonArray>()) {
      if (ch[i].id == (const char *)(item["id"] | "")) { applyItem(item, i, true); found = true; break; }
    }
    if (!found) ch[i].err = "Channel not found";
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
  Serial.println("SubCounter v5 starting");

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

  WiFi.onEvent(onWiFiEvent);
  loadSettings();

  if (cfgSsid.length() == 0 || cfgApiKey.length() == 0 || numCh == 0) { startPortal(); return; }
  if (!connectWiFi()) { drawWifiFailScreen(); delay(8000); startPortal(); return; }

  startSettingsServer();
  drawStatus("Loading...", String(numCh) + (numCh == 1 ? " channel" : " channels"), C_WHITE);
  fetchAll();
  lastFetch = millis();
  lastInteract = millis();
  drawPage();
  if (ch[page].subs > 0) { shownSubs = max(0L, ch[page].subs - 30); drawNumber(shownSubs); }  // count-up intro
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
  int s = pollSwipe();
  if (s) { changePage(s); lastInteract = millis(); }

  // Auto-switch (pauses for 30 s after you swipe)
  if (cfgAuto && numCh > 1 && millis() - lastInteract > 30000UL) {
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

  // Refresh all channels
  if (millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    String oldTitle = numCh ? ch[page].title : "";
    fetchAll();
    if (numAlerts) {
      for (int i = 0; i < numAlerts; i++) showAlert(alerts[i]);
      numAlerts = 0;
      drawPage();
    } else {
      if (numCh && ch[page].title != oldTitle) drawHeader();
      if (numCh && ch[page].subs < 0) drawNumber(-1);
      drawFooter();
    }
  }

  // Count-up animation on the visible channel
  if (numCh && ch[page].subs >= 0 && shownSubs != ch[page].subs) {
    long target = ch[page].subs;
    if (shownSubs < 0 || shownSubs > target) shownSubs = target;
    else { long gap = target - shownSubs; shownSubs += (gap > 20) ? gap / 8 : 1; }
    drawNumber(shownSubs);
  }

  static unsigned long lastFooter = 0;
  if (millis() - lastFooter > 60000UL) { lastFooter = millis(); drawFooter(); }

  delay(5);
}
