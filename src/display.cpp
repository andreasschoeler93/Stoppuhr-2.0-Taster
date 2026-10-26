#include "display.h"

#include <SPI.h>
#include <Adafruit_ST7789.h>
#include <Fonts/FreeSans9pt7b.h>

#include "config.h"
#include "netlink.h"

namespace display {
namespace {

SPIClass spi;
Adafruit_ST7789 tft(&spi, cfg::Pins::CS, cfg::Pins::DC, cfg::Pins::RST);
GFXcanvas16 gfx(cfg::Screen::W, cfg::Screen::H);
bool displayReady = false;

int bottomBarY() {
  return cfg::Screen::H - cfg::Screen::SAFE - cfg::Screen::BOTTOM_H - 2;
}

void drawBatteryIcon(int16_t x, int16_t y, uint8_t level, uint16_t fg, uint16_t bg) {
  if (level > 4) level = 4;

  gfx.drawRect(x, y - cfg::Layout::BAT_H, cfg::Layout::BAT_W, cfg::Layout::BAT_H, fg);
  gfx.fillRect(x + cfg::Layout::BAT_W, y - cfg::Layout::BAT_H + 3,
               2, cfg::Layout::BAT_H - 6, fg);
  gfx.fillRect(x + 1, y - cfg::Layout::BAT_H + 1,
               cfg::Layout::BAT_W - 2, cfg::Layout::BAT_H - 2, bg);

  const int innerW = cfg::Layout::BAT_W - 2;
  const int segGap = 1;
  const int segW = (innerW - 3 * segGap) / 4;
  int sx = x + 1;
  int sy = y - cfg::Layout::BAT_H + 1;
  int sh = cfg::Layout::BAT_H - 2;

  for (int i = 0; i < 4; ++i) {
    if (i < level) gfx.fillRect(sx, sy, segW, sh, fg);
    sx += segW + segGap;
  }
}

void drawWifiBars(int16_t x, int16_t y, uint8_t level) {
  const int barW = 5, spacing = 3, baseH = 6, step = 6;
  for (int i = 0; i < 4; ++i) {
    int h = baseH + i * step;
    int bx = x + i * (barW + spacing);
    if (i < level) gfx.fillRect(bx, y - h, barW, h, cfg::Colors::GREY);
    else gfx.drawRect(bx, y - h, barW, h, cfg::Colors::GREY);
  }
}

void bottomText(char* text, size_t size, char letter, const measure::Data& m) {
  if (!m.ahtOk || !isfinite(m.temperature) || !isfinite(m.humidity)) {
    snprintf(text, size, "--.-C --%% %c", letter);
  } else {
    snprintf(text, size, "%.1fC %.0f%% %c", m.temperature, m.humidity, letter);
  }
}

}  // namespace

void begin() {
  spi.begin(cfg::Pins::SCLK, -1, cfg::Pins::MOSI, cfg::Pins::CS);
  delay(50);
  if (!gfx.getBuffer()) {
    Serial.println("[GFX] ERROR: canvas allocation failed");
    return;
  }
  tft.init(cfg::Screen::W, cfg::Screen::H);
  tft.setRotation(0);
  gfx.setTextWrap(false);
  displayReady = true;
#if DBG
  Serial.println("[GFX] canvas buffer=OK");
#endif
}

bool ready() {
  return displayReady;
}

GFXcanvas16& canvas() {
  return gfx;
}

void flush() {
  if (!displayReady) return;
  tft.drawRGBBitmap(0, 0, gfx.getBuffer(), cfg::Screen::W, cfg::Screen::H);
}

void renderBottomBar(uint16_t accent, char letter, const measure::Data& m) {
  if (!displayReady) return;

  int rectTop = bottomBarY() - 4;
  int rectH = cfg::Screen::BOTTOM_H + 12;

  int neededBottom = cfg::Layout::BAT_Y + 2;
  int currentBottom = rectTop + rectH - 1;
  if (neededBottom > currentBottom) rectH = (neededBottom - rectTop) + 1;

  gfx.fillRect(cfg::Screen::SAFE, rectTop,
               cfg::Screen::W - 2 * cfg::Screen::SAFE, rectH, accent);
  drawBatteryIcon(cfg::Layout::BAT_X, cfg::Layout::BAT_Y,
                  m.vbatLevel, cfg::Colors::GREY, accent);
  drawWifiBars(cfg::Layout::WIFI_LEFT_X, cfg::Layout::WIFI_BARS_Y, netlink::wifiLevel());

  char text[64];
  bottomText(text, sizeof(text), letter, m);
  gfx.setFont(&FreeSans9pt7b);
  gfx.setTextColor(cfg::Colors::GREY);

  int16_t xb, yb;
  uint16_t textW, textH;
  gfx.getTextBounds(text, 0, 0, &xb, &yb, &textW, &textH);

  int leftLimit = cfg::Screen::SAFE + 4;
  int rightLimit = cfg::Layout::BAT_X - 6;
  int availableW = rightLimit - leftLimit;
  if (availableW < 10) availableW = 10;

  int x = leftLimit + (availableW - (int)textW) / 2;
  if (x < leftLimit) x = leftLimit;

  int baseline = cfg::Screen::H - cfg::Screen::SAFE - 1 + cfg::Layout::VITAL_SHIFT_DOWN;
  int bottomLimit = rectTop + rectH - 1;
  if (baseline > bottomLimit) baseline = bottomLimit;

  gfx.setCursor(x, baseline);
  gfx.print(text);
  flush();
}

}  // namespace display
