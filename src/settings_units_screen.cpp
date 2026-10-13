#include "settings_units_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "units.h"
#include "i18n.h"
#include "config.h"

namespace SettingsUnitsScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    // Zwei grosse Optionen statt vieler kleiner Zeilen (Alex' Auftrag,
    // 07.10.: "genau zwei große Optionen") - gleiches ROW_H-
    // Berechnungsprinzip wie settings_language_screen.cpp, hier mit
    // ROW_COUNT=2 entsprechend groesseren Buttons. Jeder Button zeigt
    // zusaetzlich die enthaltenen Einheitenzeichen als zweite Zeile
    // (reine Einheitensymbole, keine Woerter - wie "km/h"/"mm" anderswo
    // im Projekt, keine eigene Uebersetzung noetig) - BEIDE Zeilen in
    // derselben Textgroesse (CLAUDE.md: zwei Groessen direkt
    // uebereinander in einem engen Layout sind fehleranfaellig).
    constexpr int16_t ROW_GAP = 12;
    constexpr int16_t START_Y = 80;
    constexpr int16_t BOTTOM_MARGIN = 20;
    constexpr uint8_t ROW_COUNT = 2;
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {10, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
    }

    void drawOption(TFT_eSPI& tft, const Rect& r, const String& label, const String& symbols, bool active) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        int16_t cx = (int16_t)(r.x + r.w / 2);
        tft.drawString(label, cx, (int16_t)(r.y + r.h / 2 - 12));
        tft.drawString(symbols, cx, (int16_t)(r.y + r.h / 2 + 12));
        tft.setTextDatum(TL_DATUM);
    }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);

        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 148,
                            I18n::t(StringId::SETTINGS_UNITS_TITLE), SettingsCommon::accentColor(tft));

        Units::System current = Units::current();
        drawOption(tft, rowRect(0), I18n::t(StringId::UNITS_METRIC), "\xC2\xB0""C, km/h, mm, km",
                   current == Units::System::Metric);
        drawOption(tft, rowRect(1), I18n::t(StringId::UNITS_IMPERIAL), "\xC2\xB0""F, mph, in, mi",
                   current == Units::System::Imperial);
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    draw(tft);

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

        if (SettingsCommon::backButtonRect().contains(tap.x, tap.y)) return;

        if (rowRect(0).contains(tap.x, tap.y)) {
            // Sofort speichern (Alex' Auftrag, 06.10.-Konvention:
            // Einstellungen speichern sofort, nicht erst bei einem
            // spaeteren Commit wie in der Ersteinrichtung).
            Units::set(Units::System::Metric);
            draw(tft);
            continue;
        }
        if (rowRect(1).contains(tap.x, tap.y)) {
            Units::set(Units::System::Imperial);
            draw(tft);
            continue;
        }
    }
}

}
