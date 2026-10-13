#include "settings_display_screen.h"
#include "settings_common.h"
#include "ui_header.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "i18n.h"
#include "config.h"
#include <SD.h>

namespace SettingsDisplayScreen {

namespace {
    using Rect = SettingsCommon::Rect;

    // Persistenter Zustand (Config::SD_BRIGHTNESS_FILE/SD_SCREEN_TIMEOUT_FILE)
    // - beide werden einmalig bei loadSettings() (main.cpp::setup()) geladen
    // und bei jeder Aenderung SOFORT wieder gespeichert (Alex' Auftrag,
    // 06.10.: Einstellungen speichern sofort).
    uint8_t brightnessPercent = Config::BRIGHTNESS_DEFAULT_PERCENT;
    uint8_t screenTimeoutMinutes = 0; // 0 = Nie

    uint8_t pwmFromPercent(uint8_t percent) {
        return (uint8_t)((uint16_t)percent * 255 / 100);
    }

    void applyBacklightInternal() {
        ledcWrite(Config::BACKLIGHT_PWM_CHANNEL, pwmFromPercent(brightnessPercent));
    }

    void loadBrightnessFromSd() {
        brightnessPercent = Config::BRIGHTNESS_DEFAULT_PERCENT;
        if (!SD.exists(Config::SD_BRIGHTNESS_FILE)) return;
        File f = SD.open(Config::SD_BRIGHTNESS_FILE, FILE_READ);
        if (!f) return;
        String line = f.readStringUntil('\n');
        f.close();
        line.trim();
        int v = line.toInt();
        if (v >= Config::BRIGHTNESS_MIN_PERCENT && v <= Config::BRIGHTNESS_MAX_PERCENT) {
            brightnessPercent = (uint8_t)v;
        }
    }

    void saveBrightnessToSd() {
        File f = SD.open(Config::SD_BRIGHTNESS_FILE, FILE_WRITE);
        if (!f) return;
        f.println(brightnessPercent);
        f.close();
    }

    void loadTimeoutFromSd() {
        screenTimeoutMinutes = 0;
        if (!SD.exists(Config::SD_SCREEN_TIMEOUT_FILE)) return;
        File f = SD.open(Config::SD_SCREEN_TIMEOUT_FILE, FILE_READ);
        if (!f) return;
        String line = f.readStringUntil('\n');
        f.close();
        line.trim();
        screenTimeoutMinutes = (uint8_t)line.toInt();
    }

    void saveTimeoutToSd() {
        File f = SD.open(Config::SD_SCREEN_TIMEOUT_FILE, FILE_WRITE);
        if (!f) return;
        f.println(screenTimeoutMinutes);
        f.close();
    }

    // ---- Gemeinsame Zeichen-Helfer (lokal, siehe CLAUDE.md-Konvention) ----
    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label, bool active = false) {
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

    // Generischer "?"-Erklaer-Screen (Titel + Text + Tap zum Schliessen) -
    // fuer alle Funktionen auf dieser Seite wiederverwendet (Alex' Auftrag:
    // "Jede einzelne Funktion/jeder Button bekommt einen eigenen
    // '?'-Button").
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

    // ---- Helligkeit: Regler-Unterbildschirm (+/- in 10%-Schritten) ----
    void runBrightnessScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr int16_t BTN_SIZE = 56;
        constexpr int16_t VALUE_Y = 165;
        Rect minusBtn = {20, (int16_t)(VALUE_Y - BTN_SIZE / 2), BTN_SIZE, BTN_SIZE};
        Rect plusBtn = {(int16_t)(Config::SCREEN_WIDTH - 20 - BTN_SIZE), (int16_t)(VALUE_Y - BTN_SIZE / 2), BTN_SIZE, BTN_SIZE};
        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        auto draw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::SETTINGS_SYSTEM_BRIGHTNESS_LABEL));

            tft.setTextDatum(MC_DATUM);
            tft.setTextSize(3);
            tft.drawString(String(brightnessPercent) + "%", (int16_t)(Config::SCREEN_WIDTH / 2), VALUE_Y);
            tft.setTextSize(1);
            tft.setTextDatum(TL_DATUM);

            drawButton(tft, minusBtn, "-");
            drawButton(tft, plusBtn, "+");
            drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
        };
        draw();

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            if (minusBtn.contains(tap.x, tap.y) && brightnessPercent > Config::BRIGHTNESS_MIN_PERCENT) {
                brightnessPercent = (uint8_t)(brightnessPercent - Config::BRIGHTNESS_STEP_PERCENT);
                applyBacklightInternal();
                saveBrightnessToSd();
                draw();
            } else if (plusBtn.contains(tap.x, tap.y) && brightnessPercent < Config::BRIGHTNESS_MAX_PERCENT) {
                brightnessPercent = (uint8_t)(brightnessPercent + Config::BRIGHTNESS_STEP_PERCENT);
                applyBacklightInternal();
                saveBrightnessToSd();
                draw();
            } else if (backBtn.contains(tap.x, tap.y)) {
                return;
            }
        }
    }

    // ---- Bildschirm-Timeout: feste Zeitstufen-Auswahl ----
    void runTimeoutScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr uint8_t PRESET_COUNT = 5;
        constexpr uint8_t PRESETS[PRESET_COUNT] = {1, 2, 5, 10, 0}; // 0 = Nie

        auto presetLabel = [&](uint8_t minutes) -> String {
            if (minutes == 0) return String(I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_NEVER));
            return String(minutes) + I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_UNIT);
        };

        constexpr int16_t ROW_GAP = 8;
        constexpr int16_t START_Y = 34;
        constexpr int16_t ROW_H = 36;
        auto rowRect = [&](uint8_t i) -> Rect {
            return {10, (int16_t)(START_Y + i * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        };
        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        auto draw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(SettingsCommon::accentColor(tft), TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_LABEL));

            for (uint8_t i = 0; i < PRESET_COUNT; i++) {
                drawButton(tft, rowRect(i), presetLabel(PRESETS[i]), PRESETS[i] == screenTimeoutMinutes);
            }
            drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
        };
        draw();

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool selected = false;
            for (uint8_t i = 0; i < PRESET_COUNT && !selected; i++) {
                if (rowRect(i).contains(tap.x, tap.y)) {
                    screenTimeoutMinutes = PRESETS[i];
                    saveTimeoutToSd();
                    selected = true;
                }
            }
            if (selected) return; // Auswahl bestaetigt sich selbst, kehrt direkt zurueck.
            if (backBtn.contains(tap.x, tap.y)) return;
        }
    }

    // ---- Haupt-Seite: 3 Zeilen, je mit eigenem "?"-Button ----
    constexpr uint8_t ROW_COUNT = 3;
    constexpr int16_t ROW_GAP = 8;
    constexpr int16_t START_Y = 70;
    constexpr int16_t ROW_H = 44;
    // 31x31 (26+5) - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen (Alex'
    // Auftrag, 06.10., "Finger-only"-Regel), "?"-Buttons systemweit
    // vergroessert.
    constexpr int16_t INFO_BTN_SIZE = 31;

    Rect mainBtnRect(uint8_t i) {
        return {10, (int16_t)(START_Y + i * (ROW_H + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20 - INFO_BTN_SIZE - 8), ROW_H};
    }
    Rect infoBtnRect(uint8_t i) {
        return {(int16_t)(Config::SCREEN_WIDTH - 10 - INFO_BTN_SIZE),
                (int16_t)(START_Y + i * (ROW_H + ROW_GAP) + (ROW_H - INFO_BTN_SIZE) / 2),
                INFO_BTN_SIZE, INFO_BTN_SIZE};
    }

    String rotationRowLabel() {
        bool on = TouchInput::isRotated180();
        return String(I18n::t(StringId::SETTINGS_ROTATION_LABEL)) +
               I18n::t(on ? StringId::SETTINGS_ROTATION_ON : StringId::SETTINGS_ROTATION_OFF);
    }

    String timeoutRowLabel() {
        String val = (screenTimeoutMinutes == 0)
            ? String(I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_NEVER))
            : String(screenTimeoutMinutes) + I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_UNIT);
        return String(I18n::t(StringId::SETTINGS_SYSTEM_TIMEOUT_LABEL)) + val;
    }

    void draw(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        SettingsCommon::drawBackButton(tft);

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.) - die
        // "?"-Buttons dieses Screens sitzen zeilenweise, nicht im
        // Kopfbereich, deshalb grosszuegige maxWidth.
        UiHeader::drawLeft(tft, 82, (int16_t)(Config::HEADER_HEIGHT / 2), 148,
                            I18n::t(StringId::SETTINGS_HUB_DISPLAY_BTN), SettingsCommon::accentColor(tft));

        drawButton(tft, mainBtnRect(0), String(I18n::t(StringId::SETTINGS_SYSTEM_BRIGHTNESS_LABEL)) + String(brightnessPercent) + "%");
        drawInfoButton(tft, infoBtnRect(0));

        drawButton(tft, mainBtnRect(1), rotationRowLabel());
        drawInfoButton(tft, infoBtnRect(1));

        drawButton(tft, mainBtnRect(2), timeoutRowLabel());
        drawInfoButton(tft, infoBtnRect(2));
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    draw(tft);

    while (true) {
        TouchInput::Point tap;
        if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

        if (SettingsCommon::backButtonRect().contains(tap.x, tap.y)) return;

        if (infoBtnRect(0).contains(tap.x, tap.y)) {
            showInfoScreen(tft, StringId::SETTINGS_SYSTEM_BRIGHTNESS_INFO_TITLE, StringId::SETTINGS_SYSTEM_BRIGHTNESS_INFO_BODY);
            draw(tft);
            continue;
        }
        if (infoBtnRect(1).contains(tap.x, tap.y)) {
            showInfoScreen(tft, StringId::SETTINGS_ROTATION_INFO_TITLE, StringId::SETTINGS_ROTATION_INFO_BODY);
            draw(tft);
            continue;
        }
        if (infoBtnRect(2).contains(tap.x, tap.y)) {
            showInfoScreen(tft, StringId::SETTINGS_SYSTEM_TIMEOUT_INFO_TITLE, StringId::SETTINGS_SYSTEM_TIMEOUT_INFO_BODY);
            draw(tft);
            continue;
        }

        if (mainBtnRect(0).contains(tap.x, tap.y)) {
            runBrightnessScreen(tft);
            draw(tft);
            continue;
        }
        if (mainBtnRect(1).contains(tap.x, tap.y)) {
            // Direkt-Toggle (kein Unterbildschirm noetig) - wirkt sofort
            // und speichert sofort, siehe touch_input.h::saveRotated180().
            bool newRotated = !TouchInput::isRotated180();
            tft.setRotation(newRotated ? 2 : 0);
            TouchInput::setRotated180(newRotated);
            TouchInput::saveRotated180();
            draw(tft);
            continue;
        }
        if (mainBtnRect(2).contains(tap.x, tap.y)) {
            runTimeoutScreen(tft);
            draw(tft);
            continue;
        }
    }
}

void loadSettings() {
    loadBrightnessFromSd();
    loadTimeoutFromSd();
}

void applyBacklight() {
    applyBacklightInternal();
}

void turnBacklightOff() {
    ledcWrite(Config::BACKLIGHT_PWM_CHANNEL, 0);
}

uint32_t screenTimeoutMs() {
    return (uint32_t)screenTimeoutMinutes * 60000UL;
}

}
