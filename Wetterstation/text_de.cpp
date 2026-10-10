#include "text_de.h"
#include <string.h>

// Umlaute sind hier absichtlich als \x-Escapes geschrieben: so bleibt diese
// Datei korrekt, egal mit welcher Kodierung ein Editor sie speichert.
// Achtung: auf ein \xNN darf kein Hex-Zeichen folgen, daher "\xC3\xA4" "nner".

static const char* const WEEKDAYS_LONG[7] = {
  "Sonntag", "Montag", "Dienstag", "Mittwoch", "Donnerstag", "Freitag", "Samstag"
};
static const char* const WEEKDAYS_SHORT[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
static const char* const MONTHS_LONG[12] = {
  "J\xC3\xA4" "nner",  // Jänner, wie es sich in Österreich gehört
  "Februar",
  "M\xC3\xA4" "rz",    // März
  "April", "Mai", "Juni", "Juli", "August", "September", "Oktober", "November", "Dezember"
};
static const char* const WIND_DIRS[8] = {"N", "NO", "O", "SO", "S", "SW", "W", "NW"};

const char* weekdayLong(int wday) { return (wday >= 0 && wday < 7) ? WEEKDAYS_LONG[wday] : ""; }
const char* weekdayShort(int wday) { return (wday >= 0 && wday < 7) ? WEEKDAYS_SHORT[wday] : ""; }
const char* monthLong(int mon) { return (mon >= 0 && mon < 12) ? MONTHS_LONG[mon] : ""; }

const char* windDirection(int deg)
{
  if (deg < 0) return "";
  return WIND_DIRS[((deg % 360) * 2 + 45) / 90 % 8];
}

void copyStr(char* dst, const char* src, size_t dstSize)
{
  if (!dstSize) return;
  size_t n = src ? strlen(src) : 0;
  if (n >= dstSize) n = dstSize - 1;
  if (n) memcpy(dst, src, n);
  dst[n] = '\0';
}

// ---- UTF-8 -------------------------------------------------------------

// Grundbuchstaben für U+0100..U+017F (Latin Extended-A: č, š, ž, ł, ...)
static const char LATIN_EXT_A[129] =
  "AaAaAaCcCcCcCcDd" "DdEeEeEeEeEeGgGg" "GgGgHhHhIiIiIiIi" "IiJjJjKkkLlLlLlL"
  "lLlNnNnNnnNnOoOo" "OoOoRrRrRrSsSsSs" "SsTtTtTtUuUuUuUu" "UuUuWwYyYZzZzZzs";

// Hängt ein Zeichen (<= U+00FF) als UTF-8 an. false, wenn kein Platz mehr ist.
static bool putLatin1(unsigned cp, char* out, size_t& pos, size_t size)
{
  if (cp < 0x80)
  {
    if (pos + 1 >= size) return false;
    out[pos++] = (char)cp;
  }
  else
  {
    if (pos + 2 >= size) return false;
    out[pos++] = (char)(0xC0 | (cp >> 6));
    out[pos++] = (char)(0x80 | (cp & 0x3F));
  }
  return true;
}

static bool putAscii(const char* s, char* out, size_t& pos, size_t size)
{
  for (; *s; s++)
    if (!putLatin1((unsigned char)*s, out, pos, size)) return false;
  return true;
}

// Zeichen außerhalb von Latin-1 auf etwas Darstellbares abbilden
static bool putReplacement(unsigned long cp, char* out, size_t& pos, size_t size)
{
  switch (cp)
  {
  case 0x0150: return putLatin1(0xD6, out, pos, size);  // Ő -> Ö
  case 0x0151: return putLatin1(0xF6, out, pos, size);  // ő -> ö
  case 0x0170: return putLatin1(0xDC, out, pos, size);  // Ű -> Ü
  case 0x0171: return putLatin1(0xFC, out, pos, size);  // ű -> ü
  case 0x2018: case 0x2019: case 0x201A: case 0x201B:
    return putLatin1('\'', out, pos, size);
  case 0x201C: case 0x201D: case 0x201E: case 0x201F:
    return putLatin1('"', out, pos, size);
  case 0x2022: case 0x2219: case 0x22C5:
    return putLatin1(0xB7, out, pos, size);             // Aufzählungspunkt -> ·
  case 0x2026: return putAscii("...", out, pos, size);
  case 0x20AC: return putAscii("EUR", out, pos, size);
  case 0x2103: return putLatin1(0xB0, out, pos, size) && putLatin1('C', out, pos, size);
  case 0x2212: return putLatin1('-', out, pos, size);
  }
  if (cp >= 0x0100 && cp <= 0x017F) return putLatin1((unsigned char)LATIN_EXT_A[cp - 0x100], out, pos, size);
  if (cp >= 0x2010 && cp <= 0x2015) return putLatin1('-', out, pos, size);  // Binde-/Gedankenstriche
  if ((cp >= 0x2000 && cp <= 0x200A) || cp == 0x202F) return putLatin1(' ', out, pos, size);
  if (cp >= 0x200B && cp <= 0x200F) return true;                             // unsichtbare Zeichen
  return putLatin1('?', out, pos, size);
}

// Windows-1252-Zeichen 0x80..0x9F, falls eine Datei "ANSI" gespeichert wurde
static bool putCp1252(unsigned char b, char* out, size_t& pos, size_t size)
{
  switch (b)
  {
  case 0x80: return putAscii("EUR", out, pos, size);
  case 0x84: case 0x93: case 0x94: return putLatin1('"', out, pos, size);
  case 0x85: return putAscii("...", out, pos, size);
  case 0x8A: return putLatin1('S', out, pos, size);
  case 0x8E: return putLatin1('Z', out, pos, size);
  case 0x91: case 0x92: return putLatin1('\'', out, pos, size);
  case 0x95: return putLatin1(0xB7, out, pos, size);
  case 0x96: case 0x97: return putLatin1('-', out, pos, size);
  case 0x9A: return putLatin1('s', out, pos, size);
  case 0x9E: return putLatin1('z', out, pos, size);
  default:   return putLatin1('?', out, pos, size);
  }
}

static inline bool isCont(unsigned char c) { return (c & 0xC0) == 0x80; }

static const char BASE_VOWELS[] = "aouAOU";

void utf8Sanitize(const char* in, char* out, size_t outSize)
{
  if (!outSize) return;
  size_t pos = 0;
  const unsigned char* p = (const unsigned char*)(in ? in : "");
  bool ok = true;
  while (*p && ok)
  {
    unsigned char b = p[0];
    if (b < 0x80)
    {
      ok = (b >= 0x20 || b == '\n') ? putLatin1(b, out, pos, outSize) : true;  // Steuerzeichen weg
      p += 1;
    }
    else if (b >= 0xC2 && b <= 0xDF && isCont(p[1]))
    {
      unsigned cp = ((b & 0x1F) << 6) | (p[1] & 0x3F);
      if (cp == 0xA0)
        ok = putLatin1(' ', out, pos, outSize);   // geschütztes Leerzeichen
      else if (cp <= 0xFF)
        ok = putLatin1(cp, out, pos, outSize);    // ä ö ü ß ° ...
      else if (cp == 0x0308 && pos > 0 && out[pos - 1] && strchr(BASE_VOWELS, out[pos - 1]))
      {
        // "a" + kombinierendes Trema (z. B. von macOS) -> echtes "ä"
        static const unsigned char UML[] = {0xE4, 0xF6, 0xFC, 0xC4, 0xD6, 0xDC};
        const unsigned idx = (unsigned)(strchr(BASE_VOWELS, out[pos - 1]) - BASE_VOWELS);
        pos--;
        ok = putLatin1(UML[idx], out, pos, outSize);
      }
      else if (cp >= 0x0300 && cp <= 0x036F)
        ok = true;                                // andere kombinierende Akzente weglassen
      else
        ok = putReplacement(cp, out, pos, outSize);  // č, ő, Ł, ...
      p += 2;
    }
    else if (b >= 0xE0 && b <= 0xEF && isCont(p[1]) && isCont(p[2]))
    {
      unsigned long cp = ((unsigned long)(b & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
      ok = putReplacement(cp, out, pos, outSize);
      p += 3;
    }
    else if (b >= 0xF0 && b <= 0xF4 && isCont(p[1]) && isCont(p[2]) && isCont(p[3]))
    {
      ok = putLatin1('?', out, pos, outSize);  // Emoji o. Ä.
      p += 4;
    }
    else
    {
      // kein gültiges UTF-8 -> als Latin-1 / Windows-1252 interpretieren
      ok = (b >= 0xA0) ? putLatin1(b, out, pos, outSize) : putCp1252(b, out, pos, outSize);
      p += 1;
    }
  }
  out[pos] = '\0';
}

void utf8CapitalizeFirst(char* s)
{
  unsigned char* p = (unsigned char*)s;
  if (p[0] >= 'a' && p[0] <= 'z')
    p[0] = (unsigned char)(p[0] - 'a' + 'A');
  else if (p[0] == 0xC3 && p[1] >= 0xA0 && p[1] <= 0xBE && p[1] != 0xB7)
    p[1] = (unsigned char)(p[1] - 0x20);  // à..þ -> À..Þ, z. B. ü -> Ü
}

void urlEncode(const char* in, char* out, size_t outSize)
{
  static const char HEX[] = "0123456789ABCDEF";
  size_t pos = 0;
  for (const unsigned char* p = (const unsigned char*)in; *p; p++)
  {
    unsigned char c = *p;
    bool plain = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c == '-' || c == '_' || c == '.' || c == '~';
    if (plain)
    {
      if (pos + 1 >= outSize) break;
      out[pos++] = (char)c;
    }
    else
    {
      if (pos + 3 >= outSize) break;
      out[pos++] = '%';
      out[pos++] = HEX[c >> 4];
      out[pos++] = HEX[c & 0x0F];
    }
  }
  if (outSize) out[pos < outSize ? pos : outSize - 1] = '\0';
}
