#include "button.h"

#include "config.h"

namespace button {
namespace {

// Zeitstempel kommt direkt aus der Flanke (Interrupt), nicht aus dem loop().
// Mehrere schnell aufeinanderfolgende Druecke bleiben in einem Ringpuffer erhalten.
constexpr uint8_t QUEUE_SIZE = 16;
volatile int64_t lastEdgeUs = 0;
volatile int64_t queue[QUEUE_SIZE];
volatile uint8_t head = 0;
volatile uint8_t count = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// Eine fallende Flanke zaehlt nur nach Ruhe auf der Leitung (Entprellung);
// jede Flanke, auch das Loslassen, startet die Ruhezeit neu.
void IRAM_ATTR onEdge() {
  int64_t now = esp_timer_get_time();
  bool low = digitalRead(cfg::Pins::BUTTON) == LOW;
  if (low && now - lastEdgeUs >= (int64_t)cfg::Timing::DEBOUNCE_MS * 1000 && count < QUEUE_SIZE) {
    queue[(head + count) % QUEUE_SIZE] = now;
    count = count + 1;
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
  if (count > 0) {
    if (pressTimeUs) *pressTimeUs = queue[head];
    head = (head + 1) % QUEUE_SIZE;
    count = count - 1;
    hit = true;
  }
  portEXIT_CRITICAL(&mux);
  return hit;
}

bool down() {
  return digitalRead(cfg::Pins::BUTTON) == LOW;
}

}  // namespace button
