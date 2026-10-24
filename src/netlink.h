#pragma once

#include <Arduino.h>

// WLAN-Schnittstelle zum Pi (POST /api/triggers). Senden laeuft in einem
// eigenen Task, loop() blockiert nie.
namespace netlink {

void begin();
// Legt einen Trigger in die Warteschlange (nicht blockierend).
// Gibt false zurueck, wenn er durch Entprellung verworfen wurde.
bool sendTrigger();
size_t queued();

}  // namespace netlink
