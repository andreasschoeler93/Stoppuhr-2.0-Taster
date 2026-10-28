# Taster – Firmware

Die Firmware behält die originale Splash- und erste Assign-Ansicht bei:
Akku-Prozent und Buchstabe stehen wie zuvor im Hauptbereich, Temperatur und
Luftfeuchte bleiben in der unteren Leiste. Weitere Ansichten und die
Taster-Navigation sind vorerst deaktiviert.

## Hardware

Zielhardware ist ein Seeed XIAO ESP32-S3 mit ST7789-Display, AHT20,
Taster, Piezo-Buzzer und schaltbarem Akku-Spannungsteiler. Die verbindliche
Pinbelegung steht in [`docs/Programmier_Informationen`](docs/Programmier_Informationen).

Die Firmware mittelt acht ADC-Messungen und rechnet die ADC-Spannung mit dem
aktuell kalibrierten Faktor `2.382` in die Akkuspannung um. Der Prozentwert
ist eine Schätzung aus der Zellspannung.

## Versionierung

Schema `MAJOR.MINOR.PATCH`, die Version steht oben in `src/config.h`
(`FW_VERSION_MAJOR/MINOR/PATCH`) und erscheint auf dem Startbildschirm sowie
beim Start im seriellen Monitor.

- **PATCH**: Fehlerkorrekturen, Kalibrierwerte (z. B. Akkufaktor)
- **MINOR**: neue Funktionen oder Ansichten, abwärtskompatibel
- **MAJOR**: grundlegende Änderungen (Hardware, Bedienkonzept); `1.0.0` = erste fertige Version

Bei jedem Release: Version erhöhen, committen und mit `git tag vX.Y.Z` markieren.
Aktuell: `v0.10.6`.

Der Display-Bildspeicher belegt rund 131 KiB RAM. Kann er nicht angelegt
werden, meldet die Firmware `[GFX] ERROR` im seriellen Monitor und ueberspringt
Zeichenoperationen sicher.

## Projektstruktur

Eine Firmware, aufgeteilt in Module unter [`src/`](src):

| Datei | Aufgabe |
|---|---|
| `main.cpp` | Ablauf und Wechsel der Ansichten |
| `config.h` | Pins, Zeiten, Farben, Layout, Version, Akkufaktor |
| `measurements.*` | **Messwerte**: Temperatur, Luftfeuchte, Akku |
| `display.*` | Display, Bildspeicher, untere Leiste |
| `screens.*` | Ansichten (Splash, Zuordnung) |
| `netlink.*` | WLAN-Client: Trigger an den Pi senden, Offline-Queue |
| `button.*`, `beep.*` | Taster (Flanken-Interrupt mit Entprellung), Piezo |

Geplante Module (noch nicht angelegt, je ein eigener Minor-Schritt):

1. **Datenaustausch**: Startkarten
2. **Zuordnung**: Tasterzuordnung und Rolle
3. **Zeit**: Zeitstempel und Berechnungen
4. **Zeitsync**: Zeitsynchronisierung
5. **Backup**: Stempel-Backup
6. **Netzwerk**: WLAN-Management

## Taster ↔ Pi Schnittstelle

Der Starter unterscheidet einen kurzen Tastendruck von einem Langdruck:
Nach 2 Sekunden bricht ein Langdruck den laufenden Versuch ab und bereitet eine neue
Version desselben Laufs vor. Die neue Version kann erst nach Ablauf der Stopp-Sperre
mit einem kurzen Druck gestartet werden. Bahn-Taster behalten ihr normales
Start-/Stoppverhalten. Kurze Tastendrücke senden `POST /api/triggers`, der Langdruck
des Starters sendet `POST /api/abort-run` an den Pi (Repo `Stoppuhr`). Nur internes
WLAN/LAN, keine Cloud. Das Senden läuft in einem eigenen Task (`src/netlink.cpp`),
`loop()` blockiert nie.

Request (`Content-Type: application/json`, `Accept: application/json`,
`X-Api-Key` nur wenn konfiguriert):

```json
{"mac":"AA:BB:CC:DD:EE:01","ts":1761290000123,"stopwatch_ms":null,"run_id":null}
```

Antwort Erfolg: `{"ok":true,"received_at":1761290000150,"event_id":"..."}`;
Fehler: `{"ok":false,"error":"...","code":"..."}`.

Anforderung zum Abbrechen und Wiederholen:

```json
{"mac":"AA:BB:CC:DD:EE:01","request_id":"AA:BB:CC:DD:EE:01:123456789"}
```

### Konfiguration (`cfg::Net` in `src/config.h`)

| Variable | Default | Bedeutung |
|---|---|---|
| `WIFI_SSID`, `WIFI_PASS` | aus `src/secrets.h` | verstecktes, passwortgeschütztes WLAN des Pi |
| `PI_HOST` / `PI_PORT` | `<SECRET_PI_IP>` / 1999 | Pi-IP oder Name; ein Name wird nur einmal aufgelöst und die IP gemerkt |
| `NTP_SERVER` | `SECRET_PI_IP` | Zeitquelle für `ts` (Epoch-ms); Pi braucht einen NTP-Dienst (z. B. chrony) |
| `DEVICE_MAC` | leer = ESP32-MAC | Geräte-ID |
| `API_KEY` | aus `src/secrets.h`, leer = kein Header | optional, als `X-Api-Key` |
| `HTTP_TIMEOUT_MS` | 1500 | Timeout je Request |
| `RETRY_COUNT` | 3 | Versuche je Zustellung |
| `RETRY_BACKOFF_MS` | 200 | Start-Backoff, verdoppelt sich je Versuch |
| `ENABLE_OFFLINE_QUEUE` | true | Events puffern, wenn Senden scheitert |
| `QUEUE_MAX` | 32 | Queue-Größe (nur RAM); bei Überlauf wird das **älteste** Event verworfen |
| `TRIGGER_MIN_INTERVAL_MS` | 300 | Entprellung zusätzlich zur Hardware-Entprellung (40 ms) |

### Anmeldung und Vitaldaten

Nach dem Verbinden meldet sich der Taster mit `POST /api/taster`
(`{"mac":"...","name":"T-DDEE01"}`) an und wiederholt das alle 5 s, bis der Pi
mit 200 antwortet, und danach alle 60 s erneut (der Server vergisst Taster z. B. nach einem Reset; die Anmeldung ist wiederholbar). Der Name entsteht aus den letzten drei MAC-Bytes
(`cfg::Net::DEVICE_NAME` überschreibt ihn). Die Antwort enthält den vom Server
vergebenen Buchstaben (`"letter"`, A–J in der Reihenfolge der Anmeldung, bleibt
dauerhaft gespeichert). Der Taster zeigt ihn in der Zuordnungsansicht und in der
unteren Leiste; bis zur Anmeldung steht dort `?`. Danach sendet er alle 10 s
`POST /api/vitals` mit den Feldern des Stoppuhr-Backends:
`{"mac","battery"(%),"temp","humidity","vbat"(V),"rssi","fw","uptime_s","clock_ms"}`;
`ts` fehlt, der Server setzt die Empfangszeit. `clock_ms` ist die Uhrzeit des Tasters
(Epoch in ms, nur wenn per NTP synchron). Daraus bestimmt der Server den Versatz
zur eigenen Uhr. Er gleicht die Zeitstempel nur bei grober Abweichung (> 250 ms) aus;
kleinere Versätze sind Netzlatenz und würden die Zeiten verfälschen.

### Signaltöne

- Heller Klick (4200 Hz): eigener Tastendruck.
- Dumpfer Ton (1200 Hz, 110 ms): Bahn-Taster beim Start des Laufs, Starter bei jeder Bahn, die
  stoppt. Ausgelöst durch die Anzeigedaten des Servers (Verzögerung bis ca. 200 ms Abfrageintervall; die Abfrage läuft in eigenem Task und startet sofort nach dem eigenen Druck).
  Der erste Datensatz nach dem Einschalten piept nicht.

### Zeitgenauigkeit

- Der Zeitstempel eines Tastendrucks stammt aus einem Interrupt auf der fallenden Flanke
  (`esp_timer`, Mikrosekunden) und nicht aus dem `loop()`; Entprellung und Display-Zeichnen
  verzögern ihn nicht. Gesendet wird er in ms (gerundet).
- SNTP gleicht alle `NTP_SYNC_INTERVAL_MS` (30 s) mit dem Pi ab, damit der Quarz nicht driftet.
- Im Betrieb muss der Pi ein NTP-Server sein (z. B. chrony, `allow` für das Taster-WLAN) und
  der Stoppuhr-Server dieselbe Uhr nutzen. Dann liegt der Fehler im niedrigen ms-Bereich.

### Rollenansichten (Start / Bahn)

Der Taster fragt per `GET /api/display?mac=<mac>` (alle 200 ms mit Rolle, sonst 1 s) Rolle und Anzeigedaten ab.
Ohne Rolle bleibt die Zuordnungsansicht. Mit Rolle zeigt das Display:

- **Start** (rot): Kopfzeile „Start" / „Lauf N", darunter alle Bahnen des Laufs untereinander (links Ziffer, rechts Zeit; `--:--,--` solange die Bahn wartet, sonst laufende bzw. gestoppte Zeit). Der Server liefert sie als Feld `lanes` (`"bahn,zustand,ms;..."`).
- **Bahn N**: blau = bereit, grün = läuft, grau = gestoppt oder ohne Startkarte; Name, Disziplin, letzte Zeit. Eine gestoppte Zeit steht in Gelb (`TIME_STOPPED`), auch in der Bahnliste des Starters.

Die laufende Uhr rechnet lokal weiter (`elapsed_ms` + Zeit seit Empfang) und überträgt nur das Zeitfeld
(`display::flushRows`, alle `Timing::ROLE_CLOCK_MS`). Rollenname und Disziplin unterstützen deutsche Umlaute,
wenn das Pi-Backend sie unverändert als UTF-8 liefert. ASCII-Ersatzformen wie `ae` werden nicht automatisch
umgewandelt, da sie auch echte Buchstabenfolgen sein können.
Der Server kann über **Wettkampf beenden** einen befristeten Abschaltbefehl senden. Beim Empfang trennt die
Firmware die Selbsthaltung über GPIO44 und schaltet den Taster aus.

### Verhalten

- Retry bei Netzwerkfehler, Timeout und HTTP 5xx mit exponentiellem Backoff.
- HTTP 409 (`DUPLICATE_TRIGGER`) gilt als zugestellt; andere 4xx werden verworfen (kein Retry).
- Scheitert die Zustellung, bleibt das Event am Queue-Anfang und wird nach
  `FLUSH_RETRY_MS` erneut versucht; die Reihenfolge bleibt erhalten.
- Redirects werden nicht verfolgt. Der Zeitstempel wird zum Tastendruck
  festgehalten und erst beim Senden in Epoch-ms umgerechnet, daher bleibt er
  auch bei späterem Flush korrekt (sobald die Uhr synchron ist).
- Log (seriell): `[NET] event=<n> attempt=<n> http=<status> action=sent|buffered|retried|dropped`.

### Inbetriebnahme im internen WLAN

1. Pi mit fester IP bzw. DHCP-Reservierung im selben Netz betreiben, Stoppuhr-Server auf Port 1999.
2. `src/secrets.example.h` nach `src/secrets.h` kopieren und SSID, Passwort, Pi-IP
   (ggf. API-Key) eintragen. Die Datei wird nicht ins Repository hochgeladen.
   Ohne sie baut die Firmware mit Platzhaltern (z. B. auf GitHub), verbindet sich aber nicht.
   Das WLAN ist versteckt: SSID exakt eintragen, der Taster sucht auf allen Kanälen.
   WLAN bleibt dauerhaft an (Dauerbetrieb); das erhöht Akkuverbrauch und Eigenerwärmung.
3. Taster drücken, im seriellen Monitor `action=sent` prüfen; Pi abschalten →
   `buffered`, wieder einschalten → Queue wird in Reihenfolge geleert.

### Test ohne Pi: Simulation am PC

Der Taster hängt dafür im normalen Heim-WLAN, der PC spielt den Pi:

1. In `src/secrets.h` eintragen: das Heim-WLAN (SSID/Passwort), als `SECRET_PI_IP`
   die IP des PCs (`ipconfig`) und `#define SECRET_NTP_SERVER "pool.ntp.org"`
   (der PC bietet keinen NTP-Dienst an, der Taster holt die Uhrzeit aus dem Internet).
2. Test-Server starten (Python von PlatformIO, nichts zu installieren):
   `& 'C:\Users\<name>\.platformio\penv\Scripts\python.exe' tools\mock_pi.py`
3. Windows-Firewall: beim ersten Start „Zugriff zulassen" bestätigen (privates Netz), sonst erreicht der Taster den PC nicht.
4. Flashen, Taster drücken: Im Server-Fenster erscheint z. B.
   `#1 -> 200 mac=… Tastendruck=11:43:06.123 (vor 41 ms)`.

Fehlerfälle testen: `--fail-every 3` (jede 3. Anfrage 503 → Retry),
`--offline-s 30` (Server antwortet 30 s nicht → Events werden gepuffert und
danach in Reihenfolge nachgesendet). Zurück zum Pi: `SECRET_PI_IP`/`SECRET_NTP_SERVER`
und WLAN-Daten wieder auf den Pi stellen.

## Akku-Messung prüfen

PlatformIO-Seriellen Monitor mit `115200` Baud öffnen:

```text
pio device monitor -b 115200
```

Falls PlatformIO den Port nicht automatisch findet, zuerst `pio device list`
ausführen und den passenden Port explizit angeben, zum Beispiel
`pio device monitor -b 115200 --port COM5`.

Alle fünf Sekunden erscheint eine `[VBAT]`-Zeile mit GPIO, gemitteltem
ADC-Rohwert (inklusive Minimum/Maximum), gemessener ADC-Spannung,
Umrechnungsfaktor und berechneter Akkuspannung. `status=OK` bedeutet, dass
der berechnete Wert im erwarteten Bereich von 2,5 bis 4,5 V liegt;
`status=OUT_OF_RANGE` erklärt, warum im Display kein Akkustand angezeigt wird.

Bei `raw=0/0/0` oder `adc=0.000V` zuerst die Verbindung von GPIO1 zum
Spannungsteiler, die gemeinsame Masse und die Ansteuerung von MEAS_EN
(GPIO2) prüfen. Die Spannung am ADC-Eingang sollte beim Messen größer als
0 V und kleiner als 3,3 V sein. Zum Gegencheck die Akkuspannung mit einem
Multimeter messen und mit dem Logwert `battery` vergleichen.

## Zwei WLANs (Koffer und Bad, ab v0.10.0)

Mit `SECRET_WIFI2_SSID`/`SECRET_WIFI2_PASS` in `src/secrets.h` kennt der Taster ein zweites WLAN. Ohne Rolle ist er im ersten (Koffer-WLAN), mit Rolle wechselt er sofort ins zweite (Bad-WLAN), auch wenn dieses gerade nicht erreichbar ist. Zurück geht er nur, wenn die Rolle entzogen wird. Bleibt `SECRET_WIFI2_SSID` leer, verhält er sich wie bisher. Aufbau der Pis: siehe `deploy/zwei-pi/README.md` im Stoppuhr-Repository.

## Taster tauschen (Bereitschaft, ab v0.10.2)

Wird ein Taster auf eine bereits belegte Bahn gezogen, zeigt er „Bereit: Bahn N“ und wechselt ins zweite (Bad-)WLAN. Der erste Tastendruck vor Ort tauscht die Rollen, sofern kein Lauf läuft. Der Druck zählt nicht als Messung.

## Bauen und Hochladen

Das PlatformIO-Projekt verwendet die Umgebung `seeed_xiao_esp32s3`:

```text
pio run -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32s3 -t upload
```

USB-CDC ist beim Start aktiviert. Dadurch wird `Serial` über den nativen
USB-Anschluss des XIAO ESP32-S3 ausgegeben. Nach dem ersten Flashen das Board
einmal zurücksetzen und den neu erscheinenden USB-Seriell-Port verwenden:

```text
pio device list
pio device monitor -b 115200 --port COMx
```

`COMx` durch den Port des laufenden Programms ersetzen. Der Bootloader kann
unter einem anderen COM-Port erscheinen als die gestartete Anwendung.

Die benötigten Arduino-Bibliotheken sind in `platformio.ini` aufgeführt.
