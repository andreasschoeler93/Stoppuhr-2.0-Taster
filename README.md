# Taster – Firmware (v0.12.4)

Firmware für den Seeed XIAO ESP32-S3 (PlatformIO, Arduino-Framework). Der Taster misst den Zeitpunkt eines Tastendrucks
auf die Mikrosekunde, schickt ihn per WLAN an den Pi (Repository `Stoppuhr`) und zeigt je nach Rolle Start, Bahn oder die Zuordnung an.

Versionsverlauf: [CHANGELOG.md](CHANGELOG.md).

## Hardware

Seeed XIAO ESP32-S3 mit ST7789-Display, AHT20, Taster, Piezo-Buzzer und schaltbarem Akku-Spannungsteiler.
Die verbindliche Pinbelegung steht in [`docs/Programmier_Informationen`](docs/Programmier_Informationen).

Die Akkuspannung wird aus acht gemittelten ADC-Messungen mit dem Faktor `2.382` berechnet, der Prozentwert ist eine Schätzung aus der Zellspannung.
Der Display-Bildspeicher belegt rund 131 KiB RAM; scheitert das Anlegen, meldet die Firmware `[GFX] ERROR` und überspringt das Zeichnen.

## Bauen und flashen

```text
pio run -e seeed_xiao_esp32s3
pio run -e seeed_xiao_esp32s3 -t upload
```

USB-CDC ist aktiv: `Serial` läuft über den nativen USB-Anschluss. Nach dem ersten Flashen das Board einmal zurücksetzen
und den neu erscheinenden Port nehmen (der Bootloader nutzt evtl. einen anderen COM-Port):

```text
pio device list
pio device monitor -b 115200 --port COMx
```

### Zugangsdaten (`src/secrets.h`)

1. `src/secrets.example.h` nach `src/secrets.h` kopieren (wird nicht hochgeladen) und eintragen:
   `SECRET_WIFI_*` (Koffer-WLAN), `SECRET_WIFI2_*` (Bad-WLAN, optional), `SECRET_PI_IP` (Pi oder `stoppuhr`), optional `SECRET_API_KEY`, `SECRET_NTP_SERVER`.
2. Das WLAN ist versteckt: SSID exakt eintragen, der Taster sucht auf allen Kanälen.
3. Beim Flashen mit `secrets.h` werden die Werte im Flash (NVS) gespeichert und danach von dort gelesen. Bleibt `secrets.h` unverändert,
   bleiben auch vom Pi gesetzte WLAN-Daten erhalten; ändert sich `secrets.h`, gelten wieder deren Werte.
4. Ohne `secrets.h` (z. B. GitHub-Build) baut die Firmware mit Platzhaltern und verbindet sich nur mit bereits gespeicherten Daten.

Der **erste Flash** eines Tasters muss per USB mit `secrets.h` erfolgen. Danach geht alles drahtlos (Update und WLAN-Änderung über den Pi).

## Versionierung

Schema `MAJOR.MINOR.PATCH`, die Version steht in `src/config.h` (`FW_VERSION_*`), im Startbild und im seriellen Monitor.

- **PATCH**: Fehlerkorrekturen, Kalibrierwerte
- **MINOR**: neue Funktionen oder Ansichten, abwärtskompatibel
- **MAJOR**: grundlegende Änderungen (Hardware, Bedienkonzept); `1.0.0` = erste fertige Version

Bei jeder Änderung: Version erhöhen, `CHANGELOG.md` ergänzen, committen und `git tag vX.Y.Z && git push origin vX.Y.Z`.
Der Tag löst den Release-Build aus ([release.yml](.github/workflows/release.yml)); er dauert 2–3 Minuten.

## Update über den Pi (OTA, ab v0.11.0)

Weboberfläche des Pi → Einstellungen → Taster-Firmware → Version wählen → **Auf Taster laden**.

- Der Pi bietet die Datei allen online Tastern an. Der Taster prüft Größe und MD5, schreibt in den zweiten App-Slot und startet neu.
  Das Display zeigt „Update“ mit Fortschritt.
- Nie während eines laufenden Laufs und nur mit leerer Sendewarteschlange.
- Zugangsdaten stehen nicht im Release-Build, sondern kommen aus dem Flash. Versionen vor `v0.11.0` lassen sich nicht per Update installieren;
  ein Zurück auf eine ältere Version geht nur per USB.

## WLAN

### Zwei Netze (Koffer und Bad, ab v0.10.0)

Mit `SECRET_WIFI2_*` kennt der Taster ein zweites WLAN. Ohne Rolle ist er im Koffer-WLAN, mit Rolle (oder in Bereitschaft) wechselt er sofort
ins Bad-WLAN, auch wenn es gerade nicht erreichbar ist. Zurück geht er nur, wenn die Rolle entzogen oder der Wettkampf beendet wird.
Bleibt `SECRET_WIFI2_SSID` leer, gibt es nur ein Netz. Aufbau der Pis: `deploy/zwei-pi/README.md` im Stoppuhr-Repository.

### WLAN-Daten vom Pi ändern (ab v0.12.0)

Ändert man Name oder Kennwort in der Oberfläche des Pi, holt der Taster die neuen Daten mit der nächsten Anzeigeabfrage,
speichert sie vor und meldet in den Vitaldaten `wv` = Fassung zurück. Er nutzt weiter die alten Daten. Sobald der Pi sein WLAN umstellt
und die Verbindung abbricht, probiert der Taster abwechselnd alte und neue Daten; klappen die neuen, übernimmt er sie dauerhaft.
Ein Taster, der bei der Änderung ausgeschaltet war, kennt die neuen Daten nicht und muss per USB neu geflasht werden.

### Taster tauschen (Bereitschaft, ab v0.10.2)

Wird ein Taster auf eine belegte Bahn gezogen, zeigt er „Bereit: Bahn N“ und wechselt ins Bad-WLAN. Der erste Tastendruck vor Ort tauscht
die Rollen, sofern kein Lauf läuft. Der Druck zählt nicht als Messung.

## Bedienung am Taster

### Ansichten

Ohne Rolle zeigt der Taster die Zuordnung (Akku-Prozent und Buchstabe, untere Leiste mit Temperatur und Luftfeuchte). Mit Rolle:

- **Start** (rot): Kopfzeile „Start“ / „Lauf N“, darunter alle Bahnen des Laufs (links Ziffer, rechts Zeit; `--:--,--` solange die Bahn wartet).
- **Bahn N**: blau = bereit, grün = läuft, grau = gestoppt oder ohne Startkarte; Name, Disziplin, letzte Zeit. Gestoppte Zeiten stehen in Gelb.

Beim Stoppen zeigt die Bahn die eigene Zeit sofort in Gelb und hält sie, bis der Pi den Stopp bestätigt hat.
Die laufende Uhr rechnet lokal weiter und überträgt nur das Zeitfeld. Namen und Disziplinen unterstützen deutsche Umlaute (UTF-8 vom Pi).
„Wettkampf beenden“ im Pi sendet einen befristeten Abschaltbefehl; der Taster trennt dann die Selbsthaltung (GPIO44) und schaltet sich aus.

### Starter: Abbruch

Ein Langdruck (2 s) am Starter bricht den laufenden Versuch ab und bereitet eine neue Version desselben Laufs vor (dreimal langes Piepen).
Die neue Version startet erst nach Ablauf der Stopp-Sperre mit einem kurzen Druck. Bahn-Taster behalten Start/Stopp.

### Signaltöne

- Klick (2700 Hz, 100 ms): eigener Tastendruck.
- Dumpfer Ton (1200 Hz, 110 ms): Bahn-Taster beim Laufstart, Starter bei jeder gestoppten Bahn. Der erste Datensatz nach dem Einschalten piept nicht.
- Dreimal lang (3 × 450 ms, 2400 Hz): Lauf abgebrochen.

## Schnittstelle zum Pi

Nur internes WLAN, keine Cloud. Das Senden läuft in einem eigenen Task (`src/netlink.cpp`), `loop()` blockiert nie.

| Aufruf | Zweck |
|---|---|
| `POST /api/triggers` | Tastendruck: `{"mac":"AA:BB:CC:DD:EE:01","ts":1761290000123,"stopwatch_ms":null,"run_id":null}`; Antwort `{"ok":true,"received_at":…,"event_id":"…"}` |
| `POST /api/abort-run` | Langdruck des Starters: `{"mac":"…","request_id":"…"}` |
| `POST /api/taster` | Anmeldung `{"mac","name"}`, alle 5 s bis 200, danach alle 60 s; Antwort enthält den Buchstaben `letter` (A–J) |
| `POST /api/vitals` | alle 10 s: `{"mac","battery","temp","humidity","vbat","rssi","ssid","fw","uptime_s","clock_ms","wv"}`; `ts` setzt der Server |
| `GET /api/display?mac=…` | Rolle und Anzeigedaten (alle 200 ms mit Rolle, sonst 1 s); enthält auch Update- und WLAN-Angebote |
| `GET /api/firmware/download?mac=…`, `POST /api/ota-status` | Update-Download und Fehlermeldung |

Der Header `X-Api-Key` wird nur gesendet, wenn ein Schlüssel konfiguriert ist. Der Name des Tasters entsteht aus den letzten drei MAC-Bytes
(`cfg::Net::DEVICE_NAME` überschreibt ihn).

### Verhalten beim Senden

- Retry bei Netzfehler, Timeout und HTTP 5xx mit exponentiellem Backoff. HTTP 409 (`DUPLICATE_TRIGGER`) gilt als zugestellt, andere 4xx werden verworfen.
- Scheitert die Zustellung, bleibt das Event am Queue-Anfang und wird nach `FLUSH_RETRY_MS` erneut versucht; die Reihenfolge bleibt erhalten.
- Der Zeitstempel wird beim Druck festgehalten und erst beim Senden in Epoch-ms umgerechnet, bleibt also auch bei spätem Senden korrekt (sobald die Uhr synchron ist).
- Log (seriell): `[NET] event=<n> attempt=<n> http=<status> action=sent|buffered|retried|dropped`.

### Zeitgenauigkeit

- Der Zeitstempel stammt aus einem Interrupt auf der fallenden Flanke (`esp_timer`, µs), nicht aus `loop()`.
- SNTP gleicht alle 30 s mit dem Pi ab. Der Pi muss ein NTP-Server sein (chrony, `allow` für das Taster-WLAN) und der Server dieselbe Uhr nutzen.
- Aus `clock_ms` bestimmt der Server den Uhrversatz und gleicht nur grobe Abweichungen (> 250 ms) aus; kleinere sind Netzlatenz.

### Konfiguration (`cfg::Net` in `src/config.h`)

| Variable | Default | Bedeutung |
|---|---|---|
| `WIFI_SSID`, `WIFI_PASS`, `WIFI2_*` | aus `secrets.h` | Koffer- und Bad-WLAN (zur Laufzeit aus dem Flash) |
| `PI_HOST` / `PI_PORT` | `SECRET_PI_IP` / 1999 | Pi-IP oder Name; ein Name wird einmal aufgelöst und die IP gemerkt |
| `NTP_SERVER` | `SECRET_PI_IP` | Zeitquelle |
| `DEVICE_MAC` | leer = ESP32-MAC | Geräte-ID |
| `API_KEY` | aus `secrets.h` | optional, als `X-Api-Key` |
| `HTTP_TIMEOUT_MS` | 1500 | Timeout je Request |
| `RETRY_COUNT`, `RETRY_BACKOFF_MS` | 3, 100 | Versuche je Zustellung, Start-Backoff (verdoppelt sich) |
| `FLUSH_RETRY_MS` | 500 | Pause nach gescheiterter Zustellung |
| `ENABLE_OFFLINE_QUEUE`, `QUEUE_MAX` | true, 32 | Events puffern (nur RAM); bei Überlauf fällt das älteste weg |
| `TRIGGER_MIN_INTERVAL_MS` | 300 | Entprellung zusätzlich zur Hardware-Entprellung (40 ms) |

## Projektstruktur

| Datei | Aufgabe |
|---|---|
| `main.cpp` | Ablauf, Rollenansichten, lokale Stoppzeit |
| `config.h` | Pins, Zeiten, Farben, Layout, Version, Akkufaktor |
| `measurements.*` | Temperatur, Luftfeuchte, Akku |
| `display.*` | Display, Bildspeicher, untere Leiste |
| `screens.*` | Ansichten (Splash, Zuordnung, Rollen, Update) |
| `netlink.*` | WLAN, Senden mit Queue, Anmeldung, Anzeigedaten, Update, WLAN-Daten |
| `button.*`, `beep.*` | Taster (Interrupt mit Entprellung), Piezo |

Gezeichnet wird nur über den Bildspeicher (`display.*`), damit nichts flackert.

## Test ohne Pi (Simulation am PC)

Der Taster hängt im Heim-WLAN, der PC spielt den Pi:

1. In `src/secrets.h`: Heim-WLAN, als `SECRET_PI_IP` die IP des PCs (`ipconfig`) und `#define SECRET_NTP_SERVER "pool.ntp.org"`.
2. Test-Server starten: `& 'C:\Users\<name>\.platformio\penv\Scripts\python.exe' tools\mock_pi.py`
3. Windows-Firewall: „Zugriff zulassen“ (privates Netz).
4. Flashen und drücken: im Server-Fenster erscheint z. B. `#1 -> 200 mac=… Tastendruck=11:43:06.123 (vor 41 ms)`.

Fehlerfälle: `--fail-every 3` (jede 3. Anfrage 503), `--offline-s 30` (Server antwortet 30 s nicht; Events werden gepuffert und danach in Reihenfolge nachgesendet).
Zurück zum Pi: `secrets.h` wieder auf den Pi stellen.

## Akku-Messung prüfen

Im seriellen Monitor (115200 Baud) erscheint alle fünf Sekunden eine `[VBAT]`-Zeile mit GPIO, ADC-Rohwert (Min/Max), ADC-Spannung,
Faktor und Akkuspannung. `status=OK` heißt 2,5 bis 4,5 V; `OUT_OF_RANGE` erklärt, warum kein Akkustand erscheint.
Bei `raw=0/0/0` oder `adc=0.000V`: GPIO1 zum Spannungsteiler, Masse und MEAS_EN (GPIO2) prüfen. Zum Gegencheck die Akkuspannung mit dem Multimeter messen.
