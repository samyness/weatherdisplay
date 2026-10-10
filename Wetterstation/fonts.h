#pragma once
// U8g2-Schriften im Flash (PROGMEM), erzeugt von tools/make_fonts.py.
// _tf-Schriften enthalten den vollen Latin-1-Zeichensatz: ä ö ü Ä Ö Ü ß ° ·

#include <stdint.h>
#include <U8g2_for_Adafruit_GFX.h>

struct FlashFont
{
  const uint8_t* data;  // im Flash
  uint16_t       size;
};

extern const FlashFont FONT_LOGISOSO92_TN;  // nur Ziffern, für die große Temperatur
extern const FlashFont FONT_FUB25_TF;
extern const FlashFont FONT_FUB20_TF;
extern const FlashFont FONT_FUB17_TF;
extern const FlashFont FONT_FUB14_TF;
extern const FlashFont FONT_FUB11_TF;
extern const FlashFont FONT_FUR17_TF;
extern const FlashFont FONT_FUR14_TF;
extern const FlashFont FONT_FUR11_TF;

// Lädt die Schrift in den RAM-Puffer (falls nötig) und setzt sie aktiv.
// false = kein Speicher frei; dann darf kein Text gezeichnet/gemessen werden.
bool useFont(U8G2_FOR_ADAFRUIT_GFX& u, const FlashFont& font);
// Gibt den Schriftpuffer nach dem Zeichnen wieder frei
void releaseFonts(U8G2_FOR_ADAFRUIT_GFX& u);
