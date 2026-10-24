#pragma once

// Haupttaster mit Entprellung.
namespace button {

void begin();
// Gibt true zurueck, wenn der Taster seit dem letzten Aufruf gedrueckt wurde.
bool pressed();

}  // namespace button
