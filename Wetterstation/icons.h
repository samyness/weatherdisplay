#pragma once
// Wetter- und Symbol-Icons, als Vektorgrafik gezeichnet (skalierbar,
// gestochen scharf in jeder Größe, kein Flash-Speicher für Bitmaps nötig).

#include <Adafruit_GFX.h>
#include "weather_data.h"

// Wettericon in ein Quadrat (x, y, size). fg = Linienfarbe, bg = Hintergrund
void drawWeatherIcon(Adafruit_GFX& g, IconKind kind, bool night, int x, int y, int size,
                     uint16_t fg, uint16_t bg);

// Kleine Symbole, zentriert um (cx, cy), Höhe ca. size
void drawDropIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color);
void drawWindIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color);
void drawGaugeIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawSunriseIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawThermometerIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawHomeIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawBinIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawCheckIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color);
void drawSolarIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawPinIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color, uint16_t bg);
void drawArrowIcon(Adafruit_GFX& g, int cx, int cy, int size, bool up, uint16_t color);
void drawWifiIcon(Adafruit_GFX& g, int cx, int cy, int size, uint16_t color);
