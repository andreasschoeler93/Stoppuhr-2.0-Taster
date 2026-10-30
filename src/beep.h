#pragma once

// Piezo-Pieper (nicht blockierend).
namespace beep {

void begin();
void click();
// Dumpfer, tieferer Ton (Signal vom Server)
void dull();
// Einzelner Warnton bei niedrigem Akku.
void lowBattery();
// Lauf abgebrochen: drei lange Pieptoene
void abortRun();
// Regelmaessig in loop() aufrufen, beendet laufende Toene.
void tick();

}  // namespace beep
