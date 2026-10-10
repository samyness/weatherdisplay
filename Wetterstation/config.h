#pragma once
// =====================================================================
//  Allgemeine Einstellungen der Wetterstation
//  Persönliches (WLAN, API-Key, Ort) steht in secrets.h
// =====================================================================

// ---- Display ---------------------------------------------------------
// Genau ein Panel aktiv lassen:
#define PANEL_V2_BW     // 7.5" e-Paper V2, 800x480, schwarz/weiß (GDEW075T7) - getestet
//#define PANEL_V2_3C   // 7.5" e-Paper (B) V2, 800x480, schwarz/weiß/rot (GDEW075Z08) - ungetestet

// NodeMCU -> Waveshare e-Paper Driver HAT Rev2.1 (Schalter: Display Config = B, Interface Config = 0)
// DIN -> D7 und CLK -> D5 sind Hardware-SPI und fest vorgegeben.
#define PIN_EPD_CS    D3   // nicht D8! Der HAT zieht CS hoch, auf D8 bootet der ESP8266 nicht
#define PIN_EPD_DC    D2
#define PIN_EPD_RST   D1
#define PIN_EPD_BUSY  D6

#define DISPLAY_ROTATION 0  // 0 = Querformat, 2 = Querformat um 180° gedreht

// ---- Zeit ------------------------------------------------------------
#define TIMEZONE   "CET-1CEST,M3.5.0,M10.5.0/3"  // Österreich: MEZ/MESZ mit Sommerzeit
#define NTP_SERVER "at.pool.ntp.org"

// ---- Aktualisierung --------------------------------------------------
#define UPDATE_INTERVAL_MIN 30  // alle 30 Minuten, ausgerichtet auf :00 und :30
#define RETRY_INTERVAL_MIN  5   // nach einem Fehler

// Nachtpause: zwischen diesen Stunden wird nicht aktualisiert (kein Flackern
// in der Nacht, spart Strom). Gleiche Werte = keine Nachtpause.
#define NIGHT_PAUSE_START 0
#define NIGHT_PAUSE_END   5

// Deep Sleep für Akkubetrieb: der ESP8266 schläft zwischen den Updates.
// Dafür MUSS D0 mit RST verbunden sein, sonst wacht er nie wieder auf!
#define USE_DEEP_SLEEP false

// Demo-Modus: zeigt Beispieldaten ohne WLAN/Internet (zum Testen des Displays)
#define DEMO_MODE false
