# Versionsverlauf – Taster-Firmware

Jede Version ist als Git-Tag `vX.Y.Z` markiert. Ab `v0.11.0` entsteht beim Tag automatisch ein GitHub-Release
mit `firmware.bin`, das der Pi den Tastern anbieten kann. Die Spalte „Pi“ nennt die mindestens nötige
Pi-Software (Repository `Stoppuhr`). Versionen ohne eigenen Commit (v0.8.7, v0.9.0, v0.10.4, v0.10.5) haben kein Tag.

| Version | Datum | Pi | Änderung |
|---|---|---|---|
| v0.18.0 | 2026-11-05 | 0.33.0 | Ziel-Taster offline: so viele Uhren wie die höchste Bahn im Wettkampf. Der erste Druck startet alle Uhren (zählt nicht als Zieleinlauf und wird nicht gesendet), jeder weitere Druck stoppt die nächste Uhr und wird als Zieleinlauf nachgeliefert. Eine schon laufende Uhr zählt nahtlos weiter. |
| v0.17.0 | 2026-11-05 | 0.32.0 | Pi-Ausfall: Der Zeitstempel eines Drucks wird beim Druck festgehalten und nicht erst beim Senden berechnet (ein Zeitsprung der Taster-Uhr nach Pi-Neustart verschob sonst gespeicherte Stopps). Ohne Antwort vom Pi (3 s) zeigen Start, Bahn und Ziel „Offline“ mit einer lokalen Stoppuhr: Eine laufende Uhr zählt nahtlos weiter, sonst startet der erste Druck die Uhr und der nächste (frühestens nach 5 s) stoppt sie; beim Ziel steht die Zahl der Zeitnahmen. Der Starter bestätigt den Start mit „Start Lauf N“ (Lauf nach der Zählung des Pi, auch für den folgenden Lauf), zeigt die Laufzeit und „Start gesendet, Pi offline“; ein zweiter Druck (ab 1,5 s) beendet den Lauf wie am Pi. Die Zeiten werden weiter über die Warteschlange nachgeliefert und vom Pi berechnet. |
| v0.16.0 | 2026-11-03 | 0.15.0 | WLAN-Neuverbindung bei Trennung alle 5 s (jeder Versuch wird seriell protokolliert). Akkuwarnung erst nach drei aufeinanderfolgenden Messungen unter der Schwelle; Entwarnung nach drei Messungen mindestens 2 Prozentpunkte darüber. Die Abschaltzeit zählt nur bei Messungen unter der Schwelle und wird bei Erholung zurückgesetzt. |
| v0.15.1 | 2026-11-02 | 0.16.0 | WLAN-Wiederverbindung robuster: Ohne Empfang werden die Verbindungsversuche seltener (Abstand wächst von 5 s auf 20 s) statt dauerhaft alle 5 s neu zu scannen. Das entlastet Akku und Speicher (ab v0.16.0 wieder fester 5-s-Takt). Der Reset-Grund wird beim Start im Serial-Monitor ausgegeben. |
| v0.15.0 | 2026-10-30 | 0.16.0 | Funktest: Der Pi kann einen Taster in die Rolle „range“ schalten. Das Display zeigt dann die WLAN-Signalstärke (dBm) mit Ampelfarbe und Balken, bei kritischem Signal oder ohne WLAN piept der Taster einmal pro Sekunde. Tastendrücke werden verworfen. |
| v0.14.0 | 2026-10-30 | 0.15.0 | Akkuwarnung: ab einstellbarem Akkustand (Standard 10 %) blinkt die Akkuanzeige und der Taster piept alle 5 s, bleibt aber voll bedienbar. Nach einstellbarer Zeit unter der Schwelle (Standard 5 min) schaltet er sich ab. Beide Werte stellt man in der Pi-Oberfläche ein (Einstellungen; 0 = aus), der Taster holt sie über `/api/display`. |
| v0.13.1 | 2026-10-30 | 0.13.2 | Zieleinlauf-Taster nimmt schnelle Druckfolgen an: Tastendrücke werden in einem Puffer (16) gesammelt und alle sofort gesendet, kein Druck geht mehr verloren. |
| v0.13.0 | 2026-10-30 | 0.13.1 | Neue Rolle Zieleinlauf: gleiche Ansicht wie der Starter (Kopf „Ziel“, violetter Hintergrund). Zeile n zeigt die Zeit des n-ten Zieleinlauf-Drucks, die Zeilenzahl entspricht den Bahnen des Laufs. Zuordnungsbild „Bereit: Ziel“. |
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
