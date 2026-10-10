#pragma once
// Bildschirmaufbau für 800 x 480 Pixel (Querformat).
// Nutzt nur Adafruit_GFX + U8g2-Schriften, damit derselbe Code auch in der
// PC-Vorschau läuft.

#include <Adafruit_GFX.h>
#include <U8g2_for_Adafruit_GFX.h>
#include "weather_data.h"

// cityName: Anzeigename (UTF-8); leer = Name von OpenWeatherMap
void drawWeatherScreen(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const WeatherData& d,
                       const char* cityName);

// Vollbild-Meldung, z. B. bei fehlendem WLAN oder falschem API-Key
void drawMessageScreen(Adafruit_GFX& g, U8G2_FOR_ADAFRUIT_GFX& u, const char* title,
                       const char* line1, const char* line2);
