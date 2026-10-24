/*
  Stoppuhr 2.0 – Firmware fuer Seeed XIAO ESP32-S3 (ST7789 240x280)

  Ablauf: Splash -> Zuordnungsansicht. Die Module liegen als eigene Dateien
  neben dieser (siehe README, Abschnitt "Projektstruktur").
*/

#include <Arduino.h>

#include "beep.h"
#include "button.h"
#include "config.h"
#include "display.h"
#include "measurements.h"
#include "netlink.h"
#include "screens.h"

namespace {

enum class UiMode : uint8_t { SPLASH, ASSIGN };

UiMode uiMode = UiMode::SPLASH;
uint32_t modeStartedMs = 0;
char assignLetter = 'A';
uint16_t assignBg = cfg::Colors::PREP_BG;

}  // namespace

void setup() {
  // Selbsthaltung so frueh wie moeglich aktivieren.
  pinMode(cfg::Pins::HOLD, OUTPUT);
  digitalWrite(cfg::Pins::HOLD, HIGH);

  Serial.begin(115200);
  delay(50);
#if DBG
  Serial.println("Stoppuhr 2.0 " FW_VERSION);
#endif

  button::begin();
  beep::begin();
  measure::begin();
  netlink::begin();
  display::begin();

  uiMode = UiMode::SPLASH;
  modeStartedMs = millis();
  screens::drawSplash(assignLetter, measure::data());
}

void loop() {
  beep::tick();
  if (button::pressed()) {
    beep::click();
    netlink::sendTrigger();
  }

  uint8_t changed = measure::update();
  const measure::Data& m = measure::data();

  if (uiMode == UiMode::SPLASH) {
    if (millis() - modeStartedMs >= cfg::Timing::SPLASH_MS) {
      uiMode = UiMode::ASSIGN;
      assignBg = screens::drawAssign(assignLetter, m);
    } else if (changed) {
      display::renderBottomBar(cfg::Colors::SPLASH, assignLetter, m);
    }
  } else if (changed & measure::CHANGED_BATTERY) {
    assignBg = screens::drawAssign(assignLetter, m);
  } else if (changed & measure::CHANGED_BAR) {
    display::renderBottomBar(assignBg, assignLetter, m);
  }

  delay(5);
}
