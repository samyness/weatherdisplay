#include "owm_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "settings.h"
#include "text_de.h"

namespace {

// HTTP statt HTTPS: spart auf dem ESP8266 ca. 20 KB RAM und Zeit (Akku)
const char OWM_BASE[] = "http://api.openweathermap.org/data/2.5/";

String buildUrl(const char* endpoint, const char* extra)
{
  String url = OWM_BASE;
  url += endpoint;
#if defined(LOCATION_LAT) && defined(LOCATION_LON)
  url += "?lat=";
  url += String((double)LOCATION_LAT, 4);
  url += "&lon=";
  url += String((double)LOCATION_LON, 4);
#else
  char q[128];
  urlEncode(LOCATION_QUERY, q, sizeof(q));  // "Mödling,AT" -> "M%C3%B6dling%2CAT"
  url += "?q=";
  url += q;
#endif
  url += "&units=metric&lang=de&appid=";
  url += OWM_API_KEY;
  url += extra;
  return url;
}

void setError(FetchError& err, int code, const char* title, const char* detail)
{
  err.httpCode = code;
  copyStr(err.title, title, sizeof(err.title));
  copyStr(err.detail, detail, sizeof(err.detail));
}

// Startet die Anfrage. Bei Erfolg liefert http.getStream() die JSON-Antwort,
// die direkt aus dem Datenstrom gelesen wird (kein großer Puffer nötig).
bool startRequest(HTTPClient& http, WiFiClient& client, const char* endpoint, const char* extra, FetchError& err)
{
  http.useHTTP10(true);  // kein "chunked" Encoding, damit der Stream direkt lesbar ist
  http.setTimeout(15000);

  String url = buildUrl(endpoint, extra);
  Serial.printf("GET %s%s?... ", OWM_BASE, endpoint);
  if (!http.begin(client, url))
  {
    setError(err, -1, "Interner Fehler", "HTTP-Verbindung konnte nicht vorbereitet werden");
    return false;
  }
  const int code = http.GET();
  Serial.printf("-> %d\n", code);
  if (code != HTTP_CODE_OK)
  {
    char detail[96];
    if (code == HTTP_CODE_UNAUTHORIZED)
      setError(err, code, "API-Key ung\xC3\xBCltig",
               "Key in secrets.h pr\xC3\xBC" "fen. Neue Keys sind erst nach bis zu 2 h aktiv.");
    else if (code == HTTP_CODE_NOT_FOUND)
    {
#if defined(LOCATION_QUERY) && !(defined(LOCATION_LAT) && defined(LOCATION_LON))
      snprintf(detail, sizeof(detail), "\"%s\" ist OpenWeatherMap nicht bekannt.", LOCATION_QUERY);
#else
      snprintf(detail, sizeof(detail), "Koordinaten in secrets.h pr\xC3\xBC" "fen.");
#endif
      setError(err, code, "Ort nicht gefunden", detail);
    }
    else if (code < 0)
    {
      snprintf(detail, sizeof(detail), "Server nicht erreichbar (%s)", http.errorToString(code).c_str());
      setError(err, code, "Keine Wetterdaten", detail);
    }
    else
    {
      snprintf(detail, sizeof(detail), "OpenWeatherMap antwortet mit HTTP %d", code);
      setError(err, code, "Keine Wetterdaten", detail);
    }
    http.end();
    return false;
  }
  return true;
}

void setJsonError(FetchError& err, DeserializationError jsonErr)
{
  char detail[96];
  snprintf(detail, sizeof(detail), "Antwort nicht lesbar (%s)", jsonErr.c_str());
  setError(err, 0, "Keine Wetterdaten", detail);
  Serial.println(detail);
}

bool fetchCurrent(CurrentWeather& c, FetchError& err)
{
  JsonDocument filter;
  filter["dt"] = true;
  filter["name"] = true;
  filter["weather"][0]["id"] = true;
  filter["weather"][0]["description"] = true;
  filter["weather"][0]["icon"] = true;
  filter["main"]["temp"] = true;
  filter["main"]["feels_like"] = true;
  filter["main"]["humidity"] = true;
  filter["main"]["pressure"] = true;
  filter["wind"]["speed"] = true;
  filter["wind"]["deg"] = true;
  filter["sys"]["sunrise"] = true;
  filter["sys"]["sunset"] = true;

  WiFiClient client;
  HTTPClient http;
  if (!startRequest(http, client, "weather", "", err)) return false;
  JsonDocument doc;
  DeserializationError jsonErr = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (jsonErr)
  {
    setJsonError(err, jsonErr);
    return false;
  }
  if (!doc["main"]["temp"].is<float>())
  {
    setError(err, 0, "Keine Wetterdaten", "Antwort ohne Temperatur");
    return false;
  }

  c.temp = doc["main"]["temp"].as<float>();
  c.feelsLike = doc["main"]["feels_like"] | c.temp;
  c.humidity = doc["main"]["humidity"] | -1;
  c.pressure = doc["main"]["pressure"] | -1;
  c.windSpeed = doc["wind"]["speed"] | -1.0f;
  c.windDeg = doc["wind"]["deg"] | -1;
  c.weatherId = doc["weather"][0]["id"] | 800;
  const char* icon = doc["weather"][0]["icon"] | "01d";
  c.night = strlen(icon) >= 3 && icon[2] == 'n';
  c.dt = (time_t)(doc["dt"] | 0UL);
  c.sunrise = (time_t)(doc["sys"]["sunrise"] | 0UL);
  c.sunset = (time_t)(doc["sys"]["sunset"] | 0UL);
  // Texte kommen als UTF-8 ("überwiegend bewölkt") und werden fürs Display bereinigt
  utf8Sanitize(doc["weather"][0]["description"] | "", c.description, sizeof(c.description));
  utf8CapitalizeFirst(c.description);
  utf8Sanitize(doc["name"] | "", c.city, sizeof(c.city));

  Serial.printf("Aktuell: %.1f Grad, %s (%d)\n", c.temp, c.description, c.weatherId);
  return true;
}

// Die Vorhersage (40 Einträge, ~16 KB) wird Eintrag für Eintrag gelesen,
// damit nie die ganze Antwort gleichzeitig im RAM liegt.
bool fetchForecast(WeatherData& data, time_t now, FetchError& err)
{
  JsonDocument filter;
  filter["dt"] = true;
  filter["main"]["temp"] = true;
  filter["weather"][0]["id"] = true;
  filter["pop"] = true;
  filter["rain"]["3h"] = true;
  filter["snow"]["3h"] = true;
  filter["sys"]["pod"] = true;

  WiFiClient client;
  HTTPClient http;
  if (!startRequest(http, client, "forecast", "&cnt=40", err)) return false;
  Stream& stream = http.getStream();

  data.slotCount = 0;
  if (stream.find("\"list\":["))
  {
    JsonDocument it;
    do
    {
      DeserializationError jsonErr = deserializeJson(it, stream, DeserializationOption::Filter(filter));
      if (jsonErr)
      {
        http.end();
        setJsonError(err, jsonErr);
        return false;
      }
      if (!it["main"]["temp"].is<float>()) continue;  // unvollständiger Eintrag
      const time_t dt = (time_t)(it["dt"] | 0UL);
      if (now && dt + 90 * 60 < now) continue;        // Zeitfenster schon großteils vorbei
      ForecastSlot& s = data.slots[data.slotCount++];
      s.dt = dt;
      s.temp = it["main"]["temp"].as<float>();
      s.pop = it["pop"] | 0.0f;
      s.precip = (it["rain"]["3h"] | 0.0f) + (it["snow"]["3h"] | 0.0f);
      s.weatherId = it["weather"][0]["id"] | 800;
      const char* pod = it["sys"]["pod"] | "d";
      s.night = pod[0] == 'n';
    } while (data.slotCount < MAX_SLOTS && stream.findUntil(",", "]"));
  }
  http.end();

  Serial.printf("Vorhersage: %d Werte, freier Heap %u Bytes\n", data.slotCount, ESP.getFreeHeap());
  if (data.slotCount == 0)
  {
    setError(err, 0, "Keine Wetterdaten", "Vorhersage ist leer");
    return false;
  }
  return true;
}

}  // namespace

bool fetchWeather(WeatherData& data, time_t now, FetchError& err)
{
  return fetchCurrent(data.now, err) && fetchForecast(data, now, err);
}
