/*
  Stoppuhr 2.0 – Firmware fuer Seeed XIAO ESP32-S3 (ST7789 240x280)

  Ablauf: Splash -> Zuordnungsansicht. Die Module liegen als eigene Dateien
  neben dieser (siehe README, Abschnitt "Projektstruktur").
*/

#include <Arduino.h>
#include <esp_timer.h>

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
bool pendingButtonPress = false;
bool localStop = false;
bool lastLocalShown = false;
int32_t localStopMs = 0;
uint16_t localStopRun = 0;
uint16_t localStopAttempt = 0;
uint32_t localStopAtMs = 0;
bool longPressSent = false;
uint8_t shownPending = 0;
int64_t pendingPressUs = 0;

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
  if (netlink::shutdownRequested()) {
    digitalWrite(cfg::Pins::HOLD, LOW);
    return;
  }

  beep::tick();
  // Firmware-Update: nur Fortschritt zeigen, danach neu zeichnen
  static bool updateShown = false;
  uint8_t otaPct = 0;
  if (netlink::otaActive(otaPct)) {
    static uint32_t lastOtaDrawMs = 0;
    if (!updateShown || millis() - lastOtaDrawMs >= 250) {
      lastOtaDrawMs = millis();
      updateShown = true;
      screens::drawUpdate(otaPct);
    }
    return;
  }
  if (updateShown) {
    updateShown = false;
    uiMode = UiMode::SPLASH;
    modeStartedMs = millis() - cfg::Timing::SPLASH_MS;
  }
  int64_t pressUs = 0;
  // Zieleinlauf: jeden Druck sofort senden, auch bei schneller Folge (mehrere Schwimmer kurz nacheinander)
  netlink::RoleInfo finishRole;
  if (netlink::roleInfo(finishRole) && finishRole.role == netlink::RoleInfo::FINISH) {
    while (button::pressed(&pressUs)) {
      netlink::sendTrigger(pressUs);
      beep::click();
    }
    pendingButtonPress = false;
  } else if (!pendingButtonPress && button::pressed(&pressUs)) {
    pendingPressUs = pressUs;
    pendingButtonPress = true;
    longPressSent = false;
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
  // Bereitschaft nur ohne Rolle; Anzeige wechselt bei jeder Aenderung
  const uint8_t pendingLane = haveRole ? 0 : ri.pendingLane;
  if (pendingButtonPress) {
    if (!longPressSent && haveRole && ri.role == netlink::RoleInfo::STARTER && button::down() &&
        esp_timer_get_time() - pendingPressUs >=
            (int64_t)cfg::Timing::STARTER_LONG_PRESS_MS * 1000) {
      netlink::sendAbortRun(pendingPressUs);
      beep::abortRun();
      longPressSent = true;
    } else if (!button::down()) {
      if (!longPressSent) {
        // Bahn: Stoppzeit sofort selbst berechnen und anzeigen, der Server bestaetigt sie spaeter
        if (haveRole && ri.role == netlink::RoleInfo::LANE && ri.state == netlink::RoleInfo::RUNNING &&
            !localStop) {
          const uint32_t pressMs = millis() - (uint32_t)((esp_timer_get_time() - pendingPressUs) / 1000);
          const int32_t ms = ri.elapsedMs + (int32_t)(pressMs - ri.recvMs);
          if (ms >= 0) {
            localStop = true;
            localStopMs = ms;
            localStopRun = ri.run;
            localStopAttempt = ri.attempt;
            localStopAtMs = millis();
          }
        }
        netlink::sendTrigger(pendingPressUs);
        beep::click();
      }
      pendingButtonPress = false;
    }
  }

  // Dumpfer Ton: Bahn-Taster bei Laufstart, Starter bei jeder gestoppten Bahn.
  // Der erste Datensatz nach dem Einschalten loest nichts aus.
  static bool sigInit = false;
  static uint8_t sigState = 0;
  static uint16_t sigRun = 0;
  static uint8_t sigStopped = 0;
  if (haveRole) {
    uint8_t stopped = 0;
    for (uint8_t i = 0; i < ri.laneCount; i++) {
      if (ri.lanes[i].state == netlink::RoleInfo::STOPPED) stopped++;
    }
    if (sigInit) {
      if (ri.role == netlink::RoleInfo::LANE && ri.state == netlink::RoleInfo::RUNNING &&
          sigState != netlink::RoleInfo::RUNNING) {
        beep::dull();
      } else if (ri.role == netlink::RoleInfo::STARTER && ri.run == sigRun && stopped > sigStopped) {
        beep::dull();
      }
    }
    sigInit = true;
    sigState = ri.state;
    sigRun = ri.run;
    sigStopped = stopped;
  }

  // Lokale Stoppzeit anzeigen, bis der Server den Stopp bestaetigt (oder verwirft)
  bool localShown = false;
  if (localStop) {
    // Solange der Druck noch nicht beim Server angekommen ist, bleibt die lokale Zeit stehen
    if (netlink::queued() > 0) localStopAtMs = millis();
    if (haveRole && ri.role == netlink::RoleInfo::LANE && ri.run == localStopRun &&
        ri.attempt == localStopAttempt && ri.state == netlink::RoleInfo::RUNNING &&
        millis() - localStopAtMs < cfg::Timing::LOCAL_STOP_HOLD_MS) {
      ri.state = netlink::RoleInfo::STOPPED;
      ri.timeMs = localStopMs;
      localShown = true;
    } else {
      localStop = false;
    }
  }
  const bool localChanged = localShown != lastLocalShown;
  lastLocalShown = localShown;

  if (uiMode == UiMode::SPLASH) {
    if (millis() - modeStartedMs >= cfg::Timing::SPLASH_MS) {
      if (haveRole) {
        uiMode = UiMode::ROLE;
        roleSeq = ri.seq;
        screens::drawRole(ri, assignLetter, m);
      } else {
        uiMode = UiMode::ASSIGN;
        shownPending = pendingLane;
        assignBg = screens::drawAssign(assignLetter, m, pendingLane);
      }
    } else if (changed) {
      display::renderBottomBar(cfg::Colors::SPLASH, assignLetter, m);
    }
  } else if (haveRole) {
    // Rollenansicht: Vollbild bei Aenderung, sonst nur das Zeitfeld
    if (uiMode != UiMode::ROLE || ri.seq != roleSeq || localChanged || (changed & measure::CHANGED_BATTERY)) {
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
  } else if (uiMode == UiMode::ROLE || pendingLane != shownPending || (changed & measure::CHANGED_BATTERY)) {
    uiMode = UiMode::ASSIGN;
    shownPending = pendingLane;
    assignBg = screens::drawAssign(assignLetter, m, pendingLane);
  } else if (changed & measure::CHANGED_BAR) {
    display::renderBottomBar(assignBg, assignLetter, m);
  }

  delay(5);
}
