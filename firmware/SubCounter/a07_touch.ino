// ════════════════════════════════════════════════════════════════════════════
//  Touch (AXS5106L) — polled; swipe detection in 4 directions
// ════════════════════════════════════════════════════════════════════════════
bool touchOk = false, touching = false;
int tStartX, tStartY, tLastX, tLastY;
unsigned long tStartAt = 0, lastTouchPoll = 0;

void touchInit() {
  Wire.begin(TP_SDA, TP_SCL);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(200);
  digitalWrite(TP_RST, HIGH); delay(300);
  Wire.beginTransmission(TP_ADDR);
  touchOk = (Wire.endTransmission() == 0);
  Serial.printf("Touch controller %s\n", touchOk ? "found" : "NOT found");
}

bool touchRead(int &x, int &y) {
  uint8_t d[14] = {0};
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)14) != 14) return false;
  Wire.readBytes(d, 14);
  if (d[1] == 0 || d[1] > 5) return false;
  uint16_t rawX = ((uint16_t)(d[2] & 0x0F) << 8) | d[3];
  uint16_t rawY = ((uint16_t)(d[4] & 0x0F) << 8) | d[5];
  x = rawY;   // landscape (rotation 1): screen X = panel Y, screen Y = panel X
  y = rawX;
  return true;
}

// Returns 'L','R','U','D' for a swipe, 'T' for a tap (position in tapX/tapY),
// 'H' once when a finger is held still for 0.7 s (long-press), 0 otherwise.
int tapX = 0, tapY = 0;
bool holdFired = false;
char pollSwipe() {
  if (!touchOk || millis() - lastTouchPoll < 20) return 0;
  lastTouchPoll = millis();
  int x, y;
  if (touchRead(x, y)) {
    if (!touching) { touching = true; holdFired = false; tStartX = x; tStartY = y; tStartAt = millis(); }
    tLastX = x; tLastY = y;
    if (!holdFired && millis() - tStartAt > 700 && abs(x - tStartX) < 15 && abs(y - tStartY) < 15) { holdFired = true; return 'H'; }
    return 0;
  }
  if (!touching) return 0;
  touching = false;
  if (holdFired) return 0;
  int dx = tLastX - tStartX, dy = tLastY - tStartY;
  if (abs(dx) < 15 && abs(dy) < 15 && millis() - tStartAt < 500) { tapX = tLastX; tapY = tLastY; return 'T'; }
  if (millis() - tStartAt > 1500) return 0;
  if (abs(dx) >= abs(dy) && abs(dx) >= SWIPE_MIN_PX) return dx < 0 ? 'L' : 'R';
  if (abs(dy) > abs(dx) && abs(dy) >= SWIPE_MIN_PX * 2 / 3) return dy < 0 ? 'U' : 'D';
  return 0;
}
