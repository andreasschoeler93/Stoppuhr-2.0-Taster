#include "button.h"

#include "config.h"

namespace button {
namespace {

bool lastRead = true;
bool stable = true;
uint32_t lastChangeMs = 0;

}  // namespace

void begin() {
  pinMode(cfg::Pins::BUTTON, INPUT_PULLUP);
}

bool pressed() {
  bool readNow = digitalRead(cfg::Pins::BUTTON);
  if (readNow != lastRead) {
    lastRead = readNow;
    lastChangeMs = millis();
  }

  if (millis() - lastChangeMs >= cfg::Timing::DEBOUNCE_MS && readNow != stable) {
    stable = readNow;
    return !stable;
  }
  return false;
}

}  // namespace button
