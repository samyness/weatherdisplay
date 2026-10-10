#include "demo_data.h"
#include <math.h>
#include "forecast.h"
#include "text_de.h"

void makeDemoData(WeatherData& d, time_t now)
{
  struct Plan { int id; float base, amp, pop, mm; };
  static const Plan plan[] = {{803, 11, 4, 0.10f, 0}, {501, 10, 3, 0.80f, 2.5f}, {800, 9, 6, 0, 0},
                              {601, 1, 2, 0.90f, 1.2f}, {211, 15, 5, 0.60f, 4}, {804, 12, 3, 0, 0}};
  const struct tm today = toLocal(now);

  d.updated = now;
  CurrentWeather& c = d.now;
  c.dt = now - 300;
  c.temp = 14.4f;
  c.feelsLike = 13.1f;
  c.humidity = 68;
  c.pressure = 1016;
  c.windSpeed = 3.4f;
  c.windDeg = 300;
  c.weatherId = 803;
  c.night = today.tm_hour < 7 || today.tm_hour >= 19;
  c.sunrise = now - (today.tm_hour * 3600 + today.tm_min * 60) + 7 * 3600 + 12 * 60;
  c.sunset = c.sunrise + 11 * 3600 + 19 * 60;
  copyStr(c.description, "\xC3\x9C" "berwiegend bew\xC3\xB6lkt", sizeof(c.description));  // Überwiegend bewölkt
  copyStr(c.city, "M\xC3\xB6" "dling", sizeof(c.city));                                   // Mödling

  const time_t first = (now / 10800 + 1) * 10800;
  d.slotCount = MAX_SLOTS;
  for (int i = 0; i < MAX_SLOTS; i++)
  {
    ForecastSlot& s = d.slots[i];
    s.dt = first + (time_t)i * 10800;
    const struct tm lt = toLocal(s.dt);
    int day = (lt.tm_year - today.tm_year) * 366 + lt.tm_yday - today.tm_yday;
    const Plan& p = plan[day < 0 ? 0 : (day > 5 ? 5 : day)];
    s.temp = p.base + p.amp * sinf((lt.tm_hour - 9) / 24.0f * 2 * 3.14159f);
    s.night = lt.tm_hour < 7 || lt.tm_hour >= 19;
    s.weatherId = p.id;
    if (p.mm > 0 && i % 3 != 0)
    {
      s.pop = p.pop;
      s.precip = p.mm * (i % 3 == 1 ? 1.0f : 0.4f);
    }
    else
    {
      s.pop = p.pop * 0.3f;
      s.precip = 0;
      if (p.id < 800) s.weatherId = 803;
    }
  }
  buildDailyForecast(d, now);

  // Beispiel für den Home-Assistant-Template-Sensor: Müll, Energie, Hinweis
  d.homeCount = 0;
  HomeDisplay& h = d.homeDisplay;
  h = HomeDisplay();
  h.valid = true;
  h.hasWaste = true;
  copyStr(h.wasteKind, "Biom\xC3\xBC" "ll", sizeof(h.wasteKind));  // Biomüll
  h.wasteDays = 1;
  h.wasteOut = 0;
  h.pvTodayKwh = 18.4f;
  h.batteryPct = 72;
  h.noteCount = 1;
  h.notes[0].prio = 2;
  copyStr(h.notes[0].text, "Salz nachf\xC3\xBCllen", sizeof(h.notes[0].text));  // Salz nachfüllen
}
