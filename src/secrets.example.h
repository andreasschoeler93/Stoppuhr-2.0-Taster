#pragma once

// Vorlage: nach src/secrets.h kopieren und eigene Werte eintragen.
// src/secrets.h steht in .gitignore und wird nicht hochgeladen.
#define SECRET_WIFI_SSID "Name-des-versteckten-WLAN"
#define SECRET_WIFI_PASS "WLAN-Passwort"
// Zwei-Pi-Betrieb: oben steht das Koffer-WLAN, hier das Bad-WLAN (Taster mit Rolle).
// Leer lassen, wenn es nur ein WLAN gibt.
#define SECRET_WIFI2_SSID ""
#define SECRET_WIFI2_PASS ""
#define SECRET_PI_IP "192.168.178.50"
#define SECRET_API_KEY ""
// Optional, nur fuer Tests am PC (der PC bietet keinen NTP-Dienst an):
// #define SECRET_NTP_SERVER "pool.ntp.org"
