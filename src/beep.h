#pragma once

// Piezo-Pieper (nicht blockierend).
namespace beep {

void begin();
void click();
// Dumpfer, tieferer Ton (Signal vom Server)
void dull();
// Lauf abgebrochen: drei lange Pieptoene
void abortRun();
// Regelmaessig in loop() aufrufen, beendet laufende Toene.
void tick();

}  // namespace beep
