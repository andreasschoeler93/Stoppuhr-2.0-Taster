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
// 0 = nicht verbunden, 1..4 = Signalstaerke
uint8_t wifiLevel();
// Signalstaerke in dBm (0, wenn nicht verbunden)
int wifiRssi();

}  // namespace netlink
