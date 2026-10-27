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
#define FW_VERSION_MINOR 8
#define FW_VERSION_PATCH 6
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
  // Rollenansichten
  static constexpr uint16_t ROLE_START = rgb565(170, 30, 30);
  static constexpr uint16_t ROLE_READY = rgb565(20, 60, 180);
  static constexpr uint16_t ROLE_RUNNING = rgb565(0, 130, 0);
  static constexpr uint16_t ROLE_STOPPED = rgb565(70, 70, 85);
  // Schriftfarbe einer gestoppten Zeit
  static constexpr uint16_t TIME_STOPPED = rgb565(255, 215, 0);
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
  // Rollenansichten (Start / Bahn)
  static constexpr int ROLE_HEADER_Y = 34;
  static constexpr int ROLE_TIME_BASELINE_Y = 100;
  static constexpr int ROLE_TIME_BAND_TOP = 60;
  static constexpr int ROLE_TIME_BAND_H = 54;
  static constexpr int ROLE_NAME_Y = 136;
  static constexpr int ROLE_DISCIPLINE_Y = 156;
  static constexpr int ROLE_SEPARATOR_Y = 166;
  static constexpr int ROLE_LAST_LABEL_Y = 186;
  static constexpr int ROLE_LAST_TIME_Y = 212;
  static constexpr int ROLE_LAST_META_Y = 232;
  // Starter: Bahnliste (nur Ziffer und Zeit je Zeile)
  static constexpr int STARTER_LIST_TOP = 44;
  static constexpr int STARTER_LIST_BOTTOM = 238;
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
  // Laufende Uhr in den Rollenansichten: nur das Zeitfeld wird so oft neu uebertragen
  static constexpr uint32_t ROLE_CLOCK_MS = 60;
};

// Schnittstelle zum Pi (nur internes WLAN/LAN, siehe README "Taster <-> Pi Schnittstelle")
struct Net {
  // Das WLAN des Pi ist versteckt und passwortgeschuetzt (Werte aus secrets.h)
  static constexpr const char* WIFI_SSID = SECRET_WIFI_SSID;
  static constexpr const char* WIFI_PASS = SECRET_WIFI_PASS;
  // Statische Pi-IP (kein DNS noetig); auch als NTP-Quelle fuer den Zeitstempel
  static constexpr const char* PI_HOST = SECRET_PI_IP;
  static constexpr uint16_t PI_PORT = 1999;
  static constexpr const char* NTP_SERVER = SECRET_NTP_SERVER;
  static constexpr uint32_t NTP_SYNC_INTERVAL_MS = 30000;
  // Leer: MAC des ESP32 wird verwendet
  static constexpr const char* DEVICE_MAC = "";
  // Anzeigename im Server; leer = "T-" + letzte 3 MAC-Bytes
  static constexpr const char* DEVICE_NAME = "";
  static constexpr uint32_t REGISTER_RETRY_MS = 5000;
  static constexpr uint32_t REREGISTER_MS = 60000;
  // Abfrage der Anzeigedaten (GET /api/display): mit Rolle schnell, ohne Rolle langsam
  static constexpr uint32_t DISPLAY_POLL_MS = 200;
  static constexpr uint32_t DISPLAY_POLL_IDLE_MS = 1000;
  // Leer: kein X-Api-Key-Header
  static constexpr const char* API_KEY = SECRET_API_KEY;
  static constexpr uint32_t HTTP_TIMEOUT_MS = 1000;
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
