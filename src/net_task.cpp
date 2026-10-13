#include "net_task.h"
#include <Arduino.h>
#include <time.h>
#include <cstring>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "config.h"

namespace NetTask {

namespace {
    SemaphoreHandle_t issMutex = nullptr;
    IssClient::Position issPosition;
    uint32_t issVersionCounter = 0;
    std::atomic<uint8_t> issStatusValue{(uint8_t)Status::Pending};

    SemaphoreHandle_t rocketMutex = nullptr;
    RocketData rocketData;
    uint32_t rocketVersionCounter = 0;
    std::atomic<uint8_t> rocketStatusValue{(uint8_t)Status::Pending};
    // "Nochmal versuchen"-Button (Alex' Auftrag, 12.10., siehe
    // network_retry_view.h) - exchange(false) im taskFunc()-Loop liest UND
    // loescht das Flag atomar in einem Schritt, kein Mutex noetig.
    std::atomic<bool> rocketForceRefetch{false};

    // Sortiert/filtert die von RocketClient::fetch() gelieferten Starts so,
    // dass an Index 0 immer der zeitlich naechste noch bevorstehende Start
    // steht (netEpoch > jetzt) - Alex' Fund, 07.10.: die Launch-Library-API
    // haelt einen gerade gestarteten Eintrag erfahrungsgemaess noch eine
    // Weile in ihrer "upcoming"-Liste (Status wechselt auf "Launch
    // Successful", der Eintrag faellt aber erst verzoegert aus der Liste
    // heraus), waehrend zeitlich spaetere, wirklich noch bevorstehende
    // Starts bereits auf den folgenden Positionen auftauchen. Findet den
    // ERSTEN noch zukuenftigen Eintrag und rotiert ihn an Index 0, die
    // davor liegenden (bereits verstrichenen) Eintraege ruecken dabei je
    // eine Position nach hinten - reine In-Place-Rotation mit genau EINEM
    // Launch als Zwischenspeicher (NICHT ein komplettes zweites Array:
    // ein frueherer Versuch mit `RocketClient::Launch reordered[...]` als
    // lokalem Array hat den 20-KB-Stack von NetTask ueberlaufen lassen -
    // live per Stack-Canary-Crash bestaetigt, siehe CLAUDE.md falls der
    // Kontext nochmal gebraucht wird - jeder Launch enthaelt ca. 13
    // String-Felder, ein komplettes zweites Array davon ist auf dem Stack
    // zu teuer). Fallback: falls ALLE Eintraege bereits verstrichen sind,
    // bleibt die Reihenfolge unveraendert, damit niemals "keine Daten"
    // angezeigt wird.
    void promoteNextFutureLaunch(RocketClient::Launch* launches, uint8_t count) {
        if (count < 2) return;
        time_t now = time(nullptr);

        for (uint8_t i = 0; i < count; i++) {
            if (launches[i].netEpoch > now) {
                if (i > 0) {
                    RocketClient::Launch promoted = launches[i];
                    for (uint8_t j = i; j > 0; j--) launches[j] = launches[j - 1];
                    launches[0] = promoted;
                }
                return;
            }
        }
        // Fallback: alle verstrichen, Reihenfolge unveraendert.
    }

    SemaphoreHandle_t transitMutex = nullptr;
    TransitData transitData;
    uint32_t transitVersionCounter = 0;
    std::atomic<uint8_t> transitStatusValue{(uint8_t)Status::Pending};
    std::atomic<uint8_t> transitErrorValue{(uint8_t)TransitError::None};

    // Von Core 1 via setTransitLocation() uebergebene Koordinaten - eigener
    // kleiner Mutex, unabhaengig von transitMutex oben (unterschiedliche
    // Aktualisierungsfrequenz: die Position aendert sich selten, Abfahrten
    // alle paar Minuten).
    SemaphoreHandle_t transitLocationMutex = nullptr;
    double transitLat = 0.0;
    double transitLon = 0.0;
    bool transitLocationKnown = false;
    std::atomic<bool> transitLocationDirty{false};

    // Nur innerhalb von taskFunc (Core 0) gelesen/geschrieben - kein Mutex
    // noetig.
    String cachedStopId;

    // Wetter-Screen (weather_client.h) - einfacher als OEPNV: kein
    // Zwischenschritt wie eine Haltestellen-Suche, Open-Meteo braucht
    // direkt lat/lon. Gleiches Koordinaten-Uebergabe-Prinzip wie beim
    // OEPNV-Screen (eigener kleiner Mutex, siehe transitLocationMutex
    // oben).
    SemaphoreHandle_t weatherMutex = nullptr;
    WeatherClient::Forecast weatherData;
    uint32_t weatherVersionCounter = 0;
    std::atomic<uint8_t> weatherStatusValue{(uint8_t)Status::Pending};

    SemaphoreHandle_t weatherLocationMutex = nullptr;
    double weatherLat = 0.0;
    double weatherLon = 0.0;
    bool weatherLocationKnown = false;
    std::atomic<bool> weatherLocationDirty{false};

    // Formel-1-Screen (f1_client.h) - Saison-Daten (Kalender/Wertungen/
    // letztes Ergebnis) nach demselben einfachen Grundmuster wie
    // Wetter/Rakete oben.
    SemaphoreHandle_t f1SeasonMutex = nullptr;
    F1Client::SeasonData f1SeasonData;
    uint32_t f1SeasonVersionCounter = 0;
    std::atomic<uint8_t> f1SeasonStatusValue{(uint8_t)Status::Pending};

    // Live-Rangliste - EIGENER Mutex, unabhaengig von f1SeasonMutex oben
    // (voellig andere Aktualisierungsfrequenz: 15 Minuten vs. 25-30s).
    // Die eigentliche Zusammenfuehrung der Zeitfenster-Ausschnitte
    // (position/intervals) zu einer vollstaendigen Pro-Fahrer-Tabelle
    // passiert in EINEM Satz Variablen, die NUR von taskFunc (Core 0)
    // gelesen/geschrieben werden (kein eigener Mutex noetig, siehe
    // cachedStopId oben fuer dasselbe Prinzip) - erst das fertige Ergebnis
    // (f1LiveBoard) wird unter f1LiveMutex fuer Core 1 veroeffentlicht.
    SemaphoreHandle_t f1LiveMutex = nullptr;
    F1LiveBoard f1LiveBoardPublished;
    uint32_t f1LiveVersionCounter = 0;

    struct F1MergeEntry {
        int16_t number = -1;
        String acronym;
        String teamName;
        uint16_t teamColour565 = 0xFFFF;
        uint8_t position = 0;
        time_t lastPositionEpoch = 0;
        bool hasGapSeconds = false;
        float gapSeconds = 0;
        bool hasGapLaps = false;
        int16_t gapLaps = 0;
        time_t lastGapEpoch = 0;
        bool hasLapTime = false;
        float lapTimeSec = 0;
        time_t lastLapEpoch = 0;
    };
    F1MergeEntry f1MergeEntries[F1Client::MAX_LIVE_DRIVERS];
    uint8_t f1MergeEntryCount = 0;
    int32_t f1CurrentSessionKey = -1;
    String f1CurrentSessionName;

    int8_t findOrAddF1MergeEntry(int16_t number) {
        for (uint8_t i = 0; i < f1MergeEntryCount; i++) {
            if (f1MergeEntries[i].number == number) return (int8_t)i;
        }
        if (f1MergeEntryCount >= F1Client::MAX_LIVE_DRIVERS) return -1;
        f1MergeEntries[f1MergeEntryCount] = F1MergeEntry{};
        f1MergeEntries[f1MergeEntryCount].number = number;
        return (int8_t)(f1MergeEntryCount++);
    }

    // Veroeffentlicht den aktuellen Merge-Zustand als sortierte Rangliste
    // (Fahrer OHNE bekannte Position landen hinten, Reihenfolge darunter
    // beliebig - einfache Einfuege-Sortierung reicht, F1_LIVE_MAX_ROWS ist
    // mit max. 26 Eintraegen winzig).
    void publishF1LiveBoard(bool live) {
        F1LiveBoard board;
        board.live = live;
        board.sessionName = f1CurrentSessionName;
        board.rowCount = f1MergeEntryCount;
        for (uint8_t i = 0; i < f1MergeEntryCount; i++) {
            F1LiveRow& row = board.rows[i];
            const F1MergeEntry& e = f1MergeEntries[i];
            row.driverNumber = e.number;
            row.acronym = e.acronym;
            row.teamName = e.teamName;
            row.teamColour565 = e.teamColour565;
            row.position = e.position;
            row.hasGapSeconds = e.hasGapSeconds;
            row.gapSeconds = e.gapSeconds;
            row.hasGapLaps = e.hasGapLaps;
            row.gapLaps = e.gapLaps;
            row.hasLapTime = e.hasLapTime;
            row.lapTimeSec = e.lapTimeSec;
        }
        // Einfache Einfuege-Sortierung nach Position (0 = unbekannt, sortiert
        // ans Ende) - bewusst kein std::sort, um keine neue STL-Abhaengigkeit
        // fuer dieses eine kleine Array einzufuehren (Projekt-Konvention,
        // reine C-Schleifen wie ueberall sonst im Code).
        for (uint8_t i = 1; i < board.rowCount; i++) {
            F1LiveRow key = board.rows[i];
            uint8_t keyRank = (key.position == 0) ? 255 : key.position;
            int16_t j = (int16_t)(i - 1);
            while (j >= 0) {
                uint8_t jRank = (board.rows[j].position == 0) ? 255 : board.rows[j].position;
                if (jRank <= keyRank) break;
                board.rows[j + 1] = board.rows[j];
                j--;
            }
            board.rows[j + 1] = key;
        }

        xSemaphoreTake(f1LiveMutex, portMAX_DELAY);
        f1LiveBoardPublished = board;
        f1LiveVersionCounter++;
        xSemaphoreGive(f1LiveMutex);
    }

    // Krypto-Ticker (crypto_client.h) - EIN Mutex/Struct fuer btc/eth/
    // markets/mempool (gemeinsames Intervall) UND den Fear&Greed-Index
    // (eigenes, selteneres Intervall) - zwei getrennte Versionszaehler,
    // da beide Gruppen unabhaengig voneinander aktualisiert werden,
    // siehe net_task.h-Kommentar.
    SemaphoreHandle_t cryptoMutex = nullptr;
    CryptoData cryptoData;
    uint32_t cryptoVersionCounter = 0;
    std::atomic<uint8_t> cryptoStatusValue{(uint8_t)Status::Pending};
    uint32_t cryptoFngVersionCounter = 0;
    std::atomic<uint8_t> cryptoFngStatusValue{(uint8_t)Status::Pending};

    // Standortabhaengige Waehrung (Alex' Auftrag, 07.10., siehe
    // currency.h) - EIGENER kleiner Mutex statt cryptoMutex oben
    // (unterschiedliche Aktualisierungsfrequenz: aendert sich nur, wenn
    // CryptoScreen::enter() eine geaenderte LocationPresets::countryCode()
    // sieht, nicht bei jedem Fetch-Zyklus).
    SemaphoreHandle_t cryptoCurrencyMutex = nullptr;
    char cryptoCurrencyBuf[8] = "usd";

    // Erdbeben-Screen (earthquake_client.h) - einfaches Status/Version-
    // Paar, gleiches Grundmuster wie Wetter/Rakete oben.
    SemaphoreHandle_t earthquakeMutex = nullptr;
    EarthquakeData earthquakeData;
    uint32_t earthquakeVersionCounter = 0;
    std::atomic<uint8_t> earthquakeStatusValue{(uint8_t)Status::Pending};

    // Kalender-Screen (holiday_client.h, Alex' Auftrag, 12.10.) - gleiches
    // Grundmuster wie Erdbeben oben, plus ein eigener kleiner Mutex fuer
    // den vom Screen vorgegebenen Laendercode (gleiches Prinzip wie
    // cryptoCurrencyMutex oben).
    SemaphoreHandle_t holidayMutex = nullptr;
    HolidayData holidayData;
    uint32_t holidayVersionCounter = 0;
    std::atomic<uint8_t> holidayStatusValue{(uint8_t)Status::Pending};
    SemaphoreHandle_t holidayCountryMutex = nullptr;
    char holidayCountryBuf[3] = {0};

    // Naturereignisse-Screen (natural_events_client.h, Alex' Auftrag,
    // 12.10.) - einfaches Status/Version-Paar, gleiches Grundmuster wie
    // Erdbeben oben (rein zeitgesteuert, kein Standort-Zusatzzustand noetig).
    SemaphoreHandle_t naturalEventsMutex = nullptr;
    NaturalEventsData naturalEventsData;
    uint32_t naturalEventsVersionCounter = 0;
    std::atomic<uint8_t> naturalEventsStatusValue{(uint8_t)Status::Pending};
    std::atomic<bool> naturalEventsForceRefetch{false};

    TaskHandle_t taskHandle = nullptr;

    // Laeuft auf Core 0 (siehe begin()) - fasst NUR Netzwerk/Mutex an,
    // NIE das Display (siehe net_task.h-Kommentar).
    void taskFunc(void*) {
        uint32_t lastIssFetch = 0;
        uint32_t lastRocketFetch = 0;
        uint32_t lastTransitFetch = 0;
        uint32_t lastWeatherFetch = 0;
        uint32_t lastF1SeasonFetch = 0;
        uint32_t lastF1SessionCheck = 0;
        uint32_t lastF1LivePoll = 0;
        uint32_t lastCryptoFetch = 0;
        // Letzte tatsaechlich abgerufene Waehrung (Alex' Auftrag, 12.10.:
        // ein Waehrungswechsel - manuell oder durch neu nachgeladenen
        // Laendercode - soll NICHT erst das naechste reguläre 2,5-Minuten-
        // Intervall abwarten, siehe unten) - leer bis zum ersten Abruf.
        char lastFetchedCryptoCurrency[8] = "";
        uint32_t lastCryptoFngFetch = 0;
        uint32_t lastEarthquakeFetch = 0;
        uint32_t lastHolidayFetch = 0;
        uint32_t lastNaturalEventsFetch = 0;
        // Letztes tatsaechlich abgerufenes Land+Jahr (Alex' Auftrag,
        // 12.10.) - ein geaenderter Laendercode (neu nachgeladener
        // countryCode, siehe address_search_screen.h) ODER ein
        // Jahreswechsel (Geraet laeuft ueber Silvester durch) soll NICHT
        // erst das reguläre 24h-Intervall abwarten, gleiches Prinzip wie
        // bei der Krypto-Waehrung oben.
        char lastFetchedHolidayCountry[3] = "";
        int lastFetchedHolidayYear = 0;
        bool issFetchedOnce = false;
        bool rocketFetchedOnce = false;
        bool transitFetchedOnce = false;
        bool weatherFetchedOnce = false;
        bool f1SeasonFetchedOnce = false;
        bool f1SessionCheckedOnce = false;
        bool cryptoFetchedOnce = false;
        bool cryptoFngFetchedOnce = false;
        bool earthquakeFetchedOnce = false;
        bool holidayFetchedOnce = false;
        bool naturalEventsFetchedOnce = false;

        for (;;) {
            uint32_t now = millis();

            if (!issFetchedOnce || now - lastIssFetch >= Config::ISS_REFETCH_INTERVAL_MS) {
                issFetchedOnce = true;
                lastIssFetch = millis();

                IssClient::Position pos;
                if (IssClient::fetch(pos)) {
                    xSemaphoreTake(issMutex, portMAX_DELAY);
                    issPosition = pos;
                    issVersionCounter++;
                    xSemaphoreGive(issMutex);
                    issStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (issStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    issStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            if (!rocketFetchedOnce || now - lastRocketFetch >= Config::ROCKET_REFETCH_INTERVAL_MS ||
                rocketForceRefetch.exchange(false)) {
                rocketFetchedOnce = true;
                lastRocketFetch = millis();

                RocketClient::Launch launches[Config::ROCKET_MAX_LAUNCHES];
                uint8_t launchCount = 0;
                if (RocketClient::fetch(launches, Config::ROCKET_MAX_LAUNCHES, launchCount)) {
                    promoteNextFutureLaunch(launches, launchCount);
                    xSemaphoreTake(rocketMutex, portMAX_DELAY);
                    for (uint8_t i = 0; i < launchCount; i++) rocketData.launches[i] = launches[i];
                    rocketData.launchCount = launchCount;
                    rocketVersionCounter++;
                    xSemaphoreGive(rocketMutex);
                    rocketStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (rocketStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    rocketStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // OEPNV: zwei Faelle loesen einen Abruf aus - (1) die Position
            // hat sich geaendert (oder ist zum allerersten Mal bekannt,
            // transitLocationDirty), dann IMMER zuerst ein frischer
            // Reverse-Geocode gefolgt von Abfahrten fuer den neuen Stop;
            // (2) der normale Zeit-Refresh (nur Abfahrten, derselbe
            // bereits bekannte Stop - kein erneuter Reverse-Geocode noetig,
            // siehe CLAUDE.md/Alex' Auftrag).
            bool locationDirty = transitLocationDirty.load(std::memory_order_relaxed);
            bool timeForRefresh = !transitFetchedOnce || now - lastTransitFetch >= Config::TRANSIT_REFETCH_INTERVAL_MS;

            if (transitLocationKnown && (locationDirty || timeForRefresh)) {
                transitFetchedOnce = true;
                lastTransitFetch = millis();
                bool stopLookupFailed = false;

                if (locationDirty) {
                    transitLocationDirty.store(false, std::memory_order_relaxed);

                    double lat, lon;
                    xSemaphoreTake(transitLocationMutex, portMAX_DELAY);
                    lat = transitLat;
                    lon = transitLon;
                    xSemaphoreGive(transitLocationMutex);

                    String stopId, stopName;
                    TransitClient::StopResult stopResult = TransitClient::findNearestStop(lat, lon, stopId, stopName);
                    if (stopResult != TransitClient::StopResult::Ok) {
                        stopLookupFailed = true;
                        cachedStopId = "";
                        transitErrorValue.store((uint8_t)(stopResult == TransitClient::StopResult::NetworkError
                            ? TransitError::NetworkError : TransitError::NoStopFound), std::memory_order_relaxed);
                        if (transitStatusValue.load(std::memory_order_relaxed) != (uint8_t)Status::Ok) {
                            transitStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                        }
                    } else {
                        cachedStopId = stopId;

                        xSemaphoreTake(transitMutex, portMAX_DELAY);
                        transitData.stopName = stopName;
                        xSemaphoreGive(transitMutex);
                    }
                }

                if (!stopLookupFailed && cachedStopId.length() > 0) {
                    TransitClient::Departure departures[Config::TRANSIT_MAX_DEPARTURES];
                    uint8_t count = 0;
                    TransitClient::DeparturesResult depResult = TransitClient::fetchDepartures(
                        cachedStopId, departures, Config::TRANSIT_MAX_DEPARTURES, count);

                    if (depResult == TransitClient::DeparturesResult::Ok) {
                        xSemaphoreTake(transitMutex, portMAX_DELAY);
                        for (uint8_t i = 0; i < count; i++) transitData.departures[i] = departures[i];
                        transitData.departureCount = count;
                        transitVersionCounter++;
                        xSemaphoreGive(transitMutex);
                        transitErrorValue.store((uint8_t)TransitError::None, std::memory_order_relaxed);
                        transitStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                    } else if (transitStatusValue.load(std::memory_order_relaxed) != (uint8_t)Status::Ok) {
                        transitErrorValue.store((uint8_t)(depResult == TransitClient::DeparturesResult::NetworkError
                            ? TransitError::NetworkError : TransitError::NoDepartures), std::memory_order_relaxed);
                        transitStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                    }
                }
            }

            // Wetter: einfacher als OEPNV - kein Zwischenschritt, Open-Meteo
            // braucht direkt die Koordinaten (siehe setWeatherLocation()).
            bool weatherLocationDirtyNow = weatherLocationDirty.load(std::memory_order_relaxed);
            bool weatherTimeForRefresh = !weatherFetchedOnce ||
                now - lastWeatherFetch >= Config::WEATHER_REFETCH_INTERVAL_MS;

            if (weatherLocationKnown && (weatherLocationDirtyNow || weatherTimeForRefresh)) {
                weatherFetchedOnce = true;
                lastWeatherFetch = millis();
                if (weatherLocationDirtyNow) weatherLocationDirty.store(false, std::memory_order_relaxed);

                double wLat, wLon;
                xSemaphoreTake(weatherLocationMutex, portMAX_DELAY);
                wLat = weatherLat;
                wLon = weatherLon;
                xSemaphoreGive(weatherLocationMutex);

                WeatherClient::Forecast forecast;
                WeatherClient::FetchResult wResult = WeatherClient::fetch(wLat, wLon, forecast);
                if (wResult == WeatherClient::FetchResult::Ok) {
                    xSemaphoreTake(weatherMutex, portMAX_DELAY);
                    weatherData = forecast;
                    weatherVersionCounter++;
                    xSemaphoreGive(weatherMutex);
                    weatherStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (weatherStatusValue.load(std::memory_order_relaxed) != (uint8_t)Status::Ok) {
                    weatherStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Formel 1, Saison-Daten (Kalender/Wertungen/letztes Ergebnis) -
            // rein zeitgesteuert, keine Standort-Abhaengigkeit.
            if (!f1SeasonFetchedOnce || now - lastF1SeasonFetch >= Config::F1_SEASON_REFETCH_INTERVAL_MS) {
                f1SeasonFetchedOnce = true;
                lastF1SeasonFetch = millis();

                F1Client::SeasonData data;
                if (F1Client::fetchSeasonData(data)) {
                    xSemaphoreTake(f1SeasonMutex, portMAX_DELAY);
                    f1SeasonData = data;
                    f1SeasonVersionCounter++;
                    xSemaphoreGive(f1SeasonMutex);
                    f1SeasonStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (f1SeasonStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    f1SeasonStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Formel 1, Live-Session-Erkennung (billiger Einzelabruf, auch
            // wenn gerade keine Session laeuft - siehe Config::
            // F1_SESSION_CHECK_INTERVAL_MS). Bei (neu erkannter) laufender
            // Session wird sofort auch die Fahrerliste fuer diese Session
            // nachgeladen (einmalig pro Session, aendert sich waehrend der
            // Session nicht) - die 400ms-Pausen zwischen den OpenF1-Aufrufen
            // halten das live bestaetigte Rate-Limit von 3 Anfragen/Sekunde
            // sicher ein (siehe CLAUDE.md "Formel 1").
            if (!f1SessionCheckedOnce || now - lastF1SessionCheck >= Config::F1_SESSION_CHECK_INTERVAL_MS) {
                f1SessionCheckedOnce = true;
                lastF1SessionCheck = millis();

                F1Client::LiveSessionInfo info;
                bool haveInfo = F1Client::fetchLatestSessionInfo(info);
                time_t nowUtc = time(nullptr);
                bool isLive = haveInfo && info.valid && info.dateEnd > 0 &&
                              nowUtc >= info.dateStart && nowUtc <= info.dateEnd;

                // [F1Diag] Alex' Auftrag, 12.10. - dauerhaftes (nicht nur
                // einmaliges) Logging um die isLive-Entscheidung, um die
                // Ursache fuer den am 11.10. live beobachteten Fehlfall
                // ("Rennstart in -0 Tage 00:00:00" trotz laufendem Rennen)
                // beim naechsten echten Rennen per seriellem Monitor
                // nachvollziehen zu koennen. Bewusst bei JEDER Auswertung
                // (alle F1_SESSION_CHECK_INTERVAL_MS=60s), nicht nur beim
                // Boot - der Fehler trat waehrend einer laufenden Session
                // auf, nicht beim Start. Aendert am Screen selbst nichts.
                Serial.printf("[F1Diag] isLive=%d haveInfo=%d valid=%d sessionKey=%ld "
                               "dateStartRaw=\"%s\" dateEndRaw=\"%s\" dateStart=%ld dateEnd=%ld "
                               "nowUtc=%ld f1CurrentSessionKey=%ld\n",
                               isLive, haveInfo, info.valid, (long)info.sessionKey,
                               info.dateStartRaw.c_str(), info.dateEndRaw.c_str(),
                               (long)info.dateStart, (long)info.dateEnd,
                               (long)nowUtc, (long)f1CurrentSessionKey);

                if (isLive && info.sessionKey != f1CurrentSessionKey) {
                    // Neue (oder erstmals erkannte) laufende Session -
                    // Merge-Tabelle zuruecksetzen und Fahrerliste frisch laden.
                    f1MergeEntryCount = 0;
                    f1CurrentSessionKey = info.sessionKey;
                    f1CurrentSessionName = info.sessionName;

                    vTaskDelay(pdMS_TO_TICKS(400));
                    F1Client::LiveDriver drivers[F1Client::MAX_LIVE_DRIVERS];
                    uint8_t driverCount = 0;
                    if (F1Client::fetchLiveDrivers(info.sessionKey, drivers, F1Client::MAX_LIVE_DRIVERS, driverCount)) {
                        for (uint8_t i = 0; i < driverCount; i++) {
                            int8_t idx = findOrAddF1MergeEntry(drivers[i].number);
                            if (idx < 0) continue;
                            f1MergeEntries[idx].acronym = drivers[i].acronym;
                            f1MergeEntries[idx].teamName = drivers[i].teamName;
                            f1MergeEntries[idx].teamColour565 = drivers[i].teamColour565;
                        }
                    }
                    publishF1LiveBoard(true);
                    lastF1LivePoll = millis(); // sofortiger erster Live-Poll gleich unten faellig
                } else if (!isLive && f1CurrentSessionKey >= 0) {
                    // Session zu Ende (oder date_end liegt in der
                    // Vergangenheit) - Merge-Tabelle verwerfen, naechste
                    // Session startet wieder bei Null.
                    f1MergeEntryCount = 0;
                    f1CurrentSessionKey = -1;
                    f1CurrentSessionName = "";
                    publishF1LiveBoard(false);
                } else if (!isLive && f1LiveVersionCounter == 0) {
                    // Allererster Check ueberhaupt, keine Session live -
                    // einmalig "kein Live-Betrieb" veroeffentlichen, damit
                    // der Screen nicht dauerhaft auf "unbekannt" haengt.
                    publishF1LiveBoard(false);
                }
            }

            // Formel 1, Live-Rangliste - NUR waehrend einer tatsaechlich
            // laufenden Session, siehe f1_client.h/.cpp-Kommentar fuer die
            // Begruendung der Zeitfenster-Abfrage (volle Session-Historie
            // ist live bestaetigt teils mehrere MB gross).
            if (f1CurrentSessionKey >= 0 && now - lastF1LivePoll >= Config::F1_LIVE_POLL_INTERVAL_MS) {
                lastF1LivePoll = millis();
                time_t nowUtc = time(nullptr);

                F1Client::PositionUpdate posUpdates[32];
                uint8_t posCount = 0;
                F1Client::fetchPositionUpdates(f1CurrentSessionKey,
                    (time_t)(nowUtc - Config::F1_LIVE_POSITION_LOOKBACK_SEC), posUpdates, 32, posCount);
                for (uint8_t i = 0; i < posCount; i++) {
                    int8_t idx = findOrAddF1MergeEntry(posUpdates[i].driverNumber);
                    if (idx < 0) continue;
                    if (posUpdates[i].epoch >= f1MergeEntries[idx].lastPositionEpoch) {
                        f1MergeEntries[idx].position = posUpdates[i].position;
                        f1MergeEntries[idx].lastPositionEpoch = posUpdates[i].epoch;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(400));

                F1Client::IntervalUpdate ivUpdates[64];
                uint8_t ivCount = 0;
                F1Client::fetchIntervalUpdates(f1CurrentSessionKey,
                    (time_t)(nowUtc - Config::F1_LIVE_INTERVAL_LOOKBACK_SEC), ivUpdates, 64, ivCount);
                for (uint8_t i = 0; i < ivCount; i++) {
                    int8_t idx = findOrAddF1MergeEntry(ivUpdates[i].driverNumber);
                    if (idx < 0) continue;
                    if (ivUpdates[i].epoch >= f1MergeEntries[idx].lastGapEpoch) {
                        f1MergeEntries[idx].hasGapSeconds = ivUpdates[i].hasGapSeconds;
                        f1MergeEntries[idx].gapSeconds = ivUpdates[i].gapSeconds;
                        f1MergeEntries[idx].hasGapLaps = ivUpdates[i].hasGapLaps;
                        f1MergeEntries[idx].gapLaps = ivUpdates[i].gapLaps;
                        f1MergeEntries[idx].lastGapEpoch = ivUpdates[i].epoch;
                    }
                }

                vTaskDelay(pdMS_TO_TICKS(400));

                // Rundenzeiten (Alex' Auftrag, 06.10.) - gleicher Abruf-
                // Zyklus wie Position/Abstaende oben, keine eigene
                // Frequenz.
                F1Client::LapUpdate lapUpdates[32];
                uint8_t lapCount = 0;
                F1Client::fetchLapUpdates(f1CurrentSessionKey,
                    (time_t)(nowUtc - Config::F1_LIVE_LAP_LOOKBACK_SEC), lapUpdates, 32, lapCount);
                for (uint8_t i = 0; i < lapCount; i++) {
                    int8_t idx = findOrAddF1MergeEntry(lapUpdates[i].driverNumber);
                    if (idx < 0) continue;
                    if (lapUpdates[i].epoch >= f1MergeEntries[idx].lastLapEpoch) {
                        f1MergeEntries[idx].hasLapTime = true;
                        f1MergeEntries[idx].lapTimeSec = lapUpdates[i].lapDurationSec;
                        f1MergeEntries[idx].lastLapEpoch = lapUpdates[i].epoch;
                    }
                }

                publishF1LiveBoard(true);
            }

            // Krypto-Ticker: btc/eth/markets/mempool, EIN gemeinsames
            // Intervall (Alex' Auftrag: "alle 2-3 Minuten"). Jede der drei
            // Teilquellen wird EINZELN abgerufen und schlaegt unabhaengig
            // von den anderen fehl - cryptoVersion() erhoeht sich bereits,
            // wenn auch nur eine davon erfolgreich war, die jeweils
            // anderen Felder behalten dann ihren zuletzt bekannten Wert.
            char currentCryptoCurrency[8];
            xSemaphoreTake(cryptoCurrencyMutex, portMAX_DELAY);
            strncpy(currentCryptoCurrency, cryptoCurrencyBuf, sizeof(currentCryptoCurrency));
            xSemaphoreGive(cryptoCurrencyMutex);
            bool cryptoCurrencyChanged =
                cryptoFetchedOnce && strcmp(currentCryptoCurrency, lastFetchedCryptoCurrency) != 0;

            if (!cryptoFetchedOnce || cryptoCurrencyChanged ||
                now - lastCryptoFetch >= Config::CRYPTO_REFETCH_INTERVAL_MS) {
                cryptoFetchedOnce = true;
                lastCryptoFetch = millis();

                char currency[8];
                strncpy(currency, currentCryptoCurrency, sizeof(currency));

                CryptoClient::MainCoin btc, eth;
                bool pricesOk = CryptoClient::fetchMainPrices(currency, btc, eth);

                CryptoClient::CoinMarket markets[Config::CRYPTO_MAX_MARKETS];
                uint8_t marketCount = 0;
                bool marketsOk = CryptoClient::fetchMarkets(currency, markets, Config::CRYPTO_MAX_MARKETS, marketCount);

                CryptoClient::Mempool mempool;
                bool mempoolOk = CryptoClient::fetchMempool(mempool);

                if (pricesOk || marketsOk || mempoolOk) {
                    xSemaphoreTake(cryptoMutex, portMAX_DELAY);
                    if (pricesOk) {
                        cryptoData.btc = btc;
                        cryptoData.eth = eth;
                        strncpy(cryptoData.vsCurrency, currency, sizeof(cryptoData.vsCurrency));
                        // BUGFIX (13.10., Alex' Fund: "Automatisch bleibt
                        // beim eingestellten Wert") - lastFetchedCryptoCurrency
                        // wurde bisher IMMER gesetzt, auch wenn pricesOk
                        // fehlschlug (z.B. CoinGecko-Rate-Limit). Dadurch
                        // verlor ein fehlgeschlagener Waehrungswechsel
                        // seinen "sofort nochmal versuchen"-Trigger
                        // (cryptoCurrencyChanged) und wartete bis zu 2,5
                        // Minuten (normales Intervall) auf den naechsten
                        // Versuch. Jetzt nur bei TATSAECHLICHEM Erfolg
                        // aktualisiert - ein fehlgeschlagener Wechsel wird
                        // dadurch bei jedem naechsten NetTask-Durchlauf
                        // (Sekundentakt) sofort wiederholt, bis er klappt.
                        strncpy(lastFetchedCryptoCurrency, currency, sizeof(lastFetchedCryptoCurrency));
                    }
                    if (marketsOk) {
                        for (uint8_t i = 0; i < marketCount; i++) cryptoData.markets[i] = markets[i];
                        cryptoData.marketCount = marketCount;
                    }
                    if (mempoolOk) cryptoData.mempool = mempool;
                    cryptoVersionCounter++;
                    xSemaphoreGive(cryptoMutex);
                    cryptoStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (cryptoStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    cryptoStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Krypto-Ticker: Fear&Greed-Index - EIGENES, deutlich
            // selteneres Intervall (aktualisiert sich laut API ohnehin nur
            // einmal taeglich, siehe Config::CRYPTO_FNG_REFETCH_INTERVAL_MS).
            if (!cryptoFngFetchedOnce || now - lastCryptoFngFetch >= Config::CRYPTO_FNG_REFETCH_INTERVAL_MS) {
                cryptoFngFetchedOnce = true;
                lastCryptoFngFetch = millis();

                CryptoClient::FearGreed fng;
                if (CryptoClient::fetchFearGreed(fng)) {
                    xSemaphoreTake(cryptoMutex, portMAX_DELAY);
                    cryptoData.fearGreed = fng;
                    cryptoFngVersionCounter++;
                    xSemaphoreGive(cryptoMutex);
                    cryptoFngStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (cryptoFngStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    cryptoFngStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Erdbeben-Screen - rein zeitgesteuert, keine Standort-
            // Abhaengigkeit (die USGS-Abfrage ist weltweit).
            if (!earthquakeFetchedOnce || now - lastEarthquakeFetch >= Config::EARTHQUAKE_REFETCH_INTERVAL_MS) {
                earthquakeFetchedOnce = true;
                lastEarthquakeFetch = millis();

                EarthquakeClient::Quake quakes[Config::EARTHQUAKE_MAX_QUAKES];
                uint8_t quakeCount = 0;
                if (EarthquakeClient::fetch(quakes, Config::EARTHQUAKE_MAX_QUAKES, quakeCount)) {
                    xSemaphoreTake(earthquakeMutex, portMAX_DELAY);
                    for (uint8_t i = 0; i < quakeCount; i++) earthquakeData.quakes[i] = quakes[i];
                    earthquakeData.quakeCount = quakeCount;
                    earthquakeVersionCounter++;
                    xSemaphoreGive(earthquakeMutex);
                    earthquakeStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (earthquakeStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    earthquakeStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Kalender-Screen - Feiertage, einmal pro Tag (siehe
            // Config::HOLIDAY_REFETCH_INTERVAL_MS), PLUS sofortiger
            // Neuabruf bei geaendertem Laendercode oder Jahreswechsel
            // (gleiches Prinzip wie die Krypto-Waehrung oben).
            {
                char currentHolidayCountry[3];
                xSemaphoreTake(holidayCountryMutex, portMAX_DELAY);
                strncpy(currentHolidayCountry, holidayCountryBuf, sizeof(currentHolidayCountry));
                xSemaphoreGive(holidayCountryMutex);

                if (currentHolidayCountry[0] != 0) {
                    time_t nowUtc = time(nullptr);
                    struct tm t;
                    gmtime_r(&nowUtc, &t);
                    int currentYear = t.tm_year + 1900;

                    bool holidayParamsChanged = holidayFetchedOnce &&
                        (strcmp(currentHolidayCountry, lastFetchedHolidayCountry) != 0 ||
                         currentYear != lastFetchedHolidayYear);

                    if (!holidayFetchedOnce || holidayParamsChanged ||
                        now - lastHolidayFetch >= Config::HOLIDAY_REFETCH_INTERVAL_MS) {
                        holidayFetchedOnce = true;
                        lastHolidayFetch = millis();
                        strncpy(lastFetchedHolidayCountry, currentHolidayCountry,
                                sizeof(lastFetchedHolidayCountry));
                        lastFetchedHolidayYear = currentYear;

                        HolidayClient::Holiday holidays[Config::HOLIDAY_MAX_COUNT];
                        uint8_t holidayCount = 0;
                        if (HolidayClient::fetch(currentHolidayCountry, currentYear, holidays,
                                                  Config::HOLIDAY_MAX_COUNT, holidayCount)) {
                            xSemaphoreTake(holidayMutex, portMAX_DELAY);
                            for (uint8_t i = 0; i < holidayCount; i++) holidayData.holidays[i] = holidays[i];
                            holidayData.holidayCount = holidayCount;
                            holidayVersionCounter++;
                            xSemaphoreGive(holidayMutex);
                            holidayStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                        } else if (holidayStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                            holidayStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                        }
                    }
                }
            }

            // Naturereignisse-Screen - rein zeitgesteuert, keine Standort-
            // Abhaengigkeit (die EONET-Abfrage ist weltweit).
            if (!naturalEventsFetchedOnce ||
                now - lastNaturalEventsFetch >= Config::NATURAL_EVENTS_REFETCH_INTERVAL_MS ||
                naturalEventsForceRefetch.exchange(false)) {
                naturalEventsFetchedOnce = true;
                lastNaturalEventsFetch = millis();

                NaturalEventsClient::Event events[Config::NATURAL_EVENTS_MAX_COUNT];
                uint8_t eventCount = 0;
                if (NaturalEventsClient::fetch(events, Config::NATURAL_EVENTS_MAX_COUNT, eventCount)) {
                    xSemaphoreTake(naturalEventsMutex, portMAX_DELAY);
                    for (uint8_t i = 0; i < eventCount; i++) naturalEventsData.events[i] = events[i];
                    naturalEventsData.eventCount = eventCount;
                    naturalEventsVersionCounter++;
                    xSemaphoreGive(naturalEventsMutex);
                    naturalEventsStatusValue.store((uint8_t)Status::Ok, std::memory_order_relaxed);
                } else if (naturalEventsStatusValue.load(std::memory_order_relaxed) == (uint8_t)Status::Pending) {
                    naturalEventsStatusValue.store((uint8_t)Status::Error, std::memory_order_relaxed);
                }
            }

            // Kurze Ruhepause zwischen den Pruefungen - kein busy loop,
            // laesst Core 0 fuer WLAN/TLS-Hintergrundarbeit Luft.
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
}

void begin() {
    issMutex = xSemaphoreCreateMutex();
    rocketMutex = xSemaphoreCreateMutex();
    transitMutex = xSemaphoreCreateMutex();
    transitLocationMutex = xSemaphoreCreateMutex();
    weatherMutex = xSemaphoreCreateMutex();
    weatherLocationMutex = xSemaphoreCreateMutex();
    f1SeasonMutex = xSemaphoreCreateMutex();
    f1LiveMutex = xSemaphoreCreateMutex();
    cryptoMutex = xSemaphoreCreateMutex();
    cryptoCurrencyMutex = xSemaphoreCreateMutex();
    earthquakeMutex = xSemaphoreCreateMutex();
    holidayMutex = xSemaphoreCreateMutex();
    holidayCountryMutex = xSemaphoreCreateMutex();
    naturalEventsMutex = xSemaphoreCreateMutex();

    // Stack-Groesse (20480 Byte) 1:1 von Flightradars bewaehrtem NetTask
    // uebernommen - TLS-Handshakes (mbedTLS RSA/BIGNUM-Operationen)
    // brauchen substanziellen Stack, unabhaengig von der eigentlichen
    // Payload-Groesse.
    xTaskCreatePinnedToCore(
        taskFunc,
        "NetTask",
        20480,
        nullptr,
        1,
        &taskHandle,
        0
    );
}

Status issStatus() { return (Status)issStatusValue.load(std::memory_order_relaxed); }
uint32_t issVersion() { return issVersionCounter; }

bool getIssPosition(IssClient::Position& out) {
    if (issStatus() != Status::Ok) return false;
    xSemaphoreTake(issMutex, portMAX_DELAY);
    out = issPosition;
    xSemaphoreGive(issMutex);
    return true;
}

Status rocketStatus() { return (Status)rocketStatusValue.load(std::memory_order_relaxed); }
uint32_t rocketVersion() { return rocketVersionCounter; }

bool getRocketData(RocketData& out) {
    if (rocketStatus() != Status::Ok) return false;
    xSemaphoreTake(rocketMutex, portMAX_DELAY);
    out = rocketData;
    xSemaphoreGive(rocketMutex);
    return true;
}

void forceRocketRefetch() { rocketForceRefetch.store(true); }

void setTransitLocation(double lat, double lon) {
    xSemaphoreTake(transitLocationMutex, portMAX_DELAY);
    bool changed = !transitLocationKnown || lat != transitLat || lon != transitLon;
    transitLat = lat;
    transitLon = lon;
    transitLocationKnown = true;
    xSemaphoreGive(transitLocationMutex);
    if (changed) transitLocationDirty.store(true, std::memory_order_relaxed);
}

Status transitStatus() { return (Status)transitStatusValue.load(std::memory_order_relaxed); }
TransitError transitError() { return (TransitError)transitErrorValue.load(std::memory_order_relaxed); }
uint32_t transitVersion() { return transitVersionCounter; }

bool getTransitData(TransitData& out) {
    if (transitStatus() != Status::Ok) return false;
    xSemaphoreTake(transitMutex, portMAX_DELAY);
    out = transitData;
    xSemaphoreGive(transitMutex);
    return true;
}

void setWeatherLocation(double lat, double lon) {
    xSemaphoreTake(weatherLocationMutex, portMAX_DELAY);
    bool changed = !weatherLocationKnown || lat != weatherLat || lon != weatherLon;
    weatherLat = lat;
    weatherLon = lon;
    weatherLocationKnown = true;
    xSemaphoreGive(weatherLocationMutex);
    if (changed) weatherLocationDirty.store(true, std::memory_order_relaxed);
}

Status weatherStatus() { return (Status)weatherStatusValue.load(std::memory_order_relaxed); }
uint32_t weatherVersion() { return weatherVersionCounter; }

bool getWeatherData(WeatherClient::Forecast& out) {
    if (weatherStatus() != Status::Ok) return false;
    xSemaphoreTake(weatherMutex, portMAX_DELAY);
    out = weatherData;
    xSemaphoreGive(weatherMutex);
    return true;
}

Status f1SeasonStatus() { return (Status)f1SeasonStatusValue.load(std::memory_order_relaxed); }
uint32_t f1SeasonVersion() { return f1SeasonVersionCounter; }

bool getF1SeasonData(F1Client::SeasonData& out) {
    if (f1SeasonStatus() != Status::Ok) return false;
    xSemaphoreTake(f1SeasonMutex, portMAX_DELAY);
    out = f1SeasonData;
    xSemaphoreGive(f1SeasonMutex);
    return true;
}

uint32_t f1LiveVersion() { return f1LiveVersionCounter; }

bool getF1LiveBoard(F1LiveBoard& out) {
    if (f1LiveVersionCounter == 0) return false;
    xSemaphoreTake(f1LiveMutex, portMAX_DELAY);
    out = f1LiveBoardPublished;
    xSemaphoreGive(f1LiveMutex);
    return true;
}

void setCryptoCurrency(const char* vsCurrency) {
    xSemaphoreTake(cryptoCurrencyMutex, portMAX_DELAY);
    strncpy(cryptoCurrencyBuf, vsCurrency, sizeof(cryptoCurrencyBuf) - 1);
    cryptoCurrencyBuf[sizeof(cryptoCurrencyBuf) - 1] = 0;
    xSemaphoreGive(cryptoCurrencyMutex);
}

Status cryptoStatus() { return (Status)cryptoStatusValue.load(std::memory_order_relaxed); }
uint32_t cryptoVersion() { return cryptoVersionCounter; }

// Liefert die Daten, sobald AUCH NUR EINE der beiden unabhaengigen
// Gruppen (btc/eth/markets/mempool ODER Fear&Greed) jemals erfolgreich
// war - nicht erst, wenn beide vorliegen (gleiches Prinzip wie
// getF1LiveBoard() oben, das ebenfalls nur auf seinen eigenen
// Versionszaehler statt auf Status prueft).
bool getCryptoData(CryptoData& out) {
    if (cryptoVersionCounter == 0 && cryptoFngVersionCounter == 0) return false;
    xSemaphoreTake(cryptoMutex, portMAX_DELAY);
    out = cryptoData;
    xSemaphoreGive(cryptoMutex);
    return true;
}

Status cryptoFngStatus() { return (Status)cryptoFngStatusValue.load(std::memory_order_relaxed); }
uint32_t cryptoFngVersion() { return cryptoFngVersionCounter; }

Status earthquakeStatus() { return (Status)earthquakeStatusValue.load(std::memory_order_relaxed); }
uint32_t earthquakeVersion() { return earthquakeVersionCounter; }

bool getEarthquakeData(EarthquakeData& out) {
    if (earthquakeStatus() != Status::Ok) return false;
    xSemaphoreTake(earthquakeMutex, portMAX_DELAY);
    out = earthquakeData;
    xSemaphoreGive(earthquakeMutex);
    return true;
}

void setHolidayCountry(const char* countryCode) {
    xSemaphoreTake(holidayCountryMutex, portMAX_DELAY);
    strncpy(holidayCountryBuf, countryCode, sizeof(holidayCountryBuf) - 1);
    holidayCountryBuf[sizeof(holidayCountryBuf) - 1] = 0;
    xSemaphoreGive(holidayCountryMutex);
}

Status holidayStatus() { return (Status)holidayStatusValue.load(std::memory_order_relaxed); }
uint32_t holidayVersion() { return holidayVersionCounter; }

bool getHolidayData(HolidayData& out) {
    if (holidayStatus() != Status::Ok) return false;
    xSemaphoreTake(holidayMutex, portMAX_DELAY);
    out = holidayData;
    xSemaphoreGive(holidayMutex);
    return true;
}

Status naturalEventsStatus() { return (Status)naturalEventsStatusValue.load(std::memory_order_relaxed); }
uint32_t naturalEventsVersion() { return naturalEventsVersionCounter; }

bool getNaturalEventsData(NaturalEventsData& out) {
    if (naturalEventsStatus() != Status::Ok) return false;
    xSemaphoreTake(naturalEventsMutex, portMAX_DELAY);
    out = naturalEventsData;
    xSemaphoreGive(naturalEventsMutex);
    return true;
}

void forceNaturalEventsRefetch() { naturalEventsForceRefetch.store(true); }

}
