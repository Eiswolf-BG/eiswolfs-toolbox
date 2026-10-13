#include "earthquake_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace EarthquakeClient {

namespace {
    // Body erst komplett einsammeln statt direkt aus http.getStream() zu
    // parsen (Chunked-Transfer-Encoding - gleiches Muster wie rocket_
    // client.cpp/weather_client.cpp, siehe dortige CLAUDE.md-Begruendung).
    bool httpGet(const String& url, String& outBody) {
        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;
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

bool fetch(Quake* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    String body;
    if (!httpGet(Config::EARTHQUAKE_API_URL, body)) return false;

    // Feldfilter - "features" ist ein JSON-ARRAY am Root (GeoJSON
    // FeatureCollection), ein Filter-Eintrag fuer Index 0 reicht,
    // ArduinoJson wendet dessen Form auf jedes Element an.
    JsonDocument filter;
    JsonObject props = filter["features"][0]["properties"].to<JsonObject>();
    props["mag"] = true;
    props["place"] = true;
    props["time"] = true;
    props["tsunami"] = true;
    filter["features"][0]["geometry"]["coordinates"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray features = doc["features"].as<JsonArray>();
    if (features.isNull()) return false;

    for (JsonObject f : features) {
        if (outCount >= maxCount) break;
        JsonObject p = f["properties"];
        Quake& q = out[outCount];

        q.magnitude = p["mag"] | 0.0f;
        q.place = String((const char*)(p["place"] | ""));
        // time liegt als Unix-Millisekunden vor (live bestaetigt, z.B.
        // 1790371383309) - als double geparst statt int, da das weit
        // ausserhalb des int32-Bereichs liegt (ArduinoJsons "|"-Operator
        // wuerde sonst ueberlaufen/abschneiden).
        double timeMs = p["time"] | 0.0;
        q.epoch = (time_t)(timeMs / 1000.0);
        q.tsunami = (p["tsunami"] | 0) != 0;

        JsonArray coords = f["geometry"]["coordinates"];
        q.depthKm = coords.isNull() ? 0.0f : (float)(coords[2] | 0.0);

        q.valid = q.place.length() > 0 && q.epoch > 0;
        if (q.valid) outCount++;
    }

    return outCount > 0;
}

}
