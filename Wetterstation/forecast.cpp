#include "forecast.h"
#include <string.h>

struct tm toLocal(time_t t)
{
  struct tm result;
#ifdef _WIN32  // nur für die PC-Vorschau
  localtime_s(&result, &t);
#else
  localtime_r(&t, &result);
#endif
  return result;
}

// Siehe https://openweathermap.org/weather-conditions
IconKind iconKindFromId(int id)
{
  if (id >= 200 && id < 300) return ICON_THUNDER;
  if (id >= 300 && id < 400) return ICON_DRIZZLE;
  if (id == 511) return ICON_SLEET;               // gefrierender Regen
  if (id >= 500 && id < 520) return ICON_RAIN;
  if (id >= 520 && id < 600) return ICON_SHOWERS;
  if (id >= 611 && id <= 616) return ICON_SLEET;  // Schneeregen
  if (id >= 600 && id < 700) return ICON_SNOW;
  if (id >= 700 && id < 800) return ICON_FOG;
  if (id == 800) return ICON_CLEAR;
  if (id == 801) return ICON_FEW_CLOUDS;
  if (id == 802) return ICON_CLOUDS;
  if (id >= 803 && id < 900) return ICON_OVERCAST;
  return ICON_CLOUDS;
}

namespace {

constexpr int KINDS = ICON_THUNDER + 1;

struct DayAccumulator
{
  DayForecast* day;
  int  dayCounts[KINDS];  // Häufigkeit der Wetterlagen tagsüber
  int  allCounts[KINDS];
  int  firstId[KINDS];
  bool afternoon;         // Nachmittag (Tageshöchstwert) abgedeckt?

  void start(DayForecast* d, time_t dt, float temp)
  {
    day = d;
    day->dt = dt;
    day->tMin = day->tMax = temp;
    day->pop = 0;
    day->precip = 0;
    day->weatherId = 0;
    memset(dayCounts, 0, sizeof(dayCounts));
    memset(allCounts, 0, sizeof(allCounts));
    memset(firstId, 0, sizeof(firstId));
    afternoon = false;
  }

  void addCondition(int weatherId, bool night)
  {
    int k = iconKindFromId(weatherId);
    if (!firstId[k]) firstId[k] = weatherId;
    allCounts[k]++;
    if (!night) dayCounts[k]++;
  }

  void add(const ForecastSlot& s, int localHour)
  {
    if (s.temp < day->tMin) day->tMin = s.temp;
    if (s.temp > day->tMax) day->tMax = s.temp;
    if (s.pop > day->pop) day->pop = s.pop;
    day->precip += s.precip;
    addCondition(s.weatherId, s.night);
    if (localHour >= 12 && localHour <= 18) afternoon = true;
  }

  // Häufigste Wetterlage tagsüber; bei Gleichstand gewinnt die "heftigere"
  void finish()
  {
    bool anyDay = false;
    for (int k = 0; k < KINDS; k++) anyDay |= dayCounts[k] > 0;
    const int* counts = anyDay ? dayCounts : allCounts;
    int best = -1;
    for (int k = 0; k < KINDS; k++)
      if (counts[k] > 0 && (best < 0 || counts[k] >= counts[best])) best = k;
    day->weatherId = best >= 0 ? firstId[best] : 800;
  }
};

}  // namespace

int buildDailyForecast(WeatherData& data, time_t now)
{
  struct tm nowTm = toLocal(now);
  const int todayKey = nowTm.tm_year * 400 + nowTm.tm_yday;

  DayAccumulator acc;
  data.dayCount = 0;
  acc.start(&data.days[0], now, data.now.temp);
  acc.addCondition(data.now.weatherId, data.now.night);
  int curKey = todayKey;

  for (int i = 0; i < data.slotCount; i++)
  {
    const ForecastSlot& s = data.slots[i];
    struct tm t = toLocal(s.dt);
    int key = t.tm_year * 400 + t.tm_yday;
    if (key < todayKey) continue;
    if (key != curKey)
    {
      // vorherigen Tag abschließen (heute immer übernehmen)
      if (curKey == todayKey || acc.afternoon)
      {
        acc.finish();
        data.dayCount++;
      }
      if (data.dayCount >= MAX_DAYS) return data.dayCount;
      curKey = key;
      acc.start(&data.days[data.dayCount], s.dt, s.temp);
    }
    acc.add(s, t.tm_hour);
  }
  if (curKey == todayKey || acc.afternoon)
  {
    acc.finish();
    data.dayCount++;
  }
  return data.dayCount;
}
