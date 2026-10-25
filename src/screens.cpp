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

void printCentered(const String& text, int baselineY) {
  GFXcanvas16& c = display::canvas();
  int16_t xb, yb;
  uint16_t w, h;
  c.getTextBounds(text, 0, 0, &xb, &yb, &w, &h);
  c.setCursor((cfg::Screen::W - (int)w) / 2, baselineY);
  c.print(text);
}

}  // namespace

void drawSplash(char letter, const measure::Data& m) {
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

uint16_t drawAssign(char letter, const measure::Data& m) {
  GFXcanvas16& c = display::canvas();
  bool batteryKnown = isfinite(m.vbat);
  uint16_t bg = batteryKnown ? bgFromPercent(m.vbatPercent) : cfg::Colors::PREP_BG;
  c.fillScreen(bg);

  c.setTextColor(cfg::Colors::WHITE);
  c.setFont(&FreeSans12pt7b);
  printCentered(batteryKnown ? String(m.vbatPercent) + "%" : String("--%"),
                cfg::Layout::ASSIGN_PCT_BASELINE_Y);

  c.setFont(&FreeSans24pt7b);
  printCentered(String(letter), cfg::Layout::ASSIGN_LETTER_BASELINE_Y);

  c.setFont(&FreeSans9pt7b);
  c.setTextColor(cfg::Colors::GREY);
  printCentered("Taste: Starter", 230);

#if VBAT_SCREEN_DEBUG
  char dbg[40];
  snprintf(dbg, sizeof(dbg), "Akku %.2f V", m.vbatCalc);
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

}  // namespace screens
