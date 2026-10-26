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

enum class UiMode : uint8_t { SPLASH, ASSIGN, ROLE };

UiMode uiMode = UiMode::SPLASH;
uint32_t roleSeq = 0;
uint32_t lastClockMs = 0;
uint32_t modeStartedMs = 0;
char assignLetter = '?';
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
  int64_t pressUs = 0;
  if (button::pressed(&pressUs)) {
    netlink::sendTrigger(pressUs);
    beep::click();
  }

  uint8_t changed = measure::update();
  static uint8_t lastWifiLevel = 0xFF;
  static uint32_t lastWifiCheckMs = 0;
  if (millis() - lastWifiCheckMs >= 2000) {
    lastWifiCheckMs = millis();
    uint8_t lvl = netlink::wifiLevel();
    if (lvl != lastWifiLevel) {
      lastWifiLevel = lvl;
      changed |= measure::CHANGED_BATTERY;
    }
  }
  const measure::Data& m = measure::data();

  // Buchstabe vom Server uebernehmen und bei Aenderung neu zeichnen
  if (netlink::letter() != assignLetter) {
    assignLetter = netlink::letter();
    changed |= measure::CHANGED_BATTERY;
  }
  netlink::RoleInfo ri;
  const bool haveRole = netlink::roleInfo(ri) && ri.role != netlink::RoleInfo::NONE;

  if (uiMode == UiMode::SPLASH) {
    if (millis() - modeStartedMs >= cfg::Timing::SPLASH_MS) {
      if (haveRole) {
        uiMode = UiMode::ROLE;
        roleSeq = ri.seq;
        screens::drawRole(ri, assignLetter, m);
      } else {
        uiMode = UiMode::ASSIGN;
        assignBg = screens::drawAssign(assignLetter, m);
      }
    } else if (changed) {
      display::renderBottomBar(cfg::Colors::SPLASH, assignLetter, m);
    }
  } else if (haveRole) {
    // Rollenansicht: Vollbild bei Aenderung, sonst nur das Zeitfeld
    if (uiMode != UiMode::ROLE || ri.seq != roleSeq || (changed & measure::CHANGED_BATTERY)) {
      uiMode = UiMode::ROLE;
      roleSeq = ri.seq;
      screens::drawRole(ri, assignLetter, m);
      lastClockMs = millis();
    } else if (changed & measure::CHANGED_BAR) {
      screens::drawRole(ri, assignLetter, m);
    } else if (ri.state == netlink::RoleInfo::RUNNING &&
               millis() - lastClockMs >= cfg::Timing::ROLE_CLOCK_MS) {
      lastClockMs = millis();
      screens::drawRoleClock(ri);
    }
  } else if (uiMode == UiMode::ROLE || (changed & measure::CHANGED_BATTERY)) {
    uiMode = UiMode::ASSIGN;
    assignBg = screens::drawAssign(assignLetter, m);
  } else if (changed & measure::CHANGED_BAR) {
    display::renderBottomBar(assignBg, assignLetter, m);
  }

  delay(5);
}
