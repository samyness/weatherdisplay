#pragma once
// Optionale Werte aus Home Assistant (REST-API, Long-Lived Access Token).
// Konfiguration in secrets.h: HA_URL, HA_TOKEN, HA_ENTITIES.

#include <stddef.h>
#include "weather_data.h"

bool homeAssistantConfigured();

// Füllt data.home / data.homeCount. Nicht erreichbare oder falsche Sensoren
// werden übersprungen. Bei ungültigem Token wird authFailed gesetzt und danach
// nicht mehr gefragt (sonst sperrt Home Assistant die IP per ip_ban).
void fetchHomeAssistant(WeatherData& data, bool& authFailed);

// Holt den Template-Sensor HA_DISPLAY_ENTITY (Müll, Energie, Hinweise in einer
// Anfrage) und füllt data.homeDisplay. Ohne HA_DISPLAY_ENTITY passiert nichts.
void fetchHomeDisplay(WeatherData& data, bool& authFailed);

// "Morgen", "Donnerstag", "Do, 22.10." für einen Abholtermin (plattformunabhängig)
void formatDayDistance(const char* isoDate, int days, char* out, size_t outSize);

// Formatiert einen HA-Zustand für die Anzeige (plattformunabhängig):
// "22.37" + "°C" -> "22,4 °C", "48.2" + "%" -> "48 %", "2026-10-12" -> "Mo, 12.10."
void formatHomeState(const char* state, const char* unit, char* out, size_t outSize, HomeKind& kind);
