#include "layout.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "draw_utils.h"
#include "fonts.h"
#include "forecast.h"
#include "ha_client.h"
#include "icons.h"
#include "text_de.h"

namespace {

constexpr uint16_t BLACK = GxEPD_BLACK;
constexpr uint16_t WHITE = GxEPD_WHITE;

constexpr int W = 800;
constexpr int H = 480;
constexpr int MARGIN = 24;

// Schriften: _tf = voller Latin-1-Zeichensatz inkl. ä ö ü ß °
#define FONT_TEMP_HUGE   FONT_LOGISOSO92_TN
#define FONT_TITLE       FONT_FUB25_TF
#define FONT_TITLE_S     FONT_FUB20_TF
#define FONT_H1          FONT_FUB17_TF
#define FONT_H2          FONT_FUB14_TF
#define FONT_H3          FONT_FUB11_TF
#define FONT_BODY_L      FONT_FUR17_TF
#define FONT_BODY        FONT_FUR14_TF
#define FONT_SMALL       FONT_FUR11_TF

const char DEG[] = "\xC2\xB0";  // °
const char DOT[] = "\xC2\xB7";  // ·

enum Align { LEFT, CENTER, RIGHT };

int textWidth(U8G2_FOR_ADAFRUIT_GFX& u, const FlashFont& font, const char* s)
{
  char buf[128];
  utf8Sanitize(s, buf, sizeof(buf));
  if (!useFont(u, font)) return 0;
  return u.getUTF8Width(buf);
}

int drawText(U8G2_FOR_ADAFRUIT_GFX& u, int x, int y, const char* s, const FlashFont& font,
             uint16_t color, Align align = LEFT)
{
  char buf[128];
  utf8Sanitize(s, buf, sizeof(buf));
  if (!useFont(u, font)) return 0;
  u.setFontMode(1);
  u.setForegroundColor(color);
  int w = u.getUTF8Width(buf);
  if (align == CENTER) x -= w / 2;
  else if (align == RIGHT) x -= w;
  u.drawUTF8(x, y, buf);
  return w;
}

// Nimmt die erste Schrift, in die der Text passt; sonst wird mit "..." gekürzt
int drawFitted(U8G2_FOR_ADAFRUIT_GFX& u, int x, int y, const char* s, int maxWidth,
               const FlashFont* const* fonts, int fontCount, uint16_t color, Align align = LEFT)
{
  for (int i = 0; i < fontCount; i++)
    if (textWidth(u, *fonts[i], s) <= maxWidth)
      return drawText(u, x, y, s, *fonts[i], color, align);

  const FlashFont& font = *fonts[fontCount - 1];
  char buf[128];
  utf8Sanitize(s, buf, sizeof(buf));
  size_t len = strlen(buf);
  char tmp[132];
  while (len > 0)
  {
    len--;
    while (len > 0 && ((unsigned char)buf[len] & 0xC0) == 0x80) len--;  // nicht mitten im Umlaut trennen
    memcpy(tmp, buf, len);
    while (len > 0 && tmp[len - 1] == ' ') len--;
    strcpy(tmp + len, "...");
    if (textWidth(u, font, tmp) <= maxWidth) break;
  }
  return drawText(u, x, y, tmp, font, color, align);
}

int roundTemp(float t)
{
  int v = (int)lroundf(t);
  return v == 0 ? 0 : v;  // kein "-0"
}

void fmtTemp(char* buf, size_t size, float t)
{
  snprintf(buf, size, "%d%s", roundTemp(t), DEG);
}

void fmtTime(char* buf, size_t size, time_t t)
{
  struct tm lt = toLocal(t);
  snprintf(buf, size, "%02d:%02d", lt.tm_hour, lt.tm_min);
}

bool sameLocalDay(time_t a, time_t b)
{
  struct tm ta = toLocal(a), tb = toLocal(b);
  return ta.tm_year == tb.tm_year && ta.tm_yday == tb.tm_yday;
}

// ---- Kopfzeile ---------------------------------------------------------

int chipWidth(U8G2_FOR_ADAFRUIT_GFX& u, const char* text);
void drawChip(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, int x, int cy, const char* text, bool filled);

void drawHeader(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d, const char* cityName)
{
  const time_t now = d.updated ? d.updated : d.now.dt;
  struct tm lt = toLocal(now);

  char date[64];
  snprintf(date, sizeof(date), "%s, %d. %s", weekdayLong(lt.tm_wday), lt.tm_mday, monthLong(lt.tm_mon));
  const int dateW = drawText(u, W - MARGIN, 36, date, FONT_H2, BLACK, RIGHT);

  char upd[48], hm[8];
  fmtTime(hm, sizeof(hm), now);
  snprintf(upd, sizeof(upd), "Aktualisiert %s", hm);
  drawText(u, W - MARGIN, 60, upd, FONT_SMALL, BLACK, RIGHT);

  const char* city = (cityName && cityName[0]) ? cityName : d.now.city;
  drawPinIcon(g, MARGIN + 11, 36, 30, BLACK, WHITE);
  static const FlashFont* const cityFonts[] = {&FONT_TITLE, &FONT_TITLE_S, &FONT_H1};
  const int cityX = MARGIN + 32;
  const int cityW = drawFitted(u, cityX, 50, city, W - MARGIN - dateW - 32 - cityX, cityFonts, 3, BLACK);

  // Hinweise aus Home Assistant, so viele wie neben Ort und Datum passen
  const HomeDisplay& h = d.homeDisplay;
  int x = cityX + cityW + 16;
  const int limit = W - MARGIN - dateW - 14;
  for (int i = 0; h.valid && i < h.noteCount; i++)
  {
    const int cw = chipWidth(u, h.notes[i].text);
    if (x + cw > limit)
    {
      char more[8];  // nicht alles passt: Anzahl der übrigen Hinweise anzeigen
      snprintf(more, sizeof(more), "+%d", h.noteCount - i);
      if (x + textWidth(u, FONT_H2, more) <= limit) drawText(u, x, 43, more, FONT_H2, BLACK);
      break;
    }
    drawChip(g, u, x, 37, h.notes[i].text, h.notes[i].prio >= 2);
    x += cw + 8;
  }
}

// ---- Aktuelles Wetter --------------------------------------------------

constexpr int HERO_Y = 80;
constexpr int PANEL_X = 520;
constexpr int PANEL_W = W - MARGIN - PANEL_X;
constexpr int PANEL_H = 180;

void drawHero(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d)
{
  const CurrentWeather& c = d.now;
  drawWeatherIcon(g, iconKindFromId(c.weatherId), c.night, MARGIN - 4, HERO_Y, 180, BLACK, WHITE);

  // große Temperatur, das Gradzeichen wird als Ring gezeichnet
  const int tx = 224, base = HERO_Y + 98;
  char num[8];
  snprintf(num, sizeof(num), "%d", roundTemp(c.temp));
  const int tw = drawText(u, tx, base, num, FONT_TEMP_HUGE, BLACK);
  const int rx = tx + tw + 16, ry = base - 80;
  g.fillCircle(rx, ry, 11, BLACK);
  g.fillCircle(rx, ry, 6, WHITE);

  static const FlashFont* const descFonts[] = {&FONT_H1, &FONT_H2};
  drawFitted(u, tx, base + 42, c.description, PANEL_X - 16 - tx, descFonts, 2, BLACK);

  // Tageshöchst-/tiefstwert und gefühlte Temperatur
  char buf[24];
  int x = tx, y = base + 76;
  if (d.dayCount > 0 && sameLocalDay(d.days[0].dt, d.updated ? d.updated : c.dt) &&
      roundTemp(d.days[0].tMax) != roundTemp(d.days[0].tMin))
  {
    drawArrowIcon(g, x + 6, y - 7, 13, true, BLACK);
    fmtTemp(buf, sizeof(buf), d.days[0].tMax);
    x += 18 + drawText(u, x + 18, y, buf, FONT_H2, BLACK) + 14;
    drawArrowIcon(g, x + 6, y - 6, 13, false, BLACK);
    fmtTemp(buf, sizeof(buf), d.days[0].tMin);
    x += 18 + drawText(u, x + 18, y, buf, FONT_BODY, BLACK) + 12;
    x += drawText(u, x, y, DOT, FONT_BODY, BLACK) + 12;
  }
  char feels[40];
  fmtTemp(buf, sizeof(buf), c.feelsLike);
  snprintf(feels, sizeof(feels), "Gef\xC3\xBChlt %s", buf);  // Gefühlt
  if (x + textWidth(u, FONT_BODY, feels) <= PANEL_X - 12) drawText(u, x, y, feels, FONT_BODY, BLACK);
}

enum RowIcon { RI_DROP, RI_WIND, RI_GAUGE, RI_SUN, RI_THERMO, RI_HOME, RI_BIN, RI_SOLAR, RI_BATTERY };

struct PanelRow
{
  RowIcon icon;
  char    label[32];
  char    value[32];
};

constexpr int PANEL_ROWS = 4;

void setRow(PanelRow& r, RowIcon icon, const char* label, const char* value)
{
  r.icon = icon;
  copyStr(r.label, label, sizeof(r.label));
  copyStr(r.value, value, sizeof(r.value));
}

void drawRowIcon(Adafruit_GFX& g, RowIcon icon, int cx, int cy, uint16_t fg = WHITE, uint16_t bg = BLACK)
{
  switch (icon)
  {
  case RI_DROP:    drawDropIcon(g, cx, cy, 22, fg); break;
  case RI_WIND:    drawWindIcon(g, cx, cy, 24, fg); break;
  case RI_GAUGE:   drawGaugeIcon(g, cx, cy + 1, 24, fg, bg); break;
  case RI_SUN:     drawSunriseIcon(g, cx, cy - 2, 26, fg, bg); break;
  case RI_THERMO:  drawThermometerIcon(g, cx, cy, 24, fg, bg); break;
  case RI_HOME:    drawHomeIcon(g, cx, cy, 22, fg, bg); break;
  case RI_BIN:     drawBinIcon(g, cx, cy, 22, fg, bg); break;
  case RI_SOLAR:   drawSolarIcon(g, cx, cy, 26, fg, bg); break;
  case RI_BATTERY:
    g.fillRoundRect(cx - 6, cy - 9, 12, 20, 2, fg);
    g.fillRect(cx - 3, cy - 12, 6, 4, fg);
    g.fillRect(cx - 4, cy - 7, 8, 7, bg);  // halb voll
    break;
  }
}

void drawPanel(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d)
{
  const CurrentWeather& c = d.now;

  // Wetterzeilen in fester Reihenfolge; Home-Assistant-Werte verdrängen
  // zuerst den Luftdruck, dann die Sonnenzeiten, dann den Wind.
  PanelRow weatherRows[PANEL_ROWS];
  setRow(weatherRows[0], RI_DROP, "Luftfeuchte", "-");
  if (c.humidity >= 0) snprintf(weatherRows[0].value, sizeof(weatherRows[0].value), "%d %%", c.humidity);
  setRow(weatherRows[1], RI_WIND, "Wind", "-");
  if (c.windSpeed >= 0)
    snprintf(weatherRows[1].value, sizeof(weatherRows[1].value), "%d km/h %s", (int)lroundf(c.windSpeed * 3.6f),
             windDirection(c.windDeg));
  setRow(weatherRows[2], RI_GAUGE, "Luftdruck", "-");
  if (c.pressure >= 0) snprintf(weatherRows[2].value, sizeof(weatherRows[2].value), "%d hPa", c.pressure);
  setRow(weatherRows[3], RI_SUN, "Sonne", "-");
  if (c.sunrise)
  {
    char rise[8], set[8];
    fmtTime(rise, sizeof(rise), c.sunrise);
    fmtTime(set, sizeof(set), c.sunset);
    snprintf(weatherRows[3].value, sizeof(weatherRows[3].value), "%s %s %s", rise, DOT, set);
  }
  static const int dropOrder[PANEL_ROWS] = {2, 3, 1, 0};

  const int homeRows = d.homeCount < PANEL_ROWS ? d.homeCount : PANEL_ROWS;
  bool keep[PANEL_ROWS] = {true, true, true, true};
  for (int i = 0; i < homeRows; i++) keep[dropOrder[i]] = false;

  PanelRow rows[PANEL_ROWS];
  int n = 0;
  for (int i = 0; i < PANEL_ROWS; i++)
    if (keep[i]) rows[n++] = weatherRows[i];
  for (int i = 0; i < homeRows; i++)
  {
    const HomeValue& h = d.home[i];
    PanelRow& r = rows[n++];
    r.icon = h.kind == HOME_TEMPERATURE ? RI_THERMO
           : h.kind == HOME_HUMIDITY  ? RI_DROP
           : h.kind == HOME_WASTE     ? RI_BIN
                                      : RI_HOME;
    copyStr(r.label, h.label, sizeof(r.label));
    copyStr(r.value, h.value, sizeof(r.value));
  }

  g.fillRoundRect(PANEL_X, HERO_Y, PANEL_W, PANEL_H, 18, BLACK);
  const int ix = PANEL_X + 26, lx = PANEL_X + 50, rx = PANEL_X + PANEL_W - 18;
  static const FlashFont* const labelFonts[] = {&FONT_BODY, &FONT_SMALL};
  static const FlashFont* const valueFonts[] = {&FONT_H2, &FONT_H3};
  const int maxValueW = (rx - lx) * 70 / 100;
  for (int i = 0; i < n; i++)
  {
    const int cy = HERO_Y + 24 + i * 44;
    drawRowIcon(g, rows[i].icon, ix, cy);
    const int vw = drawFitted(u, rx, cy + 6, rows[i].value, maxValueW, valueFonts, 2, WHITE, RIGHT);
    drawFitted(u, lx, cy + 6, rows[i].label, rx - vw - 10 - lx, labelFonts, 2, WHITE);
    if (i < n - 1)
      for (int px = lx; px < rx; px += 4) g.drawPixel(px, cy + 22, WHITE);
  }
}

// ---- Zuhause-Spalte (Home-Assistant-Template-Sensor) -------------------
// Weiße Spalte statt der schwarzen Kachel: Schwarz bleibt für Handlungsbedarf
// reserviert (Müll morgen noch nicht draußen, Hinweis-Chips).

constexpr int HOME_ICON_X = PANEL_X + 14;
constexpr int HOME_LABEL_X = PANEL_X + 36;
constexpr int HOME_RIGHT = W - MARGIN;

void homeRow(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, RowIcon icon, int cy, const char* label, const char* value)
{
  static const FlashFont* const labelFonts[] = {&FONT_BODY, &FONT_SMALL};
  static const FlashFont* const valueFonts[] = {&FONT_H2, &FONT_H3};
  drawRowIcon(g, icon, HOME_ICON_X, cy, BLACK, WHITE);
  const int vw = drawFitted(u, HOME_RIGHT, cy + 6, value, (HOME_RIGHT - HOME_LABEL_X) * 70 / 100, valueFonts, 2,
                            BLACK, RIGHT);
  drawFitted(u, HOME_LABEL_X, cy + 6, label, HOME_RIGHT - vw - 10 - HOME_LABEL_X, labelFonts, 2, BLACK);
}

void dottedLine(Adafruit_GFX& g, int y)
{
  for (int x = HOME_LABEL_X; x < HOME_RIGHT; x += 4) g.drawPixel(x, y, BLACK);
}

void decimalComma(char* s)
{
  for (; *s; s++)
    if (*s == '.') *s = ',';
}

void batteryRow(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, int cy, int pct)
{
  drawRowIcon(g, RI_BATTERY, HOME_ICON_X, cy, BLACK, WHITE);
  char buf[12];
  snprintf(buf, sizeof(buf), "%d %%", pct);
  const int pw = drawText(u, HOME_RIGHT, cy + 6, buf, FONT_H2, BLACK, RIGHT);
  const int lw = drawText(u, HOME_LABEL_X, cy + 6, "Akku", FONT_BODY, BLACK);

  // Balken als große Batterie: Kontur, Pol rechts, Füllung nach Ladestand
  const int bx = HOME_LABEL_X + lw + 12, bh = 18, by = cy - bh / 2 + 1;
  const int bw = HOME_RIGHT - pw - 14 - bx - 4;
  if (bw < 30) return;
  g.fillRoundRect(bx, by, bw, bh, 4, BLACK);
  g.fillRoundRect(bx + 2, by + 2, bw - 4, bh - 4, 3, WHITE);
  g.fillRect(bx + bw, by + 5, 4, bh - 10, BLACK);
  const int inner = bw - 8;
  const int fill = inner * pct / 100;
  if (fill > 0) g.fillRect(bx + 4, by + 4, fill, bh - 8, BLACK);
}

void wasteCard(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const HomeDisplay& h, int y, int height)
{
  const int x = PANEL_X, w = W - MARGIN - PANEL_X;
  const bool soon = h.wasteDays >= 0 && h.wasteDays <= 1;  // "rausgestellt" zählt nur für die nächste Abholung
  const bool action = soon && h.wasteOut == 0;
  const bool done = soon && h.wasteOut == 1;
  const uint16_t fg = action ? WHITE : BLACK, bg = action ? BLACK : WHITE;

  g.fillRoundRect(x, y, w, height, 10, BLACK);
  if (!action) g.fillRoundRect(x + 2, y + 2, w - 4, height - 4, 8, WHITE);
  drawBinIcon(g, x + 26, y + height / 2 + 2, 28, fg, bg);

  const int tx = x + 52, maxW = x + w - 12 - tx;
  static const FlashFont* const kindFonts[] = {&FONT_H2, &FONT_BODY, &FONT_SMALL};
  drawFitted(u, tx, y + 24, h.wasteKind, maxW, kindFonts, 3, fg);

  char when[32], status[48];
  formatDayDistance(h.wasteDate, h.wasteDays, when, sizeof(when));
  if (action)
    snprintf(status, sizeof(status), h.wasteDays == 0 ? "Heute Abholung!" : "%s rausstellen!", when);
  else if (done)
    snprintf(status, sizeof(status), "%s %s bereit", when, DOT);
  else
    copyStr(status, when, sizeof(status));
  const int sw = drawText(u, tx, y + 46, status, action ? FONT_H3 : FONT_BODY, fg);
  if (done) drawCheckIcon(g, tx + sw + 14, y + 40, 16, fg);
}

void drawHomeColumn(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d)
{
  const CurrentWeather& c = d.now;
  const HomeDisplay& h = d.homeDisplay;
  char buf[40];

  // Wetterdetails, kompakt
  copyStr(buf, "-", sizeof(buf));
  if (c.windSpeed >= 0)
    snprintf(buf, sizeof(buf), "%d km/h %s", (int)lroundf(c.windSpeed * 3.6f), windDirection(c.windDeg));
  homeRow(g, u, RI_WIND, HERO_Y + 14, "Wind", buf);
  copyStr(buf, "-", sizeof(buf));
  if (c.humidity >= 0) snprintf(buf, sizeof(buf), "%d %%", c.humidity);
  homeRow(g, u, RI_DROP, HERO_Y + 44, "Luftfeuchte", buf);
  dottedLine(g, HERO_Y + 62);

  // Energie: PV-Tagesertrag und Akku; ohne Daten die übrigen Wetterwerte
  if (h.pvTodayKwh >= 0 || h.batteryPct >= 0)
  {
    copyStr(buf, "-", sizeof(buf));
    if (h.pvTodayKwh >= 0)
    {
      snprintf(buf, sizeof(buf), h.pvTodayKwh < 100 ? "%.1f kWh" : "%.0f kWh", h.pvTodayKwh);
      decimalComma(buf);
    }
    homeRow(g, u, RI_SOLAR, HERO_Y + 80, "PV heute", buf);
    if (h.batteryPct >= 0)
      batteryRow(g, u, HERO_Y + 110, h.batteryPct);
    else
      homeRow(g, u, RI_BATTERY, HERO_Y + 110, "Akku", "-");
  }
  else
  {
    copyStr(buf, "-", sizeof(buf));
    if (c.pressure >= 0) snprintf(buf, sizeof(buf), "%d hPa", c.pressure);
    homeRow(g, u, RI_GAUGE, HERO_Y + 80, "Luftdruck", buf);
    copyStr(buf, "-", sizeof(buf));
    if (c.sunrise)
    {
      char rise[8], set[8];
      fmtTime(rise, sizeof(rise), c.sunrise);
      fmtTime(set, sizeof(set), c.sunset);
      snprintf(buf, sizeof(buf), "%s %s %s", rise, DOT, set);
    }
    homeRow(g, u, RI_SUN, HERO_Y + 110, "Sonne", buf);
  }

  if (h.hasWaste) wasteCard(g, u, h, HERO_Y + 130, 54);
}

// Hinweis-Chip in der Kopfzeile: schwarz = Aufgabe, umrandet = Info
int chipWidth(U8G2_FOR_ADAFRUIT_GFX& u, const char* text) { return textWidth(u, FONT_H3, text) + 24; }

void drawChip(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, int x, int cy, const char* text, bool filled)
{
  const int w = chipWidth(u, text), h = 30, r = h / 2;
  g.fillRoundRect(x, cy - r, w, h, r, BLACK);
  if (!filled) g.fillRoundRect(x + 2, cy - r + 2, w - 4, h - 4, r - 2, WHITE);
  drawText(u, x + 12, cy + 6, text, FONT_H3, filled ? WHITE : BLACK);
}

// ---- Verlauf der nächsten 24 Stunden -----------------------------------

constexpr int CHART_POINTS = 8;
constexpr int CHART_TOP = 300;     // höchster Temperaturpunkt
constexpr int CHART_BOTTOM = 326;  // tiefster Temperaturpunkt
constexpr int BAR_BASE = 356;      // Grundlinie der Niederschlagsbalken
constexpr int BAR_MAX = 22;

void drawHourly(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d)
{
  const int n = d.slotCount < CHART_POINTS ? d.slotCount : CHART_POINTS;
  if (n < 2) return;
  const float step = (float)(W - 2 * MARGIN) / n;

  float tMin = d.slots[0].temp, tMax = d.slots[0].temp;
  for (int i = 1; i < n; i++)
  {
    if (d.slots[i].temp < tMin) tMin = d.slots[i].temp;
    if (d.slots[i].temp > tMax) tMax = d.slots[i].temp;
  }
  if (tMax - tMin < 4)  // flache Kurven nicht übertrieben steil zeichnen
  {
    float mid = (tMax + tMin) / 2;
    tMin = mid - 2;
    tMax = mid + 2;
  }
  int px[CHART_POINTS], py[CHART_POINTS];
  for (int i = 0; i < n; i++)
  {
    px[i] = (int)(MARGIN + step * (i + 0.5f));
    py[i] = (int)lroundf(CHART_BOTTOM - (d.slots[i].temp - tMin) / (tMax - tMin) * (CHART_BOTTOM - CHART_TOP));
  }

  // Niederschlag als graue Balken unter der Kurve
  const int bw = (int)(step * 0.42f);
  for (int i = 0; i < n; i++)
  {
    const ForecastSlot& s = d.slots[i];
    if (s.precip < 0.05f) continue;
    float f = s.precip / 4.0f;  // 4 mm in 3 h = voller Balken
    if (f > 1) f = 1;
    int h = 5 + (int)(f * (BAR_MAX - 5));
    ditherRect(g, px[i] - bw / 2, BAR_BASE - h, bw, h, 2, BLACK);
    g.drawRect(px[i] - bw / 2, BAR_BASE - h, bw, h + 1, BLACK);
  }
  for (int x = MARGIN; x < W - MARGIN; x += 4) g.drawPixel(x, BAR_BASE, BLACK);

  // Temperaturkurve
  for (int i = 0; i + 1 < n; i++) thickLine(g, px[i], py[i], px[i + 1], py[i + 1], 3, BLACK);

  char buf[16];
  for (int i = 0; i < n; i++)
  {
    const ForecastSlot& s = d.slots[i];
    g.fillCircle(px[i], py[i], 6, BLACK);
    g.fillCircle(px[i], py[i], 3, WHITE);
    fmtTemp(buf, sizeof(buf), s.temp);
    drawText(u, px[i], py[i] - 12, buf, FONT_H3, BLACK, CENTER);

    struct tm lt = toLocal(s.dt);
    snprintf(buf, sizeof(buf), "%02d:00", lt.tm_hour);
    drawText(u, px[i], BAR_BASE + 20, buf, FONT_BODY, BLACK, CENTER);
  }
}

// ---- Tageskarten -------------------------------------------------------

constexpr int CARDS_Y = 386;
constexpr int CARD_H = 86;
constexpr int CARD_COUNT = 4;

void drawDayCard(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const DayForecast& day, const char* name,
                 int x, int w)
{
  g.fillRoundRect(x, CARDS_Y, w, CARD_H, 10, BLACK);
  g.fillRoundRect(x + 2, CARDS_Y + 2, w - 4, CARD_H - 4, 8, WHITE);

  drawText(u, x + 14, CARDS_Y + 26, name, FONT_H2, BLACK);
  drawWeatherIcon(g, iconKindFromId(day.weatherId), false, x + 10, CARDS_Y + 33, 48, BLACK, WHITE);

  const int tx = x + 70;
  char buf[16];
  fmtTemp(buf, sizeof(buf), day.tMax);
  int mx = tx + drawText(u, tx, CARDS_Y + 57, buf, FONT_H1, BLACK) + 8;
  fmtTemp(buf, sizeof(buf), day.tMin);
  drawText(u, mx, CARDS_Y + 57, buf, FONT_BODY_L, BLACK);

  if (day.pop >= 0.1f)
  {
    drawDropIcon(g, tx + 6, CARDS_Y + 70, 15, BLACK);
    snprintf(buf, sizeof(buf), "%d %%", (int)lroundf(day.pop * 100));
    drawText(u, tx + 18, CARDS_Y + 77, buf, FONT_BODY, BLACK);
  }
}

void drawDays(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d)
{
  // Heute steht schon groß oben, die Karten zeigen die nächsten Tage
  const time_t now = d.updated ? d.updated : d.now.dt;
  struct tm tomorrow = toLocal(now);
  tomorrow.tm_mday += 1;
  tomorrow.tm_hour = 12;
  tomorrow.tm_isdst = -1;
  const time_t tomorrowNoon = mktime(&tomorrow);
  int first = 0;
  while (first < d.dayCount && sameLocalDay(d.days[first].dt, now)) first++;
  int n = d.dayCount - first;
  if (n > CARD_COUNT) n = CARD_COUNT;
  if (n <= 0) return;

  const int gap = 14;
  const int w = (W - 2 * MARGIN - (CARD_COUNT - 1) * gap) / CARD_COUNT;
  for (int i = 0; i < n; i++)
  {
    const DayForecast& day = d.days[first + i];
    struct tm lt = toLocal(day.dt);
    char name[24];
    if (sameLocalDay(day.dt, tomorrowNoon))
      copyStr(name, "Morgen", sizeof(name));
    else if (textWidth(u, FONT_H2, weekdayLong(lt.tm_wday)) <= w - 28)
      copyStr(name, weekdayLong(lt.tm_wday), sizeof(name));
    else
      snprintf(name, sizeof(name), "%s, %d.", weekdayShort(lt.tm_wday), lt.tm_mday);
    drawDayCard(g, u, day, name, MARGIN + i * (w + gap), w);
  }
}

}  // namespace

void drawWeatherScreen(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d,
                       const char* cityName)
{
  g.fillScreen(WHITE);
  drawHeader(g, u, d, cityName);
  drawHero(g, u, d);
  if (d.homeDisplay.valid)
    drawHomeColumn(g, u, d);
  else
    drawPanel(g, u, d);
  drawHourly(g, u, d);
  drawDays(g, u, d);
}

void drawMessageScreen(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const char* title,
                       const char* line1, const char* line2)
{
  g.fillScreen(WHITE);
  drawWeatherIcon(g, ICON_CLOUDS, false, W / 2 - 80, 60, 160, BLACK, WHITE);
  drawText(u, W / 2, 280, title, FONT_TITLE, BLACK, CENTER);
  if (line1 && line1[0]) drawText(u, W / 2, 330, line1, FONT_BODY_L, BLACK, CENTER);
  if (line2 && line2[0]) drawText(u, W / 2, 364, line2, FONT_BODY, BLACK, CENTER);
}
