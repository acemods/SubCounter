// ════════════════════════════════════════════════════════════════════════════
//  YouTube API
// ════════════════════════════════════════════════════════════════════════════
int ytGet(const String &endpoint, const String &query, JsonDocument &doc) {
  String url = "https://www.googleapis.com/youtube/v3/" + endpoint + "?key=" + urlEncode(cfgApiKey) + "&" + query;
  String body, date;
  int code = httpsCall(connYT, "GET", url, nullptr, 0, &body, "Date", &date, 12000);
  if (code > 0) {
    setClock(parseHttpDate(date));
    deserializeJson(doc, body);
  }
  Serial.printf("YouTube %s HTTP %d\n", endpoint.c_str(), code);
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

const char *CH_FIELDS = "items(id,snippet(title,publishedAt,country,thumbnails/default/url),"
                        "statistics(subscriberCount,hiddenSubscriberCount,viewCount,videoCount),"
                        "contentDetails/relatedPlaylists/uploads)";

void applyItem(JsonObject item, int i, bool alertOnGain) {
  Channel &c = ch[i];
  c.id = item["id"] | c.id;
  c.title = item["snippet"]["title"] | c.title;
  c.country = item["snippet"]["country"] | c.country;
  c.avatarUrl = item["snippet"]["thumbnails"]["default"]["url"] | c.avatarUrl;
  c.uploads = item["contentDetails"]["relatedPlaylists"]["uploads"] | c.uploads;
  String pub = item["snippet"]["publishedAt"] | "";
  if (pub.length()) c.joined = parseIso(pub);
  c.views = atoll(item["statistics"]["viewCount"] | "-1");
  c.videos = String(item["statistics"]["videoCount"] | "-1").toInt();
  if (item["statistics"]["hiddenSubscriberCount"] | false) { c.err = "Subscriber count is hidden"; return; }
  long n = String(item["statistics"]["subscriberCount"] | "-1").toInt();
  if (n < 0) return;
  if (n != c.subs) c.stepChangedAt = timeValid() ? nowT() : 0;
  if (alertOnGain && c.subs >= 0 && n > c.subs && numAlerts < MAX_CH * 2) {
    long m = nextMilestone(c.subs);
    if (n >= m) addAlert(Alert{ A_MILESTONE, i, -1, n - c.subs, m });   // crossed a milestone: party
    else        addAlert(Alert{ A_GAIN, i, -1, n - c.subs, n });
  }
  c.subs = n;
  c.err = "";
}

void resolveHandles() {
  for (int i = 0; i < numCh; i++) {
    if (ch[i].tw || ch[i].id.length()) continue;
    JsonDocument doc;
    int code = ytGet("channels", "part=statistics,snippet,contentDetails&fields=" + String(CH_FIELDS) +
                     "&forHandle=" + urlEncode(ch[i].handle), doc);
    if (code != 200) { netError = apiErrorText(code, doc); if (code < 0) return; continue; }
    JsonArray items = doc["items"];
    if (items.size() == 0) { ch[i].err = "Channel not found"; continue; }
    applyItem(items[0], i, false);
    loadLastSample(i);
    netError = "";
  }
}

void fetchAll() {
  resolveHandles();
  String ids;
  for (int i = 0; i < numCh; i++) if (!ch[i].tw && ch[i].id.length()) ids += (ids.length() ? "," : "") + ch[i].id;
  if (!ids.length()) { twitchFetch(); raceCheck(); return; }
  JsonDocument doc;
  int code = ytGet("channels", "part=statistics,snippet,contentDetails&fields=" + String(CH_FIELDS) + "&id=" + ids, doc);
  if (code != 200) { netError = apiErrorText(code, doc); return; }
  netError = "";
  lastFetchOk = millis();
  for (int i = 0; i < numCh; i++) {
    if (ch[i].tw || !ch[i].id.length()) continue;
    bool found = false;
    for (JsonObject item : doc["items"].as<JsonArray>()) {
      if (ch[i].id == (const char *)(item["id"] | "")) {
        if (!ch[i].lastSampleT) loadLastSample(i);
        applyItem(item, i, true); found = true; break;
      }
    }
    if (!found) ch[i].err = "Channel not found";
    recordSample(i);
    computeStats(i);
    updateRecords(i);
    pump();
  }
  twitchFetch();
  raceCheck();
}

// race: who's ahead? alert when that changes
void raceCheck() {
  if (raceSet() && ch[cfgRaceA].subs >= 0 && ch[cfgRaceB].subs >= 0) {
    int lead = ch[cfgRaceA].subs >= ch[cfgRaceB].subs ? cfgRaceA : cfgRaceB;
    if (raceLeader >= 0 && lead != raceLeader && ch[cfgRaceA].subs != ch[cfgRaceB].subs && numAlerts < MAX_CH * 2)
      addAlert(Alert{ A_OVERTAKE, lead, raceLeader, 0, ch[lead].subs });
    raceLeader = lead;
  }
}
