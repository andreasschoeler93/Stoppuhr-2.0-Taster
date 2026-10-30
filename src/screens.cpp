#include "screens.h"

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#include "config.h"
#include "display.h"
#include "netlink.h"

namespace screens {
namespace {

uint16_t bgFromPercent(uint8_t percent) {
  if (percent >= 60) return cfg::rgb565(0, 160, 0);
  if (percent >= 25) return cfg::rgb565(200, 140, 0);
  return cfg::rgb565(180, 0, 0);
}

void printCentered(const char* text, int baselineY) {
  GFXcanvas16& c = display::canvas();
  int16_t xb, yb;
  uint16_t w, h;
  c.getTextBounds(text, 0, 0, &xb, &yb, &w, &h);
  c.setCursor((cfg::Screen::W - (int)w) / 2, baselineY);
  c.print(text);
}

uint16_t roleBg(const netlink::RoleInfo& ri) {
  if (ri.role == netlink::RoleInfo::STARTER) return cfg::Colors::ROLE_START;
  if (ri.role == netlink::RoleInfo::FINISH) return cfg::Colors::ROLE_FINISH;
  switch (ri.state) {
    case netlink::RoleInfo::RUNNING: return cfg::Colors::ROLE_RUNNING;
    case netlink::RoleInfo::STOPPED:
    case netlink::RoleInfo::INACTIVE: return cfg::Colors::ROLE_STOPPED;
    default: return cfg::Colors::ROLE_READY;
  }
}

void formatTime(char* out, size_t size, int32_t ms) {
  if (ms < 0) {
    snprintf(out, size, "--:--,--");
    return;
  }
  int32_t cs = ms / 10;
  snprintf(out, size, "%02ld:%02ld,%02ld", (long)(cs / 6000), (long)((cs / 100) % 60), (long)(cs % 100));
}

// Aktuell anzuzeigende Zeit; -1 = unbekannt
int32_t shownTime(const netlink::RoleInfo& ri) {
  if (ri.state == netlink::RoleInfo::RUNNING) return ri.elapsedMs + (int32_t)(millis() - ri.recvMs);
  if (ri.state == netlink::RoleInfo::STOPPED) return ri.timeMs;
  return -1;
}

// Kuerzt Text mit "..." so, dass er in die Bildschirmbreite passt.
void printFitted(const char* text, int baselineY, uint16_t color) {
  GFXcanvas16& c = display::canvas();
  char buf[40];
  uint8_t umlauts[sizeof(buf)] = {};
  size_t i = 0;
  while (*text && i < sizeof(buf) - 1) {
    char ch = *text++;
    switch ((uint8_t)ch) {
      case 0x01: ch = 'a'; umlauts[i] = 1; break;
      case 0x02: ch = 'o'; umlauts[i] = 2; break;
      case 0x03: ch = 'u'; umlauts[i] = 3; break;
      case 0x04: ch = 'A'; umlauts[i] = 4; break;
      case 0x05: ch = 'O'; umlauts[i] = 5; break;
      case 0x06: ch = 'U'; umlauts[i] = 6; break;
    }
    buf[i++] = ch;
  }
  buf[i] = '\0';
  const int maxW = cfg::Screen::W - 2 * cfg::Screen::SAFE;
  for (;;) {
    int16_t xb, yb;
    uint16_t w, h;
    c.getTextBounds(buf, 0, 0, &xb, &yb, &w, &h);
    size_t n = strlen(buf);
    if ((int)w <= maxW || n <= 4) break;
    buf[n - 4] = '.';
    buf[n - 3] = '.';
    buf[n - 2] = '.';
    buf[n - 1] = '\0';
  }

  int16_t xb, yb;
  uint16_t w, h;
  c.getTextBounds(buf, 0, 0, &xb, &yb, &w, &h);
  int16_t cursorX = (cfg::Screen::W - (int)w) / 2;
  c.setCursor(cursorX, baselineY);
  for (size_t j = 0; buf[j]; ++j) {
    if (umlauts[j]) {
      char glyph[2] = {buf[j], '\0'};
      int16_t gx, gy;
      uint16_t gw, gh;
      c.getTextBounds(glyph, cursorX, baselineY, &gx, &gy, &gw, &gh);
      int16_t dotX = gx + (int16_t)gw / 2;
      int16_t dotY = gy - 2;
      c.fillCircle(dotX - 2, dotY, 1, color);
      c.fillCircle(dotX + 2, dotY, 1, color);
    }
    c.write((uint8_t)buf[j]);
    cursorX = c.getCursorX();
  }
}

void drawTime(const netlink::RoleInfo& ri, uint16_t bg) {
  GFXcanvas16& c = display::canvas();
  c.fillRect(0, cfg::Layout::ROLE_TIME_BAND_TOP, cfg::Screen::W, cfg::Layout::ROLE_TIME_BAND_H, bg);
  char t[16];
  formatTime(t, sizeof(t), shownTime(ri));
  c.setTextColor(ri.state == netlink::RoleInfo::STOPPED ? cfg::Colors::TIME_STOPPED : cfg::Colors::WHITE);
  c.setFont(&FreeSans24pt7b);
  int16_t xb, yb;
  uint16_t w, h;
  c.getTextBounds(t, 0, 0, &xb, &yb, &w, &h);
  if ((int)w > cfg::Screen::W - 2 * cfg::Screen::SAFE) c.setFont(&FreeSans12pt7b);
  printCentered(t, cfg::Layout::ROLE_TIME_BASELINE_Y);
}

// Starter: alle Bahnen untereinander, links Ziffer, rechts Zeit.
void drawLaneList(const netlink::RoleInfo& ri, uint16_t bg) {
  GFXcanvas16& c = display::canvas();
  const int top = cfg::Layout::STARTER_LIST_TOP;
  const int h = cfg::Layout::STARTER_LIST_BOTTOM - top;
  c.fillRect(0, top, cfg::Screen::W, h, bg);
  if (ri.laneCount == 0) {
    c.setFont(&FreeSans9pt7b);
    c.setTextColor(cfg::Colors::SUB);
    printCentered("keine Bahnen", top + h / 2);
    return;
  }
  const int rowH = min(34, h / ri.laneCount);
  const bool big = rowH >= 24;
  c.setFont(big ? &FreeSans12pt7b : &FreeSans9pt7b);
  const int shift = big ? 7 : 5;
  const int32_t running = ri.state == netlink::RoleInfo::RUNNING ? shownTime(ri) : -1;
  for (uint8_t i = 0; i < ri.laneCount; i++) {
    const netlink::RoleInfo::Lane& l = ri.lanes[i];
    const int base = top + i * rowH + rowH / 2 + shift;
    int32_t ms = l.state == netlink::RoleInfo::STOPPED ? l.ms
               : l.state == netlink::RoleInfo::RUNNING ? running
                                                        : -1;
    char num[4];
    snprintf(num, sizeof(num), "%u", l.number);
    char t[16];
    formatTime(t, sizeof(t), ms);
    c.setTextColor(cfg::Colors::WHITE);
    c.setCursor(cfg::Screen::SAFE, base);
    c.print(num);
    c.setTextColor(ms < 0 ? cfg::Colors::SUB
                          : l.state == netlink::RoleInfo::STOPPED ? cfg::Colors::TIME_STOPPED
                                                                  : cfg::Colors::WHITE);
    int16_t xb, yb;
    uint16_t w, hh;
    c.getTextBounds(t, 0, 0, &xb, &yb, &w, &hh);
    c.setCursor(cfg::Screen::W - cfg::Screen::SAFE - (int)w, base);
    c.print(t);
  }
}

}  // namespace

void drawSplash(char letter, const measure::Data& m) {
  if (!display::ready()) return;

  GFXcanvas16& c = display::canvas();
  c.fillScreen(cfg::Colors::SPLASH);

  c.setTextColor(cfg::Colors::WHITE);
  c.setFont(&FreeSans12pt7b);
  printCentered("Stoppuhr 2.0", 120);

  c.setTextColor(cfg::Colors::SUB);
  c.setFont(&FreeSans9pt7b);
  printCentered(FW_VERSION, 160);

  display::renderBottomBar(cfg::Colors::SPLASH, letter, m);
}

void drawUpdate(uint8_t percent) {
  if (!display::ready()) return;
  GFXcanvas16& c = display::canvas();
  c.fillScreen(cfg::Colors::SPLASH);
  c.setTextColor(cfg::Colors::WHITE);
  c.setFont(&FreeSans12pt7b);
  printCentered("Update", 110);
  char t[8];
  snprintf(t, sizeof(t), "%u%%", percent);
  c.setFont(&FreeSans24pt7b);
  printCentered(t, 165);
  const int x = cfg::Screen::SAFE + 4;
  const int w = cfg::Screen::W - 2 * x;
  c.drawRect(x, 190, w, 12, cfg::Colors::WHITE);
  c.fillRect(x + 2, 192, (w - 4) * percent / 100, 8, cfg::Colors::TIME_STOPPED);
  c.setFont(&FreeSans9pt7b);
  c.setTextColor(cfg::Colors::GREY);
  printCentered("nicht ausschalten", 230);
  display::flush();
}

uint16_t drawAssign(char letter, const measure::Data& m, uint8_t pendingLane) {
  if (!display::ready()) return cfg::Colors::PREP_BG;

  GFXcanvas16& c = display::canvas();
  bool batteryKnown = isfinite(m.vbat);
  uint16_t bg = batteryKnown ? bgFromPercent(m.vbatPercent) : cfg::Colors::PREP_BG;
  c.fillScreen(bg);

  c.setTextColor(cfg::Colors::WHITE);
  c.setFont(&FreeSans12pt7b);
  char percent[8];
  if (batteryKnown) snprintf(percent, sizeof(percent), "%u%%", m.vbatPercent);
  else snprintf(percent, sizeof(percent), "--%%");
  printCentered(percent, cfg::Layout::ASSIGN_PCT_BASELINE_Y);

  c.setFont(&FreeSans24pt7b);
  const char letterText[] = {letter, '\0'};
  printCentered(letterText, cfg::Layout::ASSIGN_LETTER_BASELINE_Y);

  c.setFont(&FreeSans9pt7b);
  c.setTextColor(cfg::Colors::GREY);
  if (pendingLane != 0) {
    // Bereitschaft: Tausch wartet auf den ersten Tastendruck vor Ort
    char hint[24];
    if (pendingLane == netlink::RoleInfo::PENDING_STARTER) snprintf(hint, sizeof(hint), "Bereit: Start");
    else if (pendingLane == netlink::RoleInfo::PENDING_FINISH) snprintf(hint, sizeof(hint), "Bereit: Ziel");
    else snprintf(hint, sizeof(hint), "Bereit: Bahn %u", pendingLane);
    c.setTextColor(cfg::Colors::WHITE);
    printCentered(hint, 204);
    c.setTextColor(cfg::Colors::GREY);
    printCentered("Taste = Tausch", 228);
  }

#if VBAT_SCREEN_DEBUG
  char dbg[40];
  snprintf(dbg, sizeof(dbg), "Akku %.3f V", m.vbatCalc);
  printCentered(dbg, 192);
  snprintf(dbg, sizeof(dbg), "raw %lu  adc %.3fV", (unsigned long)m.rawAverage, m.adcVoltage);
  printCentered(dbg, 208);
  if (netlink::wifiLevel()) snprintf(dbg, sizeof(dbg), "WLAN %d dBm", netlink::wifiRssi());
  else snprintf(dbg, sizeof(dbg), "WLAN offline");
  printCentered(dbg, 176);
#endif

  display::renderBottomBar(bg, letter, m);
  return bg;
}

void drawRole(const netlink::RoleInfo& ri, char letter, const measure::Data& m) {
  if (!display::ready()) return;

  GFXcanvas16& c = display::canvas();
  const uint16_t bg = roleBg(ri);
  c.fillScreen(bg);
  c.setTextColor(cfg::Colors::WHITE);

  // Kopfzeile: Rolle links, Lauf rechts
  char head[12];
  if (ri.role == netlink::RoleInfo::STARTER) snprintf(head, sizeof(head), "Start");
  else if (ri.role == netlink::RoleInfo::FINISH) snprintf(head, sizeof(head), "Ziel");
  else snprintf(head, sizeof(head), "Bahn %u", ri.lane);
  c.setFont(&FreeSans12pt7b);
  c.setCursor(cfg::Screen::SAFE, cfg::Layout::ROLE_HEADER_Y);
  c.print(head);
  char run[20];
  if (ri.attempt > 1) snprintf(run, sizeof(run), "Lauf %u/%u", ri.run, ri.attempt);
  else snprintf(run, sizeof(run), "Lauf %u", ri.run);
  int16_t xb, yb;
  uint16_t w, h;
  c.getTextBounds(run, 0, 0, &xb, &yb, &w, &h);
  c.setCursor(cfg::Screen::W - cfg::Screen::SAFE - (int)w, cfg::Layout::ROLE_HEADER_Y);
  c.print(run);

  if (ri.role == netlink::RoleInfo::STARTER || ri.role == netlink::RoleInfo::FINISH) {
    drawLaneList(ri, bg);
    display::renderBottomBar(bg, letter, m);
    return;
  }

  drawTime(ri, bg);

  c.setFont(&FreeSans12pt7b);
  printFitted(ri.name, cfg::Layout::ROLE_NAME_Y, cfg::Colors::WHITE);
  c.setFont(&FreeSans9pt7b);
  c.setTextColor(cfg::Colors::SUB);
  printFitted(ri.discipline, cfg::Layout::ROLE_DISCIPLINE_Y, cfg::Colors::SUB);

  c.drawFastHLine(cfg::Screen::SAFE, cfg::Layout::ROLE_SEPARATOR_Y, cfg::Screen::W - 2 * cfg::Screen::SAFE,
                  cfg::Colors::SUB);

  const bool starter = ri.role == netlink::RoleInfo::STARTER;
  c.setFont(&FreeSans9pt7b);
  printCentered(starter ? "Letzter Lauf" : "Letzte Zeit", cfg::Layout::ROLE_LAST_LABEL_Y);
  char last[16];
  formatTime(last, sizeof(last), ri.lastMs);
  c.setTextColor(cfg::Colors::WHITE);
  c.setFont(&FreeSans12pt7b);
  printCentered(last, cfg::Layout::ROLE_LAST_TIME_Y);
  if (ri.lastMs >= 0) {
    char meta[40];
    if (starter || !ri.lastName[0]) {
      if (ri.lastAttempt > 1) snprintf(meta, sizeof(meta), "Lauf %u/%u", ri.lastRun, ri.lastAttempt);
      else snprintf(meta, sizeof(meta), "Lauf %u", ri.lastRun);
    }
    else snprintf(meta, sizeof(meta), "%s", ri.lastName);
    c.setFont(&FreeSans9pt7b);
    c.setTextColor(cfg::Colors::SUB);
    printFitted(meta, cfg::Layout::ROLE_LAST_META_Y, cfg::Colors::SUB);
  }

  display::renderBottomBar(bg, letter, m);
}

void drawRoleClock(const netlink::RoleInfo& ri) {
  if (!display::ready()) return;
  if (ri.role == netlink::RoleInfo::STARTER || ri.role == netlink::RoleInfo::FINISH) {
    drawLaneList(ri, roleBg(ri));
    display::flushRows(cfg::Layout::STARTER_LIST_TOP,
                       cfg::Layout::STARTER_LIST_BOTTOM - cfg::Layout::STARTER_LIST_TOP);
    return;
  }
  drawTime(ri, roleBg(ri));
  display::flushRows(cfg::Layout::ROLE_TIME_BAND_TOP, cfg::Layout::ROLE_TIME_BAND_H);
}

}  // namespace screens
