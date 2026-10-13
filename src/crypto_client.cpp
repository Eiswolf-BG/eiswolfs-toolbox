#include "crypto_client.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

namespace CryptoClient {

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

    String coingeckoUrl(const char* pathWithQuery) {
        return "https://" + String(Config::CRYPTO_COINGECKO_HOST) + String(pathWithQuery);
    }

    String mempoolUrl(const char* path) {
        return "https://" + String(Config::CRYPTO_MEMPOOL_HOST) + String(path);
    }
}

bool fetchMainPrices(const char* vsCurrency, MainCoin& btc, MainCoin& eth) {
    String url = coingeckoUrl(Config::CRYPTO_COINGECKO_SIMPLE_PRICE_PATH) +
                 "&vs_currencies=" + String(vsCurrency);
    String body;
    if (!httpGet(url, body)) return false;

    // CoinGecko benennt die Preis-/Aenderungsfelder selbst nach der
    // gewaehlten Waehrung (z.B. "eur"/"eur_24h_change" statt "usd"/
    // "usd_24h_change" - live per curl verifiziert, 07.10.), deshalb
    // Filter UND Auslesen mit dynamischen Feldnamen statt fest "usd".
    String changeKey = String(vsCurrency) + "_24h_change";

    JsonDocument filter;
    filter["bitcoin"][vsCurrency] = true;
    filter["bitcoin"][changeKey] = true;
    filter["ethereum"][vsCurrency] = true;
    filter["ethereum"][changeKey] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonObject btcObj = doc["bitcoin"];
    JsonObject ethObj = doc["ethereum"];
    if (btcObj.isNull() || ethObj.isNull()) return false;

    // CoinGecko liefert Preis/Aenderung als native JSON-Zahlen (ANDERS
    // als z.B. bei Jolpica/Formel-1, dort waren vergleichbare Felder
    // JSON-Strings) - der "|"-Operator funktioniert direkt, live
    // bestaetigt.
    btc.priceUsd = btcObj[vsCurrency] | 0.0;
    btc.change24hPercent = btcObj[changeKey] | 0.0;
    btc.valid = true;

    eth.priceUsd = ethObj[vsCurrency] | 0.0;
    eth.change24hPercent = ethObj[changeKey] | 0.0;
    eth.valid = true;

    return true;
}

bool fetchMarkets(const char* vsCurrency, CoinMarket* out, uint8_t maxCount, uint8_t& outCount) {
    outCount = 0;
    String url = coingeckoUrl(Config::CRYPTO_COINGECKO_MARKETS_PATH) +
                 "&vs_currency=" + String(vsCurrency);
    String body;
    if (!httpGet(url, body)) return false;

    // Top-Level-Antwort ist ein JSON-ARRAY (anders als bei der Launch-
    // Library-/Jolpica-API, dort jeweils in ein Objekt verpackt) - ein
    // Filter-Eintrag fuer Index 0 reicht, ArduinoJson wendet dessen Form
    // auf jedes der bis zu Config::CRYPTO_MAX_MARKETS Elemente an.
    JsonDocument filter;
    JsonObject item = filter[0].to<JsonObject>();
    item["id"] = true;
    item["symbol"] = true;
    item["name"] = true;
    item["current_price"] = true;
    item["price_change_percentage_24h"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray list = doc.as<JsonArray>();
    if (list.isNull()) return false;

    for (JsonObject c : list) {
        if (outCount >= maxCount) break;
        CoinMarket& m = out[outCount];
        m.id = String((const char*)(c["id"] | ""));
        m.symbol = String((const char*)(c["symbol"] | ""));
        m.name = String((const char*)(c["name"] | ""));
        m.priceUsd = c["current_price"] | 0.0;
        m.change24hPercent = c["price_change_percentage_24h"] | 0.0;
        m.valid = m.id.length() > 0;
        if (m.valid) outCount++;
    }

    return outCount > 0;
}

bool fetchMempool(Mempool& out) {
    bool feesOk = false;
    bool heightOk = false;

    // Zwei technisch getrennte Endpunkte auf demselben Host - jeder kann
    // unabhaengig vom anderen fehlschlagen (zusaetzliche Robustheit ueber
    // Alex' Mindestanforderung hinaus, die nur Unabhaengigkeit zwischen
    // den DREI Hauptquellen CoinGecko/mempool.space/alternative.me
    // verlangt).
    String feesBody;
    if (httpGet(mempoolUrl(Config::CRYPTO_MEMPOOL_FEES_PATH), feesBody)) {
        JsonDocument filter;
        filter["fastestFee"] = true;

        JsonDocument doc;
        if (!deserializeJson(doc, feesBody, DeserializationOption::Filter(filter))) {
            out.fastestFeeSatPerVb = (int32_t)(float)(doc["fastestFee"] | 0.0f);
            feesOk = true;
        }
    }

    // /api/blocks/tip/height liefert die Blockhoehe als REINEN Text
    // (z.B. "866543"), KEIN JSON - live per curl bestaetigt (siehe
    // Config::CRYPTO_MEMPOOL_HEIGHT_PATH-Kommentar). Deshalb keine
    // deserializeJson() hier, sondern direkte Zahl-Konvertierung des
    // rohen Bodys.
    String heightBody;
    if (httpGet(mempoolUrl(Config::CRYPTO_MEMPOOL_HEIGHT_PATH), heightBody)) {
        heightBody.trim();
        long h = heightBody.toInt();
        if (h > 0) {
            out.blockHeight = h;
            heightOk = true;
        }
    }

    out.valid = feesOk || heightOk;
    return out.valid;
}

bool fetchFearGreed(FearGreed& out) {
    String body;
    if (!httpGet(Config::CRYPTO_FNG_URL, body)) return false;

    JsonDocument filter;
    filter["data"][0]["value"] = true;
    filter["data"][0]["value_classification"] = true;

    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter))) return false;

    JsonArray data = doc["data"].as<JsonArray>();
    if (data.isNull() || data.size() == 0) return false;
    JsonObject d = data[0];

    // "value" kommt als JSON-STRING ("34"), nicht als Zahl - live
    // bestaetigtes Verhalten bei alternative.me (gleiches Muster wie bei
    // Jolpica/Formel-1, siehe CLAUDE.md) - ueber String::toInt() geparst
    // statt ArduinoJsons "|"-Operator zu vertrauen.
    out.value = (int8_t)String((const char*)(d["value"] | "0")).toInt();
    out.rawClassification = String((const char*)(d["value_classification"] | ""));
    out.valid = out.rawClassification.length() > 0;

    return out.valid;
}

}
