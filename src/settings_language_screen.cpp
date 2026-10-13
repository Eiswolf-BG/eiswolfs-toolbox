#include "settings_language_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

namespace SettingsLanguageScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Button-Bereich beginnt unterhalb des Zurueck-Buttons/Titels und
    // nutzt wieder die volle restliche Bildschirmhoehe (kein unterer
    // Streifen mehr reserviert, siehe settings_common.h).
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t START_Y = 70;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr uint8_t ROW_COUNT = I18n::LANG_COUNT;
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.) -
        // kein Info-Button im Kopfbereich dieses Screens.
        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 148,
                            I18n::t(StringId::SETTINGS_LANGUAGE_TITLE), SettingsCommon::accentColor(tft));

        uint8_t current = I18n::language();
        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            drawButton(tft, rowRect(i), I18n::languageName(i), i == current);
        }
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    draw(tft);

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

        if (SettingsCommon::backButtonRect().contains(tap.x, tap.y)) return;

        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            if (rowRect(i).contains(tap.x, tap.y)) {
                // Speichert sofort (I18n::setLanguage() = Vorschau +
                // Speichern in einem - siehe i18n.h) - Aenderungen in den
                // Einstellungen sofort auf die SD-Karte, nicht erst bei
                // einem spaeteren Commit wie in der Ersteinrichtung.
                I18n::setLanguage(i);
                draw(tft);
                break;
            }
        }
    }
}

}
