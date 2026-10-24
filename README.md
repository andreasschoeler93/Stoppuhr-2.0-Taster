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
| `button.*`, `beep.*` | Taster mit Entprellung, Piezo |

Geplante Module (noch nicht angelegt, je ein eigener Minor-Schritt):

1. **Datenaustausch**: Startkarten
2. **Zuordnung**: Tasterzuordnung und Rolle
3. **Zeit**: Zeitstempel und Berechnungen
4. **Zeitsync**: Zeitsynchronisierung
5. **Backup**: Stempel-Backup
6. **Netzwerk**: WLAN-Management

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
