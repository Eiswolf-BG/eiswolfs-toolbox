#include "settings_hub_screen.h"
#include "settings_common.h"
#include "settings_wifi_manage_screen.h"
#include "settings_units_screen.h"
#include "settings_language_screen.h"
#include "settings_display_screen.h"
#include "settings_tools_screen.h"
#include "settings_led_screen.h"
#include "menu_stars.h"
#include "ui_header.h"
#include "i18n.h"
#include "config.h"

namespace SettingsHubScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    // Wieder 6 Buttons (07.10. - "Einheiten" aus dem Tools-Untermenue
    // entfernt und als eigenstaendiger Hub-Button an die Stelle des zuvor
    // entfernten "Farben"-Buttons gesetzt, da 5 Buttons mit nur 2 statt 3
    // in der unteren Reihe unausgewogen aussahen): WLAN|Einheiten|Sprache
    // / Anzeige|Tools|LED. Sauberes 2x3-Raster wie urspruenglich.
    constexpr int16_t SIDE_MARGIN = 10;
    // Config::HEADER_HEIGHT + grosszuegigerer Abstand (14 statt 2px,
    // Alex' Auftrag, 07.10.: "mehr Abstand zwischen der Ueberschrift
    // 'System' und der ersten Button-Reihe") statt eines knappen, fast
    // anliegenden Abstands.
    constexpr int16_t GRID_TOP = (int16_t)(Config::HEADER_HEIGHT + 14);
    constexpr int16_t GAP = 8;
    constexpr int16_t BTN_W = (Config::SCREEN_WIDTH - 2 * SIDE_MARGIN - 2 * GAP) / 3;
    constexpr int16_t BTN_H = 90;

    Rect btnRect(uint8_t index) {
        uint8_t row = (uint8_t)(index / 3);
        uint8_t col = (uint8_t)(index % 3);
        return {(int16_t)(SIDE_MARGIN + col * (BTN_W + GAP)),
                (int16_t)(GRID_TOP + row * (BTN_H + GAP)), BTN_W, BTN_H};
    }

    StringId btnLabelId(uint8_t index) {
        switch (index) {
            case 0: return StringId::SETTINGS_HUB_WIFI_BTN;
            case 1: return StringId::SETTINGS_UNITS_TITLE;
            case 2: return StringId::SETTINGS_LANGUAGE_TITLE;
            case 3: return StringId::SETTINGS_HUB_DISPLAY_BTN;
            case 4: return StringId::SETTINGS_HUB_TOOLS_BTN;
            default: return StringId::SETTINGS_HUB_LED_BTN;
        }
    }

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Wie layoutWrapped() in anderen Screens, hier zentriert und als
    // Textblock vertikal um cy ausgerichtet - lokal dupliziert statt
    // geteilt (Projekt-Konvention, siehe CLAUDE.md).
    void drawWrappedCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                              int16_t lineHeight, const String& text) {
        constexpr uint8_t MAX_LINES = 2;
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

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.) - der
        // Hub hat keine konkurrierenden Buttons im Kopfbereich (kein
        // Zurueck-/Info-Button), deshalb grosszuegige maxWidth.
        UiHeader::drawCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::HEADER_HEIGHT / 2),
                                (int16_t)(Config::SCREEN_WIDTH - 20),
                                I18n::t(StringId::SETTINGS_SYSTEM_TITLE), SettingsCommon::accentColor(tft));

        for (uint8_t i = 0; i < 6; i++) {
            drawButton(tft, btnRect(i), I18n::t(btnLabelId(i)));
        }

        // Hinweistext weiter nach unten gesetzt (280 statt 250, Alex'
        // Auftrag, 07.10.: "weiter nach unten setzen, damit der ganze
        // Screen nicht so gedraengt wirkt") - passt bei 2 Reihen a 90px
        // Buttons weiterhin bequem vor den Bildschirmrand (320px Hoehe).
        tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), 280,
                             (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                             I18n::t(StringId::SETTINGS_HUB_HINT));
    }
}

void enter(TFT_eSPI& tft) {
    draw(tft);
}

void update(TFT_eSPI&) {
    // Keine periodisch wechselnden Inhalte.
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    for (uint8_t i = 0; i < 6; i++) {
        if (!btnRect(i).contains(x, y)) continue;

        switch (i) {
            case 0: SettingsWifiManageScreen::run(tft); break;
            case 1: SettingsUnitsScreen::run(tft); break;
            case 2: SettingsLanguageScreen::run(tft); break;
            case 3: SettingsDisplayScreen::run(tft); break;
            case 4: SettingsToolsScreen::run(tft); break;
            case 5: SettingsLedScreen::run(tft); break;
        }

        MenuStars::reset();
        draw(tft);
        return;
    }
}

}
