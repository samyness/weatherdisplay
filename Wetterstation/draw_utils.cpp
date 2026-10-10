#include "draw_utils.h"
#include <math.h>

static inline int16_t rnd(float v) { return (int16_t)floorf(v + 0.5f); }

void fillCircleF(Adafruit_GFX& g, float cx, float cy, float r, uint16_t color)
{
  if (r < 0.5f)
  {
    g.drawPixel(rnd(cx), rnd(cy), color);
    return;
  }
  g.fillCircle(rnd(cx), rnd(cy), rnd(r), color);
}

void fillPolygon(Adafruit_GFX& g, const float* pts, int count, uint16_t color)
{
  if (count < 3) return;
  float minY = pts[1], maxY = pts[1];
  for (int i = 1; i < count; i++)
  {
    if (pts[2 * i + 1] < minY) minY = pts[2 * i + 1];
    if (pts[2 * i + 1] > maxY) maxY = pts[2 * i + 1];
  }
  float xs[16];
  for (int y = (int)ceilf(minY - 0.5f); y <= (int)floorf(maxY - 0.5f); y++)
  {
    const float yc = y + 0.5f;  // Pixelmitte
    int n = 0;
    for (int i = 0; i < count && n < 16; i++)
    {
      float x0 = pts[2 * i], y0 = pts[2 * i + 1];
      float x1 = pts[2 * ((i + 1) % count)], y1 = pts[2 * ((i + 1) % count) + 1];
      if ((y0 <= yc && y1 > yc) || (y1 <= yc && y0 > yc))
        xs[n++] = x0 + (yc - y0) * (x1 - x0) / (y1 - y0);
    }
    for (int i = 1; i < n; i++)  // Insertion-Sort, n ist klein
      for (int j = i; j > 0 && xs[j - 1] > xs[j]; j--)
      {
        float t = xs[j];
        xs[j] = xs[j - 1];
        xs[j - 1] = t;
      }
    for (int i = 0; i + 1 < n; i += 2)
    {
      int xa = (int)ceilf(xs[i] - 0.5f);
      int xb = (int)floorf(xs[i + 1] - 0.5f);
      if (xb >= xa) g.drawFastHLine(xa, y, xb - xa + 1, color);
    }
  }
}

void thickLine(Adafruit_GFX& g, float x0, float y0, float x1, float y1, float width, uint16_t color)
{
  if (width < 1.5f)
  {
    g.drawLine(rnd(x0), rnd(y0), rnd(x1), rnd(y1), color);
    return;
  }
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  float r = width / 2;
  if (len > 0.01f)
  {
    float nx = -dy / len * r, ny = dx / len * r;
    const float quad[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    fillPolygon(g, quad, 4, color);
  }
  // runde Enden: Kreis mit Durchmesser = Linienbreite
  float rc = r - 0.5f;
  fillCircleF(g, x0, y0, rc, color);
  fillCircleF(g, x1, y1, rc, color);
}

void thickArc(Adafruit_GFX& g, float cx, float cy, float r, float a0, float a1, float width, uint16_t color)
{
  int steps = (int)(fabsf(a1 - a0) * r / 4) + 2;
  if (steps > 48) steps = 48;
  float px = cx + r * cosf(a0), py = cy + r * sinf(a0);
  for (int i = 1; i <= steps; i++)
  {
    float a = a0 + (a1 - a0) * i / steps;
    float x = cx + r * cosf(a), y = cy + r * sinf(a);
    thickLine(g, px, py, x, y, width, color);
    px = x;
    py = y;
  }
}

void ditherRect(Adafruit_GFX& g, int x, int y, int w, int h, int level, uint16_t color)
{
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++)
    {
      bool on;
      switch (level)
      {
      case 1:  on = (xx % 2 == 0) && (yy % 2 == 0); break;
      case 3:  on = !((xx % 2 == 0) && (yy % 2 == 0)); break;
      default: on = ((xx + yy) % 2) == 0; break;
      }
      if (on) g.drawPixel(xx, yy, color);
    }
}
