#!/usr/bin/env python3
"""Erzeugt Wetterstation/fonts.cpp aus den U8g2-Schriften der Bibliothek
U8g2_for_Adafruit_GFX.

Warum: Die Bibliothek legt Schriften auf dem ESP8266 im RAM ab (PROGMEM gibt
es dort nur für AVR). Neun Schriften kosten so ~38 KB RAM, das passt nicht.
Dieses Skript kopiert die benötigten Schriften als PROGMEM in den Flash;
fonts.cpp lädt die jeweils aktive Schrift in einen kleinen RAM-Puffer.

Aufruf:  python tools/make_fonts.py <Pfad zu u8g2_fonts.c>
Standard: ~/Documents/Arduino/libraries/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c
"""
import os
import re
import sys

# _tf = voller Latin-1-Zeichensatz (ä ö ü ß ° ...), _tn = nur Ziffern
FONTS = [
    "logisoso92_tn",
    "fub25_tf", "fub20_tf", "fub17_tf", "fub14_tf", "fub11_tf",
    "fur17_tf", "fur14_tf", "fur11_tf",
]

here = os.path.dirname(os.path.abspath(__file__))
src = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
    "~/Documents/Arduino/libraries/U8g2_for_Adafruit_GFX/src/u8g2_fonts.c")
dst = os.path.join(here, "..", "Wetterstation", "fonts.cpp")

decl = re.compile(r'^const uint8_t u8g2_font_(\w+)\[(\d+)\]')
found = {}
current = None
with open(src, encoding="latin-1") as f:
    for line in f:
        m = decl.match(line)
        if m:
            current = m.group(1) if m.group(1) in FONTS else None
            if current:
                found[current] = {"size": int(m.group(2)), "lines": []}
            continue
        if current:
            found[current]["lines"].append(line)
            if line.rstrip().endswith('";'):
                current = None

missing = [f for f in FONTS if f not in found]
if missing:
    sys.exit("Nicht gefunden: " + ", ".join(missing))

out = [
    "// Automatisch erzeugt von tools/make_fonts.py - nicht von Hand bearbeiten.\n",
    "// Schriften aus U8g2 (https://github.com/olikraus/u8g2), Lizenzen siehe dortiges Wiki:\n",
    "// FreeUniversal (fub/fur) und Logisoso stehen unter der SIL Open Font License.\n",
    '#include "fonts.h"\n',
    "#include <stdlib.h>\n",
    "#include <string.h>\n",
    "#if defined(ESP8266) || defined(ESP32)\n#include <pgmspace.h>\n#else  // PC-Vorschau\n#define PROGMEM\n#define memcpy_P memcpy\n#endif\n\n",
]
for name in FONTS:
    font = found[name]
    out.append(f"static const uint8_t {name}[{font['size']}] PROGMEM =\n")
    out.extend(font["lines"])
    out.append(f"const FlashFont FONT_{name.upper()} = {{{name}, sizeof({name})}};\n\n")

biggest = max(found[f]["size"] for f in FONTS)
out.append(f"""// Die Bibliothek liest Schriften direkt per Zeiger, also kommt die aktive
// Schrift in einen RAM-Puffer (nur bei einem Wechsel wird kopiert). Der Puffer
// lebt nur während des Zeichnens, damit beim Datenabruf mehr Heap frei ist.
static uint8_t* fontBuffer = nullptr;
static const uint8_t* loadedFont = nullptr;

bool useFont(U8G2_FOR_ADAFRUIT_GFX& u, const FlashFont& font)
{{
  if (!fontBuffer)
  {{
    fontBuffer = (uint8_t*)malloc({biggest});
    loadedFont = nullptr;
    if (!fontBuffer)  // kein Speicher: Text weglassen statt abstürzen
    {{
      u.u8g2.font = nullptr;
      return false;
    }}
  }}
  if (loadedFont != font.data)
  {{
    memcpy_P(fontBuffer, font.data, font.size);
    loadedFont = font.data;
    // setFont() liest die Schriftdaten nur bei einem neuen Zeiger ein; der
    // Puffer bleibt gleich, nur der Inhalt wechselt -> Zeiger zurücksetzen
    u.u8g2.font = nullptr;
  }}
  u.setFont(fontBuffer);
  return true;
}}

void releaseFonts(U8G2_FOR_ADAFRUIT_GFX& u)
{{
  u.u8g2.font = nullptr;  // U8g2 soll nicht auf freigegebenen Speicher zeigen
  free(fontBuffer);
  fontBuffer = nullptr;
  loadedFont = nullptr;
}}
""")

with open(dst, "w", encoding="utf-8", newline="\n") as f:  # Font-Daten sind reines ASCII
    f.writelines(out)
print(f"{dst}: {len(FONTS)} Schriften, {sum(found[f]['size'] for f in FONTS)} Bytes Flash, {biggest} Bytes RAM")
