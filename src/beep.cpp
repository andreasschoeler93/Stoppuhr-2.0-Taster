#include "beep.h"

#include "config.h"

namespace beep {
namespace {

constexpr uint8_t RES = 10;
constexpr uint16_t FREQ_CLICK = 4200;
constexpr uint16_t DUR_CLICK_MS = 120;
// Der Klick laeuft ueber mehrere Frequenzen, damit die (unbekannte) Resonanz des Piezos sicher getroffen wird
constexpr uint16_t SWEEP_CLICK[] = {2700, 3400, 4200};
constexpr uint16_t SWEEP_STEP_MS = DUR_CLICK_MS / 3;
// Dumpfer Ton fuer Signale vom Server (Lauf gestartet / Bahn gestoppt)
constexpr uint16_t FREQ_DULL = 1200;
constexpr uint16_t DUR_DULL_MS = 110;
// Lauf abgebrochen: drei lange Pieptoene mit Pause
constexpr uint16_t FREQ_ABORT = 2400;
constexpr uint16_t DUR_ABORT_MS = 450;
constexpr uint16_t GAP_ABORT_MS = 200;
bool ok = false;
uint32_t offAt = 0;
uint8_t abortLeft = 0;      // noch zu spielende Toene
uint32_t abortNextAt = 0;   // Start des naechsten Tons
uint8_t sweepIdx = 0;       // naechster Schritt des Klicks (0 = kein Klick aktiv)
uint32_t sweepNextAt = 0;

void play(uint16_t freq, uint16_t durMs) {
  if (!ok) return;
  ledcDetach(cfg::Pins::PIEZO);
  ok = ledcAttach(cfg::Pins::PIEZO, freq, RES);
  if (!ok) return;
  ledcWrite(cfg::Pins::PIEZO, (uint16_t)((1U << RES) * 0.50f));
  offAt = millis() + durMs;
}

}  // namespace

void begin() {
  pinMode(cfg::Pins::PIEZO, OUTPUT);
  digitalWrite(cfg::Pins::PIEZO, LOW);
  ok = ledcAttach(cfg::Pins::PIEZO, FREQ_CLICK, RES);
  if (ok) ledcWrite(cfg::Pins::PIEZO, 0);
#if DBG
  Serial.printf("[BEEP] ledcAttach=%s\n", ok ? "OK" : "FAIL");
#endif
}

void click() {
  play(SWEEP_CLICK[0], SWEEP_STEP_MS);
  sweepIdx = 1;
  sweepNextAt = millis() + SWEEP_STEP_MS;
}

void dull() { sweepIdx = 0; play(FREQ_DULL, DUR_DULL_MS); }

void abortRun() {
  sweepIdx = 0;
  abortLeft = 3;
  abortNextAt = millis();
}

void tick() {
  if (sweepIdx && (int32_t)(millis() - sweepNextAt) >= 0) {
    if (sweepIdx < sizeof(SWEEP_CLICK) / sizeof(SWEEP_CLICK[0])) {
      play(SWEEP_CLICK[sweepIdx], SWEEP_STEP_MS);
      sweepIdx++;
      sweepNextAt = millis() + SWEEP_STEP_MS;
    } else {
      sweepIdx = 0;
    }
  }
  if (abortLeft && !offAt && (int32_t)(millis() - abortNextAt) >= 0) {
    play(FREQ_ABORT, DUR_ABORT_MS);
    abortLeft--;
    abortNextAt = millis() + DUR_ABORT_MS + GAP_ABORT_MS;
  }
  if (ok && offAt && (int32_t)(millis() - offAt) >= 0) {
    ledcWrite(cfg::Pins::PIEZO, 0);
    offAt = 0;
  }
}

}  // namespace beep
