#include "currency.h"
#include "config.h"
#include <SD.h>
#include <cstring>

namespace Currency {

namespace {
    // Alle 46 echten Fiat-Waehrungscodes von CoinGeckos
    // /simple/supported_vs_currencies (Alex' Auftrag, 12.10.: "alle von
    // CoinGecko unterstuetzten Waehrungen", live per curl am 12.10.
    // nachgeprueft) - die API liefert zusaetzlich Krypto-/Edelmetall-/
    // Kleinsteinheit-Codes (btc, eth, sol, ..., xag, xau, xdr, bits, sats),
    // die hier bewusst AUSGESCHLOSSEN sind: als "Preis-Waehrung" fuer den
    // Krypto-Ticker ergeben nur echte Landeswaehrungen Sinn.
    constexpr const char* SUPPORTED[] = {
        "usd", "eur", "gbp", "chf", "jpy", "cad", "aud", "nzd", "cny",
        "inr", "brl", "mxn", "zar", "rub", "try", "sek", "nok", "dkk",
        "pln", "czk", "huf", "krw", "sgd", "hkd", "thb", "idr", "myr",
        "php", "ils", "sar", "aed", "vnd", "uah", "ars", "clp", "pkr",
        "bdt", "lkr", "ngn", "gel", "bhd", "bmd", "kwd", "mmk", "twd",
        "vef",
    };
    constexpr uint8_t SUPPORTED_COUNT = sizeof(SUPPORTED) / sizeof(SUPPORTED[0]);

    // 3 Zeichen + Nullterminierung. "usd" ist der feste Standard (Alex'
    // Auftrag, 13.10.: "Standard ist USD") - nie leer.
    char currentCurrencyBuf[4] = "usd";

    bool isSupported(const String& code) {
        for (uint8_t i = 0; i < SUPPORTED_COUNT; i++) {
            if (code == SUPPORTED[i]) return true;
        }
        return false;
    }
}

String displayPrefix(const char* vsCurrency) {
    String upper = String(vsCurrency);
    upper.toUpperCase();
    return upper + " ";
}

void init() {
    strncpy(currentCurrencyBuf, "usd", sizeof(currentCurrencyBuf));
    File f = SD.open(Config::SD_CRYPTO_CURRENCY_FILE, FILE_READ);
    if (!f) return;
    String line = f.readStringUntil('\n');
    f.close();
    line.trim();
    line.toLowerCase();
    if (isSupported(line)) {
        strncpy(currentCurrencyBuf, line.c_str(), 3);
        currentCurrencyBuf[3] = 0;
    }
}

String currentCurrency() { return String(currentCurrencyBuf); }

void setCurrentCurrency(const String& vsCurrency) {
    if (!isSupported(vsCurrency)) return;
    strncpy(currentCurrencyBuf, vsCurrency.c_str(), 3);
    currentCurrencyBuf[3] = 0;

    File f = SD.open(Config::SD_CRYPTO_CURRENCY_FILE, FILE_WRITE);
    if (!f) return;
    f.println(currentCurrencyBuf);
    f.close();
}

uint8_t supportedCount() { return SUPPORTED_COUNT; }

const char* supportedCode(uint8_t index) {
    if (index >= SUPPORTED_COUNT) return "";
    return SUPPORTED[index];
}

}
