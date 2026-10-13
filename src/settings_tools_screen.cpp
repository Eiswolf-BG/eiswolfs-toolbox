#include "settings_tools_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "address_search_screen.h"
#include "location_presets.h"
#include "calibration_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

namespace SettingsToolsScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    void drawInfoButton(TFT_eSPI& tft, const Rect& r) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString("?", (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

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

    // 31x31 - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen, gleiche Position
    // wie bei den anderen Untermenues (settings_wifi_manage_screen.cpp).
    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::SETTINGS_TOOLS_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                      I18n::t(StringId::SETTINGS_TOOLS_INFO_BODY));

        tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
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

    // Zeigt, was aktuell als allgemeine Toolbox-Position gespeichert ist
    // (Alex' Auftrag, 06.10.) - ein vom Nutzer vergebener Name, sonst die
    // Koordinaten, sonst (falls per WLAN-Ortung statt Adresse ermittelt,
    // siehe LocationPresets::Source) ein entsprechender Hinweistext. Die
    // tatsaechlich eingegebene Adresse selbst wird NIRGENDS dauerhaft
    // gespeichert (nur Koordinaten + optionaler Name) - kann deshalb hier
    // nicht woertlich angezeigt werden.
    String currentLocationText() {
        if (LocationPresets::count() == 0) {
            return String(I18n::t(StringId::SETTINGS_TOOLS_NO_LOCATION));
        }
        int8_t active = LocationPresets::activeIndex();
        uint8_t idx = (active >= 0) ? (uint8_t)active : 0;

        String name = LocationPresets::getName(idx);
        String label = I18n::t(StringId::SETTINGS_TOOLS_CURRENT_LABEL);
        if (name.length() > 0) return label + name;

        if (LocationPresets::source() == LocationPresets::Source::Wifi) {
            return label + I18n::t(StringId::SETTINGS_TOOLS_WIFI_LOCATION);
        }

        double lat = 0, lon = 0;
        LocationPresets::getLatLon(idx, lat, lon);
        char buf[32];
        snprintf(buf, sizeof(buf), "%.4f, %.4f", lat, lon);
        return label + String(buf);
    }

    constexpr int16_t CHANGE_BTN_Y = 140;
    constexpr int16_t CHANGE_BTN_H = 44;
    Rect changeBtnRect() { return {10, CHANGE_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), CHANGE_BTN_H}; }

    // Touch-Kalibrierung (Alex' Auftrag, 07.10. - dieselbe 4-Punkte-
    // Routine wie bei der Ersteinrichtung, jetzt zusaetzlich jederzeit
    // hier erreichbar) - eigener Button unterhalb von "Adresse aendern",
    // kein zusaetzlicher Status-Text noetig (anders als bei der Adresse,
    // es gibt keinen sinnvoll anzuzeigenden "aktuellen Kalibrierungswert").
    constexpr int16_t CALIBRATE_BTN_Y = (int16_t)(CHANGE_BTN_Y + CHANGE_BTN_H + 20);
    constexpr int16_t CALIBRATE_BTN_H = 44;
    Rect calibrateBtnRect() { return {10, CALIBRATE_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), CALIBRATE_BTN_H}; }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);
        drawInfoButton(tft, infoBtnRect());

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.) -
        // engere maxWidth, da hier (anders als bei den anderen
        // Untermenues) zusaetzlich ein "?"-Button im Kopfbereich sitzt.
        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 117,
                            I18n::t(StringId::SETTINGS_HUB_TOOLS_BTN), SettingsCommon::accentColor(tft));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, 90, (int16_t)(Config::SCREEN_WIDTH - 20), 16, currentLocationText());

        drawButton(tft, changeBtnRect(), I18n::t(StringId::SETTINGS_TOOLS_CHANGE_ADDRESS_BTN));
        drawButton(tft, calibrateBtnRect(), I18n::t(StringId::CALIB_TITLE));
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    draw(tft);

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
        if (SettingsCommon::backButtonRect().contains(tap.x, tap.y)) return;

        if (infoBtnRect().contains(tap.x, tap.y)) {
            showInfoScreen(tft);
            draw(tft);
            continue;
        }

        if (changeBtnRect().contains(tap.x, tap.y)) {
            if (AddressSearchScreen::run(tft)) {
                // AddressSearchScreen::run() hat bereits ein neues Preset
                // hinzugefuegt UND aktiviert. Dessen Daten sichern, ALLE
                // Presets entfernen und nur dieses eine wieder anlegen -
                // damit es bei genau EINER allgemeinen Toolbox-Position
                // bleibt (Alex' Auftrag: "diese EINE, allgemeine
                // Toolbox-Position"), statt bei wiederholter Nutzung
                // Presets bis zum Limit (3) anzusammeln.
                int8_t active = LocationPresets::activeIndex();
                uint8_t idx = (active >= 0) ? (uint8_t)active : (uint8_t)(LocationPresets::count() - 1);
                double lat = 0, lon = 0;
                LocationPresets::getLatLon(idx, lat, lon);
                String name = LocationPresets::getName(idx);

                while (LocationPresets::count() > 0) LocationPresets::removePreset(0);
                LocationPresets::addPreset(lat, lon, name);
                LocationPresets::setActiveIndex(0);
                LocationPresets::setSource(LocationPresets::Source::Address);
                // Sofort speichern (Alex' Auftrag, 06.10.: Einstellungen
                // speichern sofort, nicht erst bei einem spaeteren Commit
                // wie in der Ersteinrichtung).
                LocationPresets::commit();
            }
            MenuStars::reset();
            draw(tft);
            continue;
        }

        if (calibrateBtnRect().contains(tap.x, tap.y)) {
            // BUGFIX (12.10., Alex' Meldung "Touch nach Kalibrierung UND
            // Neustart weiter kaputt"): CalibrationScreen::run() setzt die
            // Kalibrierung bewusst NUR im RAM (TouchInput::setCalibration()),
            // NICHT auf der SD-Karte - das ist fuer die Ersteinrichtung so
            // gedacht ("erst committen, dann speichern", siehe main.cpp:
            // dort folgt TouchInput::saveCalibration() an spaeterer Stelle
            // im eigentlichen Commit-Schritt). Der vorherige Code hier ging
            // faelschlich davon aus, run() wuerde selbst speichern - dadurch
            // ging jede ueber dieses Menue durchgefuehrte Kalibrierung beim
            // naechsten Neustart wieder verloren (calibration.txt existierte
            // nie), obwohl sie INNERHALB derselben Sitzung noch zu
            // funktionieren schien (RAM-Zustand war ja aktiv). Deshalb hier
            // zusaetzlich explizit speichern.
            CalibrationScreen::run(tft);
            TouchInput::saveCalibration();
            return;
        }
    }
}

}
