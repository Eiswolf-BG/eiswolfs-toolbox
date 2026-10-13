#include "first_run_language_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

// 1:1 aus Eiswolfs Flightradar CYD uebernommen (first_run_language_screen.cpp
// dort) - Titel bewusst zweisprachig hart kodiert ("Language / Sprache"),
// da die eigentliche Sprachauswahl an dieser Stelle im Ablauf noch nicht
// getroffen wurde (siehe dortiger Kommentar).
namespace FirstRunLanguageScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // ROW_H dynamisch aus dem verfuegbaren Platz berechnet (gleiches
    // Prinzip wie bei Flightradar) statt fest verdrahtet - robust, falls
    // spaeter weitere Sprachen dazukommen.
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t START_Y = 60;
    constexpr int16_t BOTTOM_MARGIN = 10;
    constexpr uint8_t ROW_COUNT = I18n::LANG_COUNT;
    constexpr int16_t ROW_H =
        (Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (ROW_COUNT - 1) * ROW_GAP) / ROW_COUNT;

    Rect rowRect(uint8_t index) {
        return {20, (int16_t)(START_Y + index * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 40), ROW_H};
    }

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, TFT_GREEN);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }
}

void run(TFT_eSPI& tft) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Language / Sprache", (int16_t)(Config::SCREEN_WIDTH / 2), 24);
    tft.setTextDatum(TL_DATUM);

    Rect langRects[I18n::LANG_COUNT];
    for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
        langRects[i] = rowRect(i);
        drawButton(tft, langRects[i], I18n::languageName(i));
    }

    MenuStars::reset();
    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) {
            MenuStars::update(tft);
            delay(20);
            continue;
        }

        for (uint8_t i = 0; i < I18n::LANG_COUNT; i++) {
            if (langRects[i].contains(tap.x, tap.y)) {
                // NUR Vorschau (RAM), NICHT speichern - siehe main.cpp
                // "erst committen, dann speichern" (Alex' Auftrag, 05.10.).
                I18n::setLanguagePreview(i);
                return;
            }
        }
    }
}

}
