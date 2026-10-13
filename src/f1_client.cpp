#include "f1_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace F1Client {

namespace {
    // Tage seit 1970-01-01 - 1:1 aus rocket_client.cpp/transit_client.cpp
    // uebernommen, lokal dupliziert (Projekt-Konvention, siehe CLAUDE.md).
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    // Parst "YYYY-MM-DDTHH:MM:SS..." (ISO-8601, egal ob mit "Z"-Suffix wie
    // bei Jolpica oder mit "+00:00"-Suffix wie bei OpenF1 - sscanf() stoppt
    // einfach nach den 6 Zahlen, der Rest wird ignoriert) zu einem
    // Unix-Zeitstempel. 0, wenn das Format nicht passt.
    time_t parseIso8601Utc(const char* iso) {
        int year, month, day, hour, minute, second;
        if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
            return 0;
        }
        long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
        return (time_t)(days * 86400L + hour * 3600L + minute * 60L + second);
    }

    // Jolpica liefert Datum ("YYYY-MM-DD") und Uhrzeit ("HH:MM:SSZ")
    // GETRENNT statt als ein ISO-8601-String (live bestaetigt, siehe
    // CLAUDE.md "Formel 1") - hier zusammengesetzt und ueber dieselbe
    // Funktion wie oben geparst.
    time_t parseJolpicaDateTime(const String& date, const String& time) {
        if (date.length() == 0 || time.length() == 0) return 0;
        String combined = date + "T" + time;
        return parseIso8601Utc(combined.c_str());
    }

    // Formatiert einen UTC-Zeitstempel zurueck in "YYYY-MM-DDTHH:MM:SS" fuer
    // OpenF1s "date>="-Query-Filter (umgekehrte Richtung zu oben) - gmtime_r()
    // ist dafuer bereits im Schwesterprojekt Eiswolfs Flightradar CYD
    // bewaehrt (dortige weather.cpp), das Geraet laeuft ohnehin nur in UTC
    // (main.cpp::setup(), configTime(0, 0, ...)).
    String formatIso8601Utc(time_t epoch) {
        struct tm t;
        gmtime_r(&epoch, &t);
        char buf[24];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
                 t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
        return String(buf);
    }

    bool httpGetHttps(const String& url, String& outBody) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;
        http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) { http.end(); return false; }
        outBody = http.getString();
        http.end();
        return true;
    }

    String jolpicaUrl(const char* path) {
        return "https://" + String(Config::F1_JOLPICA_HOST) + String(path);
    }

    // ---- Jolpica: naechstes Rennwochenende ----
    void fetchNextRace(NextRace& out) {
        String body;
        if (!httpGetHttps(jolpicaUrl(Config::F1_JOLPICA_NEXT_PATH), body)) return;

        JsonDocument filter;
        JsonObject race = filter["MRData"]["RaceTable"]["Races"][0].to<JsonObject>();
        race["raceName"] = true;
        race["date"] = true;
        race["time"] = true;
        race["Circuit"]["circuitName"] = true;
        race["Circuit"]["Location"]["locality"] = true;
        race["Circuit"]["Location"]["country"] = true;
        race["FirstPractice"] = true;
        race["SecondPractice"] = true;
        race["ThirdPractice"] = true;
        race["Qualifying"] = true;
        race["Sprint"] = true;
        race["SprintQualifying"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

        JsonArray races = doc["MRData"]["RaceTable"]["Races"].as<JsonArray>();
        if (races.isNull() || races.size() == 0) return;
        JsonObject r = races[0];

        out.raceName = String((const char*)(r["raceName"] | ""));
        out.circuitName = String((const char*)(r["Circuit"]["circuitName"] | ""));
        out.locality = String((const char*)(r["Circuit"]["Location"]["locality"] | ""));
        out.country = String((const char*)(r["Circuit"]["Location"]["country"] | ""));

        out.race.epoch = parseJolpicaDateTime(
            String((const char*)(r["date"] | "")), String((const char*)(r["time"] | "")));
        out.race.valid = out.race.epoch > 0;

        auto parseSub = [&](const char* key, SessionTime& st) {
            JsonObject sub = r[key];
            if (sub.isNull()) return;
            st.epoch = parseJolpicaDateTime(
                String((const char*)(sub["date"] | "")), String((const char*)(sub["time"] | "")));
            st.valid = st.epoch > 0;
        };
        parseSub("FirstPractice", out.firstPractice);
        parseSub("SecondPractice", out.secondPractice);
        parseSub("ThirdPractice", out.thirdPractice);
        parseSub("Qualifying", out.qualifying);
        parseSub("Sprint", out.sprint);
        parseSub("SprintQualifying", out.sprintQualifying);

        out.valid = out.raceName.length() > 0 && out.race.valid;
    }

    // ---- Jolpica: Fahrerwertung ----
    void fetchDriverStandings(SeasonData& out) {
        String body;
        if (!httpGetHttps(jolpicaUrl(Config::F1_JOLPICA_DRIVER_STANDINGS_PATH), body)) return;

        JsonDocument filter;
        JsonObject ds = filter["MRData"]["StandingsTable"]["StandingsLists"][0]["DriverStandings"][0].to<JsonObject>();
        ds["position"] = true;
        ds["points"] = true;
        ds["wins"] = true;
        ds["Driver"]["code"] = true;
        ds["Driver"]["givenName"] = true;
        ds["Driver"]["familyName"] = true;
        ds["Constructors"][0]["name"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

        JsonArray lists = doc["MRData"]["StandingsTable"]["StandingsLists"].as<JsonArray>();
        if (lists.isNull() || lists.size() == 0) return;
        JsonArray standings = lists[0]["DriverStandings"].as<JsonArray>();
        if (standings.isNull()) return;

        for (JsonObject s : standings) {
            if (out.driverStandingCount >= F1Client::MAX_STANDINGS) break;
            DriverStanding& d = out.driverStandings[out.driverStandingCount];
            // Jolpica liefert position/points/wins als JSON-STRINGS, nicht
            // als Zahlen (live bestaetigt, z.B. "points":"320") - deshalb
            // ueber String::toInt()/toFloat() geparst statt ArduinoJsons
            // "|"-Operator zu vertrauen (dessen String->Zahl-Koerzierung bei
            // dieser ArduinoJson-Version nicht sicher verifiziert ist).
            d.position = (uint8_t)String((const char*)(s["position"] | "0")).toInt();
            d.points = String((const char*)(s["points"] | "0")).toFloat();
            d.wins = (uint8_t)String((const char*)(s["wins"] | "0")).toInt();
            d.code = String((const char*)(s["Driver"]["code"] | ""));
            d.givenName = String((const char*)(s["Driver"]["givenName"] | ""));
            d.familyName = String((const char*)(s["Driver"]["familyName"] | ""));
            JsonArray cons = s["Constructors"].as<JsonArray>();
            if (!cons.isNull() && cons.size() > 0) {
                d.constructorName = String((const char*)(cons[0]["name"] | ""));
            }
            if (d.position > 0) out.driverStandingCount++;
        }
    }

    // ---- Jolpica: Konstrukteurswertung ----
    void fetchConstructorStandings(SeasonData& out) {
        String body;
        if (!httpGetHttps(jolpicaUrl(Config::F1_JOLPICA_CONSTRUCTOR_STANDINGS_PATH), body)) return;

        JsonDocument filter;
        JsonObject cs = filter["MRData"]["StandingsTable"]["StandingsLists"][0]["ConstructorStandings"][0].to<JsonObject>();
        cs["position"] = true;
        cs["points"] = true;
        cs["wins"] = true;
        cs["Constructor"]["name"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

        JsonArray lists = doc["MRData"]["StandingsTable"]["StandingsLists"].as<JsonArray>();
        if (lists.isNull() || lists.size() == 0) return;
        JsonArray standings = lists[0]["ConstructorStandings"].as<JsonArray>();
        if (standings.isNull()) return;

        for (JsonObject s : standings) {
            if (out.constructorStandingCount >= F1Client::MAX_STANDINGS) break;
            ConstructorStanding& c = out.constructorStandings[out.constructorStandingCount];
            c.position = (uint8_t)String((const char*)(s["position"] | "0")).toInt();
            c.points = String((const char*)(s["points"] | "0")).toFloat();
            c.wins = (uint8_t)String((const char*)(s["wins"] | "0")).toInt();
            c.name = String((const char*)(s["Constructor"]["name"] | ""));
            if (c.position > 0) out.constructorStandingCount++;
        }
    }

    // ---- Jolpica: Ergebnis des letzten Rennens ----
    void fetchLastResults(SeasonData& out) {
        String body;
        if (!httpGetHttps(jolpicaUrl(Config::F1_JOLPICA_LAST_RESULTS_PATH), body)) return;

        JsonDocument filter;
        JsonObject race = filter["MRData"]["RaceTable"]["Races"][0].to<JsonObject>();
        race["raceName"] = true;
        JsonObject res = race["Results"][0].to<JsonObject>();
        res["positionText"] = true;
        res["points"] = true;
        res["status"] = true;
        res["Time"]["time"] = true;
        res["Driver"]["code"] = true;
        res["Driver"]["givenName"] = true;
        res["Driver"]["familyName"] = true;
        res["Constructor"]["name"] = true;

        JsonDocument doc;
        if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return;

        JsonArray races = doc["MRData"]["RaceTable"]["Races"].as<JsonArray>();
        if (races.isNull() || races.size() == 0) return;
        JsonObject r = races[0];
        out.lastRaceName = String((const char*)(r["raceName"] | ""));

        JsonArray results = r["Results"].as<JsonArray>();
        if (results.isNull()) return;

        for (JsonObject res2 : results) {
            if (out.lastResultCount >= F1Client::MAX_RESULTS) break;
            RaceResult& rr = out.lastResults[out.lastResultCount];
            rr.positionText = String((const char*)(res2["positionText"] | ""));
            rr.driverCode = String((const char*)(res2["Driver"]["code"] | ""));
            rr.givenName = String((const char*)(res2["Driver"]["givenName"] | ""));
            rr.familyName = String((const char*)(res2["Driver"]["familyName"] | ""));
            rr.constructorName = String((const char*)(res2["Constructor"]["name"] | ""));
            rr.time = String((const char*)(res2["Time"]["time"] | ""));
            rr.points = String((const char*)(res2["points"] | ""));
            rr.status = String((const char*)(res2["status"] | ""));
            if (rr.positionText.length() > 0) out.lastResultCount++;
        }
    }
}

bool fetchSeasonData(SeasonData& out) {
    out = SeasonData{};
    fetchNextRace(out.nextRace);
    fetchDriverStandings(out);
    fetchConstructorStandings(out);
    fetchLastResults(out);
    return out.nextRace.valid || out.driverStandingCount > 0;
}

// ---- OpenF1 ----

namespace {
    String openF1Url(const char* path, const String& query) {
        return "https://" + String(Config::F1_OPENF1_HOST) + String(path) + "?" + query;
    }

    // "F47600" -> RGB565. OpenF1 liefert team_colour IMMER ohne "#"-Praefix
    // (live bestaetigt).
    uint16_t hexToRgb565(const String& hex) {
        if (hex.length() < 6) return 0xFFFF;
        long v = strtol(hex.c_str(), nullptr, 16);
        uint8_t r = (uint8_t)((v >> 16) & 0xFF);
        uint8_t g = (uint8_t)((v >> 8) & 0xFF);
        uint8_t b = (uint8_t)(v & 0xFF);
        return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }
}

bool fetchLatestSessionInfo(LiveSessionInfo& out) {
    out = LiveSessionInfo{};
    String body;
    if (!httpGetHttps(openF1Url(Config::F1_OPENF1_SESSIONS_PATH, "session_key=latest"), body)) return false;

    JsonDocument filter;
    JsonObject s = filter[0].to<JsonObject>();
    s["session_key"] = true;
    s["session_name"] = true;
    s["date_start"] = true;
    s["date_end"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) return false;
    JsonObject first = arr[0];

    out.sessionKey = first["session_key"] | -1;
    out.sessionName = String((const char*)(first["session_name"] | ""));
    out.dateStartRaw = String((const char*)(first["date_start"] | ""));
    out.dateEndRaw = String((const char*)(first["date_end"] | ""));
    out.dateStart = parseIso8601Utc(out.dateStartRaw.c_str());
    out.dateEnd = parseIso8601Utc(out.dateEndRaw.c_str());
    out.valid = out.sessionKey > 0;
    return out.valid;
}

bool fetchLiveDrivers(int32_t sessionKey, LiveDriver* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    String query = "session_key=" + String(sessionKey);
    String body;
    if (!httpGetHttps(openF1Url(Config::F1_OPENF1_DRIVERS_PATH, query), body)) return false;

    JsonDocument filter;
    JsonObject d = filter[0].to<JsonObject>();
    d["driver_number"] = true;
    d["name_acronym"] = true;
    d["team_name"] = true;
    d["team_colour"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (outCount >= maxCount) break;
        LiveDriver& ld = out[outCount];
        ld.number = (int16_t)(int)(o["driver_number"] | -1);
        ld.acronym = String((const char*)(o["name_acronym"] | ""));
        ld.teamName = String((const char*)(o["team_name"] | ""));
        ld.teamColour565 = hexToRgb565(String((const char*)(o["team_colour"] | "FFFFFF")));
        if (ld.number > 0) outCount++;
    }
    return outCount > 0;
}

bool fetchPositionUpdates(int32_t sessionKey, time_t sinceEpoch,
                           PositionUpdate* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    // "%3E" = URL-kodiertes ">" (live gegen die echte API verifiziert,
    // siehe CLAUDE.md "Formel 1" - OHNE Kodierung liefert OpenF1 einen
    // Fehler statt eines gefilterten Ergebnisses).
    String query = "session_key=" + String(sessionKey) + "&date%3E=" + formatIso8601Utc(sinceEpoch);
    String body;
    if (!httpGetHttps(openF1Url(Config::F1_OPENF1_POSITION_PATH, query), body)) return false;

    JsonDocument filter;
    JsonObject p = filter[0].to<JsonObject>();
    p["driver_number"] = true;
    p["position"] = true;
    p["date"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (outCount >= maxCount) break;
        PositionUpdate& pu = out[outCount];
        pu.driverNumber = (int16_t)(int)(o["driver_number"] | -1);
        pu.position = (uint8_t)(int)(o["position"] | 0);
        pu.epoch = parseIso8601Utc((const char*)(o["date"] | ""));
        if (pu.driverNumber > 0 && pu.position > 0) outCount++;
    }
    return true; // outCount == 0 ist normal (keine Aenderung seit sinceEpoch), KEIN Fehler
}

bool fetchIntervalUpdates(int32_t sessionKey, time_t sinceEpoch,
                           IntervalUpdate* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    String query = "session_key=" + String(sessionKey) + "&date%3E=" + formatIso8601Utc(sinceEpoch);
    String body;
    if (!httpGetHttps(openF1Url(Config::F1_OPENF1_INTERVALS_PATH, query), body)) return false;

    JsonDocument filter;
    JsonObject iv = filter[0].to<JsonObject>();
    iv["driver_number"] = true;
    iv["gap_to_leader"] = true;
    iv["date"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (outCount >= maxCount) break;
        IntervalUpdate& iu = out[outCount];
        iu.driverNumber = (int16_t)(int)(o["driver_number"] | -1);
        iu.epoch = parseIso8601Utc((const char*)(o["date"] | ""));
        if (iu.driverNumber <= 0) continue;

        JsonVariant gap = o["gap_to_leader"];
        if (gap.is<float>() || gap.is<int>()) {
            iu.hasGapSeconds = true;
            iu.gapSeconds = gap.as<float>();
        } else if (gap.is<const char*>()) {
            // z.B. "1 LAP"/"2 LAPS" bei einem ueberrundeten Fahrzeug (live
            // dokumentiert in der OpenF1-Doku) - NUR die Zahl uebernehmen,
            // die Uebersetzung ins Deutsche/etc. passiert im Screen (siehe
            // CLAUDE.md: kein roher englischer API-Text im UI).
            int laps = atoi((const char*)gap.as<const char*>());
            if (laps > 0) {
                iu.hasGapLaps = true;
                iu.gapLaps = (int16_t)laps;
            }
        }
        outCount++;
    }
    return true;
}

bool fetchLapUpdates(int32_t sessionKey, time_t sinceEpoch,
                      LapUpdate* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    // ACHTUNG: dieser Endpunkt heisst "date_start", NICHT "date" wie bei
    // "position"/"intervals" (live bestaetigt, siehe CLAUDE.md "Formel
    // 1").
    String query = "session_key=" + String(sessionKey) + "&date_start%3E=" + formatIso8601Utc(sinceEpoch);
    String body;
    if (!httpGetHttps(openF1Url(Config::F1_OPENF1_LAPS_PATH, query), body)) return false;

    JsonDocument filter;
    JsonObject l = filter[0].to<JsonObject>();
    l["driver_number"] = true;
    l["lap_duration"] = true;
    l["date_start"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (outCount >= maxCount) break;
        // "lap_duration" ist null bei einer noch nicht abgeschlossenen
        // bzw. ungueltigen Runde (z.B. Boxenein-/-ausfahrt, live
        // dokumentiert) - solche Eintraege werden uebersprungen statt
        // einer sinnlosen 0:00.000-Rundenzeit.
        JsonVariant duration = o["lap_duration"];
        if (!(duration.is<float>() || duration.is<int>())) continue;

        LapUpdate& lu = out[outCount];
        lu.driverNumber = (int16_t)(int)(o["driver_number"] | -1);
        lu.epoch = parseIso8601Utc((const char*)(o["date_start"] | ""));
        if (lu.driverNumber <= 0) continue;
        lu.lapDurationSec = duration.as<float>();
        outCount++;
    }
    return true;
}

}
