#pragma once

#include "measurements.h"
#include "netlink.h"

// Ansichten. Jede Ansicht zeichnet das komplette Bild inklusive unterer Leiste.
namespace screens {

void drawSplash(char letter, const measure::Data& m);
// Gibt die Hintergrundfarbe zurueck (fuer spaetere Updates der unteren Leiste).
uint16_t drawAssign(char letter, const measure::Data& m);

// Rollenansicht "Start" bzw. "Bahn N" mit Zeit, Name, Disziplin und letzter Zeit.
void drawRole(const netlink::RoleInfo& ri, char letter, const measure::Data& m);
// Aktualisiert nur das Zeitfeld (laufende Uhr), ohne das ganze Bild zu uebertragen.
void drawRoleClock(const netlink::RoleInfo& ri);

}  // namespace screens
