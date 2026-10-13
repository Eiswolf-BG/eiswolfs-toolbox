#include "natural_events_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace NaturalEventsClient {

namespace {
    // BUGFIX (13.10., Alex' Live-Test mit "Nochmal versuchen"-Button):
    // http.getString() brach bei JEDEM Versuch deterministisch bei exakt
    // 17844 Bytes ab (ESP32-HTTPClient-Bug beim Dekodieren von Transfer-
    // Encoding: chunked, EONET liefert keinen Content-Length-Header).
    // Zwei Zwischenschritte (eigener Chunked-Decoder in einen String via
    // concat(), danach in einen gemallocten Rohpuffer) scheiterten beide
    // an echter Heap-Fragmentierung waehrend eines aktiven TLS-Handshakes
    // - weder ein wiederholt wachsender String noch ein einmaliger
    // ~90KB-malloc() fanden einen ausreichend grossen zusammenhaengenden
    // Block. Deshalb jetzt die endgueltige Loesung: der komplette Body
    // wird NIE als ein Stueck im Speicher gehalten. Dieser Reader
    // dekodiert das Chunked-Encoding byteweise direkt vom TLS-Stream und
    // wird direkt an deserializeJson() uebergeben (ArduinoJsons Reader-
    // Konzept: eine Klasse mit int read()) - der JSON-Parser verarbeitet
    // die Daten dadurch fortlaufend, ohne je den ganzen Body gleichzeitig
    // zu puffern.
    class ChunkedStreamReader {
    public:
        ChunkedStreamReader(WiFiClient* stream, WiFiClientSecure& client, uint32_t timeoutMs)
            : stream_(stream), client_(client), timeoutMs_(timeoutMs) {}

        int read() {
            while (remainingInChunk_ == 0) {
                if (!startNextChunk()) return -1;
            }
            uint32_t deadline = millis() + timeoutMs_;
            while (millis() < deadline) {
                if (stream_->available()) {
                    int c = stream_->read();
                    if (c < 0) return -1;
                    remainingInChunk_--;
                    if (remainingInChunk_ == 0) {
                        readRawLineLocal(); // abschliessendes CRLF nach den Chunk-Daten verwerfen
                    }
                    return c;
                } else if (!client_.connected()) {
                    return -1;
                } else {
                    delay(1);
                }
            }
            return -1;
        }

    private:
        bool startNextChunk() {
            String sizeLine = readRawLineLocal();
            if (sizeLine.length() == 0) return false;
            long sz = strtol(sizeLine.c_str(), nullptr, 16);
            if (sz <= 0) return false; // Endchunk ("0") erreicht
            remainingInChunk_ = sz;
            return true;
        }

        String readRawLineLocal() {
            String line;
            uint32_t deadline = millis() + timeoutMs_;
            while (millis() < deadline) {
                if (stream_->available()) {
                    char c = (char)stream_->read();
                    if (c == '\n') return line;
                    if (c != '\r') line += c;
                    deadline = millis() + timeoutMs_;
                } else if (!client_.connected()) {
                    break;
                } else {
                    delay(1);
                }
            }
            return line;
        }

        WiFiClient* stream_;
        WiFiClientSecure& client_;
        uint32_t timeoutMs_;
        long remainingInChunk_ = 0;
    };

    // Wie f1_client.cpp::parseIso8601Utc() (1:1 dupliziert, Projekt-
    // Konvention) - EONET liefert "YYYY-MM-DDTHH:MM:SSZ", das
    // abschliessende "Z" wird von sscanf() ohnehin ignoriert.
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    time_t parseIso8601Utc(const char* iso) {
        int year, month, day, hour, minute, second;
        if (sscanf(iso, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6) {
            return 0;
        }
        long days = daysFromCivil(year, (unsigned)month, (unsigned)day);
        return (time_t)(days * 86400L + hour * 3600L + minute * 60L + second);
    }

    Category categoryFromId(const String& id) {
        if (id == "volcanoes") return Category::Volcano;
        if (id == "severeStorms") return Category::Storm;
        if (id == "floods") return Category::Flood;
        if (id == "landslides") return Category::Landslide;
        return Category::Unknown;
    }
}

bool fetch(Event* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;

    WiFiClientSecure client;
    client.setInsecure();
    // Eigenes, grosszuegigeres Timeout statt des projektweiten Standard-
    // werts (siehe config.h-Kommentar bei NATURAL_EVENTS_HTTP_TIMEOUT_MS).
    client.setTimeout(Config::NATURAL_EVENTS_HTTP_TIMEOUT_MS);

    HTTPClient http;
    http.setTimeout(Config::NATURAL_EVENTS_HTTP_TIMEOUT_MS);
    if (!http.begin(client, Config::NATURAL_EVENTS_API_URL)) {
        return false;
    }
    http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    // Feldfilter - reduziert den Parse-Speicherbedarf deutlich (ArduinoJson
    // ueberspringt gefilterte Werte bereits waehrend des Parsens, nicht
    // erst danach) - besonders wichtig hier, da "geometry" bei langlebigen
    // Ereignissen (z.B. Hurrikanen) Dutzende Bahnpunkte enthalten kann,
    // von denen nur das Datum gebraucht wird (siehe config.h-Kommentar).
    JsonDocument filter;
    JsonObject ev = filter["events"][0].to<JsonObject>();
    ev["title"] = true;
    ev["categories"][0]["id"] = true;
    ev["geometry"][0]["date"] = true;

    // Siehe Kommentar bei ChunkedStreamReader oben: der Body wird NIE
    // komplett im Speicher gehalten, der Reader liefert ihn dem Parser
    // byteweise direkt vom TLS-Stream.
    ChunkedStreamReader reader(http.getStreamPtr(), client, Config::NATURAL_EVENTS_HTTP_TIMEOUT_MS);
    JsonDocument doc;
    DeserializationError jsonErr = deserializeJson(doc, reader, DeserializationOption::Filter(filter));
    http.end();
    if (jsonErr) {
        return false;
    }

    JsonArray events = doc["events"].as<JsonArray>();
    if (events.isNull()) {
        return false;
    }

    for (JsonObject e : events) {
        if (outCount >= maxCount) break;
        Event& out_i = out[outCount];

        out_i.title = String((const char*)(e["title"] | ""));

        JsonArray cats = e["categories"];
        String catId;
        if (!cats.isNull() && cats.size() > 0) {
            catId = String((const char*)(cats[0]["id"] | ""));
        }
        out_i.category = categoryFromId(catId);

        // Juengster Bahnpunkt (hoechster Zeitstempel) statt einfach des
        // letzten Array-Eintrags - bei EONET live bestaetigt chronologisch
        // aufsteigend sortiert, hier trotzdem robust per Maximum bestimmt.
        time_t latest = 0;
        JsonArray geom = e["geometry"];
        if (!geom.isNull()) {
            for (JsonObject g : geom) {
                const char* d = g["date"] | "";
                time_t t = parseIso8601Utc(d);
                if (t > latest) latest = t;
            }
        }
        out_i.epoch = latest;

        out_i.valid = out_i.title.length() > 0;
        if (out_i.valid) outCount++;
    }

    return outCount > 0;
}

}
