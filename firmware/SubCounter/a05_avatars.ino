// ════════════════════════════════════════════════════════════════════════════
//  Avatar drawing (JPEG, round-cropped)
// ════════════════════════════════════════════════════════════════════════════
int avCx, avCy, avR;
int jpegDraw(JPEGDRAW *p) {
  for (int row = 0; row < p->iHeight; row++) {
    int y = p->y + row;
    int dy = y - avCy;
    if (dy * dy > avR * avR) continue;
    int half = (int)sqrtf((float)(avR * avR - dy * dy));
    int x0 = max(p->x, avCx - half), x1 = min(p->x + p->iWidthUsed - 1, avCx + half);
    if (x1 < x0) continue;
    gfx->draw16bitRGBBitmap(x0, y, p->pPixels + row * p->iWidth + (x0 - p->x), x1 - x0 + 1, 1);
  }
  return 1;
}

int livePill(int cx, int by, bool big);
// Draw a channel's picture at (x,y), full 88px or half size 44px
bool drawAvatar(int i, int x, int y, bool half) {
  Channel &c = ch[i];
  if (c.av565) {                       // Twitch: pre-decoded 88x88 bitmap, circle-cropped here
    int w = half ? 44 : 88, st = half ? 2 : 1;
    avCx = x + w / 2; avCy = y + w / 2; avR = w / 2;
    static uint16_t line[88];
    for (int r = 0; r < w; r++) {
      int dy = r - avR; int hw = (int)sqrtf((float)(avR * avR - dy * dy));
      int x0 = avR - hw, x1 = min(w - 1, avR + hw);
      for (int k = x0; k <= x1; k++) line[k - x0] = c.av565[(r * st) * 88 + k * st];
      gfx->draw16bitRGBBitmap(x + x0, y + r, line, x1 - x0 + 1, 1);
    }
  } else {
  if (!c.avatar) return false;
  if (!jpeg.openRAM(c.avatar, c.avatarLen, jpegDraw)) return false;
  jpeg.setPixelType(RGB565_LITTLE_ENDIAN);
  int w = jpeg.getWidth() / (half ? 2 : 1);
  avCx = x + w / 2; avCy = y + w / 2; avR = w / 2;
  jpeg.decode(x, y, half ? JPEG_SCALE_HALF : 0);
  jpeg.close();
  }
  if (c.live) {                       // YouTube-style live ring + pill
    for (int k = 1; k <= (half ? 2 : 3); k++) gfx->drawCircle(avCx, avCy, avR + k, C_RED);
    livePill(avCx, avCy + avR + (half ? 5 : 8), !half);
  }
  return true;
}
