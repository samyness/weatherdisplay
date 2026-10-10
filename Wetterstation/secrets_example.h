#pragma once
// =====================================================================
//  Vorlage: diese Datei nach "secrets.h" kopieren und ausfüllen.
//  secrets.h ist in .gitignore eingetragen und landet nie auf GitHub.
// =====================================================================

#define WIFI_SSID     "MeinWLAN"
#define WIFI_PASSWORD "geheim"

// Kostenloser API-Key von https://openweathermap.org (My API keys).
// Ein neuer Key ist erst nach bis zu 2 Stunden aktiv.
#define OWM_API_KEY   "0123456789abcdef0123456789abcdef"

// Ort: entweder Koordinaten (am genauesten) ...
//#define LOCATION_LAT  48.0833
//#define LOCATION_LON  16.2833
// ... oder Ortsname mit Ländercode. Umlaute sind erlaubt.
#define LOCATION_QUERY "Wien,AT"

// Angezeigter Name. Leer lassen ("") = Name von OpenWeatherMap verwenden.
// Beispiele: "Mödling", "Klagenfurt am Wörthersee", "Sankt Pölten"
#define CITY_NAME "Wien"

// ---- Home Assistant (optional, leerer Token = aus) ---------------------
// Adresse im LAN mit http:// und IP (".local"-Namen kann der ESP8266 nicht auflösen).
#define HA_URL   "http://192.168.1.10:8123"
// Profil -> Sicherheit -> Langlebige Zugriffstoken -> Token erstellen
#define HA_TOKEN ""
// Empfohlen: ein Template-Sensor, der Müll, PV/Akku und Hinweise aufbereitet
// (Anleitung und Template in docs/home_assistant.md). Dann wird HA_ENTITIES nicht gebraucht.
#define HA_DISPLAY_ENTITY "sensor.eink_display"

// Alternative ohne Template-Sensor: einzelne Entities in der Info-Kachel.
// Bis zu 4 Werte. Sie ersetzen in der Info-Kachel zuerst Luftdruck, dann Sonnenzeiten, dann Wind.
//   {Anzeigename, entity_id}                       -> Zustand, z. B. "21,5 °C"
//   {Anzeigename, entity_id, Name-Attribut, Wert-Attribut}
//                                                  -> Name und Wert aus Attributen
// Anzeigename "" = Name aus Home Assistant. Termine werden als "Heute", "Morgen"
// oder "Di, 13.10." angezeigt.
//
// Beispiel: Müllart aus einem Abfallkalender (Attribut "message") mit Termin ("start_time"),
// dazu eine Außentemperatur:
//#define HA_ENTITIES \
//  {"Müll", "calendar.abfallkalender", "message", "start_time"}, \
//  {"Außen", "sensor.garten_temperatur"}
