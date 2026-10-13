#pragma once
#include <Arduino.h>
#include <time.h>

// OEPNV-Abfahrten (Transitous, api.transitous.org) - siehe CLAUDE.md
// "OEPNV-Abfahrtstafel" fuer die recherchierten/live verifizierten
// Eckdaten (Endpunkte, JSON-Felder). Zweistufig: einmalig die naechste
// Haltestelle per Reverse-Geocode finden (findNearestStop()), danach
// wiederholt deren Abfahrten abrufen (fetchDepartures()) - siehe
// transit_screen.h/net_task.h fuer den Gesamtablauf.
namespace TransitClient {
    struct Departure {
        String line;        // routeShortName, z.B. "N42", "U6"
        String headsign;     // Fahrtziel/Richtung
        time_t epoch = 0;    // geplante/Echtzeit-Abfahrt als UTC-Unix-Zeitstempel
        // Zusaetzliche Detail-Daten (Alex' Auftrag, 06.10. - antippbare
        // Abfahrtszeile oeffnet einen Detail-Screen) - alle live gegen die
        // echte API verifiziert (siehe CLAUDE.md). Ein "alerts"-Feld
        // existiert in diesem Endpunkt NICHT (mehrfach live geprueft,
        // ueber verschiedene Haltestellen/Abfahrten hinweg kein einziges
        // Vorkommen) - bewusst NICHT umgesetzt, siehe dortiger Kommentar
        // in transit_screen.cpp.
        String tripFromName; // Start der Fahrt (tripFrom.name)
        String tripToName;   // Ziel der Fahrt (tripTo.name) - siehe transit_screen.cpp
                              // fuer die Begruendung, warum im Detail-Screen trotzdem
                              // "headsign" statt diesem Feld als "Nach:" angezeigt wird.
        String agencyName;   // Verkehrsunternehmen
        String track;        // Gleis/Steig (place.track) - leer, falls keins angegeben
        bool realTime = false; // true = Echtzeitdaten, false = nur Fahrplan

        // Weitere API-Felder (Alex' Auftrag, 06.10. - "alle sinnvollen
        // API-Daten anzeigen", siehe CLAUDE.md) - live gegen die echte API
        // verifiziert (siehe transit_client.cpp).
        time_t scheduledEpoch = 0; // place.scheduledDeparture - fuer Verspaetungsberechnung
        bool cancelled = false;    // cancelled ODER tripCancelled (beide live bestaetigt vorhanden)
        String routeLongName;      // volle Linienbezeichnung, oft leer (live bestaetigt)
        String wheelchairAccessible; // Rohwert, z.B. "ACCESSIBLE"/"NOT_ACCESSIBLE"/"NO_INFORMATION"
        bool bikesAllowed = false;
        String stopTz;              // place.tz, z.B. "Europe/Berlin" - fuer die
                                     // Uhrzeit-Anzeige (lokale statt UTC-Zeit, siehe
                                     // transit_screen.cpp::formatClockTime())
    };

    enum class StopResult { Ok, NetworkError, NoResults };
    enum class DeparturesResult { Ok, NetworkError, NoResults };

    // Reverse-Geocode: naechstgelegene Haltestelle zu (lat, lon) - 1:1
    // das erste Ergebnis der API (bereits nach Relevanz sortiert, siehe
    // CLAUDE.md). Liefert stopId/stopName bei Erfolg.
    StopResult findNearestStop(double lat, double lon, String& outStopId, String& outStopName);

    // Naechste Abfahrten fuer eine Haltestelle - out muss mindestens
    // maxCount Eintraege fassen. outCount <= maxCount, auch wenn die API
    // (wie dokumentiert) mehr Ergebnisse liefert als angefragt.
    DeparturesResult fetchDepartures(const String& stopId, Departure* out, uint8_t maxCount, uint8_t& outCount);
}
