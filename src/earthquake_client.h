#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

// Erdbeben-Screen (Alex' Auftrag, 07.10.) - USGS fdsnws-Query-API
// (earthquake.usgs.gov, schluessellos), siehe Config::EARTHQUAKE_API_URL
// in config.h fuer die genaue Begruendung der Endpunkt-Wahl.
namespace EarthquakeClient {
    struct Quake {
        float magnitude = 0;
        String place;
        time_t epoch = 0;   // properties.time (Unix-Millis -> Sekunden)
        bool tsunami = false;
        float depthKm = 0;  // geometry.coordinates[2]
        bool valid = false;
    };

    // Liefert bis zu maxCount Erdbeben, naechst-staerkstes zuerst (die API
    // liefert sie bereits serverseitig nach Magnitude absteigend sortiert,
    // "orderby=magnitude" in Config::EARTHQUAKE_API_URL - kein eigenes
    // Sortieren noetig). true, wenn mindestens eins gueltig uebernommen
    // wurde.
    bool fetch(Quake* out, uint8_t maxCount, uint8_t& outCount);
}
