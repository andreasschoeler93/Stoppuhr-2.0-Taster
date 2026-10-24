/*
  ESP32-S3 XIAO SEEED — ST7789

  Aktueller Fokus:
  - Originale Splash- und Assign-Ansicht
  - Zuverlaessige Aktualisierung von Temperatur, Luftfeuchte und Akkustand
  - Weitere Ansichten und Taster-Navigation sind vorerst deaktiviert
*/

#include <Wire.h>
#include <SPI.h>

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_AHTX0.h>

#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans24pt7b.h>

#define DBG 1

// Versionsschema: MAJOR.MINOR.PATCH (siehe README)
#define FW_VERSION_MAJOR 0
#define FW_VERSION_MINOR 3
#define FW_VERSION_PATCH 0
#define FW_STR_(x) #x
#define FW_STR(x) FW_STR_(x)
#define FW_VERSION "v" FW_STR(FW_VERSION_MAJOR) "." FW_STR(FW_VERSION_MINOR) "." FW_STR(FW_VERSION_PATCH)

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
  static constexpr uint16_t WHITE = ST77XX_WHITE;
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

} // namespace cfg

enum class UiMode : uint8_t { SPLASH, ASSIGN };

struct AppState {
  UiMode uiMode = UiMode::SPLASH;
  uint32_t modeStartedMs = 0;
  char assignLetter = 'A';
  uint16_t assignBg = cfg::Colors::PREP_BG;

  bool aht_ok = false;
  float humidity = NAN;
  float temperature = NAN;
  uint8_t aht_fail = 0;
  uint32_t lastAhtMs = 0;
  bool bottomBarNeedsRender = false;

  float vbat = NAN;
  uint8_t vbatLevel = 0;
  uint32_t lastVbatMs = 0;
  bool batteryNeedsRender = false;
  uint32_t dbgRaw = 0;
  float dbgAdc = 0;

  bool lastRead = true;
  bool stable = true;
  uint32_t lastChangeMs = 0;
};

static AppState S;
static SPIClass spi = SPIClass();
static Adafruit_ST7789 tft(&spi, cfg::Pins::CS, cfg::Pins::DC, cfg::Pins::RST);
static Adafruit_AHTX0 aht;

static int bottomBarY() {
  return cfg::Screen::H - cfg::Screen::SAFE - cfg::Screen::BOTTOM_H - 2;
}

static uint8_t vbatToPercent(float voltage) {
  if (!isfinite(voltage)) return 0;
  if (voltage <= 3.30f) return 0;
  if (voltage >= 4.20f) return 100;
  float percent = (voltage - 3.30f) / (4.20f - 3.30f) * 100.0f;
  return (uint8_t)(percent + 0.5f);
}

static uint8_t vbatToLevel0to4(float voltage) {
  if (!isfinite(voltage)) return 0;
  if (voltage >= 4.10f) return 4;
  if (voltage >= 3.90f) return 3;
  if (voltage >= 3.75f) return 2;
  if (voltage >= 3.60f) return 1;
  return 0;
}

static uint16_t bgFromPercent(uint8_t percent) {
  if (percent >= 60) return cfg::rgb565(0, 160, 0);
  if (percent >= 25) return cfg::rgb565(200, 140, 0);
  return cfg::rgb565(180, 0, 0);
}

namespace beep {
static constexpr uint8_t RES = 10;
static constexpr uint16_t FREQ_CLICK = 4200;
static constexpr uint16_t DUR_CLICK_MS = 55;
static bool ok = false;
static uint32_t offAt = 0;

static void attach(uint16_t frequency) {
  ledcDetach(cfg::Pins::PIEZO);
  ok = ledcAttach(cfg::Pins::PIEZO, frequency, RES);
}

static void init() {
  pinMode(cfg::Pins::PIEZO, OUTPUT);
  digitalWrite(cfg::Pins::PIEZO, LOW);
  ok = ledcAttach(cfg::Pins::PIEZO, FREQ_CLICK, RES);
  if (ok) ledcWrite(cfg::Pins::PIEZO, 0);
#if DBG
  Serial.printf("[BEEP] ledcAttach=%s\n", ok ? "OK" : "FAIL");
#endif
}

static void click() {
  if (!ok) return;
  attach(FREQ_CLICK);
  if (!ok) return;
  ledcWrite(cfg::Pins::PIEZO, (uint16_t)((1U << RES) * 0.50f));
  offAt = millis() + DUR_CLICK_MS;
}

static void tick() {
  if (ok && offAt && (int32_t)(millis() - offAt) >= 0) {
    ledcWrite(cfg::Pins::PIEZO, 0);
    offAt = 0;
  }
}
}

static constexpr float VBAT_SCALE = 2.382f;
// 1: Akku-Rohwerte klein auf dem Display anzeigen (zur Fehlersuche)
#define VBAT_SCREEN_DEBUG 1

struct BatteryReading {
  uint32_t rawAverage;
  uint32_t rawMin;
  uint32_t rawMax;
  float adcVoltage;
  float batteryVoltage;
  bool valid;
};

static BatteryReading readBatteryVoltage() {
  digitalWrite(cfg::Pins::MEAS_EN, HIGH);
  delay(cfg::Timing::VBAT_SETTLE_MS);

  uint32_t sumRaw = 0;
  uint32_t sumMilliVolts = 0;
  uint32_t rawMin = UINT32_MAX;
  uint32_t rawMax = 0;
  for (uint8_t i = 0; i < cfg::Timing::VBAT_SAMPLE_COUNT; ++i) {
    uint32_t raw = analogRead(cfg::Pins::VBAT_ADC);
    if (raw < rawMin) rawMin = raw;
    if (raw > rawMax) rawMax = raw;
    sumRaw += raw;
    sumMilliVolts += analogReadMilliVolts(cfg::Pins::VBAT_ADC);
  }

  digitalWrite(cfg::Pins::MEAS_EN, LOW);
  BatteryReading reading;
  reading.rawAverage = sumRaw / cfg::Timing::VBAT_SAMPLE_COUNT;
  reading.rawMin = rawMin;
  reading.rawMax = rawMax;
  reading.adcVoltage = sumMilliVolts / (1000.0f * cfg::Timing::VBAT_SAMPLE_COUNT);
  reading.batteryVoltage = reading.adcVoltage * VBAT_SCALE;
  reading.valid = isfinite(reading.batteryVoltage)
    && reading.batteryVoltage >= 2.5f
    && reading.batteryVoltage <= 4.5f;
  return reading;
}

static void pollBattery() {
  uint32_t now = millis();
  if (now - S.lastVbatMs < cfg::Timing::VBAT_POLL_MS) return;
  S.lastVbatMs = now;

  BatteryReading reading = readBatteryVoltage();
  float newVbat = reading.valid ? reading.batteryVoltage : NAN;
  uint8_t newLevel = vbatToLevel0to4(newVbat);
  bool changed = isfinite(newVbat) != isfinite(S.vbat)
    || vbatToPercent(newVbat) != vbatToPercent(S.vbat)
    || newLevel != S.vbatLevel;
#if VBAT_SCREEN_DEBUG
  changed = changed || S.dbgRaw != reading.rawAverage;
#endif
  S.vbat = newVbat;
  S.vbatLevel = newLevel;
  S.dbgRaw = reading.rawAverage;
  S.dbgAdc = reading.adcVoltage;
  if (changed) S.batteryNeedsRender = true;

#if DBG
  Serial.printf(
    "[VBAT] pin=GPIO%d MEAS_EN=HIGH-during-sample raw(avg/min/max)=%lu/%lu/%lu "
    "adc=%.3fV scale=%.4f battery=%.3fV status=%s",
    cfg::Pins::VBAT_ADC,
    (unsigned long)reading.rawAverage,
    (unsigned long)reading.rawMin,
    (unsigned long)reading.rawMax,
    reading.adcVoltage,
    VBAT_SCALE,
    reading.batteryVoltage,
    reading.valid ? "OK" : "OUT_OF_RANGE"
  );
  if (reading.valid) {
    Serial.printf(" level=%u percent=%u%%\n",
                  S.vbatLevel, vbatToPercent(S.vbat));
  } else {
    Serial.println();
  }
#endif
}

static void reinitializeAht() {
#if DBG
  Serial.println("[AHT] reinitializing");
#endif
  Wire.end();
  delay(5);
  Wire.begin(cfg::Pins::I2C_SDA, cfg::Pins::I2C_SCL);
  Wire.setClock(100000);
  delay(10);

  S.aht_ok = aht.begin(&Wire);
  S.aht_fail = 0;
  if (!S.aht_ok) {
    S.temperature = NAN;
    S.humidity = NAN;
    S.bottomBarNeedsRender = true;
  }

#if DBG
  Serial.printf("[AHT] begin=%s\n", S.aht_ok ? "OK" : "FAIL");
#endif
}

static void pollAht() {
  uint32_t now = millis();
  if (now - S.lastAhtMs < cfg::Timing::AHT_POLL_MS) return;
  S.lastAhtMs = now;

  if (!S.aht_ok) {
    reinitializeAht();
    return;
  }

  sensors_event_t humidity, temperature;
  bool valid = aht.getEvent(&humidity, &temperature)
    && isfinite(humidity.relative_humidity)
    && humidity.relative_humidity >= 0.0f
    && humidity.relative_humidity <= 100.0f
    && isfinite(temperature.temperature)
    && temperature.temperature >= -40.0f
    && temperature.temperature <= 85.0f;

  if (valid) {
    float newTemp = temperature.temperature;
    float newHum = humidity.relative_humidity;
    bool changed = !isfinite(S.temperature) || !isfinite(S.humidity)
      || lroundf(newTemp * 10) != lroundf(S.temperature * 10)
      || lroundf(newHum) != lroundf(S.humidity);
    S.humidity = newHum;
    S.temperature = newTemp;
    S.aht_fail = 0;
    if (changed) S.bottomBarNeedsRender = true;
    return;
  } else {
    ++S.aht_fail;
    if (S.aht_fail >= cfg::Timing::AHT_FAIL_CLEAR_AFTER) {
      S.humidity = NAN;
      S.temperature = NAN;
      S.bottomBarNeedsRender = true;
    }
#if DBG
    Serial.println("[AHT] invalid reading");
#endif
    if (S.aht_fail >= cfg::Timing::AHT_FAIL_REINIT_AFTER) reinitializeAht();
  }
}

static GFXcanvas16 canvas(cfg::Screen::W, cfg::Screen::H);

static void flushCanvas() {
  if (!canvas.getBuffer()) return;
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), cfg::Screen::W, cfg::Screen::H);
}

static void drawBatteryIcon(int16_t x, int16_t y, uint8_t level, uint16_t fg, uint16_t bg) {
  if (level > 4) level = 4;

  canvas.drawRect(x, y - cfg::Layout::BAT_H, cfg::Layout::BAT_W, cfg::Layout::BAT_H, fg);
  canvas.fillRect(x + cfg::Layout::BAT_W, y - cfg::Layout::BAT_H + 3,
               2, cfg::Layout::BAT_H - 6, fg);
  canvas.fillRect(x + 1, y - cfg::Layout::BAT_H + 1,
               cfg::Layout::BAT_W - 2, cfg::Layout::BAT_H - 2, bg);

  const int innerW = cfg::Layout::BAT_W - 2;
  const int segGap = 1;
  const int segW = (innerW - 3 * segGap) / 4;
  int sx = x + 1;
  int sy = y - cfg::Layout::BAT_H + 1;
  int sh = cfg::Layout::BAT_H - 2;

  for (int i = 0; i < 4; ++i) {
    if (i < level) canvas.fillRect(sx, sy, segW, sh, fg);
    sx += segW + segGap;
  }
}

static void drawWifiBarsDummy(int16_t x, int16_t y) {
  const int barW = 5, spacing = 3, baseH = 6, step = 6;
  for (int i = 0; i < 4; ++i) {
    int h = baseH + i * step;
    canvas.fillRect(x + i * (barW + spacing), y - h, barW, h, cfg::Colors::GREY);
  }
}

static String bottomText() {
  char text[64];
  if (!S.aht_ok || !isfinite(S.temperature) || !isfinite(S.humidity)) {
    sprintf(text, "--.-C --%% %c", S.assignLetter);
  } else {
    sprintf(text, "%.1fC %.0f%% %c", S.temperature, S.humidity, S.assignLetter);
  }
  return String(text);
}

static void renderBottomBar(uint16_t accent) {
  int rectTop = bottomBarY() - 4;
  int rectH = cfg::Screen::BOTTOM_H + 12;

  int neededBottom = cfg::Layout::BAT_Y + 2;
  int currentBottom = rectTop + rectH - 1;
  if (neededBottom > currentBottom) rectH = (neededBottom - rectTop) + 1;

  canvas.fillRect(cfg::Screen::SAFE, rectTop,
               cfg::Screen::W - 2 * cfg::Screen::SAFE, rectH, accent);
  drawBatteryIcon(cfg::Layout::BAT_X, cfg::Layout::BAT_Y,
                  S.vbatLevel, cfg::Colors::GREY, accent);
  drawWifiBarsDummy(cfg::Layout::WIFI_LEFT_X, cfg::Layout::WIFI_BARS_Y);

  String text = bottomText();
  canvas.setFont(&FreeSans9pt7b);
  canvas.setTextColor(cfg::Colors::GREY);

  int16_t xb, yb;
  uint16_t textW, textH;
  canvas.getTextBounds(text, 0, 0, &xb, &yb, &textW, &textH);

  int leftLimit = cfg::Screen::SAFE + 4;
  int rightLimit = cfg::Layout::BAT_X - 6;
  int availableW = rightLimit - leftLimit;
  if (availableW < 10) availableW = 10;

  int x = leftLimit + (availableW - (int)textW) / 2;
  if (x < leftLimit) x = leftLimit;

  int baseline = cfg::Screen::H - cfg::Screen::SAFE - 1 + cfg::Layout::VITAL_SHIFT_DOWN;
  int bottomLimit = rectTop + rectH - 1;
  if (baseline > bottomLimit) baseline = bottomLimit;

  canvas.setCursor(x, baseline);
  canvas.print(text);
  flushCanvas();
}

static void drawSplash() {
  canvas.fillScreen(cfg::Colors::SPLASH);
  canvas.setTextColor(cfg::Colors::WHITE);
  canvas.setFont(&FreeSans12pt7b);

  const String title = "Stoppuhr 2.0";
  int16_t xb, yb;
  uint16_t w, h;
  canvas.getTextBounds(title, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, 120);
  canvas.print(title);

  canvas.setTextColor(cfg::Colors::SUB);
  canvas.setFont(&FreeSans9pt7b);
  const String subtitle = FW_VERSION;
  canvas.getTextBounds(subtitle, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, 160);
  canvas.print(subtitle);

  renderBottomBar(cfg::Colors::SPLASH);
}

static void drawAssign() {
  uint8_t percent = vbatToPercent(S.vbat);
  uint16_t bg = isfinite(S.vbat) ? bgFromPercent(percent) : cfg::Colors::PREP_BG;
  S.assignBg = bg;

  canvas.fillScreen(bg);
  canvas.setTextColor(cfg::Colors::WHITE);

  String percentText = isfinite(S.vbat) ? String(percent) + "%" : "--%";
  canvas.setFont(&FreeSans12pt7b);
  int16_t xb, yb;
  uint16_t w, h;
  canvas.getTextBounds(percentText, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, cfg::Layout::ASSIGN_PCT_BASELINE_Y);
  canvas.print(percentText);

  char letter[2] = {S.assignLetter, 0};
  canvas.setFont(&FreeSans24pt7b);
  canvas.getTextBounds(letter, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, cfg::Layout::ASSIGN_LETTER_BASELINE_Y);
  canvas.print(letter);

  canvas.setFont(&FreeSans9pt7b);
  canvas.setTextColor(cfg::Colors::GREY);
  const String hint = "Taste: Starter";
  canvas.getTextBounds(hint, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, 230);
  canvas.print(hint);

#if VBAT_SCREEN_DEBUG
  char dbg[40];
  snprintf(dbg, sizeof(dbg), "raw %lu  adc %.3fV", (unsigned long)S.dbgRaw, S.dbgAdc);
  canvas.getTextBounds(dbg, 0, 0, &xb, &yb, &w, &h);
  canvas.setCursor((cfg::Screen::W - (int)w) / 2, 205);
  canvas.print(dbg);
#endif

  renderBottomBar(bg);
}

static void onButtonPressed() {
  beep::click();
}

static void handleButton() {
  bool readNow = digitalRead(cfg::Pins::BUTTON);
  if (readNow != S.lastRead) {
    S.lastRead = readNow;
    S.lastChangeMs = millis();
  }

  if (millis() - S.lastChangeMs >= cfg::Timing::DEBOUNCE_MS && readNow != S.stable) {
    S.stable = readNow;
    if (!S.stable) onButtonPressed();
  }
}

void setup() {
  Serial.begin(115200);
  delay(50);
#if DBG
  Serial.println("Stoppuhr 2.0 " FW_VERSION);
#endif

  pinMode(cfg::Pins::HOLD, OUTPUT);
  digitalWrite(cfg::Pins::HOLD, HIGH);
  delay(5);

  pinMode(cfg::Pins::BUTTON, INPUT_PULLUP);
  beep::init();

  pinMode(cfg::Pins::MEAS_EN, OUTPUT);
  digitalWrite(cfg::Pins::MEAS_EN, LOW);
  analogReadResolution(12);
  analogSetPinAttenuation(cfg::Pins::VBAT_ADC, ADC_11db);

  Wire.begin(cfg::Pins::I2C_SDA, cfg::Pins::I2C_SCL);
  Wire.setClock(100000);
  delay(50);
  S.aht_ok = aht.begin(&Wire);

  spi.begin(cfg::Pins::SCLK, -1, cfg::Pins::MOSI, cfg::Pins::CS);
  delay(50);
  tft.init(cfg::Screen::W, cfg::Screen::H);
  tft.setRotation(0);
  tft.setTextWrap(false);
  canvas.setTextWrap(false);
#if DBG
  Serial.printf("[GFX] canvas buffer=%s\n", canvas.getBuffer() ? "OK" : "FAIL");
#endif

  S.lastVbatMs = millis() - cfg::Timing::VBAT_POLL_MS;
  S.lastAhtMs = millis() - cfg::Timing::AHT_POLL_MS;
  S.uiMode = UiMode::SPLASH;
  S.modeStartedMs = millis();
  drawSplash();
}

void loop() {
  beep::tick();
  handleButton();

  uint32_t now = millis();
  pollAht();
  pollBattery();

  if (S.uiMode == UiMode::SPLASH) {
    if (now - S.modeStartedMs >= cfg::Timing::SPLASH_MS) {
      S.uiMode = UiMode::ASSIGN;
      drawAssign();
      S.batteryNeedsRender = false;
      S.bottomBarNeedsRender = false;
    } else if (S.bottomBarNeedsRender) {
      renderBottomBar(cfg::Colors::SPLASH);
      S.batteryNeedsRender = false;
      S.bottomBarNeedsRender = false;
    }
  } else if (S.uiMode == UiMode::ASSIGN) {
    if (S.batteryNeedsRender) {
      drawAssign();
      S.batteryNeedsRender = false;
      S.bottomBarNeedsRender = false;
    } else if (S.bottomBarNeedsRender) {
      renderBottomBar(S.assignBg);
      S.bottomBarNeedsRender = false;
    }
  }

  delay(5);
}
