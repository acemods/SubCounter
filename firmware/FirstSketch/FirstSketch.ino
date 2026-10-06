/*
 * FirstSketch — Waveshare ESP32-C6-Touch-LCD-1.47
 *
 * 1. Shows a "Hello!" splash screen.
 * 2. Switches to a subscriber-counter style screen (demo numbers, no Wi-Fi yet).
 *    - Press the BOOT button to add a "subscriber".
 *    - Every 10 subscribers the screen flashes a milestone celebration.
 * 3. Shows the battery voltage in the corner (reads ~0 V on USB only).
 *
 * Arduino IDE settings:
 *   Board:            ESP32C6 Dev Module   (esp32 by Espressif Systems 3.x)
 *   USB CDC On Boot:  Enabled
 *   Library:          GFX Library for Arduino (by Moon On Our Nation) 1.6.x
 */

#include <Arduino_GFX_Library.h>

// ── Pins for the ESP32-C6 version of this board ─────────────────────────────
// (Waveshare's wiki example shows ESP32-S3 pin numbers — these are the C6 ones.)
#define LCD_SCK   1
#define LCD_MOSI  2
#define LCD_CS    14
#define LCD_DC    15
#define LCD_RST   22
#define LCD_BL    23   // backlight
#define SD_CS     4    // TF card shares the SPI bus — keep it deselected
#define BOOT_BTN  9    // the BOOT button
#define BAT_ADC   0    // battery voltage via a 1/3 divider

// ── Display objects ─────────────────────────────────────────────────────────
Arduino_DataBus *bus = new Arduino_HWSPI(LCD_DC, LCD_CS, LCD_SCK, LCD_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(
  bus, LCD_RST, 0 /* rotation */, false /* IPS */,
  172 /* width */, 320 /* height */,
  34, 0, 34, 0 /* column/row offsets */);

// ── Colours (RGB565) ────────────────────────────────────────────────────────
#define C_BG      0x0000   // black
#define C_RED     0xF800
#define C_WHITE   0xFFFF
#define C_GREY    0x8410
#define C_GOLD    0xFEA0
#define C_PURPLE  0x901F

// ── State ───────────────────────────────────────────────────────────────────
uint32_t subs = 1234;           // starting demo number
uint32_t shownSubs = 0;         // what's currently drawn (for count-up animation)
unsigned long lastBattery = 0;
bool lastBtn = HIGH;

// The panel is a JD9853 that needs this register setup after gfx->begin().
void lcdRegInit() {
  static const uint8_t ops[] = {
    BEGIN_WRITE, WRITE_COMMAND_8, 0x11, END_WRITE, DELAY, 120,
    BEGIN_WRITE,
    WRITE_C8_D16, 0xDF, 0x98, 0x53,
    WRITE_C8_D8,  0xB2, 0x23,
    WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 4, 0x00, 0x47, 0x00, 0x6F,
    WRITE_COMMAND_8, 0xBB, WRITE_BYTES, 6, 0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
    WRITE_C8_D16, 0xC0, 0x44, 0xA4,
    WRITE_C8_D8,  0xC1, 0x16,
    WRITE_COMMAND_8, 0xC3, WRITE_BYTES, 8, 0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
    WRITE_COMMAND_8, 0xC4, WRITE_BYTES, 12, 0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A,
                                            0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
    WRITE_COMMAND_8, 0xC8, WRITE_BYTES, 32,
      0x3F,0x32,0x29,0x29,0x27,0x2B,0x27,0x28,0x28,0x26,0x25,0x17,0x12,0x0D,0x04,0x00,
      0x3F,0x32,0x29,0x29,0x27,0x2B,0x27,0x28,0x28,0x26,0x25,0x17,0x12,0x0D,0x04,0x00,
    WRITE_COMMAND_8, 0xD0, WRITE_BYTES, 5, 0x04, 0x06, 0x6B, 0x0F, 0x00,
    WRITE_C8_D16, 0xD7, 0x00, 0x30,
    WRITE_C8_D8,  0xE6, 0x14,
    WRITE_C8_D8,  0xDE, 0x01,
    WRITE_COMMAND_8, 0xB7, WRITE_BYTES, 5, 0x03, 0x13, 0xEF, 0x35, 0x35,
    WRITE_COMMAND_8, 0xC1, WRITE_BYTES, 3, 0x14, 0x15, 0xC0,
    WRITE_C8_D16, 0xC2, 0x06, 0x3A,
    WRITE_C8_D16, 0xC4, 0x72, 0x12,
    WRITE_C8_D8,  0xBE, 0x00,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x01, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x35, 0x00,
    WRITE_C8_D8,  0x3A, 0x05,
    WRITE_COMMAND_8, 0x2A, WRITE_BYTES, 4, 0x00, 0x22, 0x00, 0xCD,
    WRITE_COMMAND_8, 0x2B, WRITE_BYTES, 4, 0x00, 0x00, 0x01, 0x3F,
    WRITE_C8_D8,  0xDE, 0x02,
    WRITE_COMMAND_8, 0xE5, WRITE_BYTES, 3, 0x00, 0x02, 0x00,
    WRITE_C8_D8,  0xDE, 0x00,
    WRITE_C8_D8,  0x36, 0x00,
    WRITE_COMMAND_8, 0x21,
    END_WRITE, DELAY, 10,
    BEGIN_WRITE, WRITE_COMMAND_8, 0x29, END_WRITE
  };
  bus->batchOperation(ops, sizeof(ops));
}

// Print text centred horizontally at a given y.
void centreText(const char *txt, int y, uint8_t size, uint16_t colour) {
  gfx->setTextSize(size);
  gfx->setTextColor(colour, C_BG);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(txt, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor((gfx->width() - w) / 2, y);
  gfx->print(txt);
}

// Format 12345 as "12,345".
void withCommas(uint32_t n, char *out) {
  char raw[12];
  sprintf(raw, "%lu", (unsigned long)n);
  int len = strlen(raw), j = 0;
  for (int i = 0; i < len; i++) {
    out[j++] = raw[i];
    int left = len - i - 1;
    if (left > 0 && left % 3 == 0) out[j++] = ',';
  }
  out[j] = 0;
}

void drawSplash() {
  gfx->fillScreen(C_BG);
  centreText("Hello!", 50, 5, C_WHITE);
  centreText("ESP32-C6 is alive", 120, 2, C_GREY);
}

void drawCounterFrame() {
  gfx->fillScreen(C_BG);
  // Red "play button" style badge + title
  gfx->fillRoundRect(14, 12, 40, 28, 8, C_RED);
  gfx->fillTriangle(28, 18, 28, 34, 42, 26, C_WHITE);
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE, C_BG);
  gfx->setCursor(64, 19);
  gfx->print("SUBSCRIBERS");
  gfx->drawFastHLine(14, 50, gfx->width() - 28, C_GREY);
  centreText("Press BOOT to add one", 150, 1, C_GREY);
}

void drawNumber(uint32_t n) {
  char buf[16];
  withCommas(n, buf);
  gfx->fillRect(0, 70, gfx->width(), 60, C_BG);   // clear old number
  centreText(buf, 76, 6, C_WHITE);
}

void drawBattery() {
  float volts = analogReadMilliVolts(BAT_ADC) * 3.0f / 1000.0f;
  char buf[12];
  snprintf(buf, sizeof(buf), "%.2fV", volts);
  gfx->setTextSize(1);
  gfx->setTextColor(C_GREY, C_BG);
  gfx->setCursor(gfx->width() - 44, 160);
  gfx->print(buf);
}

void celebrate(uint32_t n) {
  for (int i = 0; i < 3; i++) {
    gfx->fillScreen(i % 2 ? C_PURPLE : C_GOLD);
    delay(120);
  }
  gfx->fillScreen(C_BG);
  char buf[24];
  snprintf(buf, sizeof(buf), "%lu!", (unsigned long)n);
  centreText("MILESTONE", 40, 3, C_GOLD);
  centreText(buf, 85, 5, C_WHITE);
  delay(1500);
  drawCounterFrame();
  drawNumber(n);
  drawBattery();
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("FirstSketch starting...");

  // Keep the TF card quiet so it doesn't interfere with the shared SPI bus.
  pinMode(SD_CS, OUTPUT);  digitalWrite(SD_CS, HIGH);
  pinMode(LCD_CS, OUTPUT); digitalWrite(LCD_CS, HIGH);

  pinMode(BOOT_BTN, INPUT_PULLUP);

  if (!gfx->begin()) Serial.println("gfx->begin() failed!");
  lcdRegInit();
  gfx->setRotation(1);          // landscape: 320 x 172
  gfx->fillScreen(C_BG);

  // Backlight on at ~60% via PWM
  ledcAttach(LCD_BL, 5000, 8);
  ledcWrite(LCD_BL, 150);

  drawSplash();
  delay(2000);

  drawCounterFrame();
  shownSubs = subs - 50;        // start slightly lower so it "counts up" on boot
  drawNumber(shownSubs);
  drawBattery();
  Serial.println("Ready. Press BOOT to add a subscriber.");
}

void loop() {
  // BOOT button: add one subscriber on each press
  bool btn = digitalRead(BOOT_BTN);
  if (lastBtn == HIGH && btn == LOW) {
    subs++;
    Serial.printf("Subscribers: %lu\n", (unsigned long)subs);
  }
  lastBtn = btn;

  // Smooth count-up animation towards the real number
  if (shownSubs < subs) {
    uint32_t gap = subs - shownSubs;
    shownSubs += (gap > 20) ? gap / 10 : 1;
    drawNumber(shownSubs);
    if (shownSubs == subs && subs % 10 == 0) celebrate(subs);
  }

  // Refresh battery reading every 5 s
  if (millis() - lastBattery > 5000) {
    lastBattery = millis();
    drawBattery();
  }

  delay(30);
}
