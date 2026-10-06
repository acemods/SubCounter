/*
 * SubCounter — YouTube subscriber counter for Waveshare ESP32-C6-Touch-LCD-1.47
 *
 * FIRST RUN / SETUP
 *   1. The board creates a Wi-Fi network called "SubCounter-Setup".
 *   2. Join it from your phone or Mac. The setup page should pop up by itself;
 *      if it doesn't, open http://192.168.4.1 in a browser.
 *   3. Pick your Wi-Fi, enter the password, your YouTube channel (@handle or
 *      UC... channel ID) and your YouTube Data API key, then press Save.
 *   4. The board restarts, joins your Wi-Fi and shows your subscriber count.
 *
 * CHANGING SETTINGS LATER
 *   - While running, open http://<board IP shown on screen> to edit settings, or
 *   - Hold the BOOT button for 3 seconds to go back into setup mode.
 *
 * Settings are stored in the board's own flash memory (NVS), nowhere else.
 *
 * Arduino IDE: Board "ESP32C6 Dev Module", USB CDC On Boot "Enabled",
 *              library "GFX Library for Arduino" 1.6.x
 */

#include <Arduino_GFX_Library.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// ── Pins (ESP32-C6 version of the board) ────────────────────────────────────
#define LCD_SCK   1
#define LCD_MOSI  2
#define LCD_CS    14
#define LCD_DC    15
#define LCD_RST   22
#define LCD_BL    23
#define SD_CS     4
#define BOOT_BTN  9

// ── Settings ────────────────────────────────────────────────────────────────
#define AP_NAME            "SubCounter-Setup"
#define REFRESH_MS         (5UL * 60UL * 1000UL)   // fetch every 5 minutes
#define WIFI_TIMEOUT_MS    20000UL
#define HOLD_FOR_SETUP_MS  3000UL

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

// ── State ───────────────────────────────────────────────────────────────────
Preferences prefs;
WebServer server(80);
DNSServer dns;

String cfgSsid, cfgPass, cfgChannel, cfgApiKey;
bool portalMode = false;
String scanOptions;                 // <option> list of nearby networks

long subs = -1;                     // -1 = not fetched yet
long shownSubs = -1;
String channelTitle = "";
String lastError = "";
unsigned long lastFetch = 0;
unsigned long lastFetchOk = 0;
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
//  Drawing helpers
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

// Pick the biggest text size that fits the number on screen.
uint8_t sizeForNumber(const String &s) {
  for (uint8_t sz = 7; sz > 2; sz--) {
    if ((int)s.length() * 6 * sz <= gfx->width() - 16) return sz;
  }
  return 2;
}

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
  gfx->fillRect(0, 0, gfx->width(), 52, C_BG);
  gfx->fillRoundRect(12, 12, 40, 28, 8, C_RED);
  gfx->fillTriangle(26, 18, 26, 34, 40, 26, C_WHITE);
  gfx->setTextColor(C_WHITE, C_BG);
  gfx->setCursor(62, 13);
  gfx->setTextSize(2);
  String t = channelTitle.length() ? channelTitle : String("SUBSCRIBERS");
  if (t.length() > 20) t = t.substring(0, 19) + ".";
  gfx->print(t);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BG);
  gfx->setCursor(62, 33);
  gfx->print(channelTitle.length() ? "subscribers" : "");
  gfx->drawFastHLine(12, 50, gfx->width() - 24, C_DKGREY);
}

void drawNumber(long n) {
  gfx->fillRect(0, 58, gfx->width(), 80, C_BG);
  if (n < 0) { centreText("...", 80, 5, C_GREY); return; }
  String s = withCommas(n);
  uint8_t sz = sizeForNumber(s);
  centreText(s, 98 - (sz * 8) / 2, sz, C_WHITE);
}

void drawFooter() {
  gfx->fillRect(0, 148, gfx->width(), 24, C_BG);
  gfx->setTextSize(1);
  gfx->setCursor(12, 158);
  if (lastError.length()) {
    gfx->setTextColor(C_RED, C_BG);
    gfx->print(lastError.substring(0, 50));
    return;
  }
  gfx->setTextColor(C_GREY, C_BG);
  if (lastFetchOk) {
    unsigned long mins = (millis() - lastFetchOk) / 60000UL;
    gfx->print(mins == 0 ? String("Updated just now") : "Updated " + String(mins) + " min ago");
  }
  String ip = WiFi.localIP().toString();
  gfx->setCursor(gfx->width() - 12 - ip.length() * 6, 158);
  gfx->print(ip);
}

void drawCounterScreen() {
  gfx->fillScreen(C_BG);
  drawHeader();
  drawNumber(shownSubs);
  drawFooter();
}

void celebrate(long n) {
  for (int i = 0; i < 4; i++) {
    gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD);
    delay(110);
  }
  gfx->fillScreen(C_BG);
  centreText("NEW SUBS!", 35, 3, C_GOLD);
  String s = withCommas(n);
  centreText(s, 80, sizeForNumber(s) > 5 ? 5 : sizeForNumber(s), C_WHITE);
  delay(2000);
  drawCounterScreen();
}

// ════════════════════════════════════════════════════════════════════════════
//  Settings storage
// ════════════════════════════════════════════════════════════════════════════
void loadSettings() {
  prefs.begin("subcounter", true);
  cfgSsid    = prefs.getString("ssid", "");
  cfgPass    = prefs.getString("pass", "");
  cfgChannel = prefs.getString("channel", "");
  cfgApiKey  = prefs.getString("apikey", "");
  prefs.end();
}

void saveSettings() {
  prefs.begin("subcounter", false);
  prefs.putString("ssid", cfgSsid);
  prefs.putString("pass", cfgPass);
  prefs.putString("channel", cfgChannel);
  prefs.putString("apikey", cfgApiKey);
  prefs.end();
}

// ════════════════════════════════════════════════════════════════════════════
//  Web pages
// ════════════════════════════════════════════════════════════════════════════
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

const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SubCounter setup</title><style>
body{font-family:-apple-system,system-ui,sans-serif;background:#111;color:#eee;margin:0;padding:20px}
.card{max-width:420px;margin:0 auto;background:#1c1c1e;border-radius:14px;padding:22px}
h1{font-size:22px;margin:0 0 4px}p{color:#999;font-size:14px;margin:0 0 18px}
label{display:block;font-size:13px;color:#aaa;margin:14px 0 6px}
input,select{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;border:1px solid #333;background:#000;color:#fff;font-size:16px}
button{width:100%;margin-top:22px;padding:14px;border:0;border-radius:10px;background:#e62117;color:#fff;font-size:17px;font-weight:600}
small{display:block;color:#777;font-size:12px;margin-top:6px}a{color:#4ea1ff}
</style></head><body><div class="card">)HTML";

void handleRoot() {
  String h = FPSTR(PAGE_HEAD);
  h += "<h1>&#9654; SubCounter setup</h1><p>Enter your details. They're saved on the board only.</p>";
  h += "<form method='POST' action='/save'>";
  h += "<label>Wi-Fi network</label>";
  if (scanOptions.length()) {
    h += "<select name='ssid_sel' onchange=\"document.getElementById('s').value=this.value\">";
    h += "<option value=''>Choose a network…</option>" + scanOptions + "</select>";
    h += "<small>Or type it below:</small>";
  }
  h += "<input id='s' name='ssid' value='" + htmlEscape(cfgSsid) + "' placeholder='Network name' required>";
  h += "<label>Wi-Fi password</label>";
  h += "<input name='pass' type='password' placeholder='";
  h += cfgPass.length() ? "(unchanged — leave blank to keep)" : "Password";
  h += "'>";
  h += "<label>YouTube channel</label>";
  h += "<input name='channel' value='" + htmlEscape(cfgChannel) + "' placeholder='@yourhandle or UC… channel ID' required>";
  h += "<label>YouTube Data API key</label>";
  h += "<input name='apikey' placeholder='";
  h += cfgApiKey.length() ? "(saved — leave blank to keep)" : "AIza…";
  h += "'><small>Free from Google Cloud Console → enable “YouTube Data API v3” → Credentials → Create API key.</small>";
  h += "<button type='submit'>Save &amp; restart</button></form></div></body></html>";
  server.send(200, "text/html", h);
}

void handleSave() {
  String ssid = server.arg("ssid");   ssid.trim();
  String pass = server.arg("pass");
  String ch   = server.arg("channel"); ch.trim();
  String key  = server.arg("apikey");  key.trim();

  if (ssid.length() == 0 || ch.length() == 0 || (key.length() == 0 && cfgApiKey.length() == 0)) {
    server.send(400, "text/html", String(FPSTR(PAGE_HEAD)) +
      "<h1>Missing details</h1><p>Wi-Fi name, channel and API key are all needed.</p>"
      "<a href='/'>Go back</a></div></body></html>");
    return;
  }
  if (ssid != cfgSsid || pass.length()) cfgPass = pass;   // new network → take new password
  cfgSsid = ssid;
  cfgChannel = ch;
  if (key.length()) cfgApiKey = key;
  saveSettings();

  server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) +
    "<h1>Saved &#10003;</h1><p>The board is restarting and will join <b>" + htmlEscape(cfgSsid) +
    "</b>. You can reconnect your phone to your normal Wi-Fi now.</p></div></body></html>");
  drawStatus("Saved!", "Restarting...", C_GREEN);
  delay(1500);
  ESP.restart();
}

void handleNotFound() {   // captive portal: send everything to the setup page
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
    scanOptions += "<option value=\"" + htmlEscape(s) + "\">" + htmlEscape(s) +
                   " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
}

void startPortal() {
  portalMode = true;
  Serial.println("Starting setup portal");
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
//  YouTube
// ════════════════════════════════════════════════════════════════════════════
String urlEncode(const String &s) {
  String o; const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else { o += '%'; o += hex[(c >> 4) & 0xF]; o += hex[c & 0xF]; }
  }
  return o;
}

// Pull "key": "value" out of the JSON without needing a JSON library.
String jsonValue(const String &json, const String &key) {
  int k = json.indexOf("\"" + key + "\"");
  if (k < 0) return "";
  int colon = json.indexOf(':', k);
  int q1 = json.indexOf('"', colon + 1);
  int q2 = json.indexOf('"', q1 + 1);
  if (colon < 0 || q1 < 0 || q2 < 0) return "";
  return json.substring(q1 + 1, q2);
}

bool fetchSubscribers() {
  String ch = cfgChannel;
  String param;
  if (ch.startsWith("UC") && ch.length() == 24) param = "id=" + urlEncode(ch);
  else {
    if (!ch.startsWith("@")) ch = "@" + ch;
    param = "forHandle=" + urlEncode(ch);
  }
  String url = "https://www.googleapis.com/youtube/v3/channels?part=statistics,snippet&" +
               param + "&key=" + urlEncode(cfgApiKey);

  WiFiClientSecure client;
  client.setInsecure();               // simple: skip certificate checking
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) { lastError = "Couldn't start request"; return false; }
  int code = http.GET();
  String body = http.getString();
  http.end();
  Serial.printf("YouTube API HTTP %d\n", code);

  if (code != 200) {
    String msg = jsonValue(body, "message");
    lastError = "API error " + String(code) + (msg.length() ? ": " + msg : "");
    return false;
  }
  String count = jsonValue(body, "subscriberCount");
  if (count.length() == 0) {
    lastError = body.indexOf("\"hiddenSubscriberCount\": true") >= 0
              ? "Subscriber count is hidden" : "Channel not found";
    return false;
  }
  channelTitle = jsonValue(body, "title");
  subs = count.toInt();
  lastError = "";
  lastFetchOk = millis();
  Serial.printf("%s: %ld subscribers\n", channelTitle.c_str(), subs);
  return true;
}

// ════════════════════════════════════════════════════════════════════════════
//  Normal mode
// ════════════════════════════════════════════════════════════════════════════
bool connectWiFi() {
  drawStatus("Connecting...", cfgSsid, C_WHITE);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfgSsid.c_str(), cfgPass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(250);
    if (digitalRead(BOOT_BTN) == LOW) return false;   // BOOT pressed → go to setup
  }
  return WiFi.status() == WL_CONNECTED;
}

void startSettingsServer() {   // lets you change settings from the board's IP later
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.begin();
}

// ════════════════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SubCounter starting");

  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);
  pinMode(BOOT_BTN, INPUT_PULLUP);

  gfx->begin();
  lcdRegInit();
  gfx->setRotation(1);
  gfx->fillScreen(C_BG);
  ledcAttach(LCD_BL, 5000, 8);
  ledcWrite(LCD_BL, 160);

  loadSettings();

  if (cfgSsid.length() == 0 || cfgApiKey.length() == 0 || cfgChannel.length() == 0) {
    startPortal();
    return;
  }
  if (!connectWiFi()) {
    drawStatus("Wi-Fi failed", "Opening setup...", C_RED);
    delay(1500);
    startPortal();
    return;
  }
  Serial.print("Connected, IP "); Serial.println(WiFi.localIP());
  startSettingsServer();
  drawCounterScreen();
  fetchSubscribers();
  lastFetch = millis();
  if (subs >= 0) shownSubs = max(0L, subs - 30);   // little count-up on start
  drawCounterScreen();
}

void loop() {
  server.handleClient();
  if (portalMode) { dns.processNextRequest(); delay(2); return; }

  // Hold BOOT for 3 s → setup mode
  if (digitalRead(BOOT_BTN) == LOW) {
    if (!btnDownAt) btnDownAt = millis();
    if (millis() - btnDownAt > HOLD_FOR_SETUP_MS) {
      server.stop();
      startPortal();
      btnDownAt = 0;
      return;
    }
  } else btnDownAt = 0;

  // Reconnect if Wi-Fi drops
  if (WiFi.status() != WL_CONNECTED) {
    lastError = "Wi-Fi lost, reconnecting...";
    drawFooter();
    WiFi.reconnect();
    delay(3000);
    return;
  }

  // Fetch periodically
  if (millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    long before = subs;
    String oldTitle = channelTitle;
    fetchSubscribers();
    if (channelTitle != oldTitle) drawHeader();
    if (before >= 0 && subs > before) celebrate(subs);
    drawFooter();
  }

  // Count-up animation
  if (subs >= 0 && shownSubs != subs) {
    if (shownSubs < 0 || shownSubs > subs) shownSubs = subs;
    else {
      long gap = subs - shownSubs;
      shownSubs += (gap > 20) ? gap / 8 : 1;
    }
    drawNumber(shownSubs);
  }

  // Refresh "updated X min ago" once a minute
  static unsigned long lastFooter = 0;
  if (millis() - lastFooter > 60000UL) { lastFooter = millis(); drawFooter(); }

  delay(30);
}
