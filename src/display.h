#pragma once

#include <Adafruit_GFX.h>

#include "measurements.h"

// Display (ST7789) mit Bildspeicher: Es wird erst im RAM gezeichnet und das
// fertige Bild in einem Schritt uebertragen, damit nichts flackert.
namespace display {

void begin();
bool ready();
GFXcanvas16& canvas();
void flush();
// Uebertraegt nur die Zeilen y..y+h-1 des Bildspeichers (schnell, fuer die laufende Uhr).
void flushRows(int y, int h);

// Untere Leiste mit Temperatur, Luftfeuchte, Buchstabe, Akku-Icon und WLAN.
// Uebertraegt das Bild am Ende selbst.
void renderBottomBar(uint16_t accent, char letter, const measure::Data& m);

}  // namespace display
