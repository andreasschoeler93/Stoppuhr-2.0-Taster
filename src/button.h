#pragma once

#include <Arduino.h>

// Haupttaster mit Entprellung.
namespace button {

void begin();
// Gibt true zurueck, wenn der Taster seit dem letzten Aufruf gedrueckt wurde.
// pressTimeUs: esp_timer-Zeit (Mikrosekunden) der Flanke, nicht des Abfragens.
bool pressed(int64_t* pressTimeUs);
bool pending();
// Bleibt bis zum Neustart gesetzt, wenn eine Flanke mangels Platz verloren ging.
bool overflowed();
// Aktueller elektrischer Zustand; die Funktion blockiert nicht.
bool down();
// Zeitstempel der aktuellen, entprellten Druckphase; 0, wenn nicht gedrueckt.
int64_t downSinceUs();

}  // namespace button
