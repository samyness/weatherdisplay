#pragma once
// Zeichen-Grundfunktionen, die Adafruit_GFX nicht mitbringt
// (dicke Linien mit runden Enden, Bögen, Polygone, Rasterflächen).

#include <Adafruit_GFX.h>

#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#define GxEPD_WHITE 0xFFFF
#endif

constexpr float PI_F = 3.14159265f;

void thickLine(Adafruit_GFX& g, float x0, float y0, float x1, float y1, float width, uint16_t color);
// Bogen um (cx, cy); Winkel im Bogenmaß, 0 = rechts, positiv = im Uhrzeigersinn (y nach unten)
void thickArc(Adafruit_GFX& g, float cx, float cy, float r, float a0, float a1, float width, uint16_t color);
void fillCircleF(Adafruit_GFX& g, float cx, float cy, float r, uint16_t color);
// Gefülltes Polygon, pts = x0, y0, x1, y1, ...
void fillPolygon(Adafruit_GFX& g, const float* pts, int count, uint16_t color);
// Grauwert per Raster: level 1 = 25 %, 2 = 50 %, 3 = 75 % Deckung
void ditherRect(Adafruit_GFX& g, int x, int y, int w, int h, int level, uint16_t color);
