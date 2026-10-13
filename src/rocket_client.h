#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

// Raketenstart-Daten (Launch Library 2 API, The Space Devs) - siehe
// CLAUDE.md "ISS-Radar"/"Raketenstart-Countdown" fuer die recherchierten
// Eckdaten (anonymes Limit: 15 Anfragen/Stunde, siehe Config::
// ROCKET_REFETCH_INTERVAL_MS). "limit=3" liefert die naechsten 3 Starts
// in EINER Anfrage (Alex' Auftrag, 06.10., live bestaetigt) - kein
// zusaetzlicher Request noetig.
namespace RocketClient {
    struct Launch {
        String missionName;
        String rocketName;       // kurz, z.B. "Nuri" - rocket.configuration.name
        String rocketFullName;   // vollstaendig, z.B. "KSLV-2 Nuri" - .full_name
        String agencyName;       // launch_service_provider.name, z.B. "SpaceX"
        String padName;          // Startplatz (Ort), z.B. "Kennedy Space Center, FL, USA"
        String missionType;      // mission.type, z.B. "Earth Science"
        String missionOrbitName; // mission.orbit.name, z.B. "Sun-Synchronous Orbit"
        String missionDescription; // mission.description - NUR fuer den Detail-Screen
        // Beide leer, wenn nicht vorhanden/zutreffend (API liefert dafuer
        // "", nicht null - live bestaetigt, siehe CLAUDE.md).
        String holdReason;   // holdreason
        String failReason;   // failreason
        // -1 = keine Angabe (API liefert null, live bestaetigt - noch nie
        // bei den naechsten 50 Starts live beobachtet, aber laut Schema
        // moeglich, siehe CLAUDE.md).
        int8_t probability = -1;    // probability (0-100)
        String weatherConcerns;     // weather_concerns, leer = keine Angabe
        // Rohwerte aus der API (status.id/status.name/status.abbrev) -
        // Uebersetzung ins Deutsche/etc. passiert im Screen selbst (siehe
        // ROCKET_STATUS_*-StringIds). status.id ist laut Launch-Library-2-
        // Doku die stabile, numerische Kennung (aktuell 9 feste Werte,
        // 1-9) - primaerer Schluessel fuer die Uebersetzungstabelle, da
        // deutlich stabiler als der Text in status.abbrev/status.name.
        // -1 = fehlt/unbekannt (Fallback auf statusAbbrev-Textabgleich,
        // siehe rocket_screen.cpp::rocketStatusText()).
        int16_t statusId = -1;
        String statusName;
        String statusAbbrev;
        String netIso;        // roher ISO-8601-UTC-String (fuer "TT.MM. HH:MM" ohne Epoch-Umrechnung)
        time_t netEpoch = 0;  // geplante Startzeit als UTC-Unix-Zeitstempel
        bool valid = false;
    };

    // Einmaliger blockierender HTTPS-Request mit ArduinoJson-Feldfilter
    // (nur die tatsaechlich benoetigten Felder werden geparst, nicht die
    // komplette - teils mehrere KB grosse - Missionsbeschreibung UND
    // Agentur-Metadaten) - liefert bis zu maxCount Starts (naechster
    // zuerst), outCount <= maxCount. true, wenn mindestens einer gueltig
    // uebernommen wurde.
    bool fetch(Launch* out, uint8_t maxCount, uint8_t& outCount);
}
