# Hinweise für Copilot

- Firmware für Seeed XIAO ESP32-S3 (PlatformIO, Arduino-Framework). Bauen: `pio run -e seeed_xiao_esp32s3`.
- Der Code liegt als Module in `src/`; Aufbau und Versionsschema stehen im README.
- Die erste Ansicht (Splash, Zuordnung mit Akku-% und Buchstabe, untere Leiste) darf sich grafisch nicht ändern.
- Zeichnen nur über den Bildspeicher in `display.*`, damit nichts flackert.
- Keine blockierenden Wartezeiten im `loop()`.
- Pins, Zeiten und Version nur in `src/config.h` ändern; bei jeder Änderung die Version erhöhen.
- Sprache für Doku und Kommentare: Deutsch.
