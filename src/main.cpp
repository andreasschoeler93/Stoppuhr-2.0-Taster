/*
  Stoppuhr 2.0 – Firmware fuer Seeed XIAO ESP32-S3 (ST7789 240x280)

  Ablauf: Splash -> Zuordnungsansicht. Die Module liegen als eigene Dateien
  neben dieser (siehe README, Abschnitt "Projektstruktur").
*/

#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>

#include "beep.h"
#include "button.h"
#include "config.h"
#include "device_log.h"
#include "display.h"
#include "measurements.h"
#include "netlink.h"
#include "press_gate.h"
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
bool longPressSent = false;
bool pendingTriggered = false;
bool pendingStarter = false;
bool pendingSent = false;
int64_t abortNoticePressUs = 0;
uint32_t abortNoticeAtMs = 0;
netlink::AbortStatus lastAbortStatus = netlink::AbortStatus::NONE;
uint8_t shownPending = 0;
int64_t pendingPressUs = 0;
PressGate pressGate;
// Offline-Stoppuhr (Pi nicht erreichbar)
bool offActive = false;
bool offRunning = false;
int64_t offStartUs = 0;
int32_t offStopMs = -1;
uint16_t offRun = 0;      // Lauf, den der Starter offline gestartet hat
uint16_t offNextRun = 0;  // Lauf, den der naechste Start ausloest
uint32_t lastOffKey = 0;
// Ziel offline: eine Uhr je Bahn, der erste Druck startet alle, jeder weitere stoppt die naechste
bool offFinStarted = false;
uint8_t offFinCount = 0;  // bereits gestoppte Uhren
uint8_t offFinLanes = 0;
int32_t offFinMs[netlink::RoleInfo::MAX_LANES] = {};
int64_t finPress[8];
uint8_t finPressN = 0;

}  // namespace

void setup() {
  // Selbsthaltung so frueh wie moeglich aktivieren.
  pinMode(cfg::Pins::HOLD, OUTPUT);
  digitalWrite(cfg::Pins::HOLD, HIGH);

  Serial.begin(115200);
  delay(50);
#if DBG
  Serial.println("Stoppuhr 2.0 " FW_VERSION);
  Serial.printf("Reset-Grund: %d (1=Power-on 3=SW 4=Panic 5=IntWDT 6=TaskWDT 7=WDT 9=Brownout)\n",
                (int)esp_reset_reason());
#endif
  device_log::addf("start fw=%s reset_reason=%d", FW_VERSION, (int)esp_reset_reason());

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
  if (netlink::roleInfo(finishRole) && finishRole.role == netlink::RoleInfo::RANGE) {
    // Funktest: Tastendruecke werden verworfen
    while (button::pressed(&pressUs)) {}
    pendingButtonPress = false;
  } else if (netlink::roleInfo(finishRole) && finishRole.role == netlink::RoleInfo::FINISH) {
    while (button::pressed(&pressUs)) {
      if (!netlink::serverOnline()) {
        // Pi nicht erreichbar: Offline-Uhren bedienen (weiter unten), siehe offFin*
        if (finPressN >= sizeof(finPress) / sizeof(finPress[0])) {
          device_log::addf("finish press buffer full");
        } else if (pressGate.accept(pressUs, cfg::Net::TRIGGER_MIN_INTERVAL_MS)) {
          finPress[finPressN++] = pressUs;
          beep::click();
        }
        continue;
      }
      if (pressGate.accept(pressUs, cfg::Net::TRIGGER_MIN_INTERVAL_MS) &&
          netlink::sendTrigger(pressUs)) beep::click();
    }
    pendingButtonPress = false;
  } else if (netlink::roleInfo(finishRole) && finishRole.role == netlink::RoleInfo::STARTER &&
             finishRole.combo && !netlink::serverOnline()) {
    // Starter mit Zieleinlauf, Pi aus: Druecke bedienen die lokalen Uhren (siehe offFin*)
    while (button::pressed(&pressUs)) {
      if (finPressN >= sizeof(finPress) / sizeof(finPress[0])) {
        device_log::addf("finish press buffer full");
        continue;
      }
      if (!pressGate.accept(pressUs, cfg::Net::TRIGGER_MIN_INTERVAL_MS)) continue;
      finPress[finPressN++] = pressUs;
      beep::click();
      pendingPressUs = pressUs;
      pendingButtonPress = true;
      pendingStarter = true;
      pendingTriggered = true;
      longPressSent = false;
    }
  } else if (!pendingButtonPress && button::pressed(&pressUs)) {
    if (pressGate.accept(pressUs, cfg::Net::TRIGGER_MIN_INTERVAL_MS) && netlink::sendTrigger(pressUs)) {
      pendingPressUs = pressUs;
      pendingButtonPress = true;
      pendingStarter = finishRole.role == netlink::RoleInfo::STARTER;
      pendingTriggered = false;
      longPressSent = false;
      pendingSent = true;
      beep::click();
    }
  }

  uint8_t changed = measure::update();
  changed |= measure::updateBatteryWarning(netlink::lowBatteryPercent());
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
  static bool lowBatteryWasActive = false;
  static uint32_t lastLowBatteryBeepMs = 0;
  static uint32_t lastLowBatteryBlinkMs = 0;
  const uint32_t now = millis();
  const bool lowBattery = measure::batteryWarningActive();
  if (lowBattery) {
    if (!lowBatteryWasActive) {
      device_log::addf("battery warning active");
      lastLowBatteryBeepMs = now;
      lastLowBatteryBlinkMs = now;
      beep::lowBattery();
      changed |= measure::CHANGED_BATTERY;
    } else if (now - lastLowBatteryBeepMs >= cfg::Timing::LOW_BATTERY_BEEP_INTERVAL_MS) {
      lastLowBatteryBeepMs = now;
      beep::lowBattery();
    }
    if (measure::batteryShutdownDue(netlink::lowBatteryShutdownMs())) {
      device_log::addf("shutdown due to low battery");
      digitalWrite(cfg::Pins::HOLD, LOW);
      return;
    }
    if (now - lastLowBatteryBlinkMs >= cfg::Timing::LOW_BATTERY_BLINK_MS) {
      lastLowBatteryBlinkMs = now;
      changed |= measure::CHANGED_BATTERY;
    }
  }
  lowBatteryWasActive = lowBattery;

  // Buchstabe vom Server uebernehmen und bei Aenderung neu zeichnen
  if (netlink::letter() != assignLetter) {
    assignLetter = netlink::letter();
    changed |= measure::CHANGED_BATTERY;
  }
  netlink::RoleInfo ri;
  const bool haveRole = netlink::roleInfo(ri) && ri.role != netlink::RoleInfo::NONE;
  // Pi ausgefallen: statt der eingefrorenen Serverdaten laeuft eine lokale Stoppuhr
  const bool offline = haveRole && ri.role != netlink::RoleInfo::RANGE && !netlink::serverOnline();
  const bool timed = ri.role == netlink::RoleInfo::LANE || (ri.role == netlink::RoleInfo::STARTER && !ri.combo);
  const bool finLike = ri.role == netlink::RoleInfo::FINISH || (ri.role == netlink::RoleInfo::STARTER && ri.combo);
  if (offline && !offActive) {
    offActive = true;
    offRunning = timed && ri.state == netlink::RoleInfo::RUNNING;
    offStopMs = ri.role == netlink::RoleInfo::LANE && ri.state == netlink::RoleInfo::STOPPED ? ri.timeMs : -1;
    // Starter: laufender Lauf behaelt seine Nummer, nach einem beendeten Lauf zaehlt der naechste
    offRun = ri.run;
    offNextRun = ri.state == netlink::RoleInfo::RUNNING || ri.state == netlink::RoleInfo::STOPPED
                     ? ri.run + 1 : ri.run;
    if (ri.role == netlink::RoleInfo::STARTER && ri.state == netlink::RoleInfo::STOPPED) offStopMs = 0;
    if (offRunning) {
      // Laufende Uhr nahtlos weiterzaehlen
      offStartUs = esp_timer_get_time() - ((int64_t)ri.elapsedMs + (int32_t)(millis() - ri.recvMs)) * 1000;
    }
    if (finLike) {
      const uint8_t n = ri.maxLane ? ri.maxLane :       ri.laneCount;
      offFinLanes = n > netlink::RoleInfo::MAX_LANES ? netlink::RoleInfo::MAX_LANES : n;
      if (offFinLanes == 0) offFinLanes = 1;
      offFinStarted = ri.state == netlink::RoleInfo::RUNNING;
      offFinCount = 0;
      for (uint8_t i = 0; i < ri.laneCount && i < offFinLanes; i++) {
        if (ri.lanes[i].state != netlink::RoleInfo::STOPPED) break;
        offFinMs[offFinCount++] = ri.lanes[i].ms;
      }
      if (offFinStarted) {
        offStartUs = esp_timer_get_time() - ((int64_t)ri.elapsedMs + (int32_t)(millis() - ri.recvMs)) * 1000;
      }
    }
  } else if (!offline && offActive) {
    offActive = false;
  }
  if (!offline) finPressN = 0;
  if (offline && finLike) {
    const bool isStarter = ri.role == netlink::RoleInfo::STARTER;
    for (uint8_t i = 0; i < finPressN; i++) {
      const int64_t us = finPress[i];
      if (!offFinStarted || offFinCount >= offFinLanes) {
        // Start; der Zieleinlauf-Taster sendet ihn nicht (der Pi rechnet den Start vom Starter nach)
        if (isStarter && !netlink::sendTrigger(us)) continue;
        offFinStarted = true;
        offFinCount = 0;
        offStartUs = us;
        if (isStarter) offRun = offNextRun++;
      } else if (netlink::sendTrigger(us)) {
        offFinMs[offFinCount++] = (int32_t)((us - offStartUs) / 1000);
      }
    }
  }
  finPressN = 0;
  // Bereitschaft nur ohne Rolle; Anzeige wechselt bei jeder Aenderung
  const uint8_t pendingLane = haveRole ? 0 : ri.pendingLane;
  if (pendingButtonPress) {
    if (!pendingTriggered) {
      if (offline && timed && pendingSent) {
        // Erster Druck startet, zweiter stoppt (Start kommt spaeter vom Starter und wird nachberechnet)
        const int64_t sinceStartMs = (pendingPressUs - offStartUs) / 1000;
        const bool isStarter = ri.role == netlink::RoleInfo::STARTER;
        const int64_t minRunMs = isStarter ? cfg::Timing::OFFLINE_STARTER_STOP_MIN_MS
                                           : cfg::Timing::OFFLINE_STOP_LOCKOUT_MS;
        if (!offRunning) {
          offRunning = true;
          offStartUs = pendingPressUs;
          offStopMs = -1;
          if (isStarter) offRun = offNextRun++;
        } else if (sinceStartMs >= minRunMs) {
          offRunning = false;
          offStopMs = (int32_t)sinceStartMs;
        }
      }
      pendingTriggered = true;
    }
    if (!longPressSent && pendingStarter && button::down() &&
        esp_timer_get_time() - pendingPressUs >= (int64_t)cfg::Timing::STARTER_LONG_PRESS_MS * 1000) {
      abortNoticePressUs = pendingPressUs;
      abortNoticeAtMs = millis();
      lastAbortStatus = netlink::AbortStatus::PENDING;
      if (!netlink::sendAbortRun(pendingPressUs)) lastAbortStatus = netlink::AbortStatus::FAILED;
      longPressSent = true;
    }
    if (!pendingStarter || !button::down()) {
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

  // Offline-Anzeige aus der lokalen Uhr; Aenderungen (auch Wechsel online/offline) zeichnen neu
  uint32_t offKey = 0;
  if (offActive) {
    ri.offline = true;
    ri.state = offRunning ? netlink::RoleInfo::RUNNING
             : offStopMs >= 0 ? netlink::RoleInfo::STOPPED
                              : netlink::RoleInfo::READY;
    ri.timeMs = offStopMs;
    ri.run = (offRunning || offStopMs >= 0) ? offRun : offNextRun;
    ri.recvMs = millis();
    ri.elapsedMs = offRunning ? (int32_t)((esp_timer_get_time() - offStartUs) / 1000) : 0;
    if (finLike) {
      if (ri.role == netlink::RoleInfo::STARTER) ri.run = offFinStarted ? offRun : offNextRun;
      const bool allDone = offFinStarted && offFinCount >= offFinLanes;
      ri.state = allDone ? netlink::RoleInfo::STOPPED
               : offFinStarted ? netlink::RoleInfo::RUNNING : netlink::RoleInfo::READY;
      ri.elapsedMs = offFinStarted ? (int32_t)((esp_timer_get_time() - offStartUs) / 1000) : 0;
      ri.laneCount = offFinLanes;
      for (uint8_t i = 0; i < offFinLanes; i++) {
        ri.lanes[i].number = i + 1;
        ri.lanes[i].ms = i < offFinCount ? offFinMs[i] : -1;
        ri.lanes[i].state = i < offFinCount ? netlink::RoleInfo::STOPPED
                          : offFinStarted ? netlink::RoleInfo::RUNNING : netlink::RoleInfo::READY;
      }
    }
    offKey = 1 + ri.state * 256u + ri.run * 4096u + offFinCount * 268435456u;
  }
  const bool offChanged = offKey != lastOffKey;
  lastOffKey = offKey;

  static bool abortScreenShown = false;
  static bool lastAbortRed = false;
  static netlink::AbortStatus drawnAbortStatus = netlink::AbortStatus::NONE;
  static uint32_t lastHoldSecond = 0;
  const bool holdingStarter = pendingButtonPress && pendingStarter && button::down();
  const uint32_t heldSeconds = holdingStarter
      ? (uint32_t)((esp_timer_get_time() - pendingPressUs) / 1000000) : 0;
  if (holdingStarter && heldSeconds && heldSeconds != lastHoldSecond) {
    lastHoldSecond = heldSeconds;
    beep::click();
  }
  if (!holdingStarter) lastHoldSecond = 0;
  if (abortNoticePressUs) {
    const netlink::AbortStatus status = netlink::abortStatus(abortNoticePressUs);
    if (status != netlink::AbortStatus::NONE && status != lastAbortStatus) {
      lastAbortStatus = status;
      abortNoticeAtMs = millis();
      if (status == netlink::AbortStatus::CONFIRMED) beep::abortRun();
    }
  }
  const bool showAbortNotice = abortNoticePressUs && millis() - abortNoticeAtMs < 4000;
  const bool showHoldWarning = holdingStarter &&
      showStarterHoldWarning(esp_timer_get_time() - pendingPressUs, cfg::Timing::STARTER_HOLD_WARNING_MS);
  if (showHoldWarning || showAbortNotice) {
    const netlink::AbortStatus shownStatus = holdingStarter && !longPressSent
                                                ? netlink::AbortStatus::NONE : lastAbortStatus;
    const bool red = (millis() / 500) % 2 == 0;
    if (!abortScreenShown || red != lastAbortRed || shownStatus != drawnAbortStatus) {
      screens::drawAbortNotice(shownStatus, red);
      lastAbortRed = red;
      drawnAbortStatus = shownStatus;
    }
    abortScreenShown = true;
    delay(5);
    return;
  }
  if (abortScreenShown) {
    abortScreenShown = false;
    uiMode = UiMode::SPLASH;
    modeStartedMs = millis() - cfg::Timing::SPLASH_MS;
  }

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
    if (ri.role == netlink::RoleInfo::RANGE) {
      // Funktest: Signalstaerke regelmaessig neu zeichnen, bei kritischem Signal piepen
      static uint32_t lastRangeDrawMs = 0;
      static uint32_t lastRangeBeepMs = 0;
      if (uiMode != UiMode::ROLE || ri.seq != roleSeq || millis() - lastRangeDrawMs >= 500) {
        uiMode = UiMode::ROLE;
        roleSeq = ri.seq;
        lastRangeDrawMs = millis();
        screens::drawRole(ri, assignLetter, m);
      }
      if (netlink::wifiLevel() == 0 || netlink::wifiRssi() <= ri.rangeCrit) {
        if (millis() - lastRangeBeepMs >= 1000) {
          lastRangeBeepMs = millis();
          beep::lowBattery();
        }
      }
    } else if (uiMode != UiMode::ROLE || ri.seq != roleSeq || offChanged ||
               (changed & measure::CHANGED_BATTERY)) {
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
