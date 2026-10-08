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
  netQuiesce();
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
