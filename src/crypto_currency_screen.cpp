#include "crypto_currency_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include "currency.h"

namespace CryptoCurrencyScreen {

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

    // Nur noch die einzelnen Waehrungen (currency.h::supportedCode()) -
    // der frühere "Automatisch (Standort)"-Eintrag wurde entfernt (Alex'
    // Auftrag, 13.10.: "Automatisch bleibt haengen, bitte weg - jeder
    // stellt seine Waehrung selbst ein, Standard USD").
    uint8_t rowCount() { return Currency::supportedCount(); }

    String rowLabel(uint8_t index) {
        String code = String(Currency::supportedCode(index));
        code.toUpperCase();
        return code;
    }

    bool rowIsActive(uint8_t index, const String& current) {
        String code = String(Currency::supportedCode(index));
        return current == code;
    }

    constexpr int16_t ROW_H = 30;
    constexpr int16_t ROW_GAP = 4;
    constexpr int16_t VIEW_TOP = 40;
    constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
    constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 8);
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();

    String current = Currency::currentCurrency();
    uint8_t count = rowCount();
    int16_t totalH = (int16_t)(count * (ROW_H + ROW_GAP));
    int16_t maxScroll = (int16_t)(totalH - (VIEW_BOTTOM - VIEW_TOP));
    if (maxScroll < 0) maxScroll = 0;
    bool scrollable = maxScroll > 0;
    int16_t scrollY = 0;
    constexpr int16_t SCROLL_STEP = ROW_H + ROW_GAP;

    Rect backBtn = scrollable
        ? Rect{10, BACK_Y, 130, 40}
        : Rect{10, BACK_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
    Rect upBtn   = {146, BACK_Y, 38, 40};
    Rect downBtn = {190, BACK_Y, 38, 40};

    auto redraw = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::CRYPTO_CURRENCY_TITLE));

        for (uint8_t i = 0; i < count; i++) {
            int16_t y = (int16_t)(VIEW_TOP + i * (ROW_H + ROW_GAP) - scrollY);
            if (y + ROW_H < VIEW_TOP || y > VIEW_BOTTOM) continue;

            bool active = rowIsActive(i, current);
            uint16_t bg = active ? TFT_GREEN : TFT_BLACK;
            uint16_t fg = active ? TFT_BLACK : TFT_GREEN;
            tft.fillRoundRect(10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H, 4, bg);
            tft.drawRoundRect(10, y, (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H, 4, TFT_GREEN);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(fg, bg);
            tft.drawString(rowLabel(i), (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(y + ROW_H / 2));
            tft.setTextDatum(TL_DATUM);
        }

        drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
        if (scrollable) {
            drawButton(tft, upBtn, "^");
            drawButton(tft, downBtn, "v");
        }
    };
    redraw();

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (backBtn.contains(tap.x, tap.y)) return;
            if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                scrollY = (int16_t)(scrollY - SCROLL_STEP);
                if (scrollY < 0) scrollY = 0;
                redraw();
            } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                scrollY = (int16_t)(scrollY + SCROLL_STEP);
                if (scrollY > maxScroll) scrollY = maxScroll;
                redraw();
            } else if (tap.y >= VIEW_TOP && tap.y <= VIEW_BOTTOM) {
                int16_t relY = (int16_t)(tap.y - VIEW_TOP + scrollY);
                uint8_t idx = (uint8_t)(relY / (ROW_H + ROW_GAP));
                if (idx < count && (relY % (ROW_H + ROW_GAP)) < ROW_H) {
                    Currency::setCurrentCurrency(String(Currency::supportedCode(idx)));
                    return;
                }
            }
        }
        MenuStars::update(tft);
        delay(20);
    }
}

}
