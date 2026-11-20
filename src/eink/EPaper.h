/*
 * EPaper.h - one-file driver + drawing helpers for the Waveshare 1.54" (G)
 *            200x200 four-colour e-paper (EPD_1in54g) on an ESP32-C3.
 *
 * Replaces: EPD_1in54g, DEV_Config, Debug, GUI_Paint, fonts, font*.cpp
 * Needs:    EPaperFonts.h (generated font data, same folder)
 *
 * Quick start:
 *     #include "EPaper.h"
 *     EPaper154G epd;
 *     void setup() {
 *       epd.begin();
 *       epd.text(10, 10, "Hello", 24, EPD_RED);
 *       epd.show();            // sends the picture to the panel (takes ~15 s)
 *     }
 *
 * Colours: EPD_BLACK, EPD_WHITE, EPD_YELLOW, EPD_RED
 * Drawing only changes a memory buffer; nothing appears until show().
 */
#pragma once
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "EPaperFonts.h"

// ---- Wiring (GPIO numbers). Override with #define before including this file.
#ifndef EPD_PIN_SCK
#define EPD_PIN_SCK  8
#endif
#ifndef EPD_PIN_MOSI
#define EPD_PIN_MOSI 10
#endif
#ifndef EPD_PIN_CS
#define EPD_PIN_CS   20
#endif
#ifndef EPD_PIN_RST
#define EPD_PIN_RST  3
#endif
#ifndef EPD_PIN_DC
#define EPD_PIN_DC   4
#endif
#ifndef EPD_PIN_BUSY
#define EPD_PIN_BUSY 2
#endif

// Print a message on Serial if the panel stops responding (1 = on, 0 = off)
#ifndef EPD_DEBUG
#define EPD_DEBUG 1
#endif

enum EPColor : uint8_t {
  EPD_BLACK = 0,
  EPD_WHITE = 1,
  EPD_YELLOW = 2,
  EPD_RED = 3,
  EPD_NONE = 0xFF      // "transparent" - use as a text background
};

enum EPAlign : uint8_t { EP_LEFT, EP_CENTER, EP_RIGHT };

class EPaper154G {
 public:
  static const int SIZE = 200;                 // panel is 200 x 200
  static const int BUF_SIZE = (SIZE / 4) * SIZE;  // 4 pixels per byte = 10000

  // ---------------------------------------------------------------- setup
  // rotation: 0..3 (quarter turns clockwise). Returns false if out of memory.
  bool begin(uint8_t rotation = 0) {
    pinMode(EPD_PIN_BUSY, INPUT);
    pinMode(EPD_PIN_RST, OUTPUT);
    pinMode(EPD_PIN_DC, OUTPUT);
    pinMode(EPD_PIN_SCK, OUTPUT);
    pinMode(EPD_PIN_MOSI, OUTPUT);
    pinMode(EPD_PIN_CS, OUTPUT);
    digitalWrite(EPD_PIN_CS, HIGH);
    digitalWrite(EPD_PIN_SCK, LOW);

    if (!buf) buf = (uint8_t *)malloc(BUF_SIZE);
    if (!buf) return false;
    rot = rotation & 3;
    clear(EPD_WHITE);
    return true;
  }

  void setRotation(uint8_t rotation) { rot = rotation & 3; }
  int width() const { return SIZE; }
  int height() const { return SIZE; }
  const uint8_t *buffer() const { return buf; }

  // -------------------------------------------------------------- drawing
  void clear(uint8_t color = EPD_WHITE) {
    if (!buf) return;
    color &= 3;
    memset(buf, (color << 6) | (color << 4) | (color << 2) | color, BUF_SIZE);
  }

  void pixel(int x, int y, uint8_t color) {
    if (!buf || x < 0 || y < 0 || x >= SIZE || y >= SIZE) return;
    int nx, ny;  // rotate into the panel's native orientation
    switch (rot) {
      case 0:  nx = x;            ny = y;            break;
      case 1:  nx = SIZE - 1 - y; ny = x;            break;
      case 2:  nx = SIZE - 1 - x; ny = SIZE - 1 - y; break;
      default: nx = y;            ny = SIZE - 1 - x; break;
    }
    int addr = nx / 4 + ny * (SIZE / 4);
    int sh = (nx & 3) * 2;
    buf[addr] = (uint8_t)((buf[addr] & ~(0xC0 >> sh)) | (((color & 3) << 6) >> sh));
  }

  void fillRect(int x, int y, int w, int h, uint8_t color) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = (x + w > SIZE) ? SIZE : x + w, y1 = (y + h > SIZE) ? SIZE : y + h;
    for (int yy = y0; yy < y1; yy++)
      for (int xx = x0; xx < x1; xx++) pixel(xx, yy, color);
  }

  // Outline (thickness t, drawn inwards) or filled rectangle
  void rect(int x, int y, int w, int h, uint8_t color, bool filled = false, int t = 1) {
    if (filled || t * 2 >= w || t * 2 >= h) { fillRect(x, y, w, h, color); return; }
    fillRect(x, y, w, t, color);
    fillRect(x, y + h - t, w, t, color);
    fillRect(x, y + t, t, h - 2 * t, color);
    fillRect(x + w - t, y + t, t, h - 2 * t, color);
  }

  void line(int x0, int y0, int x1, int y1, uint8_t color, int t = 1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
      dot(x0, y0, color, t);
      if (x0 == x1 && y0 == y1) break;
      int e2 = 2 * err;
      if (e2 >= dy) { err += dy; x0 += sx; }
      if (e2 <= dx) { err += dx; y0 += sy; }
    }
  }

  // Outline (thickness t, drawn inwards) or filled circle
  void circle(int cx, int cy, int r, uint8_t color, bool filled = false, int t = 1) {
    long outer = (long)r * r + r;
    int ri = r - t;
    long inner = (ri > 0) ? (long)ri * ri + ri : -1;
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++) {
        long d2 = (long)dx * dx + (long)dy * dy;
        if (d2 <= outer && (filled || d2 > inner)) pixel(cx + dx, cy + dy, color);
      }
  }

  // ----------------------------------------------------------------- text
  // size   = text height in pixels. Snaps DOWN to the nearest size that can be
  //          drawn cleanly (8, 12, 16, 20, 24, 32, 36, 40, 48, ...).
  // x,y    = top-left of the text (or the anchor point if align is CENTER/RIGHT)
  // bg     = EPD_NONE (default) leaves what is already there showing through
  // '\n' in the string starts a new line.
  void text(int x, int y, const char *s, int size = 16, uint8_t color = EPD_BLACK,
            uint8_t bg = EPD_NONE, EPAlign align = EP_LEFT) {
    Pick p = pick(size);
    int cw = p.f->w * p.scale, ch = p.f->h * p.scale;
    while (*s) {
      const char *e = s;
      while (*e && *e != '\n') e++;
      int len = e - s, px = x;
      if (align == EP_CENTER) px = x - (len * cw) / 2;
      else if (align == EP_RIGHT) px = x - len * cw;
      for (int i = 0; i < len; i++) glyph(px + i * cw, y, s[i], p, color, bg);
      y += ch;
      s = e;
      if (*s == '\n') s++;
    }
  }

  // printf-style text:  epd.textf(10, 50, 24, EPD_RED, "Temp %.1f C", t);
  __attribute__((format(printf, 6, 7)))
  void textf(int x, int y, int size, uint8_t color, const char *fmt, ...) {
    char tmp[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    text(x, y, tmp, size, color);
  }

  // Width / height in pixels the text will occupy (for right-aligning, boxes...)
  int textWidth(const char *s, int size = 16) const {
    Pick p = pick(size);
    int best = 0, cur = 0;
    for (; *s; s++) {
      if (*s == '\n') { cur = 0; continue; }
      cur += p.f->w * p.scale;
      if (cur > best) best = cur;
    }
    return best;
  }
  int textHeight(int size = 16) const {
    Pick p = pick(size);
    return p.f->h * p.scale;
  }

  // ---------------------------------------------------------- send to panel
  // Pushes the buffer to the screen, then puts the panel into deep sleep
  // (the next show() wakes it automatically). Takes roughly 10-20 seconds.
  // fast = use Waveshare's fast-init sequence.
  // Returns false if the panel's BUSY line never released (check wiring).
  bool show(bool fast = false) {
    if (!buf) return false;
    resetPanel();
    sendCmd(0x4D); sendData(0x78);
    sendCmd(0x00); sendData(0x0F); sendData(0x29);                 // PSR
    sendCmd(0x06);                                                  // BTST_P
    const uint8_t btst[] = {0x0D, 0x12, 0x30, 0x20, 0x19, 0x2A, 0x22};
    for (uint8_t b : btst) sendData(b);
    sendCmd(0x50); sendData(0x37);                                  // CDI
    sendCmd(0x61);                                                  // TRES
    sendData(SIZE / 256); sendData(SIZE % 256);
    sendData(SIZE / 256); sendData(SIZE % 256);
    sendCmd(0xE9); sendData(0x01);
    sendCmd(0x30); sendData(0x08);
    sendCmd(0x04);                                                  // power on
    bool ok = waitBusy();
    if (fast) {
      sendCmd(0xE0); sendData(0x02);
      sendCmd(0xE6); sendData(0x5D);
      sendCmd(0xA5); sendData(0x00);
      ok &= waitBusy();
    }

    sendCmd(0x10);                                                  // image data
    for (int i = 0; i < BUF_SIZE; i++) sendData(buf[i]);
    sendCmd(0x12); sendData(0x00);                                  // refresh
    ok &= waitBusy();

    sendCmd(0x02); sendData(0x00);                                  // power off
    waitBusy();
    sendCmd(0x07); sendData(0xA5);                                  // deep sleep
    return ok;
  }

 private:
  uint8_t *buf = nullptr;
  uint8_t rot = 0;

  struct Pick { const EPFont *f; uint8_t scale; };

  // Choose the font + integer scale whose height is the largest <= size
  Pick pick(int size) const {
    Pick best = { &EP_FONTS[0], 1 };
    int bestH = 0;
    for (int i = EP_FONT_COUNT - 1; i >= 0; i--) {   // big fonts first: win ties
      const EPFont &f = EP_FONTS[i];
      int s = size / f.h;
      if (s < 1) continue;
      if (s > 8) s = 8;
      if (f.h * s > bestH) { bestH = f.h * s; best = { &f, (uint8_t)s }; }
    }
    return best;
  }

  void dot(int x, int y, uint8_t color, int t) {
    if (t <= 1) pixel(x, y, color);
    else fillRect(x - t / 2, y - t / 2, t, t, color);
  }

  void glyph(int x, int y, char c, Pick p, uint8_t color, uint8_t bg) {
    if (c < 32 || c > 126) c = '?';
    const EPFont &f = *p.f;
    int bpr = (f.w + 7) / 8;
    const uint8_t *g = f.data + (c - 32) * f.h * bpr;
    if (bg != EPD_NONE) fillRect(x, y, f.w * p.scale, f.h * p.scale, bg);
    for (int row = 0; row < f.h; row++)
      for (int col = 0; col < f.w; col++)
        if (g[row * bpr + col / 8] & (0x80 >> (col % 8))) {
          if (p.scale == 1) pixel(x + col, y + row, color);
          else fillRect(x + col * p.scale, y + row * p.scale, p.scale, p.scale, color);
        }
  }

  // ---- low-level panel I/O (bit-banged SPI, same timing as the Waveshare demo)
  void spiByte(uint8_t d) {
    digitalWrite(EPD_PIN_CS, LOW);
    for (int i = 0; i < 8; i++) {
      digitalWrite(EPD_PIN_MOSI, (d & 0x80) ? HIGH : LOW);
      d <<= 1;
      digitalWrite(EPD_PIN_SCK, HIGH);
      digitalWrite(EPD_PIN_SCK, LOW);
    }
    digitalWrite(EPD_PIN_CS, HIGH);
  }
  void sendCmd(uint8_t c)  { digitalWrite(EPD_PIN_DC, LOW);  spiByte(c); }
  void sendData(uint8_t d) { digitalWrite(EPD_PIN_DC, HIGH); spiByte(d); }

  void resetPanel() {
    digitalWrite(EPD_PIN_RST, HIGH); delay(200);
    digitalWrite(EPD_PIN_RST, LOW);  delay(2);
    digitalWrite(EPD_PIN_RST, HIGH); delay(200);
  }

  // This panel signals "ready" by taking BUSY high
  bool waitBusy(uint32_t timeout_ms = 60000) {
    delay(100);
    uint32_t t0 = millis();
    while (!digitalRead(EPD_PIN_BUSY)) {
      if (millis() - t0 > timeout_ms) {
#if EPD_DEBUG
        Serial.println("EPaper: BUSY timeout - check wiring / power");
#endif
        return false;
      }
      delay(5);
    }
    return true;
  }
};
