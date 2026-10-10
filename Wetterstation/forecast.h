#pragma once
#include "weather_data.h"

// Lokale Zeit (Zeitzone über die Umgebungsvariable TZ, siehe config.h)
struct tm toLocal(time_t t);

// OWM-Wettercode -> Icon
IconKind iconKindFromId(int weatherId);

// Fasst die 3-Stunden-Werte zu Tagen zusammen. Heute ist immer dabei (inkl.
// aktueller Temperatur), weitere Tage nur, wenn der Nachmittag abgedeckt ist,
// damit Höchstwerte nicht verfälscht werden.
int buildDailyForecast(WeatherData& data, time_t now);
