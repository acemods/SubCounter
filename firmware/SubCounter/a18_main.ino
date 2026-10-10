// ════════════════════════════════════════════════════════════════════════════
//  Setup & loop
// ════════════════════════════════════════════════════════════════════════════
bool summaryActive = false;
unsigned long summaryShownAt = 0;
int lastSummaryDay = -1;
int portraitRot = 0;

// After joining Wi-Fi: the Device screen (QR code, address, version) for a few seconds.
// It checks GitHub for new firmware straight away, and the button works here too:
// if there's an update it stays up for 20 s so you can tap Install (or swipe to skip).
void showAddress() {
  if (cfgBootUpd) {                                // Settings → Updates → "Check for updates at start-up"
    devChecking = true; drawDevice();
    checkForUpdate(); lastUpdCheck = millis() | 1; // counts as today's check
    devChecking = false;
  }
  drawDevice();
  unsigned long showFor = updAvail ? 20000UL : 6000UL, start = millis();
  while (millis() - start < showFor) {
    server.handleClient();
    char s = pollSwipe();
    if (s == 'T' && tapX >= 166 && tapY >= 120) {             // the button
      if (updAvail) installGithubUpdate();                     // restarts when it works
      else { devChecking = true; drawDevice(); checkForUpdate(); devChecking = false; }
      drawDevice(); start = millis(); if (updAvail) showFor = 20000UL;
    } else if (s) break;                                       // any other touch: carry on
    if (digitalRead(BOOT_BTN) == LOW) break;                   // BOOT: carry on
    delay(10);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("SubCounter v" FW_VERSION " starting");
  setenv("TZ", TZ_UK, 1); tzset();

  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);
  pinMode(BOOT_BTN, INPUT_PULLUP);
  dataLock = xSemaphoreCreateRecursiveMutex();

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
  loadMorning();
  Serial.printf("History storage %s\n", fsOk ? "ready" : "unavailable");
  randomSeed(esp_random());

  WiFi.onEvent(onWiFiEvent);
  loadSettings();
  { prefs.begin("subcounter", false); bootWhy = prefs.getString("rstwhy", ""); if (bootWhy.length()) prefs.remove("rstwhy"); prefs.end(); }
  if (!bootWhy.length()) { esp_reset_reason_t r = esp_reset_reason();
    bootWhy = r == ESP_RST_POWERON ? "Power on" : r == ESP_RST_SW ? "Restarted by the firmware (update, settings or restart button)" :
              r == ESP_RST_PANIC ? "Crashed (software error)" : r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT ? "Watchdog (froze)" :
              r == ESP_RST_BROWNOUT ? "Power dip (brown-out)" : "Other (" + String((int)r) + ")"; }
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
  loadRecent();                         // last saved "Recent uploads", until the fresh ones arrive
  loadLatest();                         // latest-video view samples from before the restart
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
  startNetTask();
  Serial.print("Dashboard: http://"); Serial.print(WiFi.localIP()); Serial.println("  or  http://" HOSTNAME ".local");
}

// ── The network task: everything that talks to the internet on a timer ─────
void netCycle() {
  if (portalMode || netPause || pendingSwitch >= 0 || WiFi.status() != WL_CONNECTED) return;
  DataGuard g;
  if (netPause) return;
  netBusy = true; netHolding = true;
  if (updCheckReq) {                       // someone tapped "Check for update": do that first
    updCheckReq = false; lastUpdCheck = millis();
    netStage = "update check"; checkForUpdate();
  }
  netStage = "Spotify command";
  if (spPendingCmd) { char c = spPendingCmd; spPendingCmd = 0; if (cfgSpRefresh.length()) spCommandNet(c); }
  if (lastFetch == 0 || millis() - lastFetch > REFRESH_MS) {
    lastFetch = millis();
    netStage = "channel counts"; fetchAll();
    netStage = "live streams";     refreshLiveVideos();     // keeps LIVE badges and viewer counts current
    netStage = "profile pictures"; fetchAvatars();          // picks up any that failed earlier
    netRefreshed = true;
  }
  if (lastVideoFetch == 0 || millis() - lastVideoFetch > VIDEO_REFRESH_MS) {
    lastVideoFetch = millis();
    netStage = "latest videos"; fetchLatestVideos();
    vtCheckNewUpload();
    netRefreshed = true;
  }
  if (lastCommentFetch == 0 || millis() - lastCommentFetch > COMMENT_REFRESH_MS) {
    lastCommentFetch = millis();
    netStage = "comments"; fetchComments();
  }
  // your new upload: its own numbers every 5 minutes, typical views once a day
  if (vt.active && (lastVtFetch == 0 || millis() - lastVtFetch > 5UL * 60000UL)) {
    lastVtFetch = millis();
    netStage = "new video tracker"; vtPoll();
    netRefreshed = true;
  }
  if (vt.active && (lastTypicalFetch == 0 || millis() - lastTypicalFetch > 24UL * 3600000UL)) {
    lastTypicalFetch = millis();
    netStage = "typical views"; vtFetchTypical();
  }
  if (lastWxFetch == 0 || millis() - lastWxFetch > 15UL * 60000UL) {
    lastWxFetch = millis();
    netStage = "weather"; fetchWeather();
    netWxRefreshed = true;
  }
  // Spotify: every 3 s while it's on screen
  if (mode == M_NORMAL && !menuOpen && app == APP_SP && cfgSpRefresh.length() &&
      (lastSpPoll == 0 || millis() - lastSpPoll > spWait)) {
    bool wasPlaying = sp.playing, had = sp.hasTrack;
    netStage = "Spotify"; bool changed = spPoll();
    lastSpPoll = millis();
    // A failing sign-in used to be retried every 3 s, and each try freezes the board for a
    // moment, so long-presses were missed. Back off: 15 s, 30 s, 1 min ... up to 5 min.
    if (sp.err.length()) { spFails = min(spFails + 1, 6); spWait = min(300000UL, 15000UL << (spFails - 1)); }
    else { spFails = 0; spWait = sp.playing ? 3000UL : 6000UL; }
    if (changed || wasPlaying != sp.playing || had != sp.hasTrack) netSpRedraw = true;
  }

  // new firmware on GitHub? once a day (first check 2 minutes after start-up), or when asked
  if (updCheckReq || (lastUpdCheck == 0 ? millis() > 120000UL : millis() - lastUpdCheck > 24UL * 3600000UL)) {
    updCheckReq = false; lastUpdCheck = millis();
    netStage = "update check"; checkForUpdate();
  }
  static unsigned long recentWait = RECENT_REFRESH_MS;
  static bool hadSaved = false, checkedSaved = false;
  if (!checkedSaved) { checkedSaved = true; for (int i = 0; i < numCh; i++) if (ch[i].rvN) hadSaved = true; }
  if (lastRecentFetch == 0 ? millis() > (hadSaved ? 180000UL : 60000UL) : millis() - lastRecentFetch > recentWait) {
    lastRecentFetch = millis();
    netStage = "recent uploads";
    recentWait = fetchRecent() ? RECENT_REFRESH_MS : 10UL * 60000UL;   // something failed: try again in 10 min
  }
  netStage = "phone notifications"; if (noteN) sendNotes();
  netStage = "closing connections"; connCloseIdle(connYT); connCloseIdle(connTW); connCloseIdle(connSP, 30000);
  netBusy = false; netHolding = false; netStage = "idle";
}
void netTask(void *) {
  for (;;) { netCycle(); vTaskDelay(pdMS_TO_TICKS(40)); }
}
void startNetTask() {
  if (netTaskH) return;
  if (xTaskCreate(netTask, "net", 16384, nullptr, 1, &netTaskH) == pdPASS) netRunning = true;
  else { netTaskH = nullptr; Serial.println("Couldn't start the network task"); }
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

void loopBody();
const char *lastLongWhat = "drawing / inputs";
void loop() {
  static unsigned long lastEnd = 0;
  unsigned long t0 = millis();
  if (lastEnd && t0 - lastEnd > 400 && !portalMode) noteStall(t0 - lastEnd, String("main loop starved, network: ") + (const char *)netStage);
  bool held = netHolding; const char *stage = (const char *)netStage;
  {
    DataGuard g;                    // screen, touch and web pages; the network task fills in the data
    unsigned long waited = millis() - t0;
    if (waited > 400 && !portalMode) noteStall(waited, String("waiting for network task: ") + (held ? "" : "(released) ") + stage);
    unsigned long t1 = millis();
    loopBody();
    unsigned long took = millis() - t1;
    if (took > 400 && !portalMode && !lastLongIsWeb) noteStall(took, String("screen: ") + lastLongWhat);
    lastLongIsWeb = false; lastLongWhat = "drawing / inputs";
  }
  delay(2);
  lastEnd = millis();
}

void memRestartCheck() {
  // Memory too fragmented for secure connections (YouTube keeps failing): a quick restart clears
  // it. Only after 30 min up, when nobody's touched the board for a minute. Everything that
  // matters (history, summary, recent uploads, settings) is saved, so nothing is lost.
  if (!memRestartDue || millis() < 1800000UL || millis() - lastInteract < 60000UL) return;
  prefs.begin("subcounter", false);
  prefs.putString("rstwhy", "Memory was too fragmented to connect (biggest block " + String(ESP.getMaxAllocHeap()) +
                  ", " + String(millis() / 3600000UL) + " h up)");
  prefs.end();
  setBacklight(BL_NORMAL);
  drawStatus("Tidying up memory", "back in a few seconds", C_GREY);
  delay(800);
  ESP.restart();
}

void loopBody() {
  memRestartCheck();
  { unsigned long t = millis(); server.handleClient(); unsigned long d = millis() - t;
    if (d > 400 && !portalMode && !lastLongIsWeb) { noteStall(d, "web connection (slow or empty request)"); lastLongIsWeb = true; } }
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

  bool bootShort = false, bootMenu = false;
  if (digitalRead(BOOT_BTN) == LOW) {
    if (!btnDownAt) btnDownAt = millis();
    if (millis() - btnDownAt > HOLD_FOR_SETUP_MS) { server.stop(); startPortal(); btnDownAt = 0; return; }
  } else if (btnDownAt) {
    unsigned long held = millis() - btnDownAt;
    if (held > 900) bootMenu = true;            // held about 1 s (but not 3 s): home menu
    else if (held > 40) bootShort = true;
    btnDownAt = 0;
  }

  // ── daily summary (9 am unless changed in settings) ───────────────────────
  if (timeValid()) {
    time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
    int sh = cfgSummary ? cfgSumHour : 9;
    if (cfgSummary && lt.tm_hour == sh && lt.tm_min < 30 && lt.tm_yday != lastSummaryDay) {
      lastSummaryDay = lt.tm_yday;
      summaryActive = true; summaryShownAt = millis();
    }
    // keep a copy for the dashboard: once a day at the summary time (or as soon as there's data after it)
    static int capturedDay = -2;
    if (capturedDay == -2) {                     // first time: is the saved one already from today?
      capturedDay = -1;
      if (morningJson.length()) {
        JsonDocument d; if (!deserializeJson(d, morningJson)) {
          time_t at = d["at"] | 0L; struct tm a; localtime_r(&at, &a);
          if (a.tm_yday == lt.tm_yday && a.tm_year == lt.tm_year && a.tm_hour >= sh) capturedDay = lt.tm_yday;
        }
      }
    }
    bool anyStats = false; for (int i = 0; i < numCh; i++) if (ch[i].statsOk) anyStats = true;
    if (lt.tm_hour >= sh && capturedDay != lt.tm_yday && anyStats) { capturedDay = lt.tm_yday; captureMorning(); }
  }
  if (summaryActive && millis() - summaryShownAt > SUMMARY_SHOW_MS) summaryActive = false;

  // ── decide the mode ───────────────────────────────────────────────────────
  if (bootMenu) swipe = 'H';                    // same as a long-press on the screen
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
    if (deviceOpen && (swipe == 'H' || swipe == 'L' || swipe == 'R' || swipe == 'U' || swipe == 'D' || bootShort)) {
      deviceOpen = false; menuOpen = true;                  // back to the home menu
      gfx->fillScreen(C_BG); drawMenu(); swipe = 0; bootShort = false;
    }
    if (swipe == 'H') {                                     // long-press: home menu (or close it)
      menuOpen = !menuOpen;
      if (menuOpen) menuPage = 0;                           // always open on the apps page
      gfx->fillScreen(C_BG); drawApp();
      swipe = 0;
    }
    if (deviceOpen) {
      if (swipe == 'T' && tapX >= 166 && tapY >= 120) {      // the button
        if (updAvail) { updInstallReq = true; }
        else if (!devChecking) { devChecking = true; devCheckSeq = updCheckSeq; devCheckAt = millis(); updCheckReq = true; drawDevice(); }
      }
      if (devChecking && updCheckSeq != devCheckSeq) { devChecking = false; drawDevice(); }
      else if (devChecking && millis() - devCheckAt > 30000UL) {           // no answer: don't spin forever
        devChecking = false; updErr = "No reply - weak Wi-Fi? Tap to try again"; drawDevice();
      }
      if (millis() - lastInteract > 60000UL) openApp(app);  // left open: go back
    }
    else if (menuOpen) {
      if (swipe == 'T') {
        int a = menuPage * MENU_PER_PAGE + constrain(tapX * MENU_PER_PAGE / gfx->width(), 0, MENU_PER_PAGE - 1);
        if (a < NUM_APPS) openApp(a);
        else if (a == 3) openDevice();
        else if (a == 4) {                                    // Summary: today's summary for 10 minutes
          menuOpen = false; deviceOpen = false; lastInteract = millis();
          summaryActive = true; summaryShownAt = millis();
        }
      } else if ((swipe == 'L' || swipe == 'R') && menuPages() > 1) {
        menuPage = (menuPage + (swipe == 'L' ? 1 : menuPages() - 1)) % menuPages();
        gfx->fillScreen(C_BG); drawMenu();
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
      if (cfgSpRefresh.length() && (swipe == 'T' || swipe == 'L' || swipe == 'R' || swipe == 'U' || swipe == 'D')) spPendingCmd = swipe;
      if (bootShort && cfgSpRefresh.length()) spPendingCmd = 'T';
    }
  }

  // ── network ───────────────────────────────────────────────────────────────
  if (pendingSwitch >= 0) {
    int k = pendingSwitch;
    netQuiesce();
    delay(500);                                   // let the browser get its reply
    if (connectOne(k)) curNet = k;
    else if (!connectWiFi()) { drawWifiFailScreen(); delay(5000); }
    pendingSwitch = -1; netPause = false;
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
      netQuiesce();
      if (connectWiFi()) { wifiLostAt = 0; netError = ""; lastFetch = 0; gfx->fillScreen(C_BG); drawMode(); }
      else wifiLostAt = millis();
      netPause = false;
      return;
    }
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) { lastReconnect = millis(); WiFi.reconnect(); }   // no waiting here: the screen keeps working
  } else wifiLostAt = 0;

  // firmware update from GitHub: asked for on the Update page, or automatically at 3 am
  if (updInstallReq) { updInstallReq = false; installGithubUpdate(); return; }
  if (cfgAutoUpd && updAvail && timeValid()) {
    static int autoDay = -1;
    time_t n = nowT(); struct tm lt; localtime_r(&n, &lt);
    if (lt.tm_hour == 3 && lt.tm_yday != autoDay) { autoDay = lt.tm_yday; installGithubUpdate(); return; }
  }

  // results from the network task
  bool refreshed = netRefreshed; netRefreshed = false;
  bool wxRefreshed = netWxRefreshed; netWxRefreshed = false;
  if (netSpRedraw) {
    netSpRedraw = false;
    if (mode == M_NORMAL && !menuOpen && app == APP_SP) {
      if (spToastMsg.length()) { spToast(spToastMsg); spToastMsg = ""; waitShowing(900); }
      drawSpotify();
    }
  }

  if (refreshed) {
    // celebrate only when someone can see it (not face-down, side-on or at night)
    if (numAlerts && (mode == M_NORMAL || mode == M_SUMMARY || mode == M_CLOCK) && !(app == APP_SP && !menuOpen && sp.playing)) {
      setBacklight(BL_NORMAL);
      // copy them first: new alerts can arrive while these are on screen
      Alert show[MAX_CH * 2]; int n = numAlerts;
      memcpy(show, alerts, sizeof(Alert) * n); numAlerts = 0;
      for (int i = 0; i < n; i++) showAlert(show[i]);
      if (jumpToCh >= 0 && app == APP_YT && !menuOpen) { page = jumpToCh; card = 0; board = false; }
      jumpToCh = -1;
      gfx->fillScreen(C_BG);
      drawMode();
    } else numAlerts = 0;            // nobody can see them right now (face-down, night, music): drop them
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
