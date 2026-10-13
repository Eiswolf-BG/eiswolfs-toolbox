#pragma once
#include <Arduino.h>
#include "config.h"

// Kalender-Screen (Alex' Auftrag, 12.10.) - Nager.Date (date.nager.at/
// api/v3), kostenlos, kein API-Key, deckt ueber 200 Laender ab. Siehe
// Config::HOLIDAY_API_HOST/HOLIDAY_API_PATH in config.h.
namespace HolidayClient {
    struct Holiday {
        String date;  // "YYYY-MM-DD", roh aus der API
        // Bereits font-sicher gewaehlt: "localName", falls ausschliesslich
        // Zeichen aus unserem Font-Zeichensatz (0x0020-0x015F, siehe
        // ui_font.h) enthalten sind, sonst das englische "name"-Feld als
        // Rueckfall (z.B. japanische/kyrillische Feiertagsnamen) - gleiches
        // Prinzip wie beim Euro-Symbol-Problem im Krypto-Screen (siehe
        // currency.h). Wird bereits beim Abruf entschieden, der Aufrufer
        // muss sich darum nicht mehr kuemmern.
        String name;
        bool valid = false;
    };

    // Liefert ALLE von der API gelieferten Feiertage eines Landes/Jahres
    // (Alex' Auftrag: "fuer diese erste Version einfach ALLE, ohne nach
    // Region zu filtern" - die API liefert regionale/bundeslandspezifische
    // Feiertage im selben Array mit, "counties"-Feld wird hier bewusst
    // NICHT ausgewertet). true, wenn mindestens einer gueltig uebernommen
    // wurde.
    bool fetch(const char* countryCode, int year, Holiday* out, uint8_t maxCount, uint8_t& outCount);
}
