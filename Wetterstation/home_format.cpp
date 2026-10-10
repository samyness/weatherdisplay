// Formatierung von Home-Assistant-Zuständen (ohne ESP8266-Abhängigkeiten)
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "forecast.h"
#include "ha_client.h"
#include "text_de.h"

// Tage seit 1970-01-01 <-> Kalenderdatum (Algorithmus von Howard Hinnant)
static long daysFromCivil(int y, int m, int d)
{
  y -= m <= 2;
  const long era = (y >= 0 ? y : y - 399) / 400;
  const long yoe = y - era * 400;
  const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static void civilFromDays(long z, int& y, int& m, int& d)
{
  z += 719468;
  const long era = (z >= 0 ? z : z - 146096) / 146097;
  const long doe = z - era * 146097;
  const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  const long mp = (5 * doy + 2) / 153;
  d = (int)(doy - (153 * mp + 2) / 5 + 1);
  m = (int)(mp < 10 ? mp + 3 : mp - 9);
  y = (int)(yoe + era * 400 + (m <= 2));
}

// "2026-10-12" und "2026-10-13 00:00:00" (Kalender, Ortszeit) -> Tag direkt;
// "2026-10-12T23:30:00+00:00" bzw. "...Z" -> von UTC in Ortszeit umrechnen
static bool parseIsoDate(const char* s, long& days)
{
  int y, mo, d;
  if (strlen(s) < 10 || s[4] != '-' || s[7] != '-' || sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) != 3) return false;
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return false;
  days = daysFromCivil(y, mo, d);
  if (s[10] != 'T' && s[10] != ' ') return s[10] == '\0';

  int h = 0, mi = 0, sec = 0;
  if (sscanf(s + 11, "%2d:%2d:%2d", &h, &mi, &sec) < 2) return false;
  long offset = 0;  // Sekunden östlich von UTC
  const size_t len = strlen(s);
  const char* z = s + (len > 16 ? 16 : len);
  while (*z && *z != 'Z' && *z != '+' && *z != '-') z++;  // Sekunden/Bruchteile überspringen
  if (!*z) return true;  // keine Zeitzone angegeben = Ortszeit, Datum gilt direkt
  if (*z == '+' || *z == '-')
  {
    int oh = 0, om = 0;
    sscanf(z + 1, "%2d:%2d", &oh, &om);
    offset = (oh * 3600L + om * 60L) * (*z == '-' ? -1 : 1);
  }
  const time_t utc = (time_t)days * 86400 + h * 3600L + mi * 60L + sec - offset;
  const struct tm lt = toLocal(utc);
  days = daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
  return true;
}

void formatDayDistance(const char* isoDate, int days, char* out, size_t outSize)
{
  if (days == 0) return copyStr(out, "Heute", outSize);
  if (days == 1) return copyStr(out, "Morgen", outSize);
  long d;
  if (isoDate && parseIsoDate(isoDate, d))
  {
    int y, m, dd;
    civilFromDays(d, y, m, dd);
    const int wday = (int)(((d % 7) + 11) % 7);
    if (days >= 2 && days <= 6) return copyStr(out, weekdayLong(wday), outSize);  // diese Woche
    snprintf(out, outSize, "%s, %d.%d.", weekdayShort(wday), dd, m);
    return;
  }
  if (days > 1)
    snprintf(out, outSize, "in %d Tagen", days);
  else
    copyStr(out, "-", outSize);
}

static const char* translateState(const char* s)
{
  static const char* const MAP[][2] = {
    {"on", "Ein"}, {"off", "Aus"}, {"open", "Offen"}, {"closed", "Geschlossen"},
    {"home", "Zuhause"}, {"not_home", "Abwesend"}, {"locked", "Versperrt"}, {"unlocked", "Offen"},
  };
  for (const auto& m : MAP)
    if (!strcmp(s, m[0])) return m[1];
  return s;
}

void formatHomeState(const char* state, const char* unit, char* out, size_t outSize, HomeKind& kind)
{
  kind = HOME_GENERIC;
  if (!unit) unit = "";
  if (!state || !*state || !strcmp(state, "unknown") || !strcmp(state, "unavailable"))
  {
    copyStr(out, "-", outSize);
    return;
  }

  // Zahl: Dezimalkomma, sinnvolle Nachkommastellen
  char* end;
  double v = strtod(state, &end);
  if (end != state && *end == '\0')
  {
    const bool temp = !strcmp(unit, "\xC2\xB0" "C") || !strcmp(unit, "\xC2\xB0" "F");
    const bool hum = !strcmp(unit, "%");
    kind = temp ? HOME_TEMPERATURE : (hum ? HOME_HUMIDITY : HOME_GENERIC);
    int decimals = (hum || fabs(v) >= 100) ? 0 : 1;
    const double scale = decimals ? 10.0 : 1.0;
    v = floor(v * scale + 0.5) / scale;
    if (v == 0) v = 0;  // kein "-0,0"
    if (!temp && decimals == 1 && v == floor(v)) decimals = 0;  // "3" statt "3,0"
    char num[24];
    snprintf(num, sizeof(num), "%.*f", decimals, v);
    for (char* p = num; *p; p++)
      if (*p == '.') *p = ',';
    char raw[48];
    snprintf(raw, sizeof(raw), unit[0] ? "%s %s" : "%s", num, unit);
    utf8Sanitize(raw, out, outSize);
    return;
  }

  // Datum, z. B. Müllabfuhr: "2026-10-12" oder Zeitstempel "2026-10-12T05:00:00+00:00"
  long days;
  if (parseIsoDate(state, days))
  {
    const time_t now = time(nullptr);
    if (now > 1700000000)  // Uhr gestellt -> "Heute"/"Morgen" statt Datum
    {
      const struct tm lt = toLocal(now);
      const long today = daysFromCivil(lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
      if (days == today) return copyStr(out, "Heute", outSize);
      if (days == today + 1) return copyStr(out, "Morgen", outSize);
    }
    int y, m, d;
    civilFromDays(days, y, m, d);
    snprintf(out, outSize, "%s, %d.%d.", weekdayShort((int)(((days % 7) + 11) % 7)), d, m);
    return;
  }

  // Text, z. B. "Restmüll" (Umlaute werden bereinigt)
  char raw[64];
  snprintf(raw, sizeof(raw), unit[0] ? "%s %s" : "%s", translateState(state), unit);
  utf8Sanitize(raw, out, outSize);
  utf8CapitalizeFirst(out);
}
