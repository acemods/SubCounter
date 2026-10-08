// ════════════════════════════════════════════════════════════════════════════
//  Motion sensor (QMI8658) — shake, face-down, portrait, desk double-tap
// ════════════════════════════════════════════════════════════════════════════
uint8_t imuAddr = 0;
bool imuOk = false;

bool imuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(imuAddr); Wire.write(reg); Wire.write(val);
  return Wire.endTransmission() == 0;
}
bool imuRead(uint8_t reg, uint8_t *buf, uint8_t n) {
  Wire.beginTransmission(imuAddr); Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(imuAddr, n) != n) return false;
  Wire.readBytes(buf, n);
  return true;
}

void imuInit() {
  const uint8_t addrs[] = { 0x6B, 0x6A };
  for (uint8_t a : addrs) {
    imuAddr = a;
    uint8_t who = 0;
    if (imuRead(0x00, &who, 1) && who == 0x05) { imuOk = true; break; }
  }
  if (!imuOk) { Serial.println("Motion sensor NOT found"); return; }
  imuWrite(0x60, 0xB0);  delay(30);    // soft reset
  imuWrite(0x02, 0x40);                // CTRL1: register auto-increment, little endian
  imuWrite(0x03, 0x15);                // CTRL2: accel ±4 g, 250 Hz
  imuWrite(0x08, 0x01);                // CTRL7: accelerometer on
  delay(20);
  Serial.printf("Motion sensor found at 0x%02X\n", imuAddr);
}

bool imuAccel(float &x, float &y, float &z) {
  uint8_t d[6];
  if (!imuRead(0x35, d, 6)) return false;
  x = (int16_t)(d[0] | (d[1] << 8)) / 8192.0f;
  y = (int16_t)(d[2] | (d[3] << 8)) / 8192.0f;
  z = (int16_t)(d[4] | (d[5] << 8)) / 8192.0f;
  return true;
}

enum Orient { O_NORMAL, O_FACEDOWN, O_PORTRAIT_A, O_PORTRAIT_B };
Orient orient = O_NORMAL;
float gX = 0, gY = 0, gZ = 1;            // smoothed gravity (for orientation)
float fX = 0, fY = 0, fZ = 1;            // fast low-pass (for taps)
bool imuPrimed = false;
unsigned long lastImu = 0, orientSince = 0, lastTouchActivity = 0;
Orient orientCandidate = O_NORMAL;
// shake
unsigned long shakeTimes[6]; int shakeN = 0; unsigned long lastShake = 0;
// taps
unsigned long tapBurstStart = 0, tapLast = 0, tapFirst = 0, tapSecond = 0, quietBefore = 0;
int tapCount = 0;
bool evShake = false, evTap = false, evMoved = false;
unsigned long lastMoveEvent = 0;

float tapThreshold() { return cfgTapSens >= 3 ? 0.06f : (cfgTapSens == 1 ? 0.25f : 0.12f); }

// Called often from loop(); sets evShake / evTap and updates `orient`.
void imuUpdate() {
  if (!imuOk || millis() - lastImu < 10) return;
  lastImu = millis();
  float x, y, z;
  if (!imuAccel(x, y, z)) return;
  static unsigned long primedAt = 0;
  if (!imuPrimed) { gX = fX = x; gY = fY = y; gZ = fZ = z; imuPrimed = true; primedAt = millis(); return; }
  static bool autoCal = false;
  if (!g0Saved && !autoCal && millis() - primedAt > 1500) {   // not calibrated yet: power-up position = normal
    float m = sqrtf(gX * gX + gY * gY + gZ * gZ);
    if (m > 0.5f) { cfgG0x = gX / m; cfgG0y = gY / m; cfgG0z = gZ / m; }
    autoCal = true;
  }
  if (!g0Saved && !autoCal) { gX += (x - gX) * 0.04f; gY += (y - gY) * 0.04f; gZ += (z - gZ) * 0.04f; return; }
  fX += (x - fX) * 0.15f; fY += (y - fY) * 0.15f; fZ += (z - fZ) * 0.15f;
  gX += (x - gX) * 0.04f; gY += (y - gY) * 0.04f; gZ += (z - gZ) * 0.04f;
  float hx = x - fX, hy = y - fY, hz = z - fZ;
  float hp = sqrtf(hx * hx + hy * hy + hz * hz);        // sudden movement, in g
  unsigned long now = millis();
  // picked up / nudged (typing on the desk stays well below this)
  if (hp > 0.2f && now - lastMoveEvent > 500) { lastMoveEvent = now; evMoved = true; }

  // ── shake: 4+ big jolts within a second ──
  if (hp > 0.8f) {
    if (shakeN == 0 || now - shakeTimes[shakeN - 1] > 60) {
      if (shakeN == 6) { memmove(shakeTimes, shakeTimes + 1, 5 * sizeof(unsigned long)); shakeN = 5; }
      shakeTimes[shakeN++] = now;
    }
    int recent = 0;
    for (int i = 0; i < shakeN; i++) if (now - shakeTimes[i] < 1000) recent++;
    if (recent >= 4 && now - lastShake > 2500) { lastShake = now; shakeN = 0; evShake = true; }
  }

  // ── desk double-tap: exactly two small knocks 120-600 ms apart, quiet around them ──
  bool tapsAllowed = now - lastTouchActivity > 1500 && now - lastShake > 1500 && orient == O_NORMAL;
  if (tapsAllowed && hp > tapThreshold() && hp < 0.8f) {
    if (now - tapLast > 100) {                  // start of a new knock (not the ringing of the last one)
      if (tapCount == 0) { tapFirst = now; tapBurstStart = now; }
      if (tapCount == 1) tapSecond = now;
      tapCount++;
    }
    tapLast = now;
  }
  if (tapCount && now - tapLast > 450) {        // burst finished
    unsigned long gap = tapSecond - tapFirst;
    if (tapCount == 2 && gap >= 120 && gap <= 600 && tapFirst - quietBefore > 700) evTap = true;
    quietBefore = tapLast;
    tapCount = 0;
  }
  if (!tapCount && hp > tapThreshold()) quietBefore = now;   // any stray knock resets the quiet timer

  // ── orientation (relative to the calibrated "normal" position) ──
  float n = sqrtf(gX * gX + gY * gY + gZ * gZ);
  if (n < 0.5f) return;
  float ux = gX / n, uy = gY / n, uz = gZ / n;
  float zUp = (fabsf(cfgG0z) > 0.3f) ? (cfgG0z > 0 ? 1 : -1) : 1;   // which way the screen faces
  Orient o = O_NORMAL;
  if (uz * zUp < -0.75f) o = O_FACEDOWN;
  // Found on the real board: gravity runs along the sensor's Y axis when the board stands
  // in portrait, and along X when it stands in landscape. The sign of Y says which end is down.
  else if (fabsf(uy) > 0.75f) o = (uy > 0) ? O_PORTRAIT_A : O_PORTRAIT_B;
  if (o != orientCandidate) { orientCandidate = o; orientSince = now; }
  if (orientCandidate != orient && now - orientSince > 700) orient = orientCandidate;
}

// Store the current position as "normal" (from the web settings page)
void calibrateMotion() {
  float n = sqrtf(gX * gX + gY * gY + gZ * gZ);
  if (n < 0.5f) return;
  cfgG0x = gX / n; cfgG0y = gY / n; cfgG0z = gZ / n;
  prefs.begin("subcounter", false);
  prefs.putFloat("g0x", cfgG0x); prefs.putFloat("g0y", cfgG0y); prefs.putFloat("g0z", cfgG0z);
  prefs.end();
  g0Saved = true;
}
