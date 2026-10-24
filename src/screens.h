#pragma once

#include "measurements.h"

// Ansichten. Jede Ansicht zeichnet das komplette Bild inklusive unterer Leiste.
namespace screens {

void drawSplash(char letter, const measure::Data& m);
// Gibt die Hintergrundfarbe zurueck (fuer spaetere Updates der unteren Leiste).
uint16_t drawAssign(char letter, const measure::Data& m);

}  // namespace screens
