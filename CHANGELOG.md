# Versionsverlauf – Taster-Firmware

Jede Version ist als Git-Tag `vX.Y.Z` markiert. Ab `v0.11.0` entsteht beim Tag automatisch ein GitHub-Release
mit `firmware.bin`, das der Pi den Tastern anbieten kann. Die Spalte „Pi“ nennt die mindestens nötige
Pi-Software (Repository `Stoppuhr`). Versionen ohne eigenen Commit (v0.8.7, v0.9.0, v0.10.4, v0.10.5) haben kein Tag.

| Version | Datum | Pi | Änderung |
|---|---|---|---|
| v0.12.6 | 2026-10-29 | 0.8.0 | Akkusymbol in der unteren Leiste größer (28 × 14 statt 20 × 10 Pixel). |
| v0.12.5 | 2026-10-29 | 0.8.0 | Zuordnungsbild: Text „Taste: Starter“ entfernt (nur bei Bereitschaft bleibt „Bereit: …“). |
| v0.12.4 | 2026-10-29 | 0.8.0 | Tastendruck-Klick wieder wie in v0.12.2: 2700 Hz, 100 ms (klang besser als 4000 Hz). |
| v0.12.3 | 2026-10-29 | 0.8.0 | Tastendruck-Klick heller: 4000 Hz (Resonanz vieler Piezo-Summer), 120 ms, 50 % Tastverhältnis (Maximum). |
| v0.12.2 | 2026-10-29 | 0.8.0 | Tastendruck-Klick: ein einzelner Ton mit 2700 Hz (Resonanz kleiner Piezos), 100 ms. Ersetzt die Tonfolge aus v0.12.1. |
| v0.12.1 | 2026-10-29 | 0.8.0 | Lauterer Tastendruck-Klick: 120 ms lang, läuft über 2700/3400/4200 Hz, damit die Resonanz des Piezos getroffen wird. |
| v0.12.0 | 2026-10-29 | 0.8.0 | WLAN-Name und -Kennwort kommen vom Pi (Einstellungen). Neue Daten werden vorgemerkt, bestätigt und bei Bedarf übernommen. Gespeicherte Daten bleiben beim Neustart erhalten, solange `secrets.h` gleich bleibt. |
| v0.11.1 | 2026-10-28 | 0.7.0 | Nur Versionsnummer (Test des Updates über den Pi). |
| v0.11.0 | 2026-10-28 | 0.7.0 | Update über den Pi (OTA) mit Fortschritt auf dem Display. Zugangsdaten im Flash (NVS). Release-Build bei Tag. |
| v0.10.6 | 2026-10-28 | 0.6.3 | Schnellere Stoppanzeige (lokale Zeit sofort in Gelb, bis der Pi sie bestätigt). Dreimal langes Piepen beim Abbruch eines Laufs. Anmeldung und Vitaldaten in eigenem Task, kürzere Wiederholzeiten. |
| v0.10.3 | 2026-10-28 | 0.6.2 | Hinweis „Bereit“ weiter oben. |
| v0.10.2 | 2026-10-28 | 0.6.2 | Taster tauschen: Anzeige „Bereit: Bahn N“. |
| v0.10.1 | 2026-10-28 | 0.6.1 | WLAN-Name in den Vitaldaten. |
| v0.10.0 | 2026-10-28 | 0.6.0 | Zwei WLANs: Koffer und Bad, Wechsel nach Rolle. |
| v0.9.1 | 2026-10-28 | 0.5.1 | Langer Druck am Starter bricht den Lauf ab und bereitet eine Wiederholung vor. |
| v0.8.8 | 2026-10-28 | 0.5.1 | Abschaltbefehl vom Pi („Wettkampf beenden“). |
| v0.8.6 | 2026-10-27 | | WLAN-Debug-Ausgabe, WPA/WPA2 akzeptieren. |
| v0.8.5 | 2026-10-27 | | Schnellere Anzeige: eigener Abfrage-Task, Pi-Name nur einmal auflösen, WLAN ohne Energiesparen. |
| v0.8.4 | 2026-10-26 | | Gestoppte Zeit in Gelb. |
| v0.8.3 | 2026-10-26 | | Dumpfer Ton bei Laufstart und gestoppter Bahn. |
| v0.8.2 | 2026-10-26 | | Zeitstempel per Interrupt in µs, SNTP alle 30 s. |
| v0.8.1 | 2026-10-26 | | Starter zeigt alle Bahnen mit Zeiten. |
| v0.8.0 | 2026-10-26 | | Rollenansichten Start und Bahn. |
| v0.7.1 | 2026-10-26 | | Uhrzeit des Tasters in den Vitaldaten. |
| v0.7.0 | 2026-10-26 | | Buchstabe vom Server anzeigen. |
| v0.6.2 | 2026-10-26 | | Anmeldung am Server, Vitaldaten. |
| v0.5.0 | 2026-10-24 | | WLAN-Client mit Wiederholung und Warteschlange. |
| v0.4.1 | 2026-10-24 | | Entladekurve für 18650-Zellen. |
| v0.4.0 | 2026-10-24 | | Firmware in Module aufgeteilt. |
| v0.3.0 | 2026-10-24 | | Bildspeicher-Zeichnen, Versionsanzeige, Akkufaktor. |
