#pragma once

#include <Arduino.h>

// Versionsschema: MAJOR.MINOR.PATCH (siehe README)
#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 5
#define FW_VERSION_PATCH 0
#define FW_STR_(x) #x
#define FW_STR(x) FW_STR_(x)
#define FW_VERSION "v" FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)

// 1: serielle Debug-Ausgabe
#define DBG 1
// 1: Akku-Rohwerte klein auf dem Display anzeigen (zur Fehlersuche)
#define VBAT_SCREEN_DEBUG 1

namespace cfg {

struct Pins {
  static constexpr int SCLK = 4;
  static constexpr int MOSI = 5;
  static constexpr int CS = 3;
  static constexpr int DC = 43;
  static constexpr int RST = -1;
  static constexpr int BUTTON = 9;
  static constexpr int PIEZO = 8;
  static constexpr int I2C_SDA = 6;
  static constexpr int I2C_SCL = 7;
  static constexpr int VBAT_ADC = 1;
  static constexpr int MEAS_EN = 2;
  static constexpr int HOLD = 44;
};

struct Screen {
  static constexpr int W = 240;
  static constexpr int H = 280;
  static constexpr int SAFE = 14;
  static constexpr int BOTTOM_H = 20;
};

static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (uint16_t)(b >> 3);
}

struct Colors {
  static constexpr uint16_t PREP_BG = rgb565(20, 60, 180);
  static constexpr uint16_t SPLASH = rgb565(12, 18, 40);
  static constexpr uint16_t SUB = rgb565(180, 180, 180);
  static constexpr uint16_t GREY = 0xBDF7;
  static constexpr uint16_t WHITE = 0xFFFF;
};

struct Layout {
  static constexpr int VITAL_SHIFT_DOWN = 6;
  static constexpr int WIFI_BARS_Y = Screen::H - 4;
  static constexpr int WIFI_LEFT_X = Screen::W - Screen::SAFE - 36;
  static constexpr int BAT_W = 20;
  static constexpr int BAT_H = 10;
  static constexpr int BAT_GAP_WIFI = 8;
  static constexpr int BAT_X = WIFI_LEFT_X - BAT_GAP_WIFI - BAT_W;
  static constexpr int BAT_Y = WIFI_BARS_Y - 2;
  static constexpr int ASSIGN_PCT_BASELINE_Y = 105;
  static constexpr int ASSIGN_LETTER_BASELINE_Y = 175;
};

struct Timing {
  static constexpr uint32_t DEBOUNCE_MS = 40;
  static constexpr uint32_t SPLASH_MS = 5000;
  static constexpr uint32_t AHT_POLL_MS = 2000;
  static constexpr uint8_t AHT_FAIL_REINIT_AFTER = 3;
  static constexpr uint8_t AHT_FAIL_CLEAR_AFTER = 5;
  static constexpr uint32_t VBAT_POLL_MS = 5000;
  static constexpr uint32_t VBAT_SETTLE_MS = 5;
  static constexpr uint8_t VBAT_SAMPLE_COUNT = 8;
};

// Kalibrierfaktor Akku-Spannungsteiler: Akkuspannung = ADC-Spannung * VBAT_SCALE
static constexpr float VBAT_SCALE = 2.382f;

}  // namespace cfg
