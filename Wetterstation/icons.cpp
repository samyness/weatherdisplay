#include "icons.h"
#include "draw_utils.h"
#include <math.h>

namespace {

// Quadrat, in dem ein Icon gezeichnet wird; Koordinaten relativ 0..1
struct Box
{
  float x, y, s;
  float px(float u) const { return x + u * s; }
  float py(float v) const { return y + v * s; }
};

// Wolke aus drei Kreisen und einer Pille, (cx, cy) = Mitte, w = Breite.
// grow > 0 vergrößert die Form (Abstandsrand), < 0 verkleinert sie (Kontur).
void cloudShape(Adafruit_GFX& g, float cx, float cy, float w, float grow, uint16_t c)
{
  const float k = w / 100.0f;
  fillCircleF(g, cx + 1 * k, cy - 4 * k, 30 * k + grow, c);
  fillCircleF(g, cx - 27 * k, cy + 12 * k, 22 * k + grow, c);
  fillCircleF(g, cx + 29 * k, cy + 14 * k, 20 * k + grow, c);
  int x0 = (int)floorf(cx - 49 * k - grow + 0.5f);
  int y0 = (int)floorf(cy + 2 * k - grow + 0.5f);
  int x1 = (int)floorf(cx + 47 * k + grow + 0.5f);
  int y1 = (int)floorf(cy + 34 * k + grow + 0.5f);
  int w2 = x1 - x0, h2 = y1 - y0;
  if (w2 <= 0 || h2 <= 0) return;
  int r = h2 / 2;
  g.fillRoundRect(x0, y0, w2, h2, r, c);
}

// Wolkenkontur mit optionalem Freiraum zu dahinterliegenden Elementen
void cloud(Adafruit_GFX& g, float cx, float cy, float w, float t, uint16_t fg, uint16_t bg, bool halo)
{
  if (halo) cloudShape(g, cx, cy, w, t, bg);
  cloudShape(g, cx, cy, w, 0, fg);
  cloudShape(g, cx, cy, w, -t, bg);
}

void sun(Adafruit_GFX& g, float cx, float cy, float r, float t, uint16_t fg, uint16_t bg)
{
  fillCircleF(g, cx, cy, r, fg);
  fillCircleF(g, cx, cy, r - t, bg);
  const float r0 = r * 1.45f, r1 = r * 2.1f;
  for (int i = 0; i < 8; i++)
  {
    float a = i * PI_F / 4;
    float c = cosf(a), s = sinf(a);
    thickLine(g, cx + r0 * c, cy + r0 * s, cx + r1 * c, cy + r1 * s, t, fg);
  }
}

// Mondsichel als Kontur: äußerer Bogen + innerer Bogen eines versetzten Kreises
void moon(Adafruit_GFX& g, float cx, float cy, float r, float t, uint16_t fg)
{
  const float dir = -PI_F / 4;  // Ausschnitt oben rechts
  const float ro = r - t / 2, ri = r * 0.82f, d = r * 0.52f;
  float ca = (d * d + ro * ro - ri * ri) / (2 * d * ro);
  float cb = (d * d + ri * ri - ro * ro) / (2 * d * ri);
  ca = ca > 1 ? 1 : (ca < -1 ? -1 : ca);
  cb = cb > 1 ? 1 : (cb < -1 ? -1 : cb);
  const float alpha = acosf(ca), beta = acosf(cb);
  thickArc(g, cx, cy, ro, dir + alpha, dir + 2 * PI_F - alpha, t, fg);
  const float ix = cx + d * cosf(dir), iy = cy + d * sinf(dir);
  thickArc(g, ix, iy, ri, dir + PI_F + beta, dir + PI_F - beta, t, fg);
}

void sunOrMoon(Adafruit_GFX& g, bool night, float cx, float cy, float r, float t, uint16_t fg, uint16_t bg)
{
  if (night)
    moon(g, cx, cy, r * 1.45f, t, fg);
  else
    sun(g, cx, cy, r, t, fg, bg);
}

void rainLine(Adafruit_GFX& g, const Box& b, float u, float v, float len, float t, uint16_t fg)
{
  thickLine(g, b.px(u), b.py(v), b.px(u - len * 0.35f), b.py(v + len), t, fg);
}

void snowflake(Adafruit_GFX& g, float cx, float cy, float r, float t, uint16_t fg)
{
  for (int i = 0; i < 3; i++)
  {
    float a = PI_F / 2 + i * PI_F / 3;
    float c = cosf(a) * r, s = sinf(a) * r;
    thickLine(g, cx - c, cy - s, cx + c, cy + s, t, fg);
  }
}

void bolt(Adafruit_GFX& g, const Box& b, float t, uint16_t fg, uint16_t bg)
{
  const float rel[14] = {0.50f, 0.54f, 0.65f, 0.54f, 0.57f, 0.69f, 0.67f, 0.69f,
                         0.44f, 0.97f, 0.51f, 0.77f, 0.41f, 0.77f};
  float pts[14];
  for (int i = 0; i < 7; i++)
  {
    pts[2 * i] = b.px(rel[2 * i]);
    pts[2 * i + 1] = b.py(rel[2 * i + 1]);
  }
  for (int i = 0; i < 7; i++)  // Freiraum rund um den Blitz
  {
    int j = (i + 1) % 7;
    thickLine(g, pts[2 * i], pts[2 * i + 1], pts[2 * j], pts[2 * j + 1], t * 2, bg);
  }
  fillPolygon(g, pts, 7, fg);
}

}  // namespace

void drawWeatherIcon(Adafruit_GFX& g, IconKind kind, bool night, int x, int y, int size,
                     uint16_t fg, uint16_t bg)
{
  const Box b = {(float)x, (float)y, (float)size};
  float t = floorf(size * 0.055f + 0.5f);
  if (t < 2) t = 2;
  const float s = (float)size;

  switch (kind)
  {
  case ICON_CLEAR:
    if (night)
      moon(g, b.px(0.5f), b.py(0.5f), s * 0.30f, t, fg);
    else
      sun(g, b.px(0.5f), b.py(0.5f), s * 0.20f, t, fg, bg);
    break;

  case ICON_FEW_CLOUDS:
    sunOrMoon(g, night, b.px(0.37f), b.py(0.37f), s * 0.15f, t, fg, bg);
    cloud(g, b.px(0.57f), b.py(0.64f), s * 0.68f, t, fg, bg, true);
    break;

  case ICON_CLOUDS:
    cloud(g, b.px(0.5f), b.py(0.53f), s * 0.86f, t, fg, bg, false);
    break;

  case ICON_OVERCAST:
    cloud(g, b.px(0.61f), b.py(0.39f), s * 0.60f, t, fg, bg, false);
    cloud(g, b.px(0.44f), b.py(0.61f), s * 0.76f, t, fg, bg, true);
    break;

  case ICON_FOG:
    cloud(g, b.px(0.5f), b.py(0.38f), s * 0.84f, t, fg, bg, false);
    thickLine(g, b.px(0.10f), b.py(0.78f), b.px(0.70f), b.py(0.78f), t, fg);
    thickLine(g, b.px(0.30f), b.py(0.90f), b.px(0.90f), b.py(0.90f), t, fg);
    break;

  case ICON_SHOWERS:
    sunOrMoon(g, night, b.px(0.32f), b.py(0.30f), s * 0.13f, t, fg, bg);
    cloud(g, b.px(0.56f), b.py(0.46f), s * 0.74f, t, fg, bg, true);
    rainLine(g, b, 0.44f, 0.77f, 0.15f, t, fg);
    rainLine(g, b, 0.62f, 0.77f, 0.15f, t, fg);
    break;

  default:  // Niederschlag: Wolke oben, darunter Regen/Schnee/Blitz
  {
    cloud(g, b.px(0.5f), b.py(0.40f), s * 0.84f, t, fg, bg, false);
    const float flakeR = s * 0.07f, flakeT = t > 3 ? t * 0.7f : 2;
    switch (kind)
    {
    case ICON_DRIZZLE:
      fillCircleF(g, b.px(0.33f), b.py(0.79f), t * 0.85f, fg);
      fillCircleF(g, b.px(0.51f), b.py(0.79f), t * 0.85f, fg);
      fillCircleF(g, b.px(0.69f), b.py(0.79f), t * 0.85f, fg);
      fillCircleF(g, b.px(0.42f), b.py(0.92f), t * 0.85f, fg);
      fillCircleF(g, b.px(0.60f), b.py(0.92f), t * 0.85f, fg);
      break;
    case ICON_RAIN:
      rainLine(g, b, 0.35f, 0.76f, 0.16f, t, fg);
      rainLine(g, b, 0.53f, 0.80f, 0.16f, t, fg);
      rainLine(g, b, 0.71f, 0.76f, 0.16f, t, fg);
      break;
    case ICON_SNOW:
      snowflake(g, b.px(0.31f), b.py(0.81f), flakeR, flakeT, fg);
      snowflake(g, b.px(0.51f), b.py(0.89f), flakeR, flakeT, fg);
      snowflake(g, b.px(0.71f), b.py(0.81f), flakeR, flakeT, fg);
      break;
    case ICON_SLEET:
      rainLine(g, b, 0.35f, 0.76f, 0.16f, t, fg);
      snowflake(g, b.px(0.53f), b.py(0.87f), flakeR, flakeT, fg);
      rainLine(g, b, 0.75f, 0.76f, 0.16f, t, fg);
      break;
    case ICON_THUNDER:
      bolt(g, b, t, fg, bg);
      break;
    default:
      break;
    }
  }
  break;
  }
}

void drawDropIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color)
{
  const float s = (float)size;
  const float ccx = cx, ccy = cy + 0.18f * s, r = 0.30f * s;
  const float pts[6] = {(float)cx, cy - 0.5f * s,
                        ccx - r * 0.897f, ccy - r * 0.441f,
                        ccx + r * 0.897f, ccy - r * 0.441f};
  fillPolygon(g, pts, 3, color);
  fillCircleF(g, ccx, ccy, r, color);
}

void drawWindIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color)
{
  const float s = (float)size, t = s > 30 ? 3 : 2;
  const float yA = cy - 0.20f * s, xA = cx + 0.22f * s, rA = 0.15f * s;
  thickLine(g, cx - 0.5f * s, yA, xA, yA, t, color);
  thickArc(g, xA, yA - rA, rA, PI_F / 2, -PI_F, t, color);
  const float yB = cy + 0.20f * s, xB = cx + 0.30f * s, rB = 0.13f * s;
  thickLine(g, cx - 0.5f * s, yB, xB, yB, t, color);
  thickArc(g, xB, yB + rB, rB, -PI_F / 2, PI_F, t, color);
  thickLine(g, cx - 0.5f * s, cy, cx - 0.05f * s, cy, t, color);
}

void drawGaugeIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  (void)bg;
  const float s = (float)size, t = s > 30 ? 3 : 2, r = 0.42f * s;
  thickArc(g, cx, cy, r, 0.75f * PI_F, 2.25f * PI_F, t, color);
  const float a = -PI_F / 3;
  thickLine(g, cx, cy, cx + 0.30f * s * cosf(a), cy + 0.30f * s * sinf(a), t, color);
  fillCircleF(g, cx, cy, t, color);
}

void drawSunriseIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size, t = s > 30 ? 3 : 2;
  const float hy = cy + 0.22f * s, r = 0.24f * s;
  fillCircleF(g, cx, hy, r, color);
  fillCircleF(g, cx, hy, r - t, bg);
  g.fillRect(cx - (int)r - 1, (int)hy + 1, 2 * (int)r + 3, (int)r + 2, bg);
  for (int i = 1; i <= 3; i++)
  {
    float a = PI_F + i * PI_F / 4;
    float c = cosf(a), sn = sinf(a);
    thickLine(g, cx + c * r * 1.45f, hy + sn * r * 1.45f, cx + c * r * 2.0f, hy + sn * r * 2.0f, t, color);
  }
  thickLine(g, cx - 0.5f * s, hy + t, cx + 0.5f * s, hy + t, t, color);
}

void drawThermometerIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size;
  const int sw = (int)(s * 0.30f) | 1;            // Breite des Röhrchens
  const int top = (int)(cy - 0.5f * s);
  const int bulbY = (int)(cy + 0.28f * s);
  g.fillRoundRect(cx - sw / 2, top, sw, bulbY - top, sw / 2, color);
  g.fillRoundRect(cx - sw / 2 + 2, top + 2, sw - 4, bulbY - top - 2, (sw - 4) / 2, bg);
  fillCircleF(g, cx, bulbY, s * 0.22f, color);
  g.drawFastVLine(cx, top + 6, bulbY - top - 6, color);
  g.drawFastVLine(cx - 1, top + 8, bulbY - top - 8, color);
}

void drawHomeIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size;
  const float roof[6] = {cx - 0.55f * s, cy - 0.02f * s, cx + 0.55f * s, cy - 0.02f * s, (float)cx, cy - 0.52f * s};
  fillPolygon(g, roof, 3, color);
  const int bx = (int)(cx - 0.36f * s), bw = (int)(0.72f * s);
  const int by = (int)(cy - 0.06f * s), bh = (int)(0.54f * s);
  g.fillRect(bx, by, bw, bh, color);
  const int dw = (int)(0.22f * s), dh = (int)(0.30f * s);
  g.fillRect(cx - dw / 2, by + bh - dh, dw, dh, bg);  // Tür
}

// Mülltonne mit Deckel und Rillen
void drawBinIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size;
  const float top = cy - 0.30f * s, bottom = cy + 0.50f * s;
  const float body[8] = {cx - 0.36f * s, top, cx + 0.36f * s, top, cx + 0.28f * s, bottom, cx - 0.28f * s, bottom};
  fillPolygon(g, body, 4, color);
  const int lidY = (int)(top - 0.16f * s);
  g.fillRect((int)(cx - 0.46f * s), lidY, (int)(0.92f * s) + 1, (int)(0.10f * s) + 2, color);   // Deckel
  g.fillRect((int)(cx - 0.12f * s), (int)(lidY - 0.12f * s), (int)(0.24f * s) + 1, (int)(0.12f * s) + 1, color);  // Griff
  for (int i = -1; i <= 1; i++)  // Rillen
  {
    const int x = (int)(cx + i * 0.15f * s);
    g.drawFastVLine(x, (int)(top + 0.14f * s), (int)(0.52f * s), bg);
  }
}

void drawCheckIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color)
{
  const float s = (float)size, t = s > 20 ? 4 : 3;
  thickLine(g, cx - 0.45f * s, cy, cx - 0.12f * s, cy + 0.32f * s, t, color);
  thickLine(g, cx - 0.12f * s, cy + 0.32f * s, cx + 0.48f * s, cy - 0.36f * s, t, color);
}

// Sonne über einem Solarmodul
void drawSolarIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size;
  const float sx = cx + 0.18f * s, sy = cy - 0.24f * s, r = 0.16f * s;
  fillCircleF(g, sx, sy, r, color);
  for (int i = 0; i < 8; i++)
  {
    const float a = i * PI_F / 4;
    thickLine(g, sx + cosf(a) * r * 1.5f, sy + sinf(a) * r * 1.5f, sx + cosf(a) * r * 2.1f, sy + sinf(a) * r * 2.1f,
              2, color);
  }
  // Modul als Parallelogramm mit Raster
  const float y0 = cy + 0.02f * s, y1 = cy + 0.48f * s;
  const float panel[8] = {cx - 0.36f * s, y0, cx + 0.30f * s, y0, cx + 0.44f * s, y1, cx - 0.50f * s, y1};
  fillPolygon(g, panel, 4, color);
  thickLine(g, cx - 0.43f * s, (y0 + y1) / 2, cx + 0.37f * s, (y0 + y1) / 2, 1, bg);
  thickLine(g, cx - 0.03f * s, y0, cx - 0.03f * s, y1, 1, bg);
}

void drawPinIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg)
{
  const float s = (float)size;
  const float ccy = cy - 0.14f * s, r = 0.32f * s;
  const float pts[6] = {cx - r * 0.86f, ccy + r * 0.5f, cx + r * 0.86f, ccy + r * 0.5f, (float)cx, cy + 0.5f * s};
  fillPolygon(g, pts, 3, color);
  fillCircleF(g, cx, ccy, r, color);
  fillCircleF(g, cx, ccy, r * 0.42f, bg);
}

void drawArrowIcon(Adafruit_GFX& g, int cx, int cy, int size, bool up, uint16_t color)
{
  const float h = size * 0.5f, w = size * 0.5f;
  const float pts[6] = {cx - w, up ? cy + h * 0.6f : cy - h * 0.6f,
                        cx + w, up ? cy + h * 0.6f : cy - h * 0.6f,
                        (float)cx, up ? cy - h * 0.8f : cy + h * 0.8f};
  fillPolygon(g, pts, 3, color);
}

void drawWifiIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color)
{
  const float s = (float)size, t = s * 0.08f;
  for (int i = 1; i <= 3; i++)
    thickArc(g, cx, cy + 0.3f * s, i * 0.22f * s, -0.75f * PI_F, -0.25f * PI_F, t, color);
  fillCircleF(g, cx, cy + 0.3f * s, t * 1.1f, color);
}
