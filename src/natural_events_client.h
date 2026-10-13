#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

// Naturereignisse-Screen (Alex' Auftrag, 12.10.) - NASA EONET v3
// (eonet.gsfc.nasa.gov, schluessellos), siehe Config::NATURAL_EVENTS_API_URL
// in config.h fuer die genaue Begruendung der Kategorie-/Status-Filterung
// (Waldbraende bewusst ausgeschlossen).
namespace NaturalEventsClient {
    // Deckt genau die 4 abgefragten Kategorien ab (siehe
    // Config::NATURAL_EVENTS_API_URL) - "Unknown" als Fallback, falls die
    // API ausnahmsweise eine unerwartete Kategorie-ID liefert.
    enum class Category : uint8_t {
        Volcano,
        Storm,
        Flood,
        Landslide,
        Unknown
    };

    struct Event {
        String title;
        Category category = Category::Unknown;
        // Juengster Zeitpunkt aus "geometry" (ein Ereignis wie ein
        // Hurrikan hat oft Dutzende Bahnpunkte - nur der letzte/aktuellste
        // ist fuer die Anzeige relevant).
        time_t epoch = 0;
        bool valid = false;
    };

    // Liefert bis zu maxCount aktive Ereignisse. true, wenn mindestens
    // eins gueltig uebernommen wurde.
    bool fetch(Event* out, uint8_t maxCount, uint8_t& outCount);
}
