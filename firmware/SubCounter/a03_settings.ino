// ════════════════════════════════════════════════════════════════════════════
//  Settings
// ════════════════════════════════════════════════════════════════════════════
// "SomeStreamer", "twitch.tv/somestreamer", "https://www.twitch.tv/somestreamer/videos", "@somestreamer" -> "somestreamer"
String twNameFrom(String x) {
  x.trim(); x.toLowerCase();
  int p = x.indexOf("twitch.tv/"); if (p >= 0) x = x.substring(p + 10);
  if (x.startsWith("twitch:")) x = x.substring(7);
  if (x.startsWith("@")) x = x.substring(1);
  int e = 0; while (e < (int)x.length() && (isalnum((unsigned char)x[e]) || x[e] == '_')) e++;
  return x.substring(0, e);
}
bool isTwitchLine(String x) { x.trim(); x.toLowerCase(); return x.indexOf("twitch.tv/") >= 0 || x.startsWith("twitch:"); }
// Split a pasted list: YouTube lines stay in yt, Twitch lines become plain names in tw
void splitLists(const String &in, String &yt, String &tw) {
  String l = in + "\n"; l.replace(",", "\n"); l.replace("\r", "");
  int st = 0;
  while (true) {
    int nl = l.indexOf('\n', st); if (nl < 0) break;
    String x = l.substring(st, nl); x.trim(); st = nl + 1;
    if (!x.length()) continue;
    if (isTwitchLine(x)) { String n = twNameFrom(x); if (n.length()) tw += n + "\n"; }
    else yt += x + "\n";
  }
}
String normTwitchList(const String &in) {
  String out, l = in + "\n"; l.replace(",", "\n"); l.replace("\r", "");
  int st = 0;
  while (true) {
    int nl = l.indexOf('\n', st); if (nl < 0) break;
    String n = twNameFrom(l.substring(st, nl)); st = nl + 1;
    if (n.length() && out.indexOf(n + "\n") != 0 && out.indexOf("\n" + n + "\n") < 0) out += n + "\n";
  }
  out.trim();
  return out;
}

void parseChannels() {
  numCh = 0;
  String list = cfgChannels + "\n";
  { String t = cfgTwitch + "\n"; int st = 0;            // Twitch names from their own box
    while (true) { int nl = t.indexOf('\n', st); if (nl < 0) break; String n = t.substring(st, nl); n.trim(); st = nl + 1;
                   if (n.length()) list += "twitch:" + n + "\n"; } }
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
  cfgTwitch = prefs.getString("twch", "");
  if (cfgChannels.indexOf("twitch") >= 0) {           // older versions mixed Twitch into the YouTube list
    String yt, tw; splitLists(cfgChannels, yt, tw);
    yt.trim(); cfgChannels = yt; cfgTwitch = normTwitchList(cfgTwitch + "\n" + tw);
  }
  cfgApiKey   = prefs.getString("apikey", "");
  cfgAuto     = prefs.getBool("auto", false);
  cfgEst      = prefs.getBool("est", true);
  cfgCelebrate = prefs.getBool("celebrate", true);
  cfgPin = prefs.getString("pin", "");
  cfgTwId = prefs.getString("twid", "");
  cfgTwSecret = prefs.getString("twsec", "");
  cfgLiveAlert = prefs.getBool("livealert", true);
  cfgAutoUpd = prefs.getBool("autoupd", false);
  cfgBootUpd = prefs.getBool("bootupd", true);
  cfgNtfy = prefs.getString("ntfy", "");
  cfgNtfyServer = prefs.getString("ntfysrv", cfgNtfyServer);
  cfgNtfyMask = prefs.getInt("ntfymask", cfgNtfyMask);
  cfgTheme = prefs.getInt("theme", 0);
  cfgBright = prefs.getInt("bright", 63);
  cfgNightBright = prefs.getInt("nbright", 7);
  applyTheme();
  cfgUpdUrl = prefs.getString("updurl", cfgUpdUrl);
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
  prefs.putString("twch", cfgTwitch);
  prefs.putString("apikey", cfgApiKey);
  prefs.putBool("auto", cfgAuto);
  prefs.putBool("est", cfgEst);
  prefs.putBool("celebrate", cfgCelebrate);
  prefs.putString("pin", cfgPin);
  prefs.putString("twid", cfgTwId);
  prefs.putString("twsec", cfgTwSecret);
  prefs.putBool("livealert", cfgLiveAlert);
  prefs.putBool("autoupd", cfgAutoUpd);
  prefs.putBool("bootupd", cfgBootUpd);
  prefs.putString("ntfy", cfgNtfy);
  prefs.putString("ntfysrv", cfgNtfyServer);
  prefs.putInt("ntfymask", cfgNtfyMask);
  prefs.putInt("theme", cfgTheme);
  prefs.putInt("bright", cfgBright);
  prefs.putInt("nbright", cfgNightBright);
  prefs.putString("updurl", cfgUpdUrl);
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
