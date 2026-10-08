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

void drawOtaProgress(const String &line2);
void sendMessage(int code, const String &title, const String &body);
// ── Updates from GitHub ─────────────────────────────────────────────────────
// builds/latest/version.json in the repository says which version is newest:
//   { "version": "11.1", "app": "SubCounter-v11.1-APP-ONLY.bin", "size": 1717504, "md5": "…", "notes": "…" }
// The network task checks it once a day; installing happens on the main task.
String updVer, updFile, updMd5, updNotes, updErr;
long updSize = 0;
bool updAvail = false;
unsigned long lastUpdCheck = 0;
time_t updCheckedAt = 0;
bool updCheckedOnce = false;
volatile uint32_t updCheckSeq = 0;  // goes up every time a check finishes (worked or not)        // set even before the clock is known (start-up check)
volatile bool updCheckReq = false, updInstallReq = false;

// "11.10" > "11.9" > "11.1"
bool versionNewer(const String &a, const String &b) {
  int ai = 0, bi = 0;
  while (ai < (int)a.length() || bi < (int)b.length()) {
    long x = 0, y = 0;
    while (ai < (int)a.length() && isdigit((unsigned char)a[ai])) x = x * 10 + (a[ai++] - '0');
    while (bi < (int)b.length() && isdigit((unsigned char)b[bi])) y = y * 10 + (b[bi++] - '0');
    if (x != y) return x > y;
    if (ai < (int)a.length()) ai++;
    if (bi < (int)b.length()) bi++;
  }
  return false;
}

// Runs on the network task
void checkForUpdateOnce();
void checkForUpdate() { checkForUpdateOnce(); updCheckSeq++; }
void checkForUpdateOnce() {
  if (!cfgUpdUrl.startsWith("https://")) { updErr = "Update address isn't set"; return; }
  String url = cfgUpdUrl + "version.json", body;
  int code = httpsCall(connGH, "GET", url, nullptr, 0, &body);
  connClose(connGH);
  updCheckedAt = timeValid() ? nowT() : 0;
  updCheckedOnce = true;
  if (code != 200) { updErr = "Couldn't check for updates (" + String(code) + ")"; return; }
  JsonDocument doc;
  if (deserializeJson(doc, body)) { updErr = "Update info unreadable"; return; }
  updErr = "";
  updVer = doc["version"] | ""; updFile = doc["app"] | ""; updMd5 = doc["md5"] | ""; updNotes = doc["notes"] | "";
  updSize = doc["size"] | 0L;
  updAvail = updVer.length() && updFile.length() && versionNewer(updVer, FW_VERSION);
}

// Runs on the main task: download straight into the spare app slot, then restart
void installGithubUpdate() {
  if (!updAvail) return;
  netQuiesce();
  setBacklight(BL_NORMAL);
  if (mode == M_PORTRAIT) gfx->setRotation(1);
  drawStatus("Updating...", "to v" + updVer, C_GOLD);
  String err;
  {
    NetIO io;                                   // let go of the data: nothing else runs while we install
    WiFiClientSecure client; client.setInsecure();
    HTTPClient http; http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    if (!http.begin(client, cfgUpdUrl + updFile)) err = "Bad update address";
    else {
      int code = http.GET();
      int len = http.getSize();
      if (code != 200) err = "Download failed (" + String(code) + ")";
      else if (len <= 0 || (updSize > 0 && len != updSize)) err = "Unexpected file size";
      else if (!Update.begin(len, U_FLASH)) err = String("Can't start: ") + Update.errorString();
      else {
        if (updMd5.length() == 32) Update.setMD5(updMd5.c_str());
        WiFiClient *st = http.getStreamPtr();
        static uint8_t buf[2048];
        int got = 0, lastPct = -1; unsigned long lastData = millis();
        while (got < len && millis() - lastData < 15000) {
          int a = st->available();
          if (a <= 0) { if (!st->connected()) break; delay(2); continue; }
          int n = st->readBytes(buf, min(a, (int)sizeof(buf)));
          if (got == 0 && !(n > 36 && buf[0] == 0xE9 && buf[32] == 0x32 && buf[33] == 0x54 && buf[34] == 0xCD && buf[35] == 0xAB)) {
            err = "That isn't an APP-ONLY firmware file"; break;
          }
          if (Update.write(buf, n) != (size_t)n) { err = String("Write failed: ") + Update.errorString(); break; }
          got += n; lastData = millis();
          int pct = (int)((long long)got * 100 / len);
          if (pct != lastPct && pct % 5 == 0) { lastPct = pct; drawOtaProgress(String(pct) + "%"); }
        }
        if (!err.length() && got != len) err = "Download stopped at " + String(got * 100 / len) + "%";
        if (!err.length() && !Update.end(true)) err = String("Check failed: ") + Update.errorString();
        if (err.length()) Update.abort();
      }
    }
    http.end();
  }
  if (!err.length()) {
    drawStatus("Updated!", "Restarting...", C_GREEN);
    delay(1500);
    ESP.restart();
  }
  updErr = err;
  drawStatus("Update failed", err, C_RED);
  delay(4000);
  netPause = false;
  gfx->fillScreen(C_BG);
  drawMode();
}

String updStatusHtml() {
  String h = "<div style='background:#111;border:1px solid #333;border-radius:12px;padding:14px;margin:12px 0'>";
  h += "<b>From GitHub</b><br>";
  if (updAvail) {
    h += "<span style='color:#30d158'>Version <b>v" + htmlEscape(updVer) + "</b> is available.</span>";
    if (updNotes.length()) h += "<small>" + htmlEscape(updNotes) + "</small>";
    h += "<form method='POST' action='/update/github'><button type='submit' style='background:#30d158;margin-top:12px'>Install v" + htmlEscape(updVer) + " now</button></form>";
  } else {
    h += String(updCheckedAt ? "You're up to date." : "Not checked yet.");
    if (updCheckedAt) h += " <small style='display:inline'>(checked " + ago(updCheckedAt) + ")</small>";
  }
  if (updErr.length()) h += "<small style='color:#e62117'>" + htmlEscape(updErr) + "</small>";
  h += "<form method='POST' action='/update/check'><button type='submit' style='background:#2a2a2e;margin-top:10px;font-size:15px;padding:10px'>Check now</button></form>";
  h += "</div><p style='margin-top:18px'><b>Or upload a file</b></p>";
  return h;
}

void handleUpdatePage() {
  if (!authed()) return;
  String b = FPSTR(UPDATE_BODY);
  b.replace("<h1>&#11014; Update firmware</h1>", "<h1>&#11014; Update firmware</h1><p>Currently running <b>v" FW_VERSION "</b></p>" + updStatusHtml());
  server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + b);
}
void handleUpdateCheck() {
  if (!authed()) return;
  updCheckReq = true;
  server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>Checking&hellip;</h1><p>Looking for a new version on GitHub.</p>"
              "<script>setTimeout(()=>location.href='/update',5000)</script></div></body></html>");
}
void handleUpdateGithub() {
  if (!authed()) return;
  if (!updAvail) { sendMessage(400, "No update available", "You're already on the newest version."); return; }
  server.send(200, "text/html", String(FPSTR(PAGE_HEAD)) + "<h1>Installing v" + htmlEscape(updVer) + "&hellip;</h1>"
              "<p>Watch the board's screen. It restarts by itself in about a minute, and this page then goes back to the dashboard.</p>"
              "<script>setTimeout(()=>location.href='/',75000)</script></div></body></html>");
  updInstallReq = true;
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
    netPause = true;                     // keep the network task quiet while we write flash
    setBacklight(BL_NORMAL);
    if (mode == M_PORTRAIT) gfx->setRotation(1);
    drawStatus("Updating...", "receiving", C_GOLD);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) { otaErr = String("Can't start: ") + Update.errorString(); netPause = false; }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (otaErr.length()) return;
    if (otaBytes == 0) {
      // An app image carries the ESP-IDF app description (magic 0xABCD5432) at byte 32;
      // the FULL image starts with the bootloader instead, which must not go here.
      bool isApp = up.currentSize > 36 && up.buf[0] == 0xE9 &&
                   up.buf[32] == 0x32 && up.buf[33] == 0x54 && up.buf[34] == 0xCD && up.buf[35] == 0xAB;
      if (!isApp) { otaErr = "That isn't an APP-ONLY firmware file."; Update.abort(); netPause = false; return; }
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
    netPause = false;
  }
}

void handleUpdateDone() {
  if (!authed()) return;
  if (!otaOk) netPause = false;
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
