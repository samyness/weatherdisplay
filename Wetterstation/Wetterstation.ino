// =====================================================================
//  Wetterstation: NodeMCU ESP8266 + Waveshare 7.5" e-Paper V2 (800x480)
//
//  Wetter von OpenWeatherMap, optional Werte aus Home Assistant,
//  modernes Layout mit vollen Umlauten (ä ö ü ß °).
//
//  Einstellungen: config.h (allgemein) und secrets.h (WLAN, API-Key, Ort).
//  Bibliotheken: GxEPD2, Adafruit GFX, U8g2_for_Adafruit_GFX, ArduinoJson 7
//  Board: "NodeMCU 1.0 (ESP-12E Module)", serieller Monitor 115200 Baud
//
//  Befehle im seriellen Monitor: u = jetzt aktualisieren, d = Demo-Bild,
//  h = nur Home Assistant testen
// =====================================================================

#include "settings.h"

#include <ESP8266WiFi.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_BW.h>
#include <SPI.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <sys/time.h>
#include <time.h>

#include "demo_data.h"
#include "fonts.h"
#include "forecast.h"
#include "ha_client.h"
#include "layout.h"
#include "owm_client.h"
#include "text_de.h"

// Seitenweises Zeichnen, damit der Bildpuffer in den RAM des ESP8266 passt
#define MAX_DISPLAY_BUFFER_SIZE 15000ul
#if defined(PANEL_V2_3C)
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8) ? EPD::HEIGHT : (MAX_DISPLAY_BUFFER_SIZE / 2) / (EPD::WIDTH / 8))
GxEPD2_3C<GxEPD2_750c_Z08, MAX_HEIGHT(GxEPD2_750c_Z08)> display(GxEPD2_750c_Z08(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#else
#define MAX_HEIGHT(EPD) (EPD::HEIGHT <= MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8) ? EPD::HEIGHT : MAX_DISPLAY_BUFFER_SIZE / (EPD::WIDTH / 8))
GxEPD2_BW<GxEPD2_750_T7, MAX_HEIGHT(GxEPD2_750_T7)> display(GxEPD2_750_T7(PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY));
#endif

U8G2_FOR_ADAFRUIT_GFX u8g2;
WeatherData weather;  // global statt auf dem Stack (ca. 1,5 KB)

// Zustand, der im Deep Sleep im RTC-Speicher überlebt
struct RunState
{
  uint32_t magic;
  uint16_t fails;         // Fehlversuche in Folge
  uint8_t  everOk;        // schon einmal erfolgreich aktualisiert?
  uint8_t  errorShown;    // Fehlermeldung bereits am Display?
  uint8_t  haAuthFailed;  // Home-Assistant-Token ungültig -> nicht mehr fragen
  uint8_t  reserved[3];
};
constexpr uint32_t STATE_MAGIC = 0x57455432;  // "WET2"
RunState state;

uint32_t nextUpdateMs = 0;
constexpr time_t VALID_TIME = 1700000000;  // alles davor = Uhr nicht gestellt

// ---------------------------------------------------------------------

void loadState()
{
  if (USE_DEEP_SLEEP && ESP.getResetInfoPtr()->reason == REASON_DEEP_SLEEP_AWAKE &&
      ESP.rtcUserMemoryRead(0, (uint32_t*)&state, sizeof(state)) && state.magic == STATE_MAGIC)
    return;
  memset(&state, 0, sizeof(state));
  state.magic = STATE_MAGIC;
}

void saveState()
{
  if (USE_DEEP_SLEEP) ESP.rtcUserMemoryWrite(0, (uint32_t*)&state, sizeof(state));
}

void initDisplay()
{
  Serial.flush();                         // init() startet Serial neu
  display.init(115200, true, 2, false);   // 2 ms Reset-Puls für Waveshare-HATs
  pinMode(PIN_EPD_BUSY, INPUT);           // D6 ist auch MISO: SPI.begin() hat den Pin übernommen
  display.setRotation(DISPLAY_ROTATION);
  u8g2.begin(display);
}

template <typename Draw>
void render(Draw draw)
{
  const uint32_t t0 = millis();
  display.setFullWindow();
  display.firstPage();
  do
  {
    draw();
  } while (display.nextPage());
  display.hibernate();
  releaseFonts(u8g2);
  Serial.printf("Display aktualisiert in %lu ms, freier Heap %u Bytes\n", (unsigned long)(millis() - t0),
                ESP.getFreeHeap());
}

// ---- WLAN und Zeit ---------------------------------------------------

bool connectWifi(char* detail, size_t detailSize)
{
  WiFi.forceSleepWake();
  delay(1);
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.hostname("Wetterstation");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Verbinde mit WLAN \"%s\" ", WIFI_SSID);
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - t0 > 20000)
    {
      const wl_status_t st = WiFi.status();
      Serial.printf(" fehlgeschlagen (Status %d)\n", (int)st);
      if (st == WL_NO_SSID_AVAIL)
        snprintf(detail, detailSize, "WLAN \"%s\" nicht gefunden", WIFI_SSID);
      else if (st == WL_WRONG_PASSWORD)
        snprintf(detail, detailSize, "Passwort f\xC3\xBCr \"%s\" falsch", WIFI_SSID);
      else
        snprintf(detail, detailSize, "Verbindung zu \"%s\" fehlgeschlagen", WIFI_SSID);
      return false;
    }
    delay(250);
    Serial.print('.');
  }
  Serial.printf(" ok, IP %s, RSSI %d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  return true;
}

void wifiOff()
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  WiFi.forceSleepBegin();
  delay(1);
}

bool syncTime()
{
  configTime(TIMEZONE, NTP_SERVER, "pool.ntp.org");
  for (uint32_t t0 = millis(); millis() - t0 < 8000; delay(100))
    if (time(nullptr) > VALID_TIME) return true;
  Serial.println("NTP: keine Antwort, verwende Zeit von OpenWeatherMap");
  return false;
}

// ---- Zeitplan --------------------------------------------------------

// Sekunden bis zum Ende der Nachtpause, 0 = gerade keine Nachtpause
uint32_t nightPauseRemaining(time_t now)
{
  if (NIGHT_PAUSE_START == NIGHT_PAUSE_END || now < VALID_TIME) return 0;
  const struct tm lt = toLocal(now);
  const int h = lt.tm_hour;
  const bool inPause = NIGHT_PAUSE_START < NIGHT_PAUSE_END
                         ? (h >= NIGHT_PAUSE_START && h < NIGHT_PAUSE_END)
                         : (h >= NIGHT_PAUSE_START || h < NIGHT_PAUSE_END);
  if (!inPause) return 0;
  // Ende der Pause als lokale Uhrzeit; mktime() berücksichtigt die Sommerzeit-Umstellung
  struct tm end = lt;
  if (h >= NIGHT_PAUSE_END) end.tm_mday += 1;
  end.tm_hour = NIGHT_PAUSE_END;
  end.tm_min = 0;
  end.tm_sec = 0;
  end.tm_isdst = -1;
  const time_t endTime = mktime(&end);
  return endTime > now ? (uint32_t)(endTime - now) : 0;
}

// Nächstes Update kurz nach der nächsten vollen halben Stunde (bzw. Intervall)
uint32_t secondsToNextUpdate(time_t now)
{
  const uint32_t interval = UPDATE_INTERVAL_MIN * 60UL;
  if (now < VALID_TIME) return interval;
  uint32_t wait = interval - (uint32_t)(now % interval) + 60;
  if (wait < 120) wait += interval;
  return wait;
}

void scheduleNext(uint32_t seconds)
{
  Serial.printf("N\xC3\xA4" "chstes Update in %lu min\n", (unsigned long)(seconds / 60));
  if (USE_DEEP_SLEEP)
  {
    saveState();
    uint64_t us = (uint64_t)seconds * 1000000ULL;
    const uint64_t maxUs = ESP.deepSleepMax() / 100 * 95;
    if (us > maxUs) us = maxUs;  // längere Pausen werden in mehreren Etappen geschlafen
    Serial.flush();
    ESP.deepSleep(us);
  }
  nextUpdateMs = millis() + seconds * 1000UL;
}

// ---- Ablauf ----------------------------------------------------------

void showError(const char* title, const char* detail)
{
  state.fails++;
  Serial.printf("FEHLER: %s - %s\n", title, detail);
  // Alte Wetterdaten bleiben stehen, solange es nur kurz hakt. Die Meldung
  // kommt beim ersten Start oder wenn es länger als ca. 30 Minuten klemmt.
  if ((!state.everOk || state.fails >= 6) && !state.errorShown)
  {
    char retry[48];
    snprintf(retry, sizeof(retry), "N\xC3\xA4" "chster Versuch in %d Minuten", RETRY_INTERVAL_MIN);
    render([&] { drawMessageScreen(display, u8g2, title, detail, retry); });
    state.errorShown = 1;
  }
  scheduleNext(RETRY_INTERVAL_MIN * 60UL);
}

bool secretsArePlaceholders()
{
  return !strcmp(WIFI_SSID, "MeinWLAN") || !strcmp(OWM_API_KEY, "0123456789abcdef0123456789abcdef");
}

void showDemo()
{
  setenv("TZ", TIMEZONE, 1);
  tzset();
  time_t now = time(nullptr);
  if (now < VALID_TIME) now = 1791635520;  // 10. Oktober 2026, 14:32
  makeDemoData(weather, now);
  render([&] { drawWeatherScreen(display, u8g2, weather, CITY_NAME); });
}

void runUpdate()
{
  Serial.println("\n--- Aktualisierung ---");
  if (DEMO_MODE)
  {
    showDemo();
    scheduleNext(UPDATE_INTERVAL_MIN * 60UL);
    return;
  }
  if (secretsArePlaceholders())
  {
    showError("Einrichtung n\xC3\xB6tig", "Bitte WLAN und API-Key in secrets.h eintragen");
    return;
  }

  // im Dauerbetrieb läuft die Uhr weiter: Nachtpause ohne WLAN erkennen
  if (uint32_t pause = nightPauseRemaining(time(nullptr)))
  {
    scheduleNext(pause);
    return;
  }

  char detail[96];
  if (!connectWifi(detail, sizeof(detail)))
  {
    wifiOff();
    showError("Keine WLAN-Verbindung", detail);
    return;
  }
  time_t now = syncTime() ? time(nullptr) : 0;
  if (uint32_t pause = nightPauseRemaining(now))  // nach Deep Sleep erst jetzt bekannt
  {
    wifiOff();
    scheduleNext(pause);
    return;
  }

  FetchError err;
  if (!fetchWeather(weather, now, err))
  {
    wifiOff();
    showError(err.title, err.detail);
    return;
  }
  bool haAuthFailed = state.haAuthFailed;
  fetchHomeDisplay(weather, haAuthFailed);
  fetchHomeAssistant(weather, haAuthFailed);
  state.haAuthFailed = haAuthFailed;
  wifiOff();

  if (!now && weather.now.dt > VALID_TIME)
  {
    // NTP hat nicht geantwortet: Uhr mit dem Messzeitpunkt von OpenWeatherMap stellen
    now = weather.now.dt;
    const timeval tv = {now, 0};
    settimeofday(&tv, nullptr);
    if (uint32_t pause = nightPauseRemaining(now))
    {
      scheduleNext(pause);
      return;
    }
  }
  weather.updated = now;
  buildDailyForecast(weather, now);
  render([&] { drawWeatherScreen(display, u8g2, weather, CITY_NAME); });

  state.fails = 0;
  state.everOk = 1;
  state.errorShown = 0;
  scheduleNext(secondsToNextUpdate(now));
}

// Nur Home Assistant abfragen und das Ergebnis seriell ausgeben (Befehl "h"),
// z. B. um die Firewall zwischen IoT-Netz und Home Assistant zu prüfen
void testHomeAssistant()
{
  Serial.println("\n--- Test Home Assistant ---");
  if (!homeAssistantConfigured())
  {
    Serial.println("Home Assistant ist in secrets.h nicht konfiguriert");
    return;
  }
  char detail[96];
  if (!connectWifi(detail, sizeof(detail)))
  {
    Serial.println(detail);
    wifiOff();
    return;
  }
  bool haAuthFailed = false;
  fetchHomeDisplay(weather, haAuthFailed);
  fetchHomeAssistant(weather, haAuthFailed);
  wifiOff();
  Serial.println(weather.homeDisplay.valid || weather.homeCount ? "Home Assistant erreichbar" : "Home Assistant: keine Daten");
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.println("\n\n=== Wetterstation ===");
  loadState();
  initDisplay();
  runUpdate();  // mit Deep Sleep kehrt das nicht zurück
}

void loop()
{
  if (Serial.available())
  {
    const char c = Serial.read();
    if (c == 'u') nextUpdateMs = millis();
    if (c == 'h') testHomeAssistant();
    if (c == 'd')
    {
      showDemo();
      nextUpdateMs = millis() + UPDATE_INTERVAL_MIN * 60000UL;
    }
  }
  if ((int32_t)(millis() - nextUpdateMs) >= 0) runUpdate();
  delay(50);
}
