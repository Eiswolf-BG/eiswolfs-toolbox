#include "settings_led_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include "led_notifications.h"

namespace SettingsLedScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    // BUGFIX (12.10., Alex' Fund per Foto): Label+Zustand in EINER Zeile
    // ("Alle LED-Benachrichtigungen: Aus") sprengte die Button-Breite -
    // in Franzoesisch/Spanisch sind sowohl die Praefix-Labels als auch die
    // Zustandswoerter ("Désactivé"/"Desactivado") deutlich laenger als im
    // Deutschen, nachgerechnet statt angenommen. Fix: ZWEI Zeilen je
    // Button (Label oben, Zustand darunter) statt einer einzigen langen -
    // gleiches Prinzip wie settings_units_screen.cpp::drawOption()
    // (dortige Begruendung: "zwei Groessen direkt uebereinander in einem
    // engen Layout sind fehleranfaellig", hier zusaetzlich: auch EINE
    // Groesse in EINER Zeile kann bei manchen Sprachen schon zu lang sein).
    void drawToggleButton(TFT_eSPI& tft, const Rect& r, const String& label, const String& state, bool active) {
        uint16_t accent = SettingsCommon::accentColor(tft);
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        int16_t cx = (int16_t)(r.x + r.w / 2);
        tft.drawString(label, cx, (int16_t)(r.y + r.h / 2 - 12));
        tft.drawString(state, cx, (int16_t)(r.y + r.h / 2 + 12));
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

    void showInfoScreen(TFT_eSPI& tft, StringId titleId, StringId bodyId) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(titleId));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 16, I18n::t(bodyId));

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

    // ---- Liste der LED-faehigen Screens (Alex' Auftrag, 12.10.: "so
    // angelegt, dass kuenftige LED-faehige Screens einfach als weiterer
    // Eintrag ergaenzt werden koennen") - neuer Eintrag hier + passende
    // StringIds in i18n.h/allen 4 Sprachdateien reicht, keine weitere
    // Strukturaenderung noetig. ----
    struct LedEntry {
        LedNotifications::ScreenId id;
        StringId label;
        StringId infoTitle;
        StringId infoBody;
    };
    constexpr LedEntry ENTRIES[] = {
        { LedNotifications::ScreenId::Iss, StringId::SETTINGS_LED_ISS_LABEL,
          StringId::SETTINGS_LED_ISS_INFO_TITLE, StringId::SETTINGS_LED_ISS_INFO_BODY },
        { LedNotifications::ScreenId::Rocket, StringId::SETTINGS_LED_ROCKET_LABEL,
          StringId::SETTINGS_LED_ROCKET_INFO_TITLE, StringId::SETTINGS_LED_ROCKET_INFO_BODY },
        { LedNotifications::ScreenId::Reminder, StringId::SETTINGS_LED_REMINDER_LABEL,
          StringId::SETTINGS_LED_REMINDER_INFO_TITLE, StringId::SETTINGS_LED_REMINDER_INFO_BODY },
    };
    constexpr uint8_t ENTRY_COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);

    constexpr int16_t ROW_H = 44;
    constexpr int16_t ROW_GAP = 8;
    // Zusaetzlicher Abstand nach dem Haupt-Toggle (Alex' Auftrag: "klar
    // abgesetzt von der Liste") - deutlich groesser als der normale
    // Zeilenabstand zwischen den einzelnen Listeneintraegen.
    constexpr int16_t MASTER_EXTRA_GAP = 16;
    constexpr int16_t START_Y = 70;
    // 31x31 - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen.
    constexpr int16_t INFO_BTN_SIZE = 31;

    Rect masterBtnRect() {
        return {10, START_Y, (int16_t)(Config::SCREEN_WIDTH - 20 - INFO_BTN_SIZE - 8), ROW_H};
    }
    Rect masterInfoBtnRect() {
        return {(int16_t)(Config::SCREEN_WIDTH - 10 - INFO_BTN_SIZE),
                (int16_t)(START_Y + (ROW_H - INFO_BTN_SIZE) / 2), INFO_BTN_SIZE, INFO_BTN_SIZE};
    }
    int16_t dividerY() { return (int16_t)(START_Y + ROW_H + MASTER_EXTRA_GAP / 2); }
    int16_t listTop() { return (int16_t)(START_Y + ROW_H + MASTER_EXTRA_GAP); }

    Rect entryBtnRect(uint8_t i) {
        return {10, (int16_t)(listTop() + i * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20 - INFO_BTN_SIZE - 8), ROW_H};
    }
    Rect entryInfoBtnRect(uint8_t i) {
        return {(int16_t)(Config::SCREEN_WIDTH - 10 - INFO_BTN_SIZE),
                (int16_t)(listTop() + i * (ROW_H + ROW_GAP) + (ROW_H - INFO_BTN_SIZE) / 2),
                INFO_BTN_SIZE, INFO_BTN_SIZE};
    }

    String stateWord(bool on) {
        return I18n::t(on ? StringId::SETTINGS_ROTATION_ON : StringId::SETTINGS_ROTATION_OFF);
    }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);

        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 148,
                            I18n::t(StringId::SETTINGS_HUB_LED_BTN), SettingsCommon::accentColor(tft));

        bool master = LedNotifications::masterEnabled();
        drawToggleButton(tft, masterBtnRect(), I18n::t(StringId::SETTINGS_LED_MASTER_LABEL),
                          stateWord(master), master);
        drawInfoButton(tft, masterInfoBtnRect());

        tft.drawFastHLine(10, dividerY(), (int16_t)(Config::SCREEN_WIDTH - 20), SettingsCommon::accentColor(tft));

        for (uint8_t i = 0; i < ENTRY_COUNT; i++) {
            bool on = LedNotifications::screenEnabled(ENTRIES[i].id);
            drawToggleButton(tft, entryBtnRect(i), I18n::t(ENTRIES[i].label), stateWord(on), on);
            drawInfoButton(tft, entryInfoBtnRect(i));
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

        if (masterBtnRect().contains(tap.x, tap.y)) {
            LedNotifications::setMasterEnabled(!LedNotifications::masterEnabled());
            draw(tft);
            continue;
        }
        if (masterInfoBtnRect().contains(tap.x, tap.y)) {
            showInfoScreen(tft, StringId::SETTINGS_LED_MASTER_INFO_TITLE, StringId::SETTINGS_LED_MASTER_INFO_BODY);
            draw(tft);
            continue;
        }

        bool handled = false;
        for (uint8_t i = 0; i < ENTRY_COUNT && !handled; i++) {
            if (entryBtnRect(i).contains(tap.x, tap.y)) {
                LedNotifications::setScreenEnabled(ENTRIES[i].id, !LedNotifications::screenEnabled(ENTRIES[i].id));
                draw(tft);
                handled = true;
            } else if (entryInfoBtnRect(i).contains(tap.x, tap.y)) {
                showInfoScreen(tft, ENTRIES[i].infoTitle, ENTRIES[i].infoBody);
                draw(tft);
                handled = true;
            }
        }
    }
}

}
