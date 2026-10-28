#pragma once

#include <Arduino.h>

// Haupttaster mit Entprellung.
namespace button {

void begin();
// Gibt true zurueck, wenn der Taster seit dem letzten Aufruf gedrueckt wurde.
// pressTimeUs: esp_timer-Zeit (Mikrosekunden) der Flanke, nicht des Abfragens.
bool pressed(int64_t* pressTimeUs);
// Aktueller elektrischer Zustand; die Funktion blockiert nicht.
bool down();

}  // namespace button
