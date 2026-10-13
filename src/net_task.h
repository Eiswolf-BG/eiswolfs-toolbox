#pragma once
#include <cstdint>
#include "config.h"
#include "iss_client.h"
#include "rocket_client.h"
#include "transit_client.h"
#include "weather_client.h"
#include "f1_client.h"
#include "crypto_client.h"
#include "earthquake_client.h"
#include "holiday_client.h"
#include "natural_events_client.h"

// Hintergrund-Task fuer ALLE Netzwerk-Abrufe (ISS-Position, Raketenstart-
// Daten) - laeuft auf Core 0, damit Core 1 (Rendering + Touch, siehe
// app_navigation.cpp) nie durch Netzwerk-Wartezeiten blockiert wird.
// Gleiches Grundmuster wie Flightradars NetTask/AircraftTable (dort
// zusaetzlich WLAN-Status/Standort/ADS-B, siehe dortige net_task.cpp/
// aircraft_table.cpp) - hier bewusst auf das Noetigste reduziert, da dieses
// Projekt keinen OTA-Update-/pause()-Mechanismus braucht. WICHTIG (Alex'
// Auftrag, 05.10.): dieser Task fasst NIEMALS das Display an (kein tft-
// Aufruf, keine Touch-Auswertung) - er liefert nur Daten per Mutex-
// geschuetztem Struct, gerendert wird ausschliesslich auf Core 1 in den
// jeweiligen Screens. Genau das war beim Flightradar-WebServer-Task-
// Experiment das Problem (Display-Zugriff vom falschen Core fuehrte zu
// Rendering-Fehlern) - hier bewusst vermieden.
namespace NetTask {
    enum class Status : uint8_t { Pending, Ok, Error };

    // Startet den Hintergrund-Task. Muss erst NACH erfolgreichem WLAN-
    // Verbindungsaufbau in main.cpp::setup() aufgerufen werden.
    void begin();

    // ISS-Position - Status::Ok bleibt dauerhaft bestehen, sobald einmal
    // ein Abruf erfolgreich war (ein spaeter fehlschlagender Refetch
    // behaelt die zuletzt bekannten Daten, wie zuvor auch im Screen selbst
    // gehandhabt).
    Status issStatus();
    // Erhoeht sich bei jedem erfolgreichen Abruf - Screens vergleichen
    // gegen ihren zuletzt gesehenen Stand, um unnoetige Mutex-Zugriffe zu
    // vermeiden (nur kopieren, wenn sich wirklich etwas geaendert hat).
    uint32_t issVersion();
    // Threadsicherer Kopierzugriff - true, wenn bereits Daten vorliegen.
    bool getIssPosition(IssClient::Position& out);

    // Bis zu Config::ROCKET_MAX_LAUNCHES Starts (naechster zuerst) - "limit=3"
    // liefert sie in EINER Anfrage (Alex' Auftrag, 06.10.), siehe
    // rocket_client.h.
    struct RocketData {
        RocketClient::Launch launches[Config::ROCKET_MAX_LAUNCHES];
        uint8_t launchCount = 0;
    };

    Status rocketStatus();
    uint32_t rocketVersion();
    bool getRocketData(RocketData& out);

    // Erzwingt einen sofortigen Neuabruf beim naechsten taskFunc()-
    // Durchlauf, OHNE auf Config::ROCKET_REFETCH_INTERVAL_MS zu warten
    // (Alex' Auftrag, 12.10., "Nochmal versuchen"-Button nach einem
    // Verbindungsfehler, siehe network_retry_view.h) - setzt dabei
    // automatisch auch den Zeitstempel fuer den naechsten REGULAEREN
    // Abruf neu (gleicher Codepfad wie ein normaler Abruf), kein
    // doppelter Nachlauf-Abruf kurz danach noetig.
    void forceRocketRefetch();

    // OEPNV-Abfahrtstafel (transit_client.h/transit_screen.h) - einziger
    // Datensatz hier mit einem zusaetzlichen Zwischenschritt: bevor
    // Abfahrten abgerufen werden koennen, muss erst die naechste
    // Haltestelle zur aktuellen Position bekannt sein. NetTask liest dafuer
    // NIEMALS selbst LocationPresets/SD (bleibt Core-1-exklusiv, siehe
    // Datei-Kommentar oben) - der transit_screen.cpp (Core 1) uebergibt die
    // Koordinaten stattdessen explizit ueber setTransitLocation().
    struct TransitData {
        String stopName;
        TransitClient::Departure departures[Config::TRANSIT_MAX_DEPARTURES];
        uint8_t departureCount = 0;
    };
    // Genauerer Fehlergrund als das generische Status::Error - fuer
    // passende, unterscheidbare Fehlermeldungen (Alex' Auftrag: "keine
    // Verbindung"/"keine Haltestelle gefunden"/"keine Abfahrten" sollen
    // unterschiedliche Meldungen zeigen).
    enum class TransitError : uint8_t { None, NetworkError, NoStopFound, NoDepartures };

    // Vom Screen (Core 1) aufgerufen - typischerweise einmal pro enter()
    // mit der jeweils aktuell ermittelten effektiven Position (Override
    // oder allgemeiner Standort, siehe transit_screen.cpp). Ein Aufruf mit
    // GEAENDERTEN Koordinaten (oder der allererste Aufruf ueberhaupt) loest
    // beim naechsten Taskdurchlauf sofort einen frischen Reverse-Geocode
    // aus; identische Koordinaten wie beim letzten Mal loesen NICHTS aus
    // (der normale Abfahrten-Refresh im Hintergrund laeuft unabhaengig
    // weiter).
    void setTransitLocation(double lat, double lon);

    Status transitStatus();
    TransitError transitError();
    uint32_t transitVersion();
    bool getTransitData(TransitData& out);

    // Wetter-Screen (weather_client.h/weather_screen.h) - einfacher als
    // OEPNV: Open-Meteo braucht nur lat/lon direkt, KEINEN Zwischenschritt
    // wie eine Haltestellen-Suche. setWeatherLocation() funktioniert
    // trotzdem nach demselben Prinzip wie setTransitLocation() (Core 1
    // uebergibt die Koordinaten, NetTask liest NIEMALS selbst LocationPresets/
    // SD) - ein Aufruf mit geaenderten Koordinaten (oder der allererste
    // Aufruf) loest sofort einen frischen Abruf aus, identische Koordinaten
    // loesen nichts aus (der normale 20-Minuten-Refresh laeuft unabhaengig
    // weiter).
    void setWeatherLocation(double lat, double lon);

    Status weatherStatus();
    uint32_t weatherVersion();
    bool getWeatherData(WeatherClient::Forecast& out);

    // Formel-1-Screen (f1_client.h/f1_screen.h) - zwei getrennte
    // Datensaetze: Saison-Daten (Kalender/Wertungen/letztes Ergebnis, alle
    // 15 Minuten) und Live-Rangliste (nur waehrend einer tatsaechlich
    // laufenden Session, alle 25-30s). Siehe CLAUDE.md "Formel 1" fuer die
    // Begruendung der Zeitfenster-basierten Live-Abfrage (volle
    // Session-Historie ist live bestaetigt teils mehrere MB gross).
    Status f1SeasonStatus();
    uint32_t f1SeasonVersion();
    bool getF1SeasonData(F1Client::SeasonData& out);

    // Eine Zeile der Live-Rangliste - bereits fertig sortiert/aufbereitet
    // (NetTask fuehrt die Zeitfenster-Ausschnitte zu dieser Tabelle
    // zusammen, siehe net_task.cpp). hasGapSeconds/hasGapLaps sind beide
    // false, solange fuer diesen Fahrer noch kein Abstandswert bekannt ist
    // (z.B. kurz nach Session-Start) - der Screen zeigt dann einfach keinen
    // Abstand fuer diese Zeile.
    struct F1LiveRow {
        int16_t driverNumber = -1;
        String acronym;
        String teamName;
        uint16_t teamColour565 = 0xFFFF;
        uint8_t position = 0; // 0 = noch unbekannt
        bool hasGapSeconds = false;
        float gapSeconds = 0;
        bool hasGapLaps = false;
        int16_t gapLaps = 0;
        // Letzte abgeschlossene Rundenzeit (Alex' Auftrag, 06.10.) - false,
        // solange noch keine Runde dieses Fahrers im Zeitfenster gesehen
        // wurde.
        bool hasLapTime = false;
        float lapTimeSec = 0;
    };
    constexpr uint8_t F1_LIVE_MAX_ROWS = F1Client::MAX_LIVE_DRIVERS;

    struct F1LiveBoard {
        bool live = false;
        String sessionName; // Rohwert aus OpenF1 (z.B. "Race") - Uebersetzung im Screen
        F1LiveRow rows[F1_LIVE_MAX_ROWS]; // sortiert nach Position, Fahrer OHNE bekannte Position hinten
        uint8_t rowCount = 0;
    };

    uint32_t f1LiveVersion();
    bool getF1LiveBoard(F1LiveBoard& out);

    // Krypto-Ticker (crypto_client.h/crypto_screen.h) - btc/eth/markets/
    // mempool teilen sich EIN Status/Version-Paar (gemeinsames
    // Abruf-Intervall, siehe Config::CRYPTO_REFETCH_INTERVAL_MS), der
    // Fear&Greed-Index hat ein EIGENES, deutlich selteneres Intervall
    // (Config::CRYPTO_FNG_REFETCH_INTERVAL_MS) und deshalb ein eigenes
    // Status/Version-Paar - beide Datensaetze leben trotzdem im selben
    // CryptoData-Struct/Mutex (einfacher als zwei getrennte Structs, da
    // ohnehin nur ein einziger Screen beide liest). Innerhalb von
    // btc/eth/markets/mempool kann trotzdem jede Teilquelle unabhaengig
    // fehlschlagen (Alex' Auftrag) - cryptoVersion() erhoeht sich bereits,
    // wenn AUCH NUR EINE der drei Teilquellen erfolgreich war, die
    // jeweils anderen behalten dann einfach ihren zuletzt bekannten Wert
    // (gleiches Prinzip wie AirQuality.valid beim Wetter-Screen).
    struct CryptoData {
        CryptoClient::MainCoin btc;
        CryptoClient::MainCoin eth;
        CryptoClient::CoinMarket markets[Config::CRYPTO_MAX_MARKETS];
        uint8_t marketCount = 0;
        CryptoClient::Mempool mempool;
        CryptoClient::FearGreed fearGreed;
        // Welche Waehrung btc/eth/markets tatsaechlich in DIESEM Abruf
        // hatten (Alex' Auftrag, 07.10., siehe currency.h) - der Screen
        // braucht das fuer den passenden Anzeige-Praefix, ohne selbst
        // nochmal LocationPresets abzufragen. "usd", bis CryptoScreen
        // einmal setCryptoCurrency() aufgerufen hat.
        char vsCurrency[8] = "usd";
    };

    // Von CryptoScreen (Core 1) einmal pro enter() aufgerufen, NIEMALS von
    // NetTask selbst aus LocationPresets gelesen (Core 0 liest niemals
    // LocationPresets/SD direkt, siehe Datei-Kommentar oben) - gleiches
    // Prinzip wie setWeatherLocation()/setTransitLocation().
    void setCryptoCurrency(const char* vsCurrency);

    Status cryptoStatus();
    uint32_t cryptoVersion();
    bool getCryptoData(CryptoData& out);

    Status cryptoFngStatus();
    uint32_t cryptoFngVersion();

    // Erdbeben-Screen (earthquake_client.h/earthquake_screen.h) - bis zu
    // Config::EARTHQUAKE_MAX_QUAKES Beben, staerkstes zuerst (bereits
    // serverseitig sortiert, siehe earthquake_client.h).
    struct EarthquakeData {
        EarthquakeClient::Quake quakes[Config::EARTHQUAKE_MAX_QUAKES];
        uint8_t quakeCount = 0;
    };

    Status earthquakeStatus();
    uint32_t earthquakeVersion();
    bool getEarthquakeData(EarthquakeData& out);

    // Kalender-Screen (holiday_client.h/calendar_screen.h, Alex' Auftrag,
    // 12.10.) - alle Feiertage des aktuellen Jahres fuer EIN Land.
    struct HolidayData {
        HolidayClient::Holiday holidays[Config::HOLIDAY_MAX_COUNT];
        uint8_t holidayCount = 0;
    };

    // Von CalendarScreen (Core 1) einmal pro enter() aufgerufen, NIEMALS
    // von NetTask selbst aus LocationPresets gelesen (siehe Datei-
    // Kommentar oben) - gleiches Prinzip wie setCryptoCurrency().
    void setHolidayCountry(const char* countryCode);

    Status holidayStatus();
    uint32_t holidayVersion();
    bool getHolidayData(HolidayData& out);

    // Naturereignisse-Screen (natural_events_client.h/natural_events_
    // screen.h, Alex' Auftrag, 12.10.) - rein zeitgesteuert, keine
    // Standort-Abhaengigkeit (die EONET-Abfrage ist weltweit).
    struct NaturalEventsData {
        NaturalEventsClient::Event events[Config::NATURAL_EVENTS_MAX_COUNT];
        uint8_t eventCount = 0;
    };

    Status naturalEventsStatus();
    uint32_t naturalEventsVersion();
    bool getNaturalEventsData(NaturalEventsData& out);

    // Siehe forceRocketRefetch() oben fuer die genaue Begruendung -
    // gleiches Prinzip.
    void forceNaturalEventsRefetch();
}
