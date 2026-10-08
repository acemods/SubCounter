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
