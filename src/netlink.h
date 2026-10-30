#pragma once

#include <Arduino.h>

// WLAN-Schnittstelle zum Pi (POST /api/triggers). Senden laeuft in einem
// eigenen Task, loop() blockiert nie.
namespace netlink {

void begin();
// Legt einen Trigger in die Warteschlange (nicht blockierend).
// Gibt false zurueck, wenn er durch Entprellung verworfen wurde.
// pressUs: esp_timer-Zeit (Mikrosekunden) des Tastendrucks.
bool sendTrigger(int64_t pressUs);
// Legt die Anforderung zum Abbrechen und Wiederholen des aktuellen Laufs in die Warteschlange.
bool sendAbortRun(int64_t pressUs);
size_t queued();
// 0 = nicht verbunden, 1..4 = Signalstaerke
uint8_t wifiLevel();
// Signalstaerke in dBm (0, wenn nicht verbunden)
int wifiRssi();
// Vom Server vergebener Buchstabe (A-J); '?' bis zur Anmeldung
char letter();
bool shutdownRequested();
// true, solange ein Firmware-Update vom Pi laeuft; percent = Fortschritt 0..100
bool otaActive(uint8_t& percent);

// Anzeigedaten der Rolle (Starter/Bahn), vom Server per GET /api/display geholt.
struct RoleInfo {
  enum Role : uint8_t { NONE = 0, STARTER = 1, LANE = 2, FINISH = 3 };
  enum State : uint8_t { READY = 0, RUNNING = 1, STOPPED = 2, INACTIVE = 3 };
  uint8_t role = NONE;
  // Bereitschaft: Tausch vorgemerkt, 0 = keiner, 1..N = Bahn, PENDING_STARTER = Starter, PENDING_FINISH = Zieleinlauf
  static constexpr uint8_t PENDING_STARTER = 255;
  static constexpr uint8_t PENDING_FINISH = 254;
  uint8_t pendingLane = 0;
  uint8_t state = READY;
  uint8_t lane = 0;
  uint16_t run = 0;
  uint16_t attempt = 1;
  int32_t elapsedMs = 0;  // Laufzeit zum Empfangszeitpunkt (nur RUNNING)
  int32_t timeMs = -1;    // gestoppte Zeit, -1 = keine
  uint32_t recvMs = 0;    // millis() beim Empfang
  int32_t lastMs = -1;    // letzte abgeschlossene Zeit, -1 = keine
  uint16_t lastRun = 0;
  uint16_t lastAttempt = 1;
  uint32_t seq = 0;       // zaehlt hoch, wenn sich der Inhalt (ausser der Uhr) aendert
  char name[32] = "";
  char discipline[32] = "";
  char lastName[32] = "";
  // Nur Starter: Bahnen des Laufs
  struct Lane {
    uint8_t number;
    uint8_t state;  // READY = wartet, RUNNING = laeuft, STOPPED = gestoppt
    int32_t ms;     // nur bei STOPPED
  };
  static constexpr uint8_t MAX_LANES = 10;
  uint8_t laneCount = 0;
  Lane lanes[MAX_LANES] = {};
};

// Kopie der letzten Anzeigedaten; false, solange noch keine Antwort kam.
bool roleInfo(RoleInfo& out);

}  // namespace netlink
