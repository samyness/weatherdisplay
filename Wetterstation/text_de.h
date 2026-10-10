#pragma once
// Deutsche (österreichische) Texte und UTF-8-Hilfsfunktionen.
//
// Alle Texte im Projekt sind UTF-8. Die U8g2-Schriften (_tf) enthalten den
// kompletten Latin-1-Zeichensatz, also ä ö ü Ä Ö Ü ß ° · usw.
// utf8Sanitize() sorgt dafür, dass wirklich nur darstellbare Zeichen beim
// Display ankommen (siehe text_de.cpp).

#include <stddef.h>
#include <stdint.h>

const char* weekdayLong(int wday);   // 0 = Sonntag
const char* weekdayShort(int wday);  // 0 = So
const char* monthLong(int mon);      // 0 = Jänner

// Windrichtung in Grad -> "N", "NO", "O", "SO", "S", "SW", "W", "NW"
const char* windDirection(int deg);

// Kopiert 'in' nach 'out' und macht daraus gültiges UTF-8 im Latin-1-Bereich:
//  - gültiges UTF-8 bis U+00FF bleibt unverändert (ä, ö, ü, ß, °, ...)
//  - häufige Zeichen außerhalb (–, „, ", …, ő, č, š, ž, ...) werden ersetzt
//  - einzelne Latin-1/Windows-1252-Bytes (Datei nicht als UTF-8 gespeichert)
//    werden erkannt und korrekt umgewandelt
void utf8Sanitize(const char* in, char* out, size_t outSize);

// Erster Buchstabe groß, auch bei Umlauten ("überwiegend" -> "Überwiegend")
void utf8CapitalizeFirst(char* s);

// Prozent-Kodierung für URLs ("Mödling" -> "M%C3%B6dling")
void urlEncode(const char* in, char* out, size_t outSize);

// Sicheres Kopieren mit Nullterminierung
void copyStr(char* dst, const char* src, size_t dstSize);
