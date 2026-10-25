#include "netlink.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "measurements.h"

namespace netlink {
namespace {

struct Event {
  uint32_t id;
  uint32_t pressMs;
};

Event queue[cfg::Net::QUEUE_MAX];
size_t qHead = 0;
size_t qCount = 0;
uint32_t counter = 0;
uint32_t lastTriggerMs = 0;
bool haveTrigger = false;
portMUX_TYPE qMux = portMUX_INITIALIZER_UNLOCKED;
String deviceMac;

bool peek(Event& e) {
  bool ok = false;
  portENTER_CRITICAL(&qMux);
  if (qCount) {
    e = queue[qHead];
    ok = true;
  }
  portEXIT_CRITICAL(&qMux);
  return ok;
}

// Entfernt den Kopf nur, wenn es noch das gesendete Event ist. Lief die Queue
// waehrend des Sendens voll, wurde der Kopf bereits verworfen.
void popIfHead(uint32_t id) {
  portENTER_CRITICAL(&qMux);
  if (qCount && queue[qHead].id == id) {
    qHead = (qHead + 1) % cfg::Net::QUEUE_MAX;
    qCount--;
  }
  portEXIT_CRITICAL(&qMux);
}

void logLine(uint32_t id, int attempt, int status, const char* action) {
#if DBG
  Serial.printf("[NET] event=%lu attempt=%d http=%d action=%s\n", (unsigned long)id, attempt, status, action);
#endif
}

// Epoch-Zeit in ms zum Zeitpunkt des Tastendrucks; 0, wenn Uhr noch nicht synchron.
int64_t epochMsFor(uint32_t pressMs) {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1700000000) return 0;
  int64_t now = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
  return now - (int64_t)(uint32_t)(millis() - pressMs);
}

// Rueckgabe: HTTP-Status oder <=0 bei Netzwerkfehler/Timeout.
int postTrigger(const Event& e, int64_t ts) {
  String url = String(cfg::Net::PI_BASE_URL) + "/api/triggers";
  String body = String("{\"mac\":\"") + deviceMac + "\",\"ts\":" + String((unsigned long long)ts) +
                ",\"stopwatch_ms\":null,\"run_id\":null}";
  HTTPClient http;
  http.setConnectTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  if (!http.begin(url)) return -1;
  http.addHeader("Accept", "application/json");
  http.addHeader("Content-Type", "application/json");
  if (cfg::Net::API_KEY[0]) http.addHeader("X-Api-Key", cfg::Net::API_KEY);
  int code = http.POST(body);
  http.end();
  return code;
}

// Vitaldaten (Messwerte) senden. Fehlschlaege werden verworfen, der naechste Wert kommt bald.
void postVitals() {
  if (WiFi.status() != WL_CONNECTED) return;
  const measure::Data m = measure::data();
  char body[256];
  char t[16], h[16];
  if (m.ahtOk && isfinite(m.temperature) && isfinite(m.humidity)) {
    snprintf(t, sizeof(t), "%.1f", m.temperature);
    snprintf(h, sizeof(h), "%.1f", m.humidity);
  } else {
    strcpy(t, "null");
    strcpy(h, "null");
  }
  snprintf(body, sizeof(body),
           "{\"mac\":\"%s\",\"fw\":\"%s\",\"temp_c\":%s,\"humidity\":%s,"
           "\"vbat\":%.2f,\"battery_pct\":%u,\"rssi\":%d,\"uptime_s\":%lu}",
           deviceMac.c_str(), FW_VERSION, t, h, m.vbatCalc, (unsigned)m.vbatPercent,
           WiFi.RSSI(), (unsigned long)(millis() / 1000));

  HTTPClient http;
  http.setConnectTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  if (!http.begin(String(cfg::Net::PI_BASE_URL) + "/api/vitals")) return;
  http.addHeader("Content-Type", "application/json");
  if (cfg::Net::API_KEY[0]) http.addHeader("X-Api-Key", cfg::Net::API_KEY);
  int code = http.POST(body);
  http.end();
#if DBG
  Serial.printf("[NET] vitals http=%d\n", code);
#endif
}

// Eine Zustellung mit Retry/Backoff. true: Event erledigt (entfernen).
bool deliver(const Event& e) {
  int64_t ts = epochMsFor(e.pressMs);
  if (ts == 0 || WiFi.status() != WL_CONNECTED) return false;
  uint32_t backoff = cfg::Net::RETRY_BACKOFF_MS;
  for (int attempt = 1; attempt <= cfg::Net::RETRY_COUNT; attempt++) {
    int code = postTrigger(e, ts);
    if (code == 200) {
      logLine(e.id, attempt, code, "sent");
      return true;
    }
    if (code == 409) {  // Duplikat: Server kennt das Event bereits
      logLine(e.id, attempt, code, "sent");
      return true;
    }
    if (code >= 400 && code < 500) {  // Clientfehler: Retry sinnlos
      logLine(e.id, attempt, code, "dropped");
      return true;
    }
    if (attempt < cfg::Net::RETRY_COUNT) {
      logLine(e.id, attempt, code, "retried");
      vTaskDelay(pdMS_TO_TICKS(backoff));
      backoff *= 2;
    } else {
      logLine(e.id, attempt, code, "buffered");
    }
  }
  return false;
}

void netTask(void*) {
  uint32_t lastWifiTryMs = 0;
  uint32_t lastVitalsMs = 0;
  for (;;) {
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTryMs >= cfg::Net::WIFI_RETRY_MS) {
      lastWifiTryMs = millis();
      WiFi.disconnect();
      WiFi.begin(cfg::Net::WIFI_SSID, cfg::Net::WIFI_PASS);
    }
    Event e;
    if (peek(e)) {
      // Reihenfolge bleibt erhalten: nur der Kopf wird gesendet.
      if (deliver(e)) {
        popIfHead(e.id);
      } else if (!cfg::Net::ENABLE_OFFLINE_QUEUE) {
        logLine(e.id, 0, 0, "dropped");
        popIfHead(e.id);
      } else {
        vTaskDelay(pdMS_TO_TICKS(cfg::Net::FLUSH_RETRY_MS));
      }
    } else {
      if (millis() - lastVitalsMs >= cfg::Net::VITALS_INTERVAL_MS) {
        lastVitalsMs = millis();
        postVitals();
      }
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

}  // namespace

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  // Verstecktes WLAN: alle Kanaele absuchen, bester Empfang zuerst
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  deviceMac = cfg::Net::DEVICE_MAC[0] ? String(cfg::Net::DEVICE_MAC) : WiFi.macAddress();
  WiFi.begin(cfg::Net::WIFI_SSID, cfg::Net::WIFI_PASS);
  configTime(0, 0, cfg::Net::NTP_SERVER);
  xTaskCreate(netTask, "netlink", 8192, nullptr, 1, nullptr);
}

bool sendTrigger() {
  uint32_t now = millis();
  if (haveTrigger && now - lastTriggerMs < cfg::Net::TRIGGER_MIN_INTERVAL_MS) return false;
  haveTrigger = true;
  lastTriggerMs = now;

  uint32_t id = ++counter;
  bool queuedOk = false;
  bool dropped = false;
  portENTER_CRITICAL(&qMux);
  if (qCount == cfg::Net::QUEUE_MAX) {  // Drop-Strategie: aeltestes Event verwerfen
    qHead = (qHead + 1) % cfg::Net::QUEUE_MAX;
    qCount--;
    dropped = true;
  }
  if (cfg::Net::ENABLE_OFFLINE_QUEUE || qCount == 0) {
    queue[(qHead + qCount) % cfg::Net::QUEUE_MAX] = {id, now};
    qCount++;
    queuedOk = true;
  }
  portEXIT_CRITICAL(&qMux);
  if (dropped) logLine(id, 0, 0, "dropped");
  return queuedOk;
}

size_t queued() { return qCount; }

int wifiRssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }

uint8_t wifiLevel() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

}  // namespace netlink
