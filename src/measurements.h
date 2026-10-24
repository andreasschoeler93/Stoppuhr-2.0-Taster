#pragma once

#include <Arduino.h>

// Messwerte: Temperatur, Luftfeuchte und Akku.
namespace measure {

enum Changed : uint8_t {
  CHANGED_NONE = 0,
  CHANGED_BAR = 1,      // Temperatur/Feuchte/Akku-Icon der unteren Leiste
  CHANGED_BATTERY = 2,  // Akkustand der Hauptansicht
};

struct Data {
  bool ahtOk = false;
  float temperature = NAN;
  float humidity = NAN;
  float vbat = NAN;
  uint8_t vbatLevel = 0;  // 0..4 fuer das Akku-Icon
  uint8_t vbatPercent = 0;
  uint32_t rawAverage = 0;
  float adcVoltage = 0;
};

void begin();
// Gibt eine Bitmaske aus Changed zurueck.
uint8_t update();
const Data& data();

}  // namespace measure
