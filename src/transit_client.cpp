#include "transit_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace TransitClient {

namespace {
    // Tage seit 1970-01-01 - 1:1 aus rocket_client.cpp uebernommen (Howard
    // Hinnants "days_from_civil"-Algorithmus, lokal dupliziert statt
    // geteilt, siehe CLAUDE.md-Konvention).
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    // Parst "YYYY-MM-DDTHH:MM:SSZ" (ISO-8601, immer UTC bei dieser API,
    // live bestaetigt) zu einem Unix-Zeitstempel. 0, wenn das Format nicht
    // passt.
    time_t parseIso8601Utc(const char* iso) {
        int year, month, day, hour, minute, second;
        if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
            return 0;
        }
        long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
        return (time_t)(days * 86400L + hour * 3600L + minute * 60L + second);
    }

    // Prozentkodiert einen String fuer Query-Parameter (stopId enthaelt
    // ":" und "_") - 1:1 dasselbe Prinzip wie
    // address_search_screen.cpp::urlEncode(), lokal dupliziert.
    String urlEncode(const String& s) {
        String out;
        char hex[4];
        for (size_t i = 0; i < s.length(); i++) {
            uint8_t c = (uint8_t)s[i];
            bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
            if (unreserved) {
                out += (char)c;
            } else {
                snprintf(hex, sizeof(hex), "%%%02X", c);
                out += hex;
            }
        }
        return out;
    }

    // Body erst komplett einsammeln statt direkt aus http.getStream() zu
    // parsen (Chunked-Transfer-Encoding, siehe rocket_client.cpp/CLAUDE.md
    // fuer die ausfuehrliche Begruendung - dasselbe Muster hier 1:1
    // uebernommen, Transitous liefert ebenfalls chunked-codierte Antworten).
    bool httpGet(const String& url, String& outBody) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;
        // Hoeflicher User-Agent (Alex' Auftrag, 06.10.) - gleicher Text wie
        // bei der Nominatim-Adresssuche.
        http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return false;
        }
        outBody = http.getString();
        http.end();
        return true;
    }
}

StopResult findNearestStop(double lat, double lon, String& outStopId, String& outStopName) {
    char latlon[48];
    snprintf(latlon, sizeof(latlon), "%.6f,%.6f", lat, lon);

    String url = "https://" + String(Config::TRANSIT_API_HOST) + String(Config::TRANSIT_REVERSE_GEOCODE_PATH) +
                 "?place=" + String(latlon) + "&type=STOP&numResults=5";

    String body;
    if (!httpGet(url, body)) return StopResult::NetworkError;

    // Nur id/name werden gebraucht (siehe CLAUDE.md - live gegen die
    // echte API geprueftes Antwortschema: Array am Root, ein Filter-
    // Eintrag fuer Index 0 gilt fuer alle Elemente, gleiches Prinzip wie
    // rocket_client.cpp).
    JsonDocument filter;
    filter[0]["id"] = true;
    filter[0]["name"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) return StopResult::NetworkError;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull() || arr.size() == 0) return StopResult::NoResults;

    JsonObject first = arr[0];
    outStopId = String((const char*)(first["id"] | ""));
    outStopName = String((const char*)(first["name"] | ""));
    if (outStopId.length() == 0) return StopResult::NoResults;
    return StopResult::Ok;
}

DeparturesResult fetchDepartures(const String& stopId, Departure* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;

    String url = "https://" + String(Config::TRANSIT_API_HOST) + String(Config::TRANSIT_STOPTIMES_PATH) +
                 "?stopId=" + urlEncode(stopId) + "&n=" + String(maxCount);

    String body;
    if (!httpGet(url, body)) return DeparturesResult::NetworkError;

    // Feldfilter (live gegen die echte API geprueftes Antwortschema,
    // siehe CLAUDE.md) - inkl. der Detail-Felder fuer den antippbaren
    // Detail-Screen (tripFrom/tripTo/agencyName/realTime/track) UND der
    // zusaetzlichen Felder fuer Verspaetung/Ausfall/Linienname/
    // Rollstuhl-Fahrrad-Mitnahme/Zeitzone (Alex' Auftrag, 06.10. - "alle
    // sinnvollen API-Daten anzeigen"). Ein "alerts"-Feld existiert in
    // diesem Endpunkt NICHT (live mehrfach geprueft) - bewusst nicht
    // gefiltert/geparst.
    JsonDocument filter;
    filter["stopTimes"][0]["place"]["departure"] = true;
    filter["stopTimes"][0]["place"]["scheduledDeparture"] = true;
    filter["stopTimes"][0]["place"]["track"] = true;
    filter["stopTimes"][0]["place"]["tz"] = true;
    filter["stopTimes"][0]["routeShortName"] = true;
    filter["stopTimes"][0]["routeLongName"] = true;
    filter["stopTimes"][0]["headsign"] = true;
    filter["stopTimes"][0]["tripFrom"]["name"] = true;
    filter["stopTimes"][0]["tripTo"]["name"] = true;
    filter["stopTimes"][0]["agencyName"] = true;
    filter["stopTimes"][0]["realTime"] = true;
    filter["stopTimes"][0]["cancelled"] = true;
    filter["stopTimes"][0]["tripCancelled"] = true;
    filter["stopTimes"][0]["wheelchairAccessible"] = true;
    filter["stopTimes"][0]["bikesAllowed"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err) return DeparturesResult::NetworkError;

    JsonArray stopTimes = doc["stopTimes"].as<JsonArray>();
    if (stopTimes.isNull() || stopTimes.size() == 0) return DeparturesResult::NoResults;

    // "n" ist bei dieser API ein MINIMUM, kein Maximum (Alex' ausdruecklicher
    // Hinweis, live bestaetigt) - explizite Begrenzung hier, nicht nur ueber
    // den Query-Parameter.
    for (JsonObject st : stopTimes) {
        if (outCount >= maxCount) break;
        const char* departure = st["place"]["departure"] | "";
        time_t epoch = parseIso8601Utc(departure);
        if (epoch == 0) continue;

        out[outCount].line = String((const char*)(st["routeShortName"] | ""));
        out[outCount].headsign = String((const char*)(st["headsign"] | ""));
        out[outCount].epoch = epoch;
        out[outCount].tripFromName = String((const char*)(st["tripFrom"]["name"] | ""));
        out[outCount].tripToName = String((const char*)(st["tripTo"]["name"] | ""));
        out[outCount].agencyName = String((const char*)(st["agencyName"] | ""));
        out[outCount].track = String((const char*)(st["place"]["track"] | ""));
        out[outCount].realTime = st["realTime"] | false;
        out[outCount].scheduledEpoch = parseIso8601Utc(st["place"]["scheduledDeparture"] | "");
        // Entweder das Stop-Time- oder das Trip-weite Cancelled-Flag reicht
        // (beide live bestaetigt vorhanden, leicht unterschiedliche
        // Bedeutung - Stop vs. ganze Fahrt - fuer die Anzeige zaehlt beides
        // gleich als "faellt aus").
        out[outCount].cancelled = (st["cancelled"] | false) || (st["tripCancelled"] | false);
        out[outCount].routeLongName = String((const char*)(st["routeLongName"] | ""));
        out[outCount].wheelchairAccessible = String((const char*)(st["wheelchairAccessible"] | ""));
        out[outCount].bikesAllowed = st["bikesAllowed"] | false;
        out[outCount].stopTz = String((const char*)(st["place"]["tz"] | ""));
        outCount++;
    }

    if (outCount == 0) return DeparturesResult::NoResults;
    return DeparturesResult::Ok;
}

}
