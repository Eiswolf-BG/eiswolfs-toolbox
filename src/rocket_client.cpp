#include "rocket_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace RocketClient {

namespace {
    // Tage seit 1970-01-01 fuer ein gegebenes (Jahr, Monat, Tag) - Howard
    // Hinnants "days_from_civil"-Algorithmus (reine Ganzzahl-Arithmetik,
    // funktioniert fuer jedes Gregorianische Datum, keine libc-
    // Zeitzonen-Abhaengigkeit noetig - wichtig, weil die API-Zeiten immer
    // UTC sind und der ESP32 keine Zeitzonendatenbank hat).
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    // Parst "YYYY-MM-DDTHH:MM:SSZ" (ISO-8601, immer UTC bei dieser API) zu
    // einem Unix-Zeitstempel. Gibt 0 zurueck, wenn das Format nicht passt.
    time_t parseIso8601Utc(const char* iso) {
        int year, month, day, hour, minute, second;
        if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
            return 0;
        }
        long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
        return (time_t)(days * 86400L + hour * 3600L + minute * 60L + second);
    }
}

bool fetch(Launch* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(Config::HTTP_TIMEOUT_MS);

    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    if (!http.begin(client, Config::ROCKET_API_URL)) {
        return false;
    }

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    // Erst vollstaendig als String einlesen (wie iss_client.cpp) statt
    // direkt vom Stream zu parsen - die API liefert ihre Antwort chunked-
    // codiert (Transfer-Encoding: chunked, live per curl bestaetigt), und
    // HTTPClient entfernt diese Codierung nur bei getString() zuverlaessig,
    // nicht beim rohen Stream-Zugriff (siehe CLAUDE.md "Raketenstart-
    // Countdown" fuer die Fehlersuche, die genau das zeigte - leeres bzw.
    // kaputtes Ergebnis trotz HTTP 200 und erfolgreichem JSON-Parsing).
    // Der Feldfilter greift unveraendert, auch bei String-Eingabe, der
    // ArduinoJson-Speicherbedarf bleibt dadurch weiterhin klein - nur der
    // rohe HTTP-Antworttext selbst liegt kurzzeitig vollstaendig im RAM
    // (ein paar KB, siehe Heap-Werte aus der Fehlersuche).
    String body = http.getString();
    http.end();

    // Feldfilter (siehe CLAUDE.md "Raketenstart-Countdown") - inkl. der
    // zusaetzlichen Felder fuer Agentur/volle Raketenbezeichnung/
    // Missionsart+Zielorbit/Hold-Fehlgrund/Go-Chance/Wetter-Hinweis/
    // Missionsbeschreibung (Alex' Auftrag, 06.10. - "alle sinnvollen
    // API-Daten anzeigen", siehe CLAUDE.md). Bei Array-Feldern reicht ein
    // Filter-Eintrag fuer Index 0 - ArduinoJson wendet dessen Form auf
    // jedes der bis zu 3 Elemente an.
    JsonDocument filter;
    filter["results"][0]["name"] = true;
    filter["results"][0]["net"] = true;
    filter["results"][0]["status"]["id"] = true;
    filter["results"][0]["status"]["name"] = true;
    filter["results"][0]["status"]["abbrev"] = true;
    filter["results"][0]["rocket"]["configuration"]["name"] = true;
    filter["results"][0]["rocket"]["configuration"]["full_name"] = true;
    filter["results"][0]["launch_service_provider"]["name"] = true;
    filter["results"][0]["pad"]["name"] = true;
    filter["results"][0]["pad"]["location"]["name"] = true;
    filter["results"][0]["mission"]["type"] = true;
    filter["results"][0]["mission"]["description"] = true;
    filter["results"][0]["mission"]["orbit"]["name"] = true;
    filter["results"][0]["holdreason"] = true;
    filter["results"][0]["failreason"] = true;
    filter["results"][0]["probability"] = true;
    filter["results"][0]["weather_concerns"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) {
        return false;
    }

    JsonArray results = doc["results"].as<JsonArray>();
    if (results.isNull()) {
        return false;
    }

    for (JsonObject launch : results) {
        if (outCount >= maxCount) break;
        Launch& l = out[outCount];

        l.missionName = String((const char*)(launch["name"] | ""));
        l.rocketName = String((const char*)(launch["rocket"]["configuration"]["name"] | ""));
        l.rocketFullName = String((const char*)(launch["rocket"]["configuration"]["full_name"] | ""));
        l.agencyName = String((const char*)(launch["launch_service_provider"]["name"] | ""));
        l.padName = String((const char*)(launch["pad"]["location"]["name"] | ""));
        l.statusId = (int16_t)(int)(launch["status"]["id"] | -1);
        l.statusName = String((const char*)(launch["status"]["name"] | ""));
        l.statusAbbrev = String((const char*)(launch["status"]["abbrev"] | ""));

        // "mission" kann als Ganzes null sein (unbekannte/TBD-Mission,
        // laut Schema moeglich) - dann bleiben die mission.*-Felder leer.
        JsonObject mission = launch["mission"];
        if (!mission.isNull()) {
            l.missionType = String((const char*)(mission["type"] | ""));
            l.missionDescription = String((const char*)(mission["description"] | ""));
            l.missionOrbitName = String((const char*)(mission["orbit"]["name"] | ""));
        }

        l.holdReason = String((const char*)(launch["holdreason"] | ""));
        l.failReason = String((const char*)(launch["failreason"] | ""));
        l.probability = (int8_t)(int)(launch["probability"] | -1);
        l.weatherConcerns = String((const char*)(launch["weather_concerns"] | ""));

        l.netIso = String((const char*)(launch["net"] | ""));
        l.netEpoch = parseIso8601Utc(l.netIso.c_str());
        l.valid = (l.netEpoch > 0) && l.missionName.length() > 0;

        if (l.valid) outCount++;
    }

    return outCount > 0;
}

}
