#pragma once
#include <Arduino.h>

// Waehrung fuer den Krypto-Ticker (Alex' Auftrag, 07.10., vereinfacht
// 13.10.) - urspruenglich stand- ortabhaengig per Laendercode automatisch
// zugeordnet, das hat sich in der Praxis als unzuverlaessig erwiesen
// (Alex' Fund, 13.10.: "Automatisch bleibt auf der letzten Waehrung
// haengen"). Jetzt rein manuell: jede*r stellt die gewuenschte Waehrung
// selbst ein, Standard ist USD.
namespace Currency {
    // Anzeige-Praefix: IMMER der 3-Buchstaben-ISO-Code in Grossbuchstaben
    // + Leerzeichen (z.B. "EUR "), bewusst KEIN Waehrungssymbol - "€"
    // (U+20AC) liegt ausserhalb des Zeichensatzes unseres eigenen Fonts
    // (0x0020-0x015F, siehe ui_font.h) und wuerde als Luecken-/"Tofu"-Box
    // dargestellt, exakt die Fehlerklasse, die gerade erst im OEPNV-Screen
    // gefunden und behoben wurde (siehe CLAUDE.md "Pflicht: automatisches
    // Scrollen bei zu langem Text"). Der 3-Buchstaben-Code ist fuer JEDE
    // unterstuetzte Waehrung garantiert im Zeichensatz enthalten und
    // eindeutig (keine Verwechslungsgefahr zwischen z.B. CAD-$/USD-$).
    String displayPrefix(const char* vsCurrency);

    // Einmal beim Boot aufrufen (laedt von SD, siehe Config::
    // SD_CRYPTO_CURRENCY_FILE) - faellt auf "usd" zurueck, falls keine
    // Datei existiert oder der gespeicherte Code nicht (mehr) unterstuetzt
    // wird.
    void init();

    // Die aktuell gewaehlte CoinGecko-vs_currency-Code (klein, z.B. "eur"),
    // NIE leer - "usd" ist der feste Standard.
    String currentCurrency();

    // Setzt die Waehrung UND speichert sofort (Projekt-Konvention:
    // Einstellungen speichern sofort, siehe CLAUDE.md). Ungueltige/nicht
    // unterstuetzte Codes werden ignoriert (bleibt bei der vorherigen
    // Waehrung).
    void setCurrentCurrency(const String& vsCurrency);

    // Fuer die Auswahlliste (crypto_currency_screen.cpp): Anzahl/Code der
    // von CoinGecko unterstuetzten Waehrungen (dedupliziert, gaengigste
    // zuerst).
    uint8_t supportedCount();
    const char* supportedCode(uint8_t index); // "" bei ungueltigem Index
}
