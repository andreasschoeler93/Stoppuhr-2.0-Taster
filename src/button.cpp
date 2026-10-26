#include "button.h"

#include "config.h"

namespace button {
namespace {

// Zeitstempel kommt direkt aus der Flanke (Interrupt), nicht aus dem loop().
volatile int64_t lastEdgeUs = 0;
volatile int64_t pressUs = 0;
volatile bool pending = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Eine fallende Flanke zaehlt nur nach Ruhe auf der Leitung (Entprellung);
// jede Flanke, auch das Loslassen, startet die Ruhezeit neu.
void IRAM_ATTR onEdge() {
  int64_t now = esp_timer_get_time();
  bool low = digitalRead(cfg::Pins::BUTTON) == LOW;
  if (low && now - lastEdgeUs >= (int64_t)cfg::Timing::DEBOUNCE_MS * 1000 && !pending) {
    pressUs = now;
    pending = true;
  }
  lastEdgeUs = now;
}

}  // namespace

void begin() {
  pinMode(cfg::Pins::BUTTON, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(cfg::Pins::BUTTON), onEdge, CHANGE);
}

bool pressed(int64_t* pressTimeUs) {
  bool hit = false;
  portENTER_CRITICAL(&mux);
  if (pending) {
    pending = false;
    if (pressTimeUs) *pressTimeUs = pressUs;
    hit = true;
  }
  portEXIT_CRITICAL(&mux);
  return hit;
}

}  // namespace button
