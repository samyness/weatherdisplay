#pragma once
// Bindet config.h und secrets.h ein und prüft die Einstellungen.

#include "config.h"

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "secrets.h fehlt: secrets_example.h nach secrets.h kopieren und WLAN, API-Key und Ort eintragen."
#endif

#if !defined(LOCATION_QUERY) && !(defined(LOCATION_LAT) && defined(LOCATION_LON))
#error "In secrets.h entweder LOCATION_QUERY oder LOCATION_LAT und LOCATION_LON setzen."
#endif

#ifndef CITY_NAME
#define CITY_NAME ""
#endif

#if defined(PANEL_V2_BW) == defined(PANEL_V2_3C)
#error "In config.h genau eines von PANEL_V2_BW oder PANEL_V2_3C aktivieren."
#endif
