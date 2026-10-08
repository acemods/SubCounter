// ════════════════════════════════════════════════════════════════════════════
//  Small helpers
// ════════════════════════════════════════════════════════════════════════════

// ── Colour themes and brightness ────────────────────────────────────────────
struct Theme { const char *name; uint16_t accent, grey, dkgrey; const char *css; };
const Theme THEMES[] = {
  { "Classic (gold)",  0xFEA0, 0x8410, 0x39E7, "#ffc53d" },
  { "YouTube red",     0xF8E3, 0x8410, 0x39E7, "#ff3b30" },
  { "Twitch purple",   0x9C1F, 0x8410, 0x39E7, "#a48aff" },
  { "Ocean",           0x367F, 0x8410, 0x39E7, "#33ccff" },
  { "Mint",            0x3EF2, 0x8410, 0x39E7, "#3ddc97" },
  { "High contrast",   0xFFE0, 0xC618, 0x8410, "#ffe600" },
};
const int NUM_THEMES = sizeof(THEMES) / sizeof(THEMES[0]);
void applyTheme() {
  cfgTheme = constrain(cfgTheme, 0, NUM_THEMES - 1);
  C_GOLD = THEMES[cfgTheme].accent; C_GREY = THEMES[cfgTheme].grey; C_DKGREY = THEMES[cfgTheme].dkgrey;
  cfgBright = constrain(cfgBright, 10, 100); cfgNightBright = constrain(cfgNightBright, 1, 40);
  BL_NORMAL = cfgBright * 255 / 100; BL_NIGHT = max(3, cfgNightBright * 255 / 100);
}
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
