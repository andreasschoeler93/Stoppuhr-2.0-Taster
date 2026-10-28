#include "netlink.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_mac.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <time.h>

#include "config.h"
#include "measurements.h"

namespace netlink {
namespace {

struct Event {
  uint32_t id;
  int64_t pressUs;
  bool abortRun;
};

Event queue[cfg::Net::QUEUE_MAX];
size_t qHead = 0;
size_t qCount = 0;
uint32_t counter = 0;
uint32_t lastTriggerMs = 0;
bool haveTrigger = false;
portMUX_TYPE qMux = portMUX_INITIALIZER_UNLOCKED;
String deviceMac;
String deviceName;
volatile char tasterLetter = '?';
volatile bool shutdownRequestedFlag = false;
RoleInfo roleState;
bool roleValid = false;
portMUX_TYPE roleMux = portMUX_INITIALIZER_UNLOCKED;
// Pi-Adresse einmal aufloesen und merken: ein Namensabruf pro Anfrage kostete teils Sekunden.
volatile uint32_t piIp = 0;
volatile bool pollNow = false;

String baseUrl() {
  uint32_t ip = piIp;
  if (ip == 0) {
    IPAddress a;
    if (WiFi.status() == WL_CONNECTED && WiFi.hostByName(cfg::Net::PI_HOST, a) == 1 && (uint32_t)a != 0) {
      piIp = ip = (uint32_t)a;
    }
  }
  String host = ip ? IPAddress(ip).toString() : String(cfg::Net::PI_HOST);
  return String("http://") + host + ":" + String(cfg::Net::PI_PORT);
}

// Bei Verbindungsfehler die Adresse beim naechsten Mal neu aufloesen.
void forgetIp(int code) {
  if (code <= 0) piIp = 0;
}

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

// Epoch-Zeit in ms zum Zeitpunkt des Tastendrucks (in Mikrosekunden gerechnet, dann gerundet);
// 0, wenn Uhr noch nicht synchron.
int64_t epochMsFor(int64_t pressUs) {
  int64_t espNow = esp_timer_get_time();
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  if (tv.tv_sec < 1700000000) return 0;
  int64_t nowUs = (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
  return (nowUs - (espNow - pressUs) + 500) / 1000;
}

// Rueckgabe: HTTP-Status oder <=0 bei Netzwerkfehler/Timeout.
int postTrigger(const Event& e, int64_t ts) {
  String url = baseUrl() + "/api/triggers";
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
  forgetIp(code);
  return code;
}

int postAbortRun(const Event& e) {
  String url = baseUrl() + "/api/abort-run";
  String requestId = deviceMac + ":" + String((unsigned long long)e.pressUs);
  String body = String("{\"mac\":\"") + deviceMac + "\",\"request_id\":\"" + requestId + "\"}";
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
  forgetIp(code);
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
  // Feldnamen wie im Stoppuhr-Backend (battery = Prozent, temp, humidity);
  // ts fehlt absichtlich, der Server setzt dann die Empfangszeit.
  // clock_ms (Uhr des Tasters, nur wenn synchron) dient dem Server, den Uhrversatz auszugleichen.
  char clock[40] = "";
  int64_t nowEpoch = epochMsFor(esp_timer_get_time());
  if (nowEpoch != 0) snprintf(clock, sizeof(clock), ",\"clock_ms\":%llu", (unsigned long long)nowEpoch);
  snprintf(body, sizeof(body),
           "{\"mac\":\"%s\",\"battery\":%u,\"temp\":%s,\"humidity\":%s,"
           "\"vbat\":%.3f,\"rssi\":%d,\"fw\":\"%s\",\"uptime_s\":%lu%s}",
           deviceMac.c_str(), (unsigned)m.vbatPercent, t, h, m.vbatCalc,
           WiFi.RSSI(), FW_VERSION, (unsigned long)(millis() / 1000), clock);

  HTTPClient http;
  http.setConnectTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  if (!http.begin(baseUrl() + "/api/vitals")) return;
  http.addHeader("Content-Type", "application/json");
  if (cfg::Net::API_KEY[0]) http.addHeader("X-Api-Key", cfg::Net::API_KEY);
  int code = http.POST(body);
  http.end();
#if DBG
  Serial.printf("[NET] vitals http=%d\n", code);
#endif
}

// Meldet den Taster beim Server an (POST /api/taster). true: angemeldet.
bool registerTaster() {
  if (WiFi.status() != WL_CONNECTED) return false;
  char body[96];
  snprintf(body, sizeof(body), "{\"mac\":\"%s\",\"name\":\"%s\"}",
           deviceMac.c_str(), deviceName.c_str());
  HTTPClient http;
  http.setConnectTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  if (!http.begin(baseUrl() + "/api/taster")) return false;
  http.addHeader("Content-Type", "application/json");
  if (cfg::Net::API_KEY[0]) http.addHeader("X-Api-Key", cfg::Net::API_KEY);
  int code = http.POST(body);
  // Antwort: {"ok":true,"taster":{...,"letter":"A",...}}; der Server vergibt den Buchstaben
  if (code == 200) {
    String resp = http.getString();
    int p = resp.indexOf("\"letter\":\"");
    if (p >= 0 && p + 10 < (int)resp.length() && resp[p + 10] >= 'A' && resp[p + 10] <= 'Z') {
      tasterLetter = resp[p + 10];
    }
  }
  http.end();
#if DBG
  Serial.printf("[NET] register name=%s letter=%c http=%d\n", deviceName.c_str(), tasterLetter, code);
#endif
  return code == 200;
}

// Minimaler Zugriff auf flaches JSON ("key":wert); der Server sendet nur ASCII ohne Escapes.
const char* jsonValue(const String& s, const char* key) {
  String pat = String("\"") + key + "\":";
  int p = s.indexOf(pat);
  if (p < 0) return nullptr;
  p += pat.length();
  while (p < (int)s.length() && s[p] == ' ') p++;
  return s.c_str() + p;
}

int32_t jsonInt(const String& s, const char* key, int32_t fallback) {
  const char* v = jsonValue(s, key);
  if (!v || *v == 'n' || *v == '"') return fallback;  // null oder Text
  return (int32_t)atol(v);
}

bool jsonBool(const String& s, const char* key) {
  const char* v = jsonValue(s, key);
  return v && strncmp(v, "true", 4) == 0;
}

void jsonStr(const String& s, const char* key, char* out, size_t size) {
  out[0] = '\0';
  const char* v = jsonValue(s, key);
  if (!v || *v != '"') return;
  size_t i = 0;
  for (++v; *v && *v != '"' && i < size - 1; ++v) {
    if (*v >= 32 && *v < 127) {
      out[i++] = *v;
    } else if ((uint8_t)v[0] == 0xC3 && v[1] && v[1] != '"') {
      switch ((uint8_t)v[1]) {
        case 0x84: out[i++] = '\x04'; break;
        case 0x96: out[i++] = '\x05'; break;
        case 0x9C: out[i++] = '\x06'; break;
        case 0xA4: out[i++] = '\x01'; break;
        case 0xB6: out[i++] = '\x02'; break;
        case 0xBC: out[i++] = '\x03'; break;
        default: out[i++] = '?'; break;
      }
      ++v;
    } else {
      out[i++] = '?';
    }
  }
  out[i] = '\0';
}

// Holt Rolle und Anzeigedaten (GET /api/display?mac=...). Gibt die aktuelle Rolle zurueck.
uint8_t pollDisplay() {
  if (WiFi.status() != WL_CONNECTED) return RoleInfo::NONE;
  HTTPClient http;
  http.setConnectTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  http.setTimeout(cfg::Net::HTTP_TIMEOUT_MS);
  if (!http.begin(baseUrl() + "/api/display?mac=" + deviceMac)) return roleState.role;
  if (cfg::Net::API_KEY[0]) http.addHeader("X-Api-Key", cfg::Net::API_KEY);
  int code = http.GET();
  String body = code == 200 ? http.getString() : String();
  http.end();
  forgetIp(code);
  if (code != 200) return roleState.role;
  if (jsonBool(body, "shutdown")) shutdownRequestedFlag = true;

  RoleInfo r;
  char roleText[12];
  jsonStr(body, "role", roleText, sizeof(roleText));
  r.role = !strcmp(roleText, "starter") ? RoleInfo::STARTER
         : !strcmp(roleText, "lane")    ? RoleInfo::LANE
                                        : RoleInfo::NONE;
  char stateText[12];
  jsonStr(body, "state", stateText, sizeof(stateText));
  r.state = !strcmp(stateText, "running")  ? RoleInfo::RUNNING
          : !strcmp(stateText, "stopped")  ? RoleInfo::STOPPED
          : !strcmp(stateText, "inactive") ? RoleInfo::INACTIVE
                                           : RoleInfo::READY;
  r.lane = (uint8_t)jsonInt(body, "lane", 0);
  r.run = (uint16_t)jsonInt(body, "run", 0);
  r.attempt = (uint16_t)jsonInt(body, "attempt", 1);
  r.elapsedMs = jsonInt(body, "elapsed_ms", 0);
  r.timeMs = jsonInt(body, "time_ms", -1);
  r.lastMs = jsonInt(body, "last_ms", -1);
  r.lastRun = (uint16_t)jsonInt(body, "last_run", 0);
  r.lastAttempt = (uint16_t)jsonInt(body, "last_attempt", 1);
  jsonStr(body, "name", r.name, sizeof(r.name));
  jsonStr(body, "discipline", r.discipline, sizeof(r.discipline));
  jsonStr(body, "last_name", r.lastName, sizeof(r.lastName));
  if (r.role == RoleInfo::STARTER) {
    const char* v = jsonValue(body, "lanes");
    if (v && *v == '"') {
      ++v;
      while (*v && *v != '"' && r.laneCount < RoleInfo::MAX_LANES) {
        RoleInfo::Lane& l = r.lanes[r.laneCount];
        l.number = (uint8_t)atoi(v);
        const char* c = strchr(v, ',');
        if (!c || !c[1]) break;
        l.state = c[1] == 's' ? RoleInfo::STOPPED : c[1] == 'r' ? RoleInfo::RUNNING : RoleInfo::READY;
        c = strchr(c + 1, ',');
        l.ms = c ? (int32_t)atol(c + 1) : -1;
        r.laneCount++;
        v = strchr(v, ';');
        if (!v) break;
        ++v;
      }
    }
  }
  r.recvMs = millis();

  portENTER_CRITICAL(&roleMux);
  bool same = roleValid && r.role == roleState.role && r.state == roleState.state &&
              r.lane == roleState.lane && r.run == roleState.run && r.attempt == roleState.attempt &&
              r.timeMs == roleState.timeMs && r.lastMs == roleState.lastMs &&
              r.lastRun == roleState.lastRun && r.lastAttempt == roleState.lastAttempt &&
              !strcmp(r.name, roleState.name) && !strcmp(r.discipline, roleState.discipline) &&
              !strcmp(r.lastName, roleState.lastName) && r.laneCount == roleState.laneCount &&
              !memcmp(r.lanes, roleState.lanes, sizeof(r.lanes));
  r.seq = roleState.seq + (same ? 0 : 1);
  roleState = r;
  roleValid = true;
  portEXIT_CRITICAL(&roleMux);
#if DBG
  if (!same) Serial.printf("[NET] rolle=%s state=%s bahn=%u lauf=%u\n", roleText, stateText, r.lane, r.run);
#endif
  return r.role;
}

// Eine Zustellung mit Retry/Backoff. true: Event erledigt (entfernen).
bool deliver(const Event& e) {
  if (WiFi.status() != WL_CONNECTED) return false;
  int64_t ts = e.abortRun ? 0 : epochMsFor(e.pressUs);
  if (!e.abortRun && ts == 0) return false;
  uint32_t backoff = cfg::Net::RETRY_BACKOFF_MS;
  for (int attempt = 1; attempt <= cfg::Net::RETRY_COUNT; attempt++) {
    int code = e.abortRun ? postAbortRun(e) : postTrigger(e, ts);
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

// Zweites WLAN (Bad) ist nur aktiv, wenn eine SSID dafuer konfiguriert ist.
bool badWifiConfigured() { return cfg::Net::WIFI2_SSID[0] != '\0'; }

// Taster mit Rolle gehoeren ins Bad-WLAN, alle anderen ins Koffer-WLAN.
// Die Rolle bleibt gespeichert, auch wenn das Bad-WLAN gerade nicht erreichbar ist.
bool wantBadWifi() {
  if (!badWifiConfigured()) return false;
  portENTER_CRITICAL(&roleMux);
  bool want = roleValid && roleState.role != RoleInfo::NONE;
  portEXIT_CRITICAL(&roleMux);
  return want;
}

void connectProfile(bool bad) {
  WiFi.disconnect();
  if (bad) WiFi.begin(cfg::Net::WIFI2_SSID, cfg::Net::WIFI2_PASS);
  else WiFi.begin(cfg::Net::WIFI_SSID, cfg::Net::WIFI_PASS);
}

void netTask(void*) {
  uint32_t lastWifiTryMs = 0;
  uint32_t lastVitalsMs = 0;
  uint32_t lastRegisterMs = 0;
  bool registered = false;
  bool onBadWifi = false;
  for (;;) {
    bool wantBad = wantBadWifi();
    if (wantBad != onBadWifi) {
      // Rollenwechsel: sofort ins passende WLAN, auch wenn es noch nicht erreichbar ist
      onBadWifi = wantBad;
      registered = false;
      lastWifiTryMs = millis();
      connectProfile(onBadWifi);
#if DBG
      Serial.printf("[WIFI] wechsle ins %s-WLAN\n", onBadWifi ? "Bad" : "Koffer");
#endif
    } else if (WiFi.status() != WL_CONNECTED && millis() - lastWifiTryMs >= cfg::Net::WIFI_RETRY_MS) {
      lastWifiTryMs = millis();
      connectProfile(onBadWifi);
    }
    Event e;
    if (peek(e)) {
      // Reihenfolge bleibt erhalten: nur der Kopf wird gesendet.
      if (deliver(e)) {
        popIfHead(e.id);
          pollNow = true;
      } else if (!cfg::Net::ENABLE_OFFLINE_QUEUE) {
        logLine(e.id, 0, 0, "dropped");
        popIfHead(e.id);
      } else {
        vTaskDelay(pdMS_TO_TICKS(cfg::Net::FLUSH_RETRY_MS));
      }
    } else {
      // Anmeldung regelmaessig wiederholen: der Server vergisst Taster z. B. nach einem Reset
      if (millis() - lastRegisterMs >= (registered ? cfg::Net::REREGISTER_MS : cfg::Net::REGISTER_RETRY_MS)) {
        lastRegisterMs = millis();
        registered = registerTaster();
      }
      if (millis() - lastVitalsMs >= cfg::Net::VITALS_INTERVAL_MS) {
        lastVitalsMs = millis();
        postVitals();
      }
      vTaskDelay(pdMS_TO_TICKS(20));
    }
  }
}

// Eigener Task nur fuer die Anzeige: wird nicht von Senden, Anmeldung oder Vitaldaten aufgehalten.
void displayTask(void*) {
  uint32_t lastPollMs = 0;
  uint8_t role = RoleInfo::NONE;
  for (;;) {
    uint32_t interval = role != RoleInfo::NONE ? cfg::Net::DISPLAY_POLL_MS : cfg::Net::DISPLAY_POLL_IDLE_MS;
    if (pollNow || millis() - lastPollMs >= interval) {
      pollNow = false;
      lastPollMs = millis();
      role = pollDisplay();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

}  // namespace

void begin() {
  WiFi.mode(WIFI_STA);
  // Ohne WLAN-Energiesparen antwortet der Chip sofort statt erst im naechsten Beacon-Intervall
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  // Auch WPA/WPA2-Mischbetrieb akzeptieren (sonst Abbruch mit Grund 211)
  WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK);
#if DBG
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t info) {
    Serial.printf("[WIFI] getrennt, Grund=%d (2=Auth, 15=4-Wege-Handshake, 201=SSID nicht gefunden, 202=Auth fehlgeschlagen)\n",
                  info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) {
    Serial.printf("[WIFI] verbunden: SSID=%s IP=%s RSSI=%d Kanal=%d\n", WiFi.SSID().c_str(),
                  WiFi.localIP().toString().c_str(), WiFi.RSSI(), WiFi.channel());
  }, ARDUINO_EVENT_WIFI_STA_GOT_IP);
  Serial.printf("[WIFI] suche SSID=\"%s\" (%u Zeichen Passwort)\n", cfg::Net::WIFI_SSID,
                (unsigned)strlen(cfg::Net::WIFI_PASS));
#endif
  // Verstecktes WLAN: alle Kanaele absuchen, bester Empfang zuerst
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  if (cfg::Net::DEVICE_MAC[0]) {
    deviceMac = cfg::Net::DEVICE_MAC;
  } else {
    // Direkt aus dem eFuse lesen; WiFi.macAddress() liefert vor dem WLAN-Start 00:..:00
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    deviceMac = buf;
  }
  // Eindeutiger Name aus den letzten drei MAC-Bytes, z. B. "T-DD:EE:01" -> "T-DDEE01"
  deviceName = cfg::Net::DEVICE_NAME[0] ? String(cfg::Net::DEVICE_NAME) : String("T-");
  if (!cfg::Net::DEVICE_NAME[0]) {
    for (size_t i = deviceMac.length() - 8; i < deviceMac.length(); ++i) {
      if (deviceMac[i] != ':') deviceName += deviceMac[i];
    }
  }
#if DBG
  Serial.printf("[NET] mac=%s name=%s\n", deviceMac.c_str(), deviceName.c_str());
#endif
  WiFi.begin(cfg::Net::WIFI_SSID, cfg::Net::WIFI_PASS);
  // Haeufiger abgleichen als der Standard (1 h), damit die Quarz-Drift klein bleibt
  sntp_set_sync_interval(cfg::Net::NTP_SYNC_INTERVAL_MS);
  configTime(0, 0, cfg::Net::NTP_SERVER);
  xTaskCreate(netTask, "netlink", 8192, nullptr, 1, nullptr);
  xTaskCreate(displayTask, "display", 8192, nullptr, 1, nullptr);
}

bool enqueueEvent(int64_t pressUs, bool abortRun) {
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
    queue[(qHead + qCount) % cfg::Net::QUEUE_MAX] = {id, pressUs, abortRun};
    qCount++;
    queuedOk = true;
  }
  portEXIT_CRITICAL(&qMux);
  if (dropped) logLine(id, 0, 0, "dropped");
  return queuedOk;
}

bool sendTrigger(int64_t pressUs) {
  uint32_t now = millis();
  if (haveTrigger && now - lastTriggerMs < cfg::Net::TRIGGER_MIN_INTERVAL_MS) return false;
  haveTrigger = true;
  lastTriggerMs = now;
  return enqueueEvent(pressUs, false);
}

bool sendAbortRun(int64_t pressUs) {
  return enqueueEvent(pressUs, true);
}

size_t queued() { return qCount; }

int wifiRssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0; }

char letter() { return tasterLetter; }
bool shutdownRequested() { return shutdownRequestedFlag; }

bool roleInfo(RoleInfo& out) {
  portENTER_CRITICAL(&roleMux);
  out = roleState;
  bool ok = roleValid;
  portEXIT_CRITICAL(&roleMux);
  return ok;
}

uint8_t wifiLevel() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  int rssi = WiFi.RSSI();
  if (rssi >= -55) return 4;
  if (rssi >= -65) return 3;
  if (rssi >= -75) return 2;
  return 1;
}

}  // namespace netlink
