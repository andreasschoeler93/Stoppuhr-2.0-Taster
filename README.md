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
Aktuell: `v0.5.0`.

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
| `button.*`, `beep.*` | Taster mit Entprellung, Piezo |

Geplante Module (noch nicht angelegt, je ein eigener Minor-Schritt):

1. **Datenaustausch**: Startkarten
2. **Zuordnung**: Tasterzuordnung und Rolle
3. **Zeit**: Zeitstempel und Berechnungen
4. **Zeitsync**: Zeitsynchronisierung
5. **Backup**: Stempel-Backup
6. **Netzwerk**: WLAN-Management

## Taster ↔ Pi Schnittstelle

Bei jedem Tastendruck sendet der Taster `POST /api/triggers` an den Pi
(Repo `Stoppuhr`). Nur internes WLAN/LAN, keine Cloud. Das Senden läuft in
einem eigenen Task (`src/netlink.cpp`), `loop()` blockiert nie.

Request (`Content-Type: application/json`, `Accept: application/json`,
`X-Api-Key` nur wenn konfiguriert):

```json
{"mac":"AA:BB:CC:DD:EE:01","ts":1761290000123,"stopwatch_ms":null,"run_id":null}
```

Antwort Erfolg: `{"ok":true,"received_at":1761290000150,"event_id":"..."}`;
Fehler: `{"ok":false,"error":"...","code":"..."}`.

### Konfiguration (`cfg::Net` in `src/config.h`)

| Variable | Default | Bedeutung |
|---|---|---|
| `WIFI_SSID`, `WIFI_PASS` | aus `src/secrets.h` | verstecktes, passwortgeschütztes WLAN des Pi |
| `PI_BASE_URL` | `http://<SECRET_PI_IP>:8000` | statische Pi-IP (kein DNS nötig) |
| `NTP_SERVER` | `SECRET_PI_IP` | Zeitquelle für `ts` (Epoch-ms); Pi braucht einen NTP-Dienst (z. B. chrony) |
| `DEVICE_MAC` | leer = ESP32-MAC | Geräte-ID |
| `API_KEY` | aus `src/secrets.h`, leer = kein Header | optional, als `X-Api-Key` |
| `HTTP_TIMEOUT_MS` | 1500 | Timeout je Request |
| `RETRY_COUNT` | 3 | Versuche je Zustellung |
| `RETRY_BACKOFF_MS` | 200 | Start-Backoff, verdoppelt sich je Versuch |
| `ENABLE_OFFLINE_QUEUE` | true | Events puffern, wenn Senden scheitert |
| `QUEUE_MAX` | 32 | Queue-Größe (nur RAM); bei Überlauf wird das **älteste** Event verworfen |
| `TRIGGER_MIN_INTERVAL_MS` | 300 | Entprellung zusätzlich zur Hardware-Entprellung (40 ms) |

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

1. Pi mit fester IP bzw. DHCP-Reservierung im selben Netz betreiben, Stoppuhr-Server auf Port 8000.
2. `src/secrets.example.h` nach `src/secrets.h` kopieren und SSID, Passwort, Pi-IP
   (ggf. API-Key) eintragen. Die Datei wird nicht ins Repository hochgeladen.
   Ohne sie baut die Firmware mit Platzhaltern (z. B. auf GitHub), verbindet sich aber nicht.
   Das WLAN ist versteckt: SSID exakt eintragen, der Taster sucht auf allen Kanälen.
   WLAN bleibt dauerhaft an (Dauerbetrieb); das erhöht Akkuverbrauch und Eigenerwärmung.
3. Taster drücken, im seriellen Monitor `action=sent` prüfen; Pi abschalten →
   `buffered`, wieder einschalten → Queue wird in Reihenfolge geleert.

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
