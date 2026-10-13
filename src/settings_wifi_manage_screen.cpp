#include "settings_wifi_manage_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "wifi_setup_screen.h"
#include "wifi_manager.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"

namespace SettingsWifiManageScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : SettingsCommon::accentColor(tft);
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
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

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        // Gleicher generischer "?"-Titel wie bei den anderen Untermenues.
        tft.println(I18n::t(StringId::SETTINGS_ROTATION_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                      I18n::t(StringId::SETTINGS_WIFI_MANAGE_INFO_BODY));

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

    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t START_Y = 70;
    constexpr int16_t ROW_H = 44;
    constexpr int16_t REMOVE_BTN_W = 56;

    // 31x31 (26+5) - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen (Alex'
    // Auftrag, 06.10., "Finger-only"-Regel), "?"-Buttons systemweit
    // vergroessert.
    Rect infoBtnRect() {
        return {(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31};
    }

    Rect slotRect(uint8_t i) {
        return {10, (int16_t)(START_Y + i * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20 - REMOVE_BTN_W - 6), ROW_H};
    }
    Rect removeRect(uint8_t i) {
        return {(int16_t)(Config::SCREEN_WIDTH - 10 - REMOVE_BTN_W),
                (int16_t)(START_Y + i * (ROW_H + ROW_GAP)), REMOVE_BTN_W, ROW_H};
    }
    Rect addBtnRect() {
        return {10, (int16_t)(START_Y + Config::MAX_WIFI_NETWORKS * (ROW_H + ROW_GAP) + 6),
                (int16_t)(Config::SCREEN_WIDTH - 20), 40};
    }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);
        drawButton(tft, infoBtnRect(), "?");

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.) -
        // engere maxWidth, da hier zusaetzlich ein "?"-Button im
        // Kopfbereich sitzt.
        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 117,
                            I18n::t(StringId::SETTINGS_WIFI_MANAGE_TITLE), SettingsCommon::accentColor(tft));

        uint8_t count = WifiMgr::networkCount();
        for (uint8_t i = 0; i < Config::MAX_WIFI_NETWORKS; i++) {
            if (i < count) {
                drawButton(tft, slotRect(i), WifiMgr::networkSsid(i));
                drawButton(tft, removeRect(i), "X", true);
            } else {
                uint16_t dim = SettingsCommon::accentColor(tft);
                Rect r = slotRect(i);
                tft.drawRoundRect(r.x, r.y, (int16_t)(r.w + REMOVE_BTN_W + 6), r.h, 4, dim);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(dim, TFT_BLACK);
                tft.drawString(I18n::t(StringId::SETTINGS_WIFI_EMPTY_SLOT),
                                (int16_t)(r.x + (r.w + REMOVE_BTN_W + 6) / 2), (int16_t)(r.y + r.h / 2));
                tft.setTextDatum(TL_DATUM);
            }
        }

        if (count < Config::MAX_WIFI_NETWORKS) {
            drawButton(tft, addBtnRect(), I18n::t(StringId::SETTINGS_WIFI_ADD_NETWORK));
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

        if (infoBtnRect().contains(tap.x, tap.y)) {
            showInfoScreen(tft);
            draw(tft);
            continue;
        }

        uint8_t count = WifiMgr::networkCount();
        bool handled = false;
        for (uint8_t i = 0; i < count && !handled; i++) {
            if (removeRect(i).contains(tap.x, tap.y)) {
                WifiMgr::removeNetwork(i);
                // Sofort speichern (Alex' Auftrag, 06.10.: Einstellungen
                // speichern sofort, nicht erst bei einem spaeteren Commit
                // wie in der Ersteinrichtung) - WifiMgr speichert seit der
                // "erst committen, dann speichern"-Umstellung selbst nicht
                // mehr automatisch.
                WifiMgr::commit();
                handled = true;
            }
        }
        if (handled) { draw(tft); continue; }

        if (count < Config::MAX_WIFI_NETWORKS && addBtnRect().contains(tap.x, tap.y)) {
            WifiSetupScreen::run(tft);
            WifiMgr::commit();
            draw(tft);
            continue;
        }
    }
}

}
