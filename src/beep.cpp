#include "beep.h"

#include "config.h"

namespace beep {
namespace {

constexpr uint8_t RES = 10;
constexpr uint16_t FREQ_CLICK = 4200;
constexpr uint16_t DUR_CLICK_MS = 55;
bool ok = false;
uint32_t offAt = 0;

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
  if (!ok) return;
  ledcDetach(cfg::Pins::PIEZO);
  ok = ledcAttach(cfg::Pins::PIEZO, FREQ_CLICK, RES);
  if (!ok) return;
  ledcWrite(cfg::Pins::PIEZO, (uint16_t)((1U << RES) * 0.50f));
  offAt = millis() + DUR_CLICK_MS;
}

void tick() {
  if (ok && offAt && (int32_t)(millis() - offAt) >= 0) {
    ledcWrite(cfg::Pins::PIEZO, 0);
    offAt = 0;
  }
}

}  // namespace beep
