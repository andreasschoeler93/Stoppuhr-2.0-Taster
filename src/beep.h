#pragma once

// Piezo-Pieper (nicht blockierend).
namespace beep {

void begin();
void click();
// Regelmaessig in loop() aufrufen, beendet laufende Toene.
void tick();

}  // namespace beep
