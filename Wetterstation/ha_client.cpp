#include "ha_client.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>

#include "settings.h"
#include "text_de.h"

#ifndef HA_URL
#define HA_URL ""
#endif
#ifndef HA_TOKEN
#define HA_TOKEN ""
#endif
#ifndef HA_DISPLAY_ENTITY
#define HA_DISPLAY_ENTITY ""
#endif
#ifndef HA_ENTITIES
#define HA_ENTITIES {nullptr, nullptr}
#endif

namespace {

struct HaEntity
{
  const char* label;      // Anzeigename, "" = friendly_name aus Home Assistant
  const char* entityId;
  const char* labelAttr;  // optional: Attribut als Name, z. B. "message" (Müllart im Abfallkalender)
  const char* valueAttr;  // optional: Attribut als Wert, z. B. "start_time" (Abholtermin)
};

const HaEntity ENTITIES[] = {HA_ENTITIES, {nullptr, nullptr}};

bool hasText(const char* s) { return s && s[0]; }

// Prüft, ob überhaupt gefragt werden darf
bool haReady(bool authFailed)
{
  if (!HA_URL[0] || !HA_TOKEN[0]) return false;
  if (authFailed)
  {
    Serial.println("Home Assistant: Token ungueltig, Abfrage bis zum Neustart deaktiviert");
    return false;
  }
  if (strncmp(HA_URL, "http://", 7) != 0)
  {
    // https bräuchte ~20 KB RAM für TLS; im LAN reicht http://IP:8123
    Serial.println("Home Assistant: HA_URL muss mit http:// beginnen (https wird nicht unterstuetzt)");
    return false;
  }
  return true;
}

// GET /api/states/<entityId>, gefiltert direkt aus dem Datenstrom gelesen.
// Rückgabe: HTTP-Status, < 0 = nicht erreichbar, 0 = Antwort nicht lesbar.
int getState(const char* entityId, JsonDocument& doc, const JsonDocument& filter, bool& authFailed)
{
  String url = HA_URL;
  while (url.endsWith("/")) url.remove(url.length() - 1);
  url += "/api/states/";
  url += entityId;

  WiFiClient client;
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(5000);
  if (!http.begin(client, url)) return -1;
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  int code = http.GET();
  Serial.printf("Home Assistant %s -> %d\n", entityId, code);
  if (code == HTTP_CODE_OK)
  {
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    if (err)
    {
      Serial.printf("  Antwort nicht lesbar (%s)\n", err.c_str());
      code = 0;
    }
  }
  else if (code == HTTP_CODE_UNAUTHORIZED || code == HTTP_CODE_FORBIDDEN)
  {
    Serial.println("Home Assistant: Token ungueltig (HA_TOKEN in secrets.h pruefen)");
    authFailed = true;
  }
  http.end();
  return code;
}

// Attributwert als Text (Strings direkt, Zahlen & Co. als JSON)
void variantToText(JsonVariantConst v, char* out, size_t size)
{
  if (v.isNull())
    out[0] = '\0';
  else if (v.is<const char*>())
    copyStr(out, v.as<const char*>(), size);
  else
    serializeJson(v, out, size);
}

// Müll-Zeilen bekommen ein Tonnen-Icon
bool looksLikeWaste(const char* entityId, const char* label)
{
  static const char* const WORDS[] = {"muell", "m\xC3\xBCll", "M\xC3\xBCll", "abfall", "Abfall", "waste",
                                      "trash", "garbage", "tonne", "Tonne"};
  for (const char* w : WORDS)
    if (strstr(entityId, w) || (label && strstr(label, w))) return true;
  return false;
}

}  // namespace

bool homeAssistantConfigured()
{
  return HA_URL[0] && HA_TOKEN[0] && (ENTITIES[0].entityId || HA_DISPLAY_ENTITY[0]);
}

void fetchHomeAssistant(WeatherData& data, bool& authFailed)
{
  data.homeCount = 0;
  if (!ENTITIES[0].entityId || !haReady(authFailed)) return;

  for (int i = 0; ENTITIES[i].entityId && data.homeCount < MAX_HOME_VALUES; i++)
  {
    const HaEntity& e = ENTITIES[i];

    // nur die benötigten Felder lesen (Kalender & Co. haben viele Attribute)
    JsonDocument filter;
    filter["state"] = true;
    filter["attributes"]["unit_of_measurement"] = true;
    filter["attributes"]["friendly_name"] = true;
    if (hasText(e.labelAttr)) filter["attributes"][e.labelAttr] = true;
    if (hasText(e.valueAttr)) filter["attributes"][e.valueAttr] = true;

    JsonDocument doc;
    const int code = getState(e.entityId, doc, filter, authFailed);
    if (authFailed)
    {
      data.homeCount = 0;
      return;
    }
    if (code < 0) return;  // Home Assistant nicht erreichbar -> restliche Sensoren gar nicht erst versuchen
    if (code != HTTP_CODE_OK) continue;  // z. B. 404 = entity_id falsch geschrieben
    JsonVariantConst attrs = doc["attributes"];

    // Name: Attribut (z. B. Müllart) > eigener Name > friendly_name > entity_id
    char label[64] = "";
    if (hasText(e.labelAttr)) variantToText(attrs[e.labelAttr], label, sizeof(label));
    if (!label[0]) copyStr(label, hasText(e.label) ? e.label : (attrs["friendly_name"] | e.entityId), sizeof(label));

    // Wert: Attribut (z. B. Abholtermin) oder Zustand mit Einheit
    char state[64];
    const char* unit = "";
    if (hasText(e.valueAttr))
      variantToText(attrs[e.valueAttr], state, sizeof(state));
    else
    {
      copyStr(state, doc["state"] | "", sizeof(state));
      unit = attrs["unit_of_measurement"] | "";
    }

    HomeValue& hv = data.home[data.homeCount++];
    utf8Sanitize(label, hv.label, sizeof(hv.label));
    utf8CapitalizeFirst(hv.label);
    formatHomeState(state, unit, hv.value, sizeof(hv.value), hv.kind);
    if (looksLikeWaste(e.entityId, e.label)) hv.kind = HOME_WASTE;
    Serial.printf("  %s: %s\n", hv.label, hv.value);
  }
}

void fetchHomeDisplay(WeatherData& data, bool& authFailed)
{
  HomeDisplay& h = data.homeDisplay;
  h = HomeDisplay();
  if (!HA_DISPLAY_ENTITY[0] || !haReady(authFailed)) return;

  // Der Template-Helfer liefert kompaktes JSON als Zustand (max. 255 Zeichen):
  // {"w":{"d":"2026-10-15","k":"Biomüll","t":5,"o":0},"e":{"pv":6.1,"b":17},"n":[[2,"Salz nachfüllen"]]}
  JsonDocument filter;
  filter["state"] = true;
  JsonDocument doc;
  if (getState(HA_DISPLAY_ENTITY, doc, filter, authFailed) != HTTP_CODE_OK) return;

  JsonDocument json;
  DeserializationError err = deserializeJson(json, doc["state"] | "");
  if (err || !json.is<JsonObject>())
  {
    Serial.printf("  Zustand ist kein JSON (%s) - Template pruefen\n", err.c_str());
    return;
  }
  h.valid = true;

  JsonVariantConst w = json["w"];
  const char* kind = w["k"] | "";
  if (kind[0] && w["t"].is<int>())
  {
    h.hasWaste = true;
    utf8Sanitize(kind, h.wasteKind, sizeof(h.wasteKind));
    copyStr(h.wasteDate, w["d"] | "", sizeof(h.wasteDate));
    h.wasteDays = w["t"].as<int>();
    h.wasteOut = (int8_t)constrain(w["o"] | -1, -1, 1);
  }

  JsonVariantConst e = json["e"];
  if (e["pv"].is<float>()) h.pvTodayKwh = e["pv"].as<float>();
  if (e["b"].is<float>()) h.batteryPct = constrain((int)lroundf(e["b"].as<float>()), 0, 100);

  for (JsonVariantConst n : json["n"].as<JsonArrayConst>())
  {
    const char* text = n[1] | "";
    if (!text[0] || h.noteCount >= MAX_NOTES) continue;
    HomeNote& note = h.notes[h.noteCount++];
    note.prio = (uint8_t)constrain(n[0] | 1, 1, 3);
    utf8Sanitize(text, note.text, sizeof(note.text));
  }
  // wichtigste Hinweise zuerst (stabil, damit die Reihenfolge aus HA erhalten bleibt)
  for (int i = 1; i < h.noteCount; i++)
    for (int j = i; j > 0 && h.notes[j - 1].prio < h.notes[j].prio; j--)
    {
      HomeNote t = h.notes[j];
      h.notes[j] = h.notes[j - 1];
      h.notes[j - 1] = t;
    }

  Serial.printf("  Muell: %s (%d Tage, raus %d), PV %.1f kWh, Akku %d %%, %d Hinweise\n", h.wasteKind,
                h.wasteDays, h.wasteOut, h.pvTodayKwh, h.batteryPct, h.noteCount);
}
