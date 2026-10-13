#include "first_run_units_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "units.h"
#include "i18n.h"
#include "config.h"

namespace FirstRunUnitsScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr int16_t ROW_GAP = 16;
    constexpr int16_t START_Y = 90;
    constexpr int16_t BOTTOM_MARGIN = 30;
    constexpr uint8_t ROW_COUNT = 2;
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {20, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 40), ROW_H};
    }

    void drawOption(TFT_eSPI& tft, const Rect& r, const String& label, const String& symbols) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t cx = (int16_t)(r.x + r.w / 2);
        tft.drawString(label, cx, (int16_t)(r.y + r.h / 2 - 12));
        tft.drawString(symbols, cx, (int16_t)(r.y + r.h / 2 + 12));
        tft.setTextDatum(TL_DATUM);
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(I18n::t(StringId::SETTINGS_UNITS_TITLE), (int16_t)(Config::SCREEN_WIDTH / 2), 30);
    tft.setTextDatum(TL_DATUM);

    Rect rects[2] = {rowRect(0), rowRect(1)};
    drawOption(tft, rects[0], I18n::t(StringId::UNITS_METRIC), "\xC2\xB0""C, km/h, mm, km");
    drawOption(tft, rects[1], I18n::t(StringId::UNITS_IMPERIAL), "\xC2\xB0""F, mph, in, mi");

    MenuStars::reset();
    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) {
            MenuStars::update(tft);
            delay(20);
            continue;
        }

        if (rects[0].contains(tap.x, tap.y)) {
            // NUR Vorschau (RAM), NICHT speichern - siehe main.cpp
            // "erst committen, dann speichern" (Alex' Auftrag, 05.10.).
            Units::setPreview(Units::System::Metric);
            return;
        }
        if (rects[1].contains(tap.x, tap.y)) {
            Units::setPreview(Units::System::Imperial);
            return;
        }
    }
}

}
