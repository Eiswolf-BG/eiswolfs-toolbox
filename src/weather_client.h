#pragma once
#include <Arduino.h>
#include "config.h"

// Wetter-Screen (Open-Meteo, api.open-meteo.com) - siehe CLAUDE.md
// "Wetter-Screen" fuer die live per curl verifizierten Eckdaten
// (Endpunkt, Felder, "timezone=auto"-Verhalten). Ein einziger Aufruf
// liefert aktuelles Wetter + stuendliche + taegliche Vorhersage in
// EINER Antwort (siehe weather_client.cpp fuer den genauen Feldfilter).
namespace WeatherClient {
    struct HourlyPoint {
        String timeLocal;             // "YYYY-MM-DDTHH:MM", bereits lokale Wanduhrzeit
        float temperature = 0;
        uint8_t precipProbability = 0;
        uint8_t weatherCode = 0;
    };

    struct DailyPoint {
        String dateLocal;             // "YYYY-MM-DD"
        uint8_t weatherCode = 0;
        float tempMax = 0;
        float tempMin = 0;
        float precipSum = 0;
        uint8_t precipProbabilityMax = 0;
        float windSpeedMax = 0;
        float uvIndexMax = 0;
        String sunrise;               // "YYYY-MM-DDTHH:MM"
        String sunset;
    };

    // Luftqualitaet (air-quality-api.open-meteo.com - ANDERE Subdomain als
    // das uebrige Wetter, deshalb zwingend ein zweiter, eigener HTTPS-
    // Request, auch wenn er im selben Abruf-Zyklus laeuft wie fetch() oben
    // - siehe CLAUDE.md "Wetter-Screen: Luftqualitaet". -1 = Index nicht
    // verfuegbar (API liefert dafuer null, z.B. nachts/je nach Messnetz-
    // Abdeckung je nach Schadstoff moeglich).
    struct AirQuality {
        bool valid = false;
        int16_t europeanAqi = -1;
        int16_t usAqi = -1;
        int16_t aqiPm25 = -1;
        int16_t aqiPm10 = -1;
        int16_t aqiNo2 = -1;
        int16_t aqiO3 = -1;
        int16_t aqiSo2 = -1;
        // Rohmesswerte, alle in µg/m³ (current_units der API, live
        // bestaetigt - einheitlich fuer alle hier genutzten Schadstoffe).
        float pm10 = 0;
        float pm25 = 0;
        float carbonMonoxide = 0;
        float nitrogenDioxide = 0;
        float sulphurDioxide = 0;
        float ozone = 0;
        float dust = 0;
        float ammonia = 0;
    };

    struct Forecast {
        // "current"
        String currentTimeLocal;      // "YYYY-MM-DDTHH:MM" - Referenzpunkt fuer
                                       // "jetzt" in der stuendlichen Vorhersage
                                       // (weather_screen.cpp), da das Geraet
                                       // selbst nur UTC kennt (siehe CLAUDE.md).
        // Alle Werte bereits in der aktuell gewaehlten Einheit (Metrisch/
        // Imperial, siehe units.h "Globale Einheiten-Einstellung")
        // geliefert - Open-Meteo rechnet serverseitig um (siehe
        // weather_client.cpp::fetch()), der Client selbst NICHT. Die
        // Feldnamen "...Kmh"/"...Mm" sind historisch (vor der Einheiten-
        // Umstellung) und bewusst NICHT umbenannt (geringinvasive
        // Aenderung) - bei Imperial stehen hier tatsaechlich mph/inch.
        float temperature = 0;
        float apparentTemperature = 0;
        uint8_t humidityPercent = 0;
        uint8_t weatherCode = 0;
        float windSpeedKmh = 0;
        int16_t windDirectionDeg = 0;
        float precipitationMm = 0;
        bool isDay = true;

        HourlyPoint hourly[Config::WEATHER_MAX_HOURLY];
        uint8_t hourlyCount = 0;

        DailyPoint daily[Config::WEATHER_MAX_DAILY];
        uint8_t dailyCount = 0;

        // Luftqualitaet (Alex' Auftrag, 06.10.) - bewusst TEIL dieses
        // Structs statt eines eigenen NetTask-Datensatzes: soll laut
        // Auftrag "im gleichen Abruf-Rhythmus" wie das uebrige Wetter
        // aktualisiert werden, ein gemeinsames Struct/Version/Mutex-Paar
        // garantiert das automatisch. airQuality.valid bleibt false, wenn
        // NUR dieser zweite Request fehlschlaegt - der Rest der
        // Wettervorhersage wird davon nicht beeintraechtigt (Teilausfall-
        // Toleranz, gleiches Prinzip wie bei SpaceWeatherClient::
        // fetchNoaaData zuvor).
        AirQuality airQuality;
    };

    enum class FetchResult { Ok, NetworkError, ParseError };

    // Ein einziger Aufruf fuer alles (Alex' Auftrag, 06.10.) - "current"/
    // "hourly"/"daily" in einer Antwort, kein zweiter Request noetig. Holt
    // INTERN zusaetzlich die Luftqualitaet (zweiter HTTPS-Request an eine
    // andere Subdomain, siehe AirQuality-Kommentar oben) - ein Fehlschlag
    // dabei laesst fetch() trotzdem mit FetchResult::Ok zurueckkehren,
    // wenn der Hauptwetter-Teil erfolgreich war (out.airQuality.valid
    // zeigt an, ob die Luftqualitaet tatsaechlich vorliegt).
    FetchResult fetch(double lat, double lon, Forecast& out);
}
