#pragma once
#include <Arduino.h>
#include "config.h"

// Krypto-Ticker (Alex' Auftrag, 07.10.) - drei voneinander unabhaengige,
// schluessellose APIs, siehe Config::CRYPTO_*-Konstanten in config.h fuer
// die genauen Endpunkte. Jede der vier Abruf-Funktionen unten kann
// unabhaengig von den anderen fehlschlagen, ohne die uebrigen Kacheln im
// Screen zu beeintraechtigen (Alex' Auftrag: "jede der drei Datenquellen
// soll unabhaengig voneinander fehlschlagen koennen") - ein Fehlschlag
// laesst das jeweilige Ziel-Struct einfach unveraendert, der Aufrufer
// (net_task.cpp) behaelt dann die zuletzt bekannten Werte.
namespace CryptoClient {
    // "priceUsd" bewusst NICHT umbenannt, obwohl seit 07.10. standort-
    // abhaengig auch andere Waehrungen moeglich sind (Alex' Auftrag,
    // siehe currency.h) - geringinvasive Aenderung, gleiches Prinzip wie
    // "windSpeedKmh" in weather_client.h, das bei Imperial tatsaechlich
    // mph enthaelt. Welche Waehrung tatsaechlich drinsteht, steht in
    // NetTask::CryptoData::vsCurrency (net_task.h).
    struct MainCoin {
        double priceUsd = 0;
        double change24hPercent = 0;
        bool valid = false;
    };

    // Fuer die "Weitere Coins"-Liste (Top 10 nach Marktkapitalisierung,
    // BTC/ETH werden beim Anzeigen uebersprungen, siehe crypto_screen.cpp).
    struct CoinMarket {
        String id;     // z.B. "solana" - fuer den BTC/ETH-Ausschluss beim Anzeigen
        String symbol;  // z.B. "sol"
        String name;    // z.B. "Solana"
        double priceUsd = 0; // siehe MainCoin-Kommentar oben
        double change24hPercent = 0;
        bool valid = false;
    };

    struct Mempool {
        int32_t fastestFeeSatPerVb = 0;
        long blockHeight = 0;
        bool valid = false;
    };

    // "value" liegt in der alternative.me-API als JSON-STRING vor (nicht
    // als Zahl, live bestaetigtes Verhalten - gleiches Muster wie bei
    // Jolpica/Formel-1), rawClassification bleibt roh (Uebersetzung
    // passiert im Screen, siehe crypto_screen.cpp::fngText()).
    struct FearGreed {
        int8_t value = 0; // 0-100
        String rawClassification;
        bool valid = false;
    };

    // vsCurrency: CoinGecko-vs_currency-Code (klein, z.B. "usd"/"eur"),
    // siehe currency.h::currentCurrency().
    bool fetchMainPrices(const char* vsCurrency, MainCoin& btc, MainCoin& eth);
    bool fetchMarkets(const char* vsCurrency, CoinMarket* out, uint8_t maxCount, uint8_t& outCount);
    bool fetchMempool(Mempool& out);
    bool fetchFearGreed(FearGreed& out);
}
