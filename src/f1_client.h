#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

// Formel-1-Screen (Alex' Auftrag, 06.10.) - zwei Datenquellen, siehe
// CLAUDE.md "Formel 1": Jolpica-F1 (Ergast-Nachfolger, api.jolpi.ca) fuer
// Kalender/Wertungen/letztes Rennergebnis, OpenF1 (api.openf1.org) fuer
// echte Live-Daten waehrend einer laufenden Session. Beide schluessellos,
// kostenlos, live per curl verifiziert.
//
// WICHTIG zu OpenF1 "position"/"intervals": ein kompletter, ungefilterter
// Abruf fuer eine volle Session ist live bestaetigt teils MEHRERE MB gross
// (21448 Eintraege / 3 MB bei "intervals" fuer eine 2h-Session, siehe
// CLAUDE.md "Formel 1") - dieselbe Fehlerklasse wie das bereits einmal
// dokumentierte xrays-6-hour.json-Problem beim (inzwischen wieder
// entfernten) Weltraumwetter-Screen. Diese Client-Funktionen liefern
// deshalb bewusst nur EINEN ZEITFENSTER-AUSSCHNITT (sinceEpoch-Parameter,
// "date>="-Query), NIEMALS die volle Session-Historie - der Aufrufer
// (net_task.cpp) fuehrt die Ergebnisse ueber mehrere Abrufe hinweg zu
// einer persistenten Pro-Fahrer-Tabelle zusammen, statt bei jedem Zyklus
// alles neu zu laden.
namespace F1Client {
    constexpr uint8_t MAX_STANDINGS = 26;
    constexpr uint8_t MAX_RESULTS = 26;
    constexpr uint8_t MAX_LIVE_DRIVERS = 26;

    struct SessionTime {
        bool valid = false;
        time_t epoch = 0; // UTC
    };

    struct NextRace {
        bool valid = false;
        String raceName;
        String circuitName;
        String locality;
        String country;
        SessionTime race;
        SessionTime firstPractice;
        SessionTime secondPractice;  // nur bei "normalen" (Nicht-Sprint-)Wochenenden vorhanden
        SessionTime thirdPractice;   // dito
        SessionTime qualifying;
        SessionTime sprint;              // nur bei Sprint-Wochenenden vorhanden
        SessionTime sprintQualifying;    // dito
    };

    struct DriverStanding {
        uint8_t position = 0;
        float points = 0;
        uint8_t wins = 0;
        String code;          // 3-Buchstaben-Kuerzel, z.B. "VER"
        String givenName;
        String familyName;
        String constructorName;
    };

    struct ConstructorStanding {
        uint8_t position = 0;
        float points = 0;
        uint8_t wins = 0;
        String name;
    };

    // "status" bleibt als Rohwert aus der API erhalten (z.B. "Finished",
    // "Retired", "Accident", "+1 Lap") - Uebersetzung ins Deutsche/etc.
    // passiert erst im Screen (siehe f1_screen.cpp::resultStatusStringId()),
    // gleiches Prinzip wie bei RocketClient::Launch::statusAbbrev.
    struct RaceResult {
        String positionText;  // z.B. "1", "R" (= nicht klassifiziert/ausgeschieden)
        String driverCode;
        String givenName;
        String familyName;
        String constructorName;
        String time;       // z.B. "+2.307" oder die Siegerzeit - leer, wenn nicht vorhanden
        String points;
        String status;
    };

    struct SeasonData {
        NextRace nextRace;
        DriverStanding driverStandings[MAX_STANDINGS];
        uint8_t driverStandingCount = 0;
        ConstructorStanding constructorStandings[MAX_STANDINGS];
        uint8_t constructorStandingCount = 0;
        String lastRaceName;
        RaceResult lastResults[MAX_RESULTS];
        uint8_t lastResultCount = 0;
    };

    // Buendelt alle 4 Jolpica-Endpunkte in einem Abruf-Zyklus (kein
    // kombinierter Endpunkt verfuegbar) - tolerant gegenueber Teilausfaellen
    // (gleiches Prinzip wie SpaceWeatherClient::fetchNoaaData), true wenn
    // mindestens das naechste Rennen ODER die Fahrerwertung geholt werden
    // konnte.
    bool fetchSeasonData(SeasonData& out);

    // ---- OpenF1 (Live-Daten waehrend einer laufenden Session) ----

    struct LiveSessionInfo {
        bool valid = false;
        int32_t sessionKey = -1;
        String sessionName;   // z.B. "Race", "Qualifying", "Practice 1"
        time_t dateStart = 0;
        time_t dateEnd = 0;
        // Rohe ISO-8601-Strings direkt aus der API, VOR dem Parsen (Alex'
        // Auftrag, 12.10. - [F1Diag]-Logging in net_task.cpp um die
        // isLive-Entscheidung, siehe dort) - fuer die Fehlersuche beim
        // naechsten echten Rennen, falls parseIso8601Utc() den Rohwert aus
        // irgendeinem Grund falsch interpretiert.
        String dateStartRaw;
        String dateEndRaw;
    };
    // "session_key=latest" - liefert IMMER die letzte (ggf. bereits
    // beendete) Session, unabhaengig davon, ob gerade live gefahren wird.
    // Der Aufrufer entscheidet anhand dateStart/dateEnd + aktueller Uhrzeit,
    // ob "live" ist (siehe net_task.cpp).
    bool fetchLatestSessionInfo(LiveSessionInfo& out);

    struct LiveDriver {
        int16_t number = -1;
        String acronym;      // name_acronym, z.B. "VER"
        String teamName;
        uint16_t teamColour565 = 0xFFFF; // bereits in TFT_eSPI-565-Farbe umgerechnet (aus team_colour-Hex)
    };
    // Einmal pro (neuer) Session noetig, NICHT bei jedem Live-Poll - die
    // Fahrerliste aendert sich waehrend einer Session nicht.
    bool fetchLiveDrivers(int32_t sessionKey, LiveDriver* out, uint8_t maxCount, uint8_t& outCount);

    struct PositionUpdate {
        int16_t driverNumber = -1;
        uint8_t position = 0;
        time_t epoch = 0;
    };
    // Nur Eintraege mit date >= sinceEpoch (siehe Datei-Kommentar oben) -
    // outCount kann 0 sein, wenn sich seit sinceEpoch nichts geaendert hat
    // (normal, KEIN Fehler).
    bool fetchPositionUpdates(int32_t sessionKey, time_t sinceEpoch,
                               PositionUpdate* out, uint8_t maxCount, uint8_t& outCount);

    struct IntervalUpdate {
        int16_t driverNumber = -1;
        time_t epoch = 0;
        // gap_to_leader ist bei OpenF1 ENTWEDER eine Zahl (Sekunden) ODER
        // (bei einem bereits ueberrundeten Fahrzeug) ein Text wie "1 LAP" -
        // deshalb zwei getrennte, optionale Felder statt eines rohen
        // String-Durchreichens (siehe CLAUDE.md: kein roher englischer
        // API-Text im UI). Formatierung/Uebersetzung passiert im Screen.
        bool hasGapSeconds = false;
        float gapSeconds = 0;
        bool hasGapLaps = false;
        int16_t gapLaps = 0;
    };
    bool fetchIntervalUpdates(int32_t sessionKey, time_t sinceEpoch,
                               IntervalUpdate* out, uint8_t maxCount, uint8_t& outCount);

    // Letzte Rundenzeit (Alex' Auftrag, 06.10.) - "/v1/laps" ist wie
    // "position"/"intervals" fuer eine komplette Session live bestaetigt
    // viel zu gross (527 KB/1145 Eintraege), deshalb genau dasselbe
    // Zeitfenster-Prinzip (date_start>=sinceEpoch) statt eines
    // Komplettabrufs.
    struct LapUpdate {
        int16_t driverNumber = -1;
        float lapDurationSec = 0;
        time_t epoch = 0;
    };
    bool fetchLapUpdates(int32_t sessionKey, time_t sinceEpoch,
                          LapUpdate* out, uint8_t maxCount, uint8_t& outCount);
}
