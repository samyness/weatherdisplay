#pragma once
// Datenmodell für Wetter und Vorhersage. Plattformunabhängig, damit sich das
// Layout auch am PC rendern lässt.

#include <stdint.h>
#include <time.h>

constexpr int MAX_SLOTS = 40;  // OpenWeatherMap 5-Tage-Vorhersage: 40 x 3 Stunden
constexpr int MAX_DAYS  = 6;

// Vereinfachte Wetterlagen, aus denen die Icons gezeichnet werden
enum IconKind : uint8_t
{
  ICON_CLEAR,
  ICON_FEW_CLOUDS,
  ICON_CLOUDS,
  ICON_OVERCAST,
  ICON_FOG,
  ICON_DRIZZLE,
  ICON_RAIN,
  ICON_SHOWERS,
  ICON_SNOW,
  ICON_SLEET,
  ICON_THUNDER
};

struct CurrentWeather
{
  float  temp       = 0;      // °C
  float  feelsLike  = 0;      // °C
  int    humidity   = -1;     // %
  int    pressure   = -1;     // hPa
  float  windSpeed  = -1;     // m/s
  int    windDeg    = -1;     // Grad, -1 = unbekannt
  int    weatherId  = 0;      // OWM-Wettercode
  bool   night      = false;
  time_t dt         = 0;      // Messzeitpunkt (UTC)
  time_t sunrise    = 0;
  time_t sunset     = 0;
  char   description[64] = "";  // UTF-8, z. B. "Überwiegend bewölkt"
  char   city[48]        = "";  // UTF-8, Ortsname laut OpenWeatherMap
};

struct ForecastSlot
{
  time_t dt;        // Beginn des 3-Stunden-Fensters (UTC)
  float  temp;      // °C
  float  pop;       // Niederschlagswahrscheinlichkeit 0..1
  float  precip;    // Regen + Schnee in mm / 3 h
  int    weatherId;
  bool   night;
};

struct DayForecast
{
  time_t dt;        // ein Zeitpunkt innerhalb des Tages
  float  tMin;
  float  tMax;
  float  pop;       // höchste Niederschlagswahrscheinlichkeit des Tages
  float  precip;    // Summe in mm
  int    weatherId; // typisches Wetter tagsüber
};

// Ein Wert aus Home Assistant, fertig formatiert für die Anzeige
constexpr int MAX_HOME_VALUES = 4;

enum HomeKind : uint8_t
{
  HOME_GENERIC,
  HOME_TEMPERATURE,
  HOME_HUMIDITY,
  HOME_WASTE  // Müllabfuhr (Name = Müllart, Wert = Termin)
};

struct HomeValue
{
  char     label[32];  // UTF-8, z. B. "Wohnzimmer"
  char     value[32];  // UTF-8, z. B. "22,4 °C"
  HomeKind kind;
};

// Aufbereitete Daten aus dem Home-Assistant-Template-Sensor (HA_DISPLAY_ENTITY),
// siehe docs/home_assistant.md
constexpr int MAX_NOTES = 3;

struct HomeNote
{
  uint8_t prio;       // 1 = Info, 2 = Aufgabe, 3 = dringend
  char    text[40];   // UTF-8, z. B. "Salz nachfüllen"
};

struct HomeDisplay
{
  bool  valid = false;
  // Müll
  bool   hasWaste = false;
  char   wasteKind[48] = "";  // z. B. "Papier, Restmüll"
  char   wasteDate[12] = "";  // "2026-10-15"
  int    wasteDays = -1;      // Tage bis zur Abholung
  int8_t wasteOut = -1;       // 1 = rausgestellt, 0 = nein, -1 = unbekannt
  // Energie (-1 = unbekannt)
  float  pvTodayKwh = -1;
  int    batteryPct = -1;
  // Hinweise, nach Priorität sortiert
  HomeNote notes[MAX_NOTES];
  int      noteCount = 0;
};

struct WeatherData
{
  CurrentWeather now;
  ForecastSlot   slots[MAX_SLOTS];
  int            slotCount = 0;
  DayForecast    days[MAX_DAYS];
  int            dayCount  = 0;
  HomeValue      home[MAX_HOME_VALUES];
  int            homeCount = 0;
  HomeDisplay    homeDisplay;
  time_t         updated   = 0;  // Zeitpunkt der Abfrage (UTC)
};
