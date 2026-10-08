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
