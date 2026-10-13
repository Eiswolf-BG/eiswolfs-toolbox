#include "crypto_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"
#include "crypto_client.h"
#include "net_task.h"
#include "currency.h"
#include "crypto_currency_screen.h"

namespace CryptoScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Wortweiser Umbruch anhand echter Pixelbreite - gleiches Muster wie
    // in rocket_screen.cpp/iss_radar_screen.cpp, hier lokal dupliziert
    // statt geteilt (Projekt-Konvention, siehe CLAUDE.md).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;
            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }
            tft.setCursor(x, y);
            tft.print(line);
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    void drawWrappedCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                              int16_t lineHeight, const String& text) {
        constexpr uint8_t MAX_LINES = 4;
        String lines[MAX_LINES];
        uint8_t lineCount = 0;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len && lineCount < MAX_LINES) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;
            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }
            lines[lineCount++] = line;
            start += line.length();
        }
        tft.setTextDatum(MC_DATUM);
        int16_t top = (int16_t)(cy - ((lineCount - 1) * lineHeight) / 2);
        for (uint8_t i = 0; i < lineCount; i++) {
            tft.drawString(lines[i], cx, (int16_t)(top + i * lineHeight));
        }
        tft.setTextDatum(TL_DATUM);
    }

    // Tausendertrennzeichen fuer USD-Betraege (eigener kleiner Helfer, es
    // gab dafuer noch keinen im Projekt) - rein ganzzahlig, keine
    // Lokalisierung des Trennzeichens (Komma statt Punkt), da CoinGecko
    // selbst USD-Betraege ebenfalls mit Komma gruppiert anzeigt und die
    // Waehrung (USD) ohnehin sprachunabhaengig literal bleibt.
    String formatThousands(long value) {
        bool negative = value < 0;
        if (negative) value = -value;
        String digits = String(value);
        String result;
        int count = 0;
        for (int i = (int)digits.length() - 1; i >= 0; i--) {
            result = digits[i] + result;
            count++;
            if (count % 3 == 0 && i != 0) result = "," + result;
        }
        return negative ? ("-" + result) : result;
    }

    // Preise unter 1 Einheit der Waehrung brauchen mehr Nachkommastellen
    // (viele Altcoins kosten Bruchteile eines Cents), ab 1 reichen 2
    // Nachkommastellen plus Tausendertrennzeichen - gaengige Konvention in
    // Krypto-Apps. "prefix" statt fest "$" (Alex' Auftrag, 07.10. -
    // standortabhaengige Waehrung, siehe currency.h) - Zahlenformatierung
    // selbst bewusst unveraendert gelassen, nur das Symbol/Praefix ist
    // jetzt variabel.
    String formatCoinPrice(double value, const String& prefix) {
        if (value >= 1.0) {
            long whole = (long)value;
            int cents = (int)((value - (double)whole) * 100.0 + 0.5);
            if (cents >= 100) { whole += 1; cents = 0; }
            char buf[8];
            snprintf(buf, sizeof(buf), ".%02d", cents);
            return prefix + formatThousands(whole) + String(buf);
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%.4f", value);
        return prefix + String(buf);
    }

    String formatChangePercent(double pct) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%+.2f%%", pct);
        return String(buf);
    }

    // BUGFIX (13.10., Alex' Fund per Foto, gleicher Fehler wie bei
    // drawCoinTile() oben) - bei Waehrungen mit sehr grossen Zahlenwerten
    // (z.B. IDR/VND) kann selbst formatCoinPrice() bei Groesse 1 noch zu
    // breit fuer die schmale Listenzeile in showMoreCoinsScreen() werden
    // und in die rechte Prozent-Spalte hineinlaufen. Laesst zuerst die
    // Nachkommastellen weg, dann zusaetzlich das Waehrungspraefix, bis der
    // Text in maxWidth passt (Kontext liefert in dem Fall weiterhin der
    // Waehrungs-Button oben auf der Hauptseite).
    String formatCoinPriceFitting(TFT_eSPI& tft, double value, const String& prefix, int16_t maxWidth) {
        String full = formatCoinPrice(value, prefix);
        if (tft.textWidth(full) <= maxWidth) return full;

        long whole = (long)(value + 0.5);
        String noCents = prefix + formatThousands(whole);
        if (tft.textWidth(noCents) <= maxWidth) return noCents;

        String noPrefix = formatThousands(whole);
        return noPrefix;
    }

    // Fear&Greed-Klassifikation: primaer ueber den rohen API-Text
    // (value_classification, feste 5er-Tabelle laut alternative.me-Doku)
    // abgeglichen - faellt auf den rohen Text zurueck, falls die API
    // ausnahmsweise einen unbekannten Wert liefert (Alex' Auftrag:
    // "lieber den rohen API-Text anzeigen statt abzustuerzen oder nichts
    // anzuzeigen"), gleiches Prinzip wie bei den F1-/Rocket-Status-Tabellen.
    StringId fngClassificationStringId(const String& raw) {
        if (raw == "Extreme Fear") return StringId::CRYPTO_FNG_EXTREME_FEAR;
        if (raw == "Fear") return StringId::CRYPTO_FNG_FEAR;
        if (raw == "Neutral") return StringId::CRYPTO_FNG_NEUTRAL;
        if (raw == "Greed") return StringId::CRYPTO_FNG_GREED;
        if (raw == "Extreme Greed") return StringId::CRYPTO_FNG_EXTREME_GREED;
        return StringId::COUNT;
    }

    String fngText(const CryptoClient::FearGreed& fng) {
        StringId id = fngClassificationStringId(fng.rawClassification);
        return (id != StringId::COUNT) ? String(I18n::t(id)) : fng.rawClassification;
    }

    // Rot (Extreme Angst) bis Gruen (Extreme Gier) - literal/status-
    // gebundene Farbskala wie bei den AQI-Kategorien, NICHT das Akzentgruen
    // (eigene Bedeutung, siehe CLAUDE.md). Nur benannte TFT_eSPI-Konstanten
    // verwendet, keine geratenen Hex-Werte.
    uint16_t fngColor(const String& raw) {
        if (raw == "Extreme Fear") return TFT_RED;
        if (raw == "Fear") return TFT_ORANGE;
        if (raw == "Neutral") return TFT_YELLOW;
        if (raw == "Greed") return TFT_GREENYELLOW;
        if (raw == "Extreme Greed") return TFT_GREEN;
        return TFT_WHITE;
    }

    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 10);
    constexpr int16_t COIN_TILE_H = 64;
    constexpr int16_t TILE_GAP = 8;
    constexpr int16_t BONUS_ROW_TOP = (int16_t)(CONTENT_TOP + 2 * COIN_TILE_H + TILE_GAP + TILE_GAP);
    constexpr int16_t BONUS_TILE_H = 78;
    constexpr int16_t BONUS_TILE_W = (int16_t)((Config::SCREEN_WIDTH - 20 - TILE_GAP) / 2);

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    // Gleiches Zentrierungs-Prinzip wie rocket_screen.cpp::HEADER_TITLE_*
    // (Mitte der vom "?"-Button freien Zone, nicht der wahren
    // Bildschirmmitte) - lokal dupliziert (Projekt-Konvention).
    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    Rect btcTileRect() { return Rect{10, CONTENT_TOP, (int16_t)(Config::SCREEN_WIDTH - 20), COIN_TILE_H}; }
    Rect ethTileRect() {
        return Rect{10, (int16_t)(CONTENT_TOP + COIN_TILE_H + TILE_GAP),
                    (int16_t)(Config::SCREEN_WIDTH - 20), COIN_TILE_H};
    }
    // Tap-Flaeche "Hauptflaeche" (Alex' Auftrag: "Tap auf die Hauptflaeche
    // oeffnet eine 'Weitere Coins'-Liste") - deckt beide Muenz-Kacheln als
    // EINE zusammenhaengende Zone ab, inkl. der kleinen Luecke dazwischen.
    Rect mainAreaRect() {
        Rect b = btcTileRect();
        Rect e = ethTileRect();
        return Rect{b.x, b.y, b.w, (int16_t)(e.y + e.h - b.y)};
    }
    Rect mempoolTileRect() { return Rect{10, BONUS_ROW_TOP, BONUS_TILE_W, BONUS_TILE_H}; }
    Rect fngTileRect() {
        return Rect{(int16_t)(10 + BONUS_TILE_W + TILE_GAP), BONUS_ROW_TOP, BONUS_TILE_W, BONUS_TILE_H};
    }

    // Manuelle Waehrungsauswahl (Alex' Auftrag, 12.10., siehe
    // crypto_currency_screen.h) - unterhalb der Bonus-Kacheln, letzte
    // verbleibende freie Zeile auf dem Screen (nachgerechnet: BONUS_ROW_TOP
    // + BONUS_TILE_H = 276px bei Standardaufloesung, Bildschirm endet bei
    // 320px - 38px Rest reichen knapp fuer eine Zeile mit kleinem Rand,
    // nachgerechnet statt geraten).
    constexpr int16_t CURRENCY_BTN_GAP = 6;
    constexpr int16_t CURRENCY_BTN_H = 34;
    Rect currencyBtnRect() {
        return Rect{10, (int16_t)(BONUS_ROW_TOP + BONUS_TILE_H + CURRENCY_BTN_GAP),
                    (int16_t)(Config::SCREEN_WIDTH - 20), CURRENCY_BTN_H};
    }

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::CRYPTO_TITLE), TFT_GREEN);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::CRYPTO_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    // Eigenname + grosser Preis (Weiss, Pflicht: abgefragte Daten in
    // Weiss, siehe CLAUDE.md) + 24h-Aenderung in Gruen/Rot (literal/
    // status-gebunden - weltweit uebliche Konvention in Krypto-Apps,
    // NICHT die Akzentfarbe, Alex' ausdruecklicher Auftrag).
    // Transparenter Text (kein zweites TFT_BLACK-Hintergrund-Argument)
    // statt wie urspruenglich opak hinterlegt (Alex' Fund, 07.10.: TFT_eSPIs
    // automatisches Loeschrechteck fuer die grosse Preis-/Aenderungs-Zeile
    // (Groesse 2, nah an der Box-Unterkante) riss ein Stueck aus der
    // gerade erst gezeichneten unteren Rahmenlinie heraus - exakt derselbe
    // Fehler wie zuvor bei der AQI-Zahl im Wetter-Screen) - unproblematisch,
    // da drawCoinTile() die gesamte Kachel bei JEDEM Aufruf ohnehin
    // komplett schwarz vorfuellt, ein manuelles Nach-Loeschen ist hier nie
    // noetig.
    void drawCoinTile(TFT_eSPI& tft, const Rect& r, const char* name, const CryptoClient::MainCoin& coin,
                       const String& pricePrefix) {
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(1);
        tft.drawString(name, (int16_t)(r.x + 10), (int16_t)(r.y + 14));

        if (!coin.valid) {
            tft.setTextColor(TFT_WHITE);
            tft.drawString(I18n::t(StringId::CRYPTO_LOADING), (int16_t)(r.x + 10), (int16_t)(r.y + r.h - 18));
            tft.setTextDatum(TL_DATUM);
            return;
        }

        // Preis UND 24h-Aenderung jetzt auf getrennten Zeilen statt
        // nebeneinander (Alex' Fund, 12.10., Foto: waehrungspraefigierte
        // Preise wie "USD 82,830.00" sind bei Groesse 2 oft zu breit, um
        // links neben der rechtsbuendigen Prozentangabe noch Platz zu
        // lassen - ueberschnitt sich je nach Waehrung/Kursstand). Eine
        // eigene Zeile fuer die Prozentangabe macht die Breite des Preises
        // unkritisch, unabhaengig davon wie lang das Waehrungspraefix ist.
        // BUGFIX (13.10., Alex' Fund per Foto: "CAD 116,020.00" lief bei
        // Groesse 2 rechts aus der Kachel heraus) - Praefix+Zahl ist je
        // nach Waehrung (laengere Codes wie "CAD"/kuerzere wie "USD") UND
        // Kursstand unterschiedlich breit, nie ungeprueft bei fester
        // Groesse gezeichnet. Faellt auf Groesse 1 zurueck, wenn der Text
        // bei Groesse 2 nicht in die Kachelbreite passt - exakt das
        // gleiche Prinzip wie UiHeader::drawCentered()/drawLeft() fuer die
        // Screen-Ueberschriften (siehe CLAUDE.md "Pflicht: einheitliche
        // grosse Ueberschrift").
        String priceText = formatCoinPrice(coin.priceUsd, pricePrefix);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        int16_t maxPriceW = (int16_t)(r.w - 20);
        if (tft.textWidth(priceText) > maxPriceW) {
            tft.setTextSize(1);
        }
        tft.drawString(priceText, (int16_t)(r.x + 10), (int16_t)(r.y + 38));
        tft.setTextSize(1);

        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(coin.change24hPercent >= 0 ? TFT_GREEN : TFT_RED);
        tft.drawString(formatChangePercent(coin.change24hPercent),
                        (int16_t)(r.x + r.w - 10), (int16_t)(r.y + r.h - 10));

        tft.setTextDatum(TL_DATUM);
    }

    // Kompakte Werte ohne eigene Wort-Label fuer Blockhoehe/Gebuehr (nur
    // "#"/"~"-Praefix + literal-technische Einheit "sat/vB", bewusst KEINE
    // uebersetzten Labels) - ein kombiniertes "Label: Wert" auf einer
    // Zeile wuerde bei der schmalen Kachelbreite (106px) in mind. einer
    // der 4 Sprachen ueberlaufen (nachgerechnet, nicht geraten); die
    // Kachel selbst traegt den Kontext bereits ueber "Mempool" im Kopf.
    // Transparenter Text (kein zweites TFT_BLACK-Argument), konsistent mit
    // drawCoinTile()/drawFngTile() (Alex' Fund, 07.10.: moeglicher Rest
    // von weissem Text neben der F&G-Kachel - diese Funktion war die
    // einzige der vier Kacheln, die noch opak gezeichnet hat, als
    // plausibler Mitverursacher jetzt ebenfalls korrigiert, gleiche
    // Begruendung wie bei drawCoinTile() oben: die Kachel wird bei JEDEM
    // Aufruf ohnehin komplett schwarz vorgefuellt).
    void drawMempoolTile(TFT_eSPI& tft, const Rect& r, const CryptoClient::Mempool& m) {
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(1);
        tft.drawString("Mempool", (int16_t)(r.x + 8), (int16_t)(r.y + 14));

        if (!m.valid) {
            tft.setTextColor(TFT_WHITE);
            tft.drawString(I18n::t(StringId::CRYPTO_LOADING), (int16_t)(r.x + 8), (int16_t)(r.y + r.h - 20));
            tft.setTextDatum(TL_DATUM);
            return;
        }

        tft.setTextColor(TFT_WHITE);
        tft.drawString("#" + formatThousands(m.blockHeight), (int16_t)(r.x + 8), (int16_t)(r.y + 36));
        tft.drawString("~" + String(m.fastestFeeSatPerVb) + " sat/vB", (int16_t)(r.x + 8), (int16_t)(r.y + 56));

        tft.setTextDatum(TL_DATUM);
    }

    void drawFngTile(TFT_eSPI& tft, const Rect& r, const CryptoClient::FearGreed& fng) {
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);

        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_GREEN);
        tft.setTextSize(1);
        // Sprachabhaengiges 3-Zeichen-Kuerzel statt fest "F&G" (Alex'
        // Fund, 07.10.: ohne Englischkenntnisse nicht verstaendlich) -
        // CRYPTO_FNG_TILE_LABEL liefert je Sprache die passende
        // Abkuerzung (DE "A&G" fuer Angst&Gier, ES "M&C" fuer
        // Miedo&Codicia, FR "P&A" fuer Peur&Avidité, EN weiterhin "F&G").
        tft.drawString(I18n::t(StringId::CRYPTO_FNG_TILE_LABEL), (int16_t)(r.x + 8), (int16_t)(r.y + 14));

        if (!fng.valid) {
            tft.setTextColor(TFT_WHITE);
            tft.drawString(I18n::t(StringId::CRYPTO_LOADING), (int16_t)(r.x + 8), (int16_t)(r.y + r.h - 20));
            tft.setTextDatum(TL_DATUM);
            return;
        }

        // Transparenter Text (siehe drawCoinTile()-Kommentar oben, gleiche
        // Begruendung) PLUS groesserer Abstand zwischen Zahl und
        // Klassifikationstext (Alex' Fund, 07.10.: "71" und "Gier"
        // ueberschnitten sich unleserlich - derselbe Baseline/Ascent-
        // Fehler wie zuvor bei ISS-Radar/Rocket-Screen behoben: der
        // bisherige 14px-Abstand zwischen den beiden Baselines liess der
        // Klassifikationszeile keinerlei Reserve fuer ihren eigenen
        // Ascent). Erste Klassifikationszeile jetzt bei y+58 statt y+50.
        uint16_t col = fngColor(fng.rawClassification);
        tft.setTextColor(col);
        tft.setTextSize(2);
        tft.drawString(String((int)fng.value), (int16_t)(r.x + 8), (int16_t)(r.y + 36));
        tft.setTextSize(1);

        // Klassifikationswort ueber layoutWrapped() statt einer
        // ungeprueften Einzeiler-Annahme (Pflicht: Textbreite in ALLEN 4
        // Sprachen verifizieren, siehe CLAUDE.md) - "Codicia Extrema"/
        // "Extreme Angst" etc. passen nicht zuverlaessig in 90px auf
        // einer Zeile, der Umbruch faengt das automatisch ab.
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(col);
        layoutWrapped(tft, (int16_t)(r.x + 8), (int16_t)(r.y + 58), (int16_t)(r.w - 16), 13, fngText(fng));
    }

    // BUGFIX (13.10., Alex' Fund: "stelle ich GBP ein, zeigt es EUR" / bei
    // "Automatisch" "bleibt es beim eingestellten Wert"): live per Serial-
    // Mitschnitt diagnostiziert - die Waehrungs-Auswahl selbst (Tap ->
    // setManualOverride() -> NetTask::setCryptoCurrency() -> Refetch)
    // funktionierte in JEDEM Fall korrekt. Der eigentliche Effekt: schlug
    // ein einzelner Abruf fehl (z.B. CoinGecko-Rate-Limit bei schnell
    // hintereinander gewechselten Waehrungen, hier live beobachtet),
    // behaelt data.vsCurrency stillschweigend den Wert der letzten
    // ERFOLGREICHEN Waehrung (net_task.cpp aktualisiert vsCurrency nur bei
    // pricesOk) - das sah fuer Alex exakt wie "meine Wahl wird ignoriert"
    // aus, obwohl die Wahl technisch korrekt gespeichert war, nur der
    // zugehoerige Abruf noch aussteht/fehlgeschlagen ist. Fix: die
    // tatsaechlich GEWUENSCHTE Waehrung separat mitfuehren (siehe
    // desiredCryptoCurrency unten) und anzeigen, wenn sie (noch) nicht mit
    // den gerade dargestellten Daten uebereinstimmt - macht den
    // Zwischenzustand sichtbar statt ihn stillschweigend zu verstecken.
    String desiredCryptoCurrency = "usd";

    // Zeigt den Code der tatsaechlich AKTIVEN Waehrung (data.vsCurrency,
    // von NetTask bereits unter Beruecksichtigung einer evtl. manuellen
    // Ueberschreibung ermittelt, siehe enter() unten) - nicht separat neu
    // berechnet, damit die Anzeige immer zur gerade abgefragten Waehrung
    // passt. Haengt bei einer noch ausstehenden/fehlgeschlagenen
    // Umstellung " ..." an, siehe Kommentar oben.
    void drawCurrencyButton(TFT_eSPI& tft, const NetTask::CryptoData& data) {
        String code = String(data.vsCurrency);
        code.toUpperCase();
        String desired = desiredCryptoCurrency;
        desired.toUpperCase();
        bool pending = code != desired;
        String label = String(I18n::t(StringId::CRYPTO_CURRENCY_BTN_LABEL)) + ": " + code;
        if (pending) label += " ...";
        drawButton(tft, currencyBtnRect(), label);
    }

    void drawTiles(TFT_eSPI& tft, const NetTask::CryptoData& data) {
        String pricePrefix = Currency::displayPrefix(data.vsCurrency);
        drawCoinTile(tft, btcTileRect(), "Bitcoin", data.btc, pricePrefix);
        drawCoinTile(tft, ethTileRect(), "Ethereum", data.eth, pricePrefix);
        drawMempoolTile(tft, mempoolTileRect(), data.mempool);
        drawFngTile(tft, fngTileRect(), data.fearGreed);
        drawCurrencyButton(tft, data);
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::CRYPTO_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::CRYPTO_INFO_BODY));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(StringId::BACK_BTN), (int16_t)(Config::SCREEN_WIDTH / 2),
                        (int16_t)(Config::SCREEN_HEIGHT - 20));
        tft.setTextDatum(TL_DATUM);

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) return;
            MenuStars::update(tft);
            delay(20);
        }
    }

    // ---- "Weitere Coins" (Alex' Auftrag, 07.10.) - Top-10-Liste nach
    // Marktkapitalisierung, BTC/ETH werden uebersprungen (schon oben zu
    // sehen). Gleiches Button-/Listen-Muster wie "Weitere Starts" im
    // Rocket-Screen (rocket_screen.cpp::showMoreLaunchesScreen()), hier
    // aber als EINZEILIGE Zeilen (Name links per Marquee, Preis+Aenderung
    // rechts) statt zweizeiliger Boxen - 8 moegliche Zeilen (Top 10 minus
    // BTC/ETH) passen dadurch ohne eigene Scroll-Infrastruktur auf den
    // Bildschirm. ----
    void showMoreCoinsScreen(TFT_eSPI& tft, const NetTask::CryptoData& data) {
        MenuStars::reset();

        CryptoClient::CoinMarket others[Config::CRYPTO_MAX_MARKETS];
        uint8_t otherCount = 0;
        for (uint8_t i = 0; i < data.marketCount; i++) {
            const CryptoClient::CoinMarket& m = data.markets[i];
            if (!m.valid) continue;
            if (m.id == "bitcoin" || m.id == "ethereum") continue;
            others[otherCount++] = m;
        }

        static MarqueeLabel::State nameMarquee[Config::CRYPTO_MAX_MARKETS];
        for (uint8_t i = 0; i < otherCount; i++) MarqueeLabel::reset(nameMarquee[i]);

        constexpr int16_t LIST_TOP = 40;
        constexpr int16_t ROW_H = 25;
        constexpr int16_t ROW_GAP = 3;
        constexpr int16_t NAME_X = 10;
        // NAME_W/PRICE_X enger gefasst als urspruenglich (90/104) - Alex'
        // Auftrag, 07.10.: standortabhaengige Waehrungspraefixe wie "EUR "
        // sind laenger als das bisherige feste "$" und brauchten mehr
        // Platz, nachgerechnet statt geraten (max. realistische Breite
        // "EUR 1,322.84" o.ae. bei Groesse 1 ~77px, passt in die 85px bis
        // zur rechten Aenderungs-Spalte).
        constexpr int16_t NAME_W = 75;
        constexpr int16_t PRICE_X = 90;
        constexpr int16_t CHANGE_RIGHT_X = (int16_t)(Config::SCREEN_WIDTH - 10);

        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::CRYPTO_MORE_COINS_TITLE));

        String pricePrefix = Currency::displayPrefix(data.vsCurrency);

        if (otherCount == 0) {
            drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                                 (int16_t)(Config::SCREEN_WIDTH - 40), 16, I18n::t(StringId::CRYPTO_LOADING));
        }

        for (uint8_t i = 0; i < otherCount; i++) {
            int16_t rowCy = (int16_t)(LIST_TOP + i * (ROW_H + ROW_GAP) + ROW_H / 2);

            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(others[i].change24hPercent >= 0 ? TFT_GREEN : TFT_RED, TFT_BLACK);
            tft.drawString(formatChangePercent(others[i].change24hPercent), CHANGE_RIGHT_X, rowCy);

            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            // CHANGE_COL_RESERVE_W: Platz fuer "+123.45%" (worst case, 8
            // Zeichen) plus kleiner Luecke - siehe formatCoinPriceFitting()-
            // Kommentar oben.
            constexpr int16_t CHANGE_COL_RESERVE_W = 62;
            int16_t maxPriceW = (int16_t)(CHANGE_RIGHT_X - CHANGE_COL_RESERVE_W - PRICE_X);
            tft.drawString(formatCoinPriceFitting(tft, others[i].priceUsd, pricePrefix, maxPriceW), PRICE_X, rowCy);

            tft.setTextDatum(TL_DATUM);
        }

        drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap) && backBtn.contains(tap.x, tap.y)) return;

            for (uint8_t i = 0; i < otherCount; i++) {
                int16_t rowCy = (int16_t)(LIST_TOP + i * (ROW_H + ROW_GAP) + ROW_H / 2);
                MarqueeLabel::draw(tft, nameMarquee[i], NAME_X, rowCy, NAME_W, (int16_t)(ROW_H - 2),
                                    others[i].name, TFT_WHITE, TFT_BLACK);
            }
            MenuStars::update(tft);
            delay(20);
        }
    }

    bool haveData = false;
    uint32_t lastSeenCryptoVersion = 0;
    uint32_t lastSeenFngVersion = 0;
    NetTask::Status lastSeenCryptoStatus = NetTask::Status::Pending;
    NetTask::CryptoData lastData;
}

void enter(TFT_eSPI& tft) {
    // Rein manuelle Waehrungswahl (Alex' Auftrag, 07.10., vereinfacht
    // 13.10. - keine Standort-Zuordnung mehr, siehe currency.h) - einmal
    // pro enter() an NetTask (Core 0) weitergereicht, NetTask liest
    // LocationPresets NIEMALS selbst (siehe net_task.h-Kommentar).
    String currentCur = Currency::currentCurrency();
    desiredCryptoCurrency = currentCur;
    NetTask::setCryptoCurrency(currentCur.c_str());

    drawChrome(tft);
    if (haveData) {
        drawTiles(tft, lastData);
    } else {
        drawCenteredHint(tft, StringId::CRYPTO_LOADING);
    }
}

void update(TFT_eSPI& tft) {
    uint32_t v = NetTask::cryptoVersion();
    uint32_t vf = NetTask::cryptoFngVersion();
    bool changed = false;

    if (v != lastSeenCryptoVersion || vf != lastSeenFngVersion) {
        NetTask::CryptoData newData;
        if (NetTask::getCryptoData(newData)) {
            lastSeenCryptoVersion = v;
            lastSeenFngVersion = vf;
            lastData = newData;
            haveData = true;
            changed = true;
        }
    }

    if (!haveData) {
        NetTask::Status st = NetTask::cryptoStatus();
        if (st != lastSeenCryptoStatus) {
            lastSeenCryptoStatus = st;
            if (st == NetTask::Status::Error) {
                drawCenteredHint(tft, StringId::CRYPTO_FETCH_ERROR);
            }
        }
        return;
    }

    if (changed) {
        drawTiles(tft, lastData);
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        drawChrome(tft);
        if (haveData) {
            drawTiles(tft, lastData);
        } else {
            drawCenteredHint(tft, StringId::CRYPTO_LOADING);
        }
        return;
    }

    if (haveData && mainAreaRect().contains(x, y)) {
        showMoreCoinsScreen(tft, lastData);
        MenuStars::reset();
        drawChrome(tft);
        drawTiles(tft, lastData);
        return;
    }

    if (haveData && currencyBtnRect().contains(x, y)) {
        CryptoCurrencyScreen::run(tft);
        // Evtl. geaenderte manuelle Waehrung sofort an NetTask weiterreichen
        // (Alex' Auftrag, 12.10.) - gleiche Logik wie enter(), net_task.cpp
        // erkennt die geaenderte Waehrung selbst und fordert beim naechsten
        // Durchlauf sofort einen frischen Abruf an statt das reguläre
        // 2,5-Minuten-Intervall abzuwarten.
        String currentCur = Currency::currentCurrency();
        desiredCryptoCurrency = currentCur;
        NetTask::setCryptoCurrency(currentCur.c_str());

        MenuStars::reset();
        drawChrome(tft);
        drawTiles(tft, lastData);
        return;
    }
}

}
