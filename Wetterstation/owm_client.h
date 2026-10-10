#pragma once
// Abfrage von OpenWeatherMap (aktuelles Wetter + 5-Tage-Vorhersage, kostenloser Tarif)

#include "weather_data.h"

struct FetchError
{
  int  httpCode = 0;       // HTTP-Status oder negativer Fehlercode von HTTPClient
  char title[48] = "";     // kurze Meldung fürs Display (UTF-8)
  char detail[96] = "";
};

// Füllt data.now und data.slots. now = aktuelle Zeit (0 = unbekannt), damit
// bereits vergangene Vorhersage-Werte übersprungen werden.
bool fetchWeather(WeatherData& data, time_t now, FetchError& err);
