#include "first_run_location_screen.h"
#include "address_search_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>

namespace FirstRunLocationScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Lokale Kopie, siehe Konvention in location_presets_screen.cpp.
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

    // Kleiner "?"-Infobutton rechts in der Frage-Zeile, gleicher Stil
    // wie die "?"-Buttons auf den Feature-Screens (iss_radar_screen.cpp/
    // rocket_screen.cpp), hier aber inline in einer Textzeile statt frei
    // in der Bildschirmecke - Alex' Auftrag, 05.10.: komplett neue
    // Nutzer verstehen ohne Erklaerung nicht, wieso die App gleich zu
    // Beginn nach der Adresse fragt.
    void drawInfoButton(TFT_eSPI& tft, const Rect& r) {
        uint16_t accent = TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString("?", (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Modaler Erklaer-Screen, schliesst sich bei jedem Tap - gleiches
    // Muster wie iss_radar_screen.cpp/rocket_screen.cpp::showInfoScreen().
    void showWhyInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::FIRST_RUN_LOCATION_WHY_QUESTION));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::FIRST_RUN_LOCATION_WHY_BODY));

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

    // Grobe automatische Standortbestimmung ueber die oeffentliche IP
    // (ip-api.com, siehe Config::IP_GEO_HOST/IP_GEO_PATH) - 1:1 derselbe
    // Dienst wie bei Eiswolfs Flightradar CYD (dortige location_manager.cpp)
    // - Alex' Auftrag, 06.10.: Alternative zur Adresseingabe fuer alle,
    // die keine exakte Adresse eintragen wollen. Body statt direkt aus
    // http.getStream() zu parsen erst komplett eingesammelt (gleicher
    // Grund wie bei rocket_client.cpp/address_search_screen.cpp::geocode():
    // Chunked-Transfer-Encoding fuehrt sonst zu kaputtem JSON).
    bool fetchIpLocation(double& lat, double& lon, String& countryCode) {
        if (WiFi.status() != WL_CONNECTED) return false;

        WiFiClient client;
        HTTPClient http;
        String url = "http://" + String(Config::IP_GEO_HOST) + String(Config::IP_GEO_PATH);
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return false;

        int code = http.GET();
        if (code != HTTP_CODE_OK) { http.end(); return false; }

        String body = http.getString();
        http.end();

        JsonDocument doc;
        if (deserializeJson(doc, body)) return false;

        const char* status = doc["status"] | "";
        if (strcmp(status, "success") != 0) return false;

        lat = doc["lat"] | 0.0;
        lon = doc["lon"] | 0.0;
        if (lat == 0.0 && lon == 0.0) return false;
        // 2-Buchstaben-ISO-Laendercode (ip-api.com liefert "countryCode"
        // GROSS geschrieben, z.B. "DE") - fuer die standortabhaengige
        // Waehrung im Krypto-Ticker (Alex' Auftrag, 07.10., siehe
        // currency.h). LocationPresets::setCountryCode() wandelt selbst
        // in Kleinbuchstaben um.
        countryCode = String((const char*)(doc["countryCode"] | ""));
        return true;
    }

    // Zeigt "Suche laeuft..." waehrend des IP-Abrufs, fuegt bei Erfolg ein
    // (unbenanntes) Preset hinzu und aktiviert es direkt (RAM-only, siehe
    // "erst committen, dann speichern" - main.cpp). Bei Fehlschlag
    // Fehlermeldung mit "Erneut versuchen"/"Zurueck" statt stillschweigend
    // weiterzugehen (gleiches Prinzip wie AddressSearchScreen::
    // showErrorRetry()). Gibt true zurueck, wenn ein Standort erfolgreich
    // gesetzt wurde.
    bool runWifiLocationFlow(TFT_eSPI& tft) {
        while (true) {
            MenuStars::reset();
            tft.fillScreen(TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(I18n::t(StringId::ADDRESS_SEARCH_SEARCHING),
                            (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2));
            tft.setTextDatum(TL_DATUM);

            double lat = 0, lon = 0;
            String countryCode;
            if (fetchIpLocation(lat, lon, countryCode)) {
                LocationPresets::addPreset(lat, lon, "");
                uint8_t idx = LocationPresets::count();
                if (idx > 0) LocationPresets::setActiveIndex((int8_t)(idx - 1));
                LocationPresets::setSource(LocationPresets::Source::Wifi);
                LocationPresets::setCountryCode(countryCode);
                return true;
            }

            MenuStars::reset();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED, TFT_BLACK);
            layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                          I18n::t(StringId::ADDRESS_SEARCH_ERROR));

            Rect retryBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 96), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
            Rect backBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
            drawButton(tft, retryBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
            drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));

            bool retry = false;
            bool decided = false;
            while (!decided) {
                TouchInput::Point tap;
                if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
                if (retryBtn.contains(tap.x, tap.y)) { retry = true; decided = true; }
                else if (backBtn.contains(tap.x, tap.y)) { retry = false; decided = true; }
            }
            if (!retry) return false;
            // sonst: while(true) oben wiederholt den Versuch.
        }
    }
}

// Zeichnet die Auswahl (Titel/Hinweistext/"?"-Frage/2 Buttons) und wartet
// auf EINEN Tap - Rueckgabe ueber tap/whyBtn/setBtn/wifiBtn an den Aufrufer,
// der entscheidet, was als naechstes passiert. Als eigene Funktion statt
// inline in run(), damit sie nach einem abgebrochenen Adress-/WLAN-Standort-
// Versuch (siehe run() unten) einfach erneut aufgerufen werden kann, ohne
// Zeichen-Code zu duplizieren.
namespace {
    struct ChoiceResult { bool isWhy = false; bool isSet = false; bool isWifi = false; };

    ChoiceResult runChoiceScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::FIRST_RUN_LOCATION_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        int16_t textEndY = layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                          I18n::t(StringId::FIRST_RUN_LOCATION_BODY));

        // Zusaetzliche Frage-Zeile "Warum soll ich meine Adresse eingeben?"
        // mit kleinem "?"-Button daneben - Alex' Auftrag, 05.10.: neue
        // Nutzer verstehen sonst nicht, wieso die App schon in der
        // Ersteinrichtung nach der Adresse fragt. Buttons ruecken dafuer
        // entsprechend weiter nach unten.
        constexpr int16_t WHY_BTN_SIZE = 22;
        int16_t whyY = textEndY + 10;
        Rect whyBtn = {(int16_t)(Config::SCREEN_WIDTH - 10 - WHY_BTN_SIZE), whyY, WHY_BTN_SIZE, WHY_BTN_SIZE};
        layoutWrapped(tft, 10, (int16_t)(whyY + 4), (int16_t)(whyBtn.x - 10 - 10), 16,
                      I18n::t(StringId::FIRST_RUN_LOCATION_WHY_QUESTION));
        drawInfoButton(tft, whyBtn);

        // Buttons folgen direkt unter dem Text (mit etwas Abstand) statt an
        // einer fest einprogrammierten Position - eine laengere Uebersetzung
        // hat sonst den Text bis unter/hinter die Buttons laufen lassen.
        // Nach unten hin trotzdem an den Bildschirmrand geklemmt, falls ein
        // Text ausnahmsweise doch sehr lang waere.
        constexpr int16_t BTN_H = 40;
        constexpr int16_t BTN_GAP = 8;
        int16_t setY = whyBtn.y + WHY_BTN_SIZE + 14;
        int16_t maxSetY = (int16_t)(Config::SCREEN_HEIGHT - (2 * BTN_H + BTN_GAP + 10));
        if (setY > maxSetY) setY = maxSetY;

        Rect setBtn  = {10, setY, (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
        Rect wifiBtn = {10, (int16_t)(setY + BTN_H + BTN_GAP), (int16_t)(Config::SCREEN_WIDTH - 20), BTN_H};
        drawButton(tft, setBtn, I18n::t(StringId::FIRST_RUN_LOCATION_SET_BTN));
        drawButton(tft, wifiBtn, I18n::t(StringId::FIRST_RUN_LOCATION_SKIP_BTN));

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            ChoiceResult r;
            if (whyBtn.contains(tap.x, tap.y)) { r.isWhy = true; return r; }
            if (setBtn.contains(tap.x, tap.y)) { r.isSet = true; return r; }
            if (wifiBtn.contains(tap.x, tap.y)) { r.isWifi = true; return r; }
        }
    }
}

void run(TFT_eSPI& tft) {
    // Schleife statt eines einzelnen Durchlaufs (Alex' Nachdruck, 06.10.):
    // ein abgebrochener Adress-/WLAN-Standort-Versuch (z.B. Fehltipp auf
    // "Abbrechen" waehrend der Tastatureingabe, oder ein gescheiterter
    // IP-Abruf) darf NIEMALS still weiterrutschen zum naechsten
    // Ersteinrichtungs-Schritt - nur ein ECHTER Erfolg (Adresse bestaetigt
    // ODER WLAN-Standort erfolgreich ermittelt) darf diese Funktion
    // verlassen. Bei Abbruch wird einfach die Auswahl erneut gezeigt.
    while (true) {
        ChoiceResult choice = runChoiceScreen(tft);

        if (choice.isWhy) {
            showWhyInfoScreen(tft);
            continue;
        }

        if (choice.isSet) {
            if (AddressSearchScreen::run(tft)) {
                // Direkt aktivieren - das ist der ganze Sinn dieses
                // Screens (sofort beim ersten Start den praezisen
                // Standort nutzen, statt weiter auf Auto/IP zu bleiben).
                uint8_t idx = LocationPresets::count();
                if (idx > 0) LocationPresets::setActiveIndex((int8_t)(idx - 1));
                LocationPresets::setSource(LocationPresets::Source::Address);
                return;
            }
            // Abgebrochen (Tastatur-Cancel, kein Ergebnis bestaetigt) -
            // zurueck zur Auswahl, KEIN stilles Weiterrutschen.
            continue;
        }

        if (choice.isWifi) {
            if (runWifiLocationFlow(tft)) return;
            continue;
        }
    }
}

}
