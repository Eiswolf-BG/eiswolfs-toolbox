#include "holiday_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace HolidayClient {

namespace {
    // Body erst komplett einsammeln statt direkt aus http.getStream() zu
    // parsen (Chunked-Transfer-Encoding - gleiches Muster wie in allen
    // anderen *_client.cpp-Dateien des Projekts).
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

    // Prueft, ob JEDER UTF-8-codierte Codepunkt in "s" innerhalb des
    // Zeichensatzes unseres eigenen Fonts liegt (0x0020-0x015F, siehe
    // ui_font.h::UiFont11pt - first/last-Codepunkt des GFXfont-Headers).
    // Fuer Feiertagsnamen mit z.B. japanischen/kyrillischen/arabischen
    // Zeichen in "localName" (Alex' Auftrag, 12.10.: "pruefen, ob
    // localName nur Zeichen enthaelt, die unser Font darstellen kann,
    // sonst auf 'name' ausweichen") - gleiches Prinzip wie die bewusste
    // Vermeidung des Euro-Symbols im Krypto-Screen (currency.h), hier
    // aber als echte Laufzeit-Pruefung statt einer festen Design-
    // Entscheidung, da die API-Antwort nicht vorhersehbar ist.
    bool isFontSafe(const String& s) {
        const uint8_t* p = (const uint8_t*)s.c_str();
        while (*p) {
            uint32_t cp;
            int len;
            if ((*p & 0x80) == 0x00) { cp = *p; len = 1; }
            else if ((*p & 0xE0) == 0xC0) { cp = *p & 0x1F; len = 2; }
            else if ((*p & 0xF0) == 0xE0) { cp = *p & 0x0F; len = 3; }
            else if ((*p & 0xF8) == 0xF0) { cp = *p & 0x07; len = 4; }
            else return false; // ungueltiges UTF-8-Startbyte

            for (int i = 1; i < len; i++) {
                if ((p[i] & 0xC0) != 0x80) return false; // ungueltiges Folgebyte
                cp = (cp << 6) | (p[i] & 0x3F);
            }
            if (cp < 0x0020 || cp > 0x015F) return false;
            p += len;
        }
        return true;
    }
}

bool fetch(const char* countryCode, int year, Holiday* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;

    String url = "https://" + String(Config::HOLIDAY_API_HOST) + String(Config::HOLIDAY_API_PATH) +
                 "/" + String(year) + "/" + String(countryCode);
    String body;
    if (!httpGet(url, body)) return false;

    JsonDocument filter;
    JsonObject f = filter[0].to<JsonObject>();
    f["date"] = true;
    f["localName"] = true;
    f["name"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (outCount >= maxCount) break;
        Holiday& h = out[outCount];

        h.date = String((const char*)(o["date"] | ""));
        String localName = String((const char*)(o["localName"] | ""));
        String enName = String((const char*)(o["name"] | ""));
        h.name = isFontSafe(localName) ? localName : enName;

        h.valid = h.date.length() == 10 && h.name.length() > 0;
        if (h.valid) outCount++;
    }

    return outCount > 0;
}

}
