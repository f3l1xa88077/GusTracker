/*
 * FeederUI.h - dog feeder tracker screen for the 200x200 e-paper (landscape).
 * Needs EPaper.h / EPaperFonts.h in the same folder.
 *
 * In your sketch:
 *     #include "EPaper.h"
 *     #include "FeederUI.h"
 *     EPaper154G epd;
 *     FeederUI ui(epd);
 *
 *     void setup() {
 *       epd.begin(1);                       // 0..3 = which edge is up
 *       ui.setTitle("BUDDY");               // optional, ~10 characters max
 *       ui.update(BOX_TICK, BOX_EMPTY, BATT_FULL, "7:30 AM", "");
 *     }
 *
 * ui.draw(...)   draws into the buffer only
 * ui.update(...) draws, then sends to the screen (epd.show(), ~15 s)
 */
#pragma once
#include "EPaper.h"

enum BoxState { BOX_EMPTY, BOX_TICK, BOX_CROSS, BOX_FILL, BOX_DOT };
enum Battery  { BATT_FULL, BATT_HALF, BATT_LOW };

class FeederUI {
 public:
  explicit FeederUI(EPaper154G &display) : epd(display) {}

  // Text in the black header bar (left side). Keep it short.
  void setTitle(const char *t) { title = t; }

  // morningNote / eveningNote: optional small line under each label, e.g. "7:30 AM".
  // Pass nullptr or "" for none.
  void draw(BoxState morning, BoxState evening, Battery battery,
            const char *morningNote = nullptr, const char *eveningNote = nullptr) {
    epd.clear(EPD_WHITE);
    drawHeader(battery);
    drawRow(40,  false, "MORNING", morningNote, morning);
    drawRow(120, true,  "EVENING", eveningNote, evening);
  }

  bool update(BoxState morning, BoxState evening, Battery battery,
              const char *morningNote = nullptr, const char *eveningNote = nullptr,
              bool fast = false) {
    draw(morning, evening, battery, morningNote, eveningNote);
    return epd.show(fast);
  }

 private:
  EPaper154G &epd;
  const char *title = "FEEDING";

  // Colour of the mark for each state - change these to taste
  static uint8_t markColor(BoxState s) { return s == BOX_CROSS ? EPD_RED : EPD_BLACK; }

  // ------------------------------------------------------------ helpers
  void boldText(int x, int y, const char *s, int size, uint8_t color) {
    epd.text(x,     y, s, size, color);
    epd.text(x + 1, y, s, size, color);
  }

  void fillRound(int x, int y, int w, int h, int r, uint8_t c) {
    if (r <= 0) { epd.fillRect(x, y, w, h, c); return; }
    epd.fillRect(x + r, y, w - 2 * r, h, c);
    epd.fillRect(x, y + r, w, h - 2 * r, c);
    epd.circle(x + r,         y + r,         r, c, true);
    epd.circle(x + w - 1 - r, y + r,         r, c, true);
    epd.circle(x + r,         y + h - 1 - r, r, c, true);
    epd.circle(x + w - 1 - r, y + h - 1 - r, r, c, true);
  }

  void roundRect(int x, int y, int w, int h, int r, int t, uint8_t border, uint8_t bg) {
    fillRound(x, y, w, h, r, border);
    fillRound(x + t, y + t, w - 2 * t, h - 2 * t, r > t ? r - t : 0, bg);
  }

  // ------------------------------------------------------------- header
  void drawHeader(Battery level) {
    epd.fillRect(0, 0, 200, 32, EPD_BLACK);
    boldText(10, 8, title, 16, EPD_YELLOW);

    // battery: white outline on the black bar, level fills from the left
    const int x = 142, y = 7, w = 44, h = 18;
    epd.rect(x, y, w, h, EPD_WHITE, false, 2);
    epd.fillRect(x + w, y + 5, 4, h - 10, EPD_WHITE);            // terminal
    int ix = x + 4, iy = y + 4, iw = w - 8, ih = h - 8;
    int fw = iw;
    uint8_t c = EPD_WHITE;
    if (level == BATT_HALF) fw = iw / 2;
    if (level == BATT_LOW)  { fw = iw / 5; c = EPD_RED; }
    epd.fillRect(ix, iy, fw, ih, c);
  }

  // --------------------------------------------------------------- icons
  void drawSun(int cx, int cy) {
    epd.circle(cx, cy, 7, EPD_YELLOW, true);
    static const int8_t d[8][2] = {{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1},{0,-1},{1,-1}};
    for (int i = 0; i < 8; i++) {
      int dx = d[i][0], dy = d[i][1];
      int a = (dx && dy) ? 10 : 11, b = (dx && dy) ? 13 : 15;   // shorter on diagonals
      epd.line(cx + dx * a, cy + dy * a, cx + dx * b, cy + dy * b, EPD_YELLOW, 2);
    }
  }

  void drawMoon(int cx, int cy) {
    epd.circle(cx, cy, 11, EPD_BLACK, true);
    epd.circle(cx + 6, cy - 4, 9, EPD_WHITE, true);              // bite out of it
  }

  // ----------------------------------------------------------------- box
  void drawBox(int x, int y, BoxState s) {   // 40x40
    const int B = 40;
    uint8_t c = markColor(s);
    roundRect(x, y, B, B, 7, 3, EPD_BLACK, EPD_WHITE);
    switch (s) {
      case BOX_TICK:
        epd.line(x + 10, y + 21, x + 17, y + 29, c, 4);
        epd.line(x + 17, y + 29, x + 31, y + 11, c, 4);
        break;
      case BOX_CROSS:
        epd.line(x + 12, y + 12, x + B - 13, y + B - 13, c, 4);
        epd.line(x + B - 13, y + 12, x + 12, y + B - 13, c, 4);
        break;
      case BOX_FILL:
        fillRound(x + 7, y + 7, B - 14, B - 14, 3, c);
        break;
      case BOX_DOT:
        epd.circle(x + B / 2, y + B / 2, 8, c, true);
        break;
      default: break;
    }
  }

  // ----------------------------------------------------------------- row
  void drawRow(int y, bool evening, const char *label, const char *note, BoxState s) {
    const int h = 72, cy = y + h / 2;
    roundRect(8, y, 184, h, 8, 2, EPD_BLACK, EPD_WHITE);        // card

    if (evening) drawMoon(34, cy); else drawSun(34, cy);

    if (note && *note) {
      boldText(58, cy - 17, label, 16, EPD_BLACK);
      epd.text(58, cy + 3, note, 12, EPD_BLACK);
    } else {
      boldText(58, cy - 8, label, 16, EPD_BLACK);
    }

    drawBox(144, cy - 20, s);
  }
};
