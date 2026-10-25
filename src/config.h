#pragma once

#include <Arduino.h>

// Zugangsdaten stehen in src/secrets.h (nicht im Repository, siehe
// src/secrets.example.h). Ohne diese Datei gelten die Platzhalter unten.
#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef SECRET_WIFI_SSID
#define SECRET_WIFI_SSID "Stoppuhr"
#endif
#ifndef SECRET_WIFI_PASS
#define SECRET_WIFI_PASS ""
#endif
#ifndef SECRET_PI_IP
#define SECRET_PI_IP "192.168.178.50"
#endif
#ifndef SECRET_API_KEY
#define SECRET_API_KEY ""
#endif
// Zeitquelle (SNTP). Standard: der Pi. Fuer Tests am PC z. B. "pool.ntp.org".
#ifndef SECRET_NTP_SERVER
#define SECRET_NTP_SERVER SECRET_PI_IP
#endif

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
#define VBAT_SCREEN_DEBUG 0

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

// Schnittstelle zum Pi (nur internes WLAN/LAN, siehe README "Taster <-> Pi Schnittstelle")
struct Net {
  // Das WLAN des Pi ist versteckt und passwortgeschuetzt (Werte aus secrets.h)
  static constexpr const char* WIFI_SSID = SECRET_WIFI_SSID;
  static constexpr const char* WIFI_PASS = SECRET_WIFI_PASS;
  // Statische Pi-IP (kein DNS noetig); auch als NTP-Quelle fuer den Zeitstempel
  static constexpr const char* PI_BASE_URL = "http://" SECRET_PI_IP ":8000";
  static constexpr const char* NTP_SERVER = SECRET_NTP_SERVER;
  // Leer: MAC des ESP32 wird verwendet
  static constexpr const char* DEVICE_MAC = "";
  // Leer: kein X-Api-Key-Header
  static constexpr const char* API_KEY = SECRET_API_KEY;
  static constexpr uint32_t HTTP_TIMEOUT_MS = 1500;
  static constexpr uint8_t RETRY_COUNT = 3;
  static constexpr uint32_t RETRY_BACKOFF_MS = 200;
  static constexpr bool ENABLE_OFFLINE_QUEUE = true;
  static constexpr size_t QUEUE_MAX = 32;
  static constexpr uint32_t TRIGGER_MIN_INTERVAL_MS = 300;
  static constexpr uint32_t FLUSH_RETRY_MS = 3000;
  static constexpr uint32_t VITALS_INTERVAL_MS = 10000;
  static constexpr uint32_t WIFI_RETRY_MS = 5000;
};

// Kalibrierfaktor Akku-Spannungsteiler: Akkuspannung = ADC-Spannung * VBAT_SCALE
static constexpr float VBAT_SCALE = 2.382f;

}  // namespace cfg
