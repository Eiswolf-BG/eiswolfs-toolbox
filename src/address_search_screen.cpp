#include "address_search_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <cstring>

namespace AddressSearchScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label,
                     bool active = false, bool danger = false) {
        uint16_t accent = danger ? TFT_RED : TFT_GREEN;
        uint16_t bg = active ? accent : TFT_BLACK;
        uint16_t fg = active ? TFT_BLACK : accent;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(fg, bg);
        tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
        tft.setTextDatum(TL_DATUM);
    }

    // Lokale Kopie (siehe Konvention in location_presets_screen.cpp - jeder
    // Screen haelt seine eigenen kleinen Helfer statt eines gemeinsamen
    // Moduls). Ohne Scroll-Unterstuetzung - hier immer nur kurze,
    // vorab abgeschnittene Texte (siehe runConfirmScreen/showErrorRetry).
    // Lokale Kopie (siehe Konvention in location_presets_screen.cpp - jeder
    // Screen haelt seine eigenen kleinen Helfer statt eines gemeinsamen
    // Moduls). Optionale Scroll-Unterstuetzung (scrollY/viewTop/viewBottom/
    // draw) fuer runConfirmScreen() unten - bei den bestehenden Aufrufstellen
    // (showErrorRetry, Kopfzeilen) bleiben die Defaults aktiv, es wird also
    // wie bisher immer alles auf einmal gezeichnet.
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text,
                           int16_t scrollY = 0, int16_t viewTop = -32000, int16_t viewBottom = 32000,
                           bool draw = true) {
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
            if (draw) {
                int16_t screenY = (int16_t)(y - scrollY);
                if (screenY >= viewTop && screenY <= viewBottom) {
                    tft.setCursor(x, screenY);
                    tft.print(line);
                }
            }
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    // ---- UTF-8-bewusste Puffer-Helfer (fuer die Sonderzeichen-Tasten wie
    // "Ä", die als 2-Byte-UTF-8-Sequenz eingefuegt werden muessen) ----
    void appendUtf8(char* buf, uint8_t& len, uint8_t cap, const char* s) {
        size_t sl = strlen(s);
        if (len + sl >= cap) return;
        memcpy(buf + len, s, sl);
        len += (uint8_t)sl;
        buf[len] = 0;
    }

    // Entfernt beim Loeschen eine ganze UTF-8-Zeichensequenz (nicht nur ein
    // einzelnes Byte) - sonst bliebe bei Sonderzeichen ein kaputtes
    // halbes Byte im Puffer stehen.
    void backspaceUtf8(char* buf, uint8_t& len) {
        if (len == 0) return;
        len--;
        while (len > 0 && (((uint8_t)buf[len]) & 0xC0) == 0x80) len--;
        buf[len] = 0;
    }

    // Zeigt nur so viel vom ENDE des Puffers, wie in maxWidth passt - der
    // Nutzer tippt am Ende weiter, das sichtbare Fenster soll dem folgen
    // (wie bei einem gewoehnlichen einzeiligen Texteingabefeld).
    String visibleTail(TFT_eSPI& tft, const char* buf, int16_t maxWidth) {
        String s(buf);
        while (s.length() > 0 && tft.textWidth(s) > maxWidth) {
            int32_t cut = ((((uint8_t)s[0]) & 0xE0) == 0xC0) ? 2 : 1;
            if (cut > (int32_t)s.length()) cut = s.length();
            s = s.substring(cut);
        }
        return s;
    }

    // ---- Tastatur-Layout ----
    constexpr const char* DIGITS = "1234567890";
    constexpr const char* ROW1 = "QWERTYUIOP";
    constexpr const char* ROW2 = "ASDFGHJKL";
    // Um Komma, Punkt und Bindestrich erweitert (7->10 Tasten) - werden bei
    // einer Adresseingabe staendig gebraucht (Hausnummer "45/3", "Str. 12,
    // Ort", Abkuerzungen) und muessen deshalb auf der Hauptseite erreichbar
    // sein (Alex' Nachdruck, 06.10.: der Umweg ueber die Sonderzeichen-
    // Seite fuer sowas Alltaegliches wie einen Punkt war "uebel umstaendlich"
    // und hat durch das Hin-und-Her-Tippen zu Vertippern gefuehrt).
    constexpr const char* ROW3 = "ZXCVBNM,.-";

    // Sprachspezifische Sonderzeichen-Seite (Alex' Auftrag, 06.10.):
    // anders als beim 1:1 uebernommenen Flightradar-Original (dort EINE
    // feste Seite mit den Akzenten ALLER 8 Sprachen gemischt, da dort die
    // Sprache erst WAEHREND der Adresseingabe recherchiert wurde) zeigt
    // diese Seite NUR die Akzente der zu diesem Zeitpunkt bereits im
    // vorherigen Ersteinrichtungs-Schritt gewaehlten Toolbox-Sprache
    // (I18n::language()) - kein Umschalten zwischen Sprachen noetig.
    // Die vier Satzzeichen "/ . ' -" (Hausnummern wie "45/3", Abkuerzungen,
    // Apostroph-Namen) sind unabhaengig von der Sprache immer dabei.
    // SPECIAL_MAX = laengste Sprache (Franzoesisch, 12 Akzente) + 4
    // Satzzeichen = 16, daraus folgt bei max. 6 Tasten/Zeile die
    // Obergrenze von 3 Zeilen (SPECIAL_ROW_MAX).
    constexpr uint8_t SPECIAL_MAX = 16;
    constexpr uint8_t SPECIAL_PER_ROW = 6;
    constexpr uint8_t SPECIAL_ROW_MAX = 3;

    // Fuellt out[] mit den fuer die aktuell gewaehlte Sprache relevanten
    // Sonderzeichen + den sprachunabhaengigen Satzzeichen und gibt die
    // tatsaechliche Anzahl zurueck (immer <= SPECIAL_MAX).
    uint8_t specialCharsForLanguage(const char** out) {
        uint8_t n = 0;
        switch (I18n::language()) {
            case I18n::LANG_DE:
                out[n++] = "Ä"; out[n++] = "Ö"; out[n++] = "Ü"; out[n++] = "ß";
                break;
            case I18n::LANG_ES:
                out[n++] = "Á"; out[n++] = "É"; out[n++] = "Í"; out[n++] = "Ó";
                out[n++] = "Ú"; out[n++] = "Ñ"; out[n++] = "Ü";
                break;
            case I18n::LANG_FR:
                out[n++] = "À"; out[n++] = "Â"; out[n++] = "Ç"; out[n++] = "É";
                out[n++] = "È"; out[n++] = "Ê"; out[n++] = "Ë"; out[n++] = "Î";
                out[n++] = "Ï"; out[n++] = "Ô"; out[n++] = "Ù"; out[n++] = "Û";
                break;
            default: // LANG_EN - keine eigenen Akzentbuchstaben noetig.
                break;
        }
        out[n++] = "/"; out[n++] = "."; out[n++] = "'"; out[n++] = "-";
        return n;
    }

    // Kurzer, sprachspezifischer Hinweis auf dem Umschalt-Button (rein
    // kosmetisch - zeigt beispielhaft 2 Zeichen der jeweiligen Sprache,
    // bei Englisch stattdessen ein Satzzeichen-Beispiel, da es dort keine
    // eigenen Akzente gibt).
    const char* specialToggleLabel() {
        switch (I18n::language()) {
            case I18n::LANG_DE: return "ÄÖÜ";
            case I18n::LANG_ES: return "ÑÁ";
            case I18n::LANG_FR: return "ÀÇ";
            default: return "/.-";
        }
    }

    // Gibt die eingegebene Adresse zurueck, oder einen leeren String, wenn
    // der Nutzer abgebrochen hat.
    String runAddressKeyboard(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr uint8_t CAP = 64;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        // Sprachspezifische Sonderzeichen EINMAL fuer die gesamte
        // Bildschirmlebensdauer ermitteln (die Sprache steht zu diesem
        // Zeitpunkt der Ersteinrichtung bereits fest, siehe Kommentar an
        // specialCharsForLanguage() oben).
        const char* specials[SPECIAL_MAX];
        uint8_t specialCount = specialCharsForLanguage(specials);
        uint8_t specialRowCount = (uint8_t)((specialCount + SPECIAL_PER_ROW - 1) / SPECIAL_PER_ROW);

        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        // Kopfbereich (Titel + Format-Hinweis) einmal zeichnen, um seine
        // tatsaechliche Hoehe per getCursorY() zu messen (variiert je nach
        // Sprache/Uebersetzungslaenge) - Eingabefeld und Tastatur-Start
        // werden daraus abgeleitet statt wie zuvor eine feste Pixelposition
        // (78) anzunehmen. redraw() unten zeichnet denselben Kopf bei jedem
        // Aufruf identisch neu.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::ADDRESS_SEARCH_TITLE));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(I18n::t(StringId::ADDRESS_SEARCH_HINT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

        // KEY_H wird dynamisch aus dem tatsaechlich verbleibenden Platz
        // berechnet (gleiches Prinzip wie ROW_H in radar_theme_screen.cpp)
        // statt fest 30px anzunehmen. Ungueststigster Fall ist NICHT mehr
        // automatisch die Sonderzeichen-Seite - seit der sprachspezifischen
        // Umstellung (06.10.) hat sie oft WENIGER Zeilen als die Buchstaben-
        // Seite (max. SPECIAL_ROW_MAX=3 vs. LETTER_ROW_COUNT=4 feste
        // Buchstaben-Zeilen). BUGFIX (06.10., Alex' Fund per Foto): TOTAL_ROWS
        // nur anhand von SPECIAL_ROW_MAX zu berechnen liess die Buchstaben-
        // Seite (4 Inhaltszeilen + 2 Aktionszeilen = 6) ueber eine fuer nur
        // 5 Zeilen (3+2) berechnete, dadurch zu grosse KEY_H hinauswachsen -
        // die unterste Aktionszeile (Suchen/Abbrechen) fiel dadurch unten
        // vom Bildschirm. Jetzt der tatsaechliche Maximalwert ueber BEIDE
        // Seiten + Leertaste/Loeschen-Zeile + Aktionszeile - dieselbe KEY_H
        // gilt weiterhin fuer BEIDE Tastatur-Seiten (einheitliche
        // Tastengroesse statt springender Groesse beim Umschalten).
        constexpr uint8_t LETTER_ROW_COUNT = 4;
        constexpr uint8_t TOTAL_ROWS =
            (SPECIAL_ROW_MAX > LETTER_ROW_COUNT ? SPECIAL_ROW_MAX : LETTER_ROW_COUNT) + 2;
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableH = (int16_t)(Config::SCREEN_HEIGHT - ROW0_Y - BOTTOM_MARGIN);
        int16_t KEY_H = (int16_t)((availableH - (TOTAL_ROWS - 1) * KEY_GAP) / TOTAL_ROWS);

        auto layoutRow = [&](int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[10];
        layoutRow(ROW0_Y, digitRects, 10);
        layoutRow((int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow((int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow((int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 10);

        // Sonderzeichen-Zeilen: so viele wie fuer die aktuelle Sprache
        // tatsaechlich gebraucht werden (specialRowCount, 1-3), je bis zu
        // SPECIAL_PER_ROW Tasten - die Y-Position der Leertaste/Aktions-
        // leiste unten haengt deshalb von specialRowCount ab, nicht mehr
        // von einer festen Konstante (siehe layoutTrailingButtons() unten).
        Rect specRects[SPECIAL_ROW_MAX][SPECIAL_PER_ROW];
        for (uint8_t r = 0; r < specialRowCount; r++) {
            uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
            uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
            layoutRow((int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], n);
        }

        Rect spaceBtn, backspaceBtn, toggleBtn, cancelBtn, searchBtn;
        constexpr int16_t BTN_W = (Config::SCREEN_WIDTH - 8 - 2 * KEY_GAP) / 3;

        // Buchstaben-Seite hat immer 4 Inhaltszeilen, die Sonderzeichen-
        // Seite je nach Sprache 1-3 (specialRowCount) - die Leertaste/
        // Aktionsleiste ruecken entsprechend unterschiedlich weit nach
        // unten.
        auto layoutTrailingButtons = [&]() {
            uint8_t contentRows = specialPage ? specialRowCount : 4;
            int16_t spaceRowY = (int16_t)(ROW0_Y + contentRows * (KEY_H + KEY_GAP));
            int16_t btnRowY = (int16_t)(ROW0_Y + (contentRows + 1) * (KEY_H + KEY_GAP));
            spaceBtn     = {4, spaceRowY, 150, KEY_H};
            backspaceBtn = {158, spaceRowY, (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
            toggleBtn = {4, btnRowY, BTN_W, KEY_H};
            cancelBtn = {(int16_t)(4 + BTN_W + KEY_GAP), btnRowY, BTN_W, KEY_H};
            searchBtn = {(int16_t)(4 + 2 * (BTN_W + KEY_GAP)), btnRowY,
                         (int16_t)(Config::SCREEN_WIDTH - 8 - 2 * (BTN_W + KEY_GAP)), KEY_H};
        };
        layoutTrailingButtons();

        bool done = false;
        bool searched = false;

        auto redraw = [&]() {
            layoutTrailingButtons();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_TITLE));
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_HINT));

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 26));
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            if (!specialPage) {
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(DIGITS[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(ROW1[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(ROW2[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row3Rects[i], String(ROW3[i]));
            } else {
                for (uint8_t r = 0; r < specialRowCount; r++) {
                    uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
                    uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
                    for (uint8_t i = 0; i < n; i++) {
                        drawButton(tft, specRects[r][i], specials[r * SPECIAL_PER_ROW + i]);
                    }
                }
            }

            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, toggleBtn, specialPage ? "ABC" : specialToggleLabel());
            drawButton(tft, cancelBtn, I18n::t(StringId::ADDRESS_SEARCH_CANCEL), false, true);
            drawButton(tft, searchBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            if (!specialPage) {
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (digitRects[i].contains(tap.x, tap.y)) {
                        char s[2] = {DIGITS[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (row1Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW1[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 9 && !handled; i++) {
                    if (row2Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW2[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (row3Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {ROW3[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
            } else {
                for (uint8_t r = 0; r < specialRowCount && !handled; r++) {
                    uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
                    uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
                    for (uint8_t i = 0; i < n && !handled; i++) {
                        if (specRects[r][i].contains(tap.x, tap.y)) {
                            appendUtf8(buf, len, CAP, specials[r * SPECIAL_PER_ROW + i]);
                            handled = true;
                        }
                    }
                }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, " "); handled = true; }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) { backspaceUtf8(buf, len); handled = true; }
            if (!handled && toggleBtn.contains(tap.x, tap.y)) { specialPage = !specialPage; handled = true; }
            if (!handled && cancelBtn.contains(tap.x, tap.y)) {
                buf[0] = 0;
                len = 0;
                done = true;
                searched = false;
                handled = true;
            }
            if (!handled && searchBtn.contains(tap.x, tap.y) && len > 0) {
                done = true;
                searched = true;
                handled = true;
            }

            if (handled) redraw();
        }

        return searched ? String(buf) : String();
    }

    // Namens-Tastatur fuer den finalen Schritt (Preset-Name vergeben) -
    // spiegelt runPresetNameKeypad() aus location_presets_screen.cpp.
    // Seit der Font-/Tastatur-Analyse (Alex' Wunsch) jetzt MIT
    // Sonderzeichen-Seite (gleicher Zeichensatz/gleiches Muster wie
    // runAddressKeyboard() oben, per appendUtf8()/backspaceUtf8()), damit
    // z.B. "Zürich" oder "São Paulo" korrekt als Preset-Name eingetippt
    // werden koennen - vorher bewusst ASCII-only, das war der Auftrag.
    String runNameKeypad(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* NDIGITS = "1234567890";
        constexpr const char* NROW1 = "QWERTYUIOP";
        constexpr const char* NROW2 = "ASDFGHJKL";
        constexpr const char* NROW3 = "ZXCVBNM";

        // Kapazitaet bewusst identisch zu Preset::name (siehe
        // location_presets.cpp, char name[17]) gehalten, NICHT groesser -
        // ein laengerer Eingabepuffer, der dann per strncpy() in
        // addPreset() auf 16 Byte gekuerzt wird, koennte ein Mehrbyte-
        // UTF-8-Sonderzeichen mitten entzweischneiden und einen kaputten
        // Namen abspeichern.
        constexpr uint8_t CAP = 17;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        const char* specials[SPECIAL_MAX];
        uint8_t specialCount = specialCharsForLanguage(specials);
        uint8_t specialRowCount = (uint8_t)((specialCount + SPECIAL_PER_ROW - 1) / SPECIAL_PER_ROW);

        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        // Kopfbereich (Titel + Namens-Hinweis) einmal zeichnen, um seine
        // tatsaechliche Hoehe per getCursorY() zu messen - selbes Muster wie
        // in runAddressKeyboard() oben.
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
        tft.setTextColor(TFT_CYAN, TFT_BLACK);
        tft.println(I18n::t(StringId::LOCATION_NAME_HINT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

        // KEY_H dynamisch berechnet - siehe ausfuehrlicher Kommentar (inkl.
        // BUGFIX 06.10.) in runAddressKeyboard() oben.
        constexpr uint8_t LETTER_ROW_COUNT = 4;
        constexpr uint8_t TOTAL_ROWS =
            (SPECIAL_ROW_MAX > LETTER_ROW_COUNT ? SPECIAL_ROW_MAX : LETTER_ROW_COUNT) + 2;
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableH = (int16_t)(Config::SCREEN_HEIGHT - ROW0_Y - BOTTOM_MARGIN);
        int16_t KEY_H = (int16_t)((availableH - (TOTAL_ROWS - 1) * KEY_GAP) / TOTAL_ROWS);

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[7];
        layoutRow(NDIGITS, ROW0_Y, digitRects, 10);
        layoutRow(NROW1, (int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow(NROW2, (int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow(NROW3, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 7);

        // Sprachspezifische Sonderzeichen-Zeilen - gleiches Prinzip wie in
        // runAddressKeyboard() oben (specialCharsForLanguage(), Datei-
        // weit definiert und von beiden Funktionen genutzt).
        Rect specRects[SPECIAL_ROW_MAX][SPECIAL_PER_ROW];
        for (uint8_t r = 0; r < specialRowCount; r++) {
            uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
            uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
            layoutRow(nullptr, (int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], n);
        }

        Rect spaceBtn, backspaceBtn, toggleBtn, skipBtn, confirmBtn;

        // Buchstaben-Seite hat immer 4 Inhaltszeilen, die Sonderzeichen-
        // Seite je nach Sprache 1-3 (specialRowCount) - gleiches Prinzip
        // wie in runAddressKeyboard() oben.
        auto layoutTrailingButtons = [&]() {
            uint8_t contentRows = specialPage ? specialRowCount : 4;
            int16_t spaceRowY = (int16_t)(ROW0_Y + contentRows * (KEY_H + KEY_GAP));
            int16_t btnRowY = (int16_t)(ROW0_Y + (contentRows + 1) * (KEY_H + KEY_GAP));
            spaceBtn     = {4, spaceRowY, 150, KEY_H};
            backspaceBtn = {158, spaceRowY, (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
            toggleBtn = {4, btnRowY, 80, KEY_H};
            skipBtn   = {(int16_t)(4 + 80 + KEY_GAP), btnRowY, 90, KEY_H};
            confirmBtn = {(int16_t)(4 + 80 + KEY_GAP + 90 + KEY_GAP), btnRowY,
                          (int16_t)(Config::SCREEN_WIDTH - 8 - (80 + KEY_GAP + 90 + KEY_GAP)), KEY_H};
        };
        layoutTrailingButtons();

        bool done = false;

        auto redraw = [&]() {
            layoutTrailingButtons();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::LOCATION_NAME_PROMPT));
            tft.setTextColor(TFT_CYAN, TFT_BLACK);
            tft.println(I18n::t(StringId::LOCATION_NAME_HINT));

            tft.fillRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_BLACK);
            tft.drawRect(8, fieldY, Config::SCREEN_WIDTH - 16, FIELD_H, TFT_GREEN);
            tft.setTextSize(2);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(14, (int16_t)(fieldY + 26));
            tft.print(visibleTail(tft, buf, (int16_t)(Config::SCREEN_WIDTH - 28)));
            tft.setTextSize(1);

            if (!specialPage) {
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, digitRects[i], String(NDIGITS[i]));
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row1Rects[i], String(NROW1[i]));
                for (uint8_t i = 0; i < 9; i++) drawButton(tft, row2Rects[i], String(NROW2[i]));
                for (uint8_t i = 0; i < 7; i++) drawButton(tft, row3Rects[i], String(NROW3[i]));
            } else {
                for (uint8_t r = 0; r < specialRowCount; r++) {
                    uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
                    uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
                    for (uint8_t i = 0; i < n; i++) {
                        drawButton(tft, specRects[r][i], specials[r * SPECIAL_PER_ROW + i]);
                    }
                }
            }

            drawButton(tft, spaceBtn, I18n::t(StringId::WIFI_SPACE));
            drawButton(tft, backspaceBtn, "<-");
            drawButton(tft, toggleBtn, specialPage ? "ABC" : specialToggleLabel());
            drawButton(tft, skipBtn, I18n::t(StringId::LOCATION_NAME_SKIP));
            drawButton(tft, confirmBtn, I18n::t(StringId::OK));
        };

        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            if (!specialPage) {
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (digitRects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NDIGITS[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 10 && !handled; i++) {
                    if (row1Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW1[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 9 && !handled; i++) {
                    if (row2Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW2[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
                for (uint8_t i = 0; i < 7 && !handled; i++) {
                    if (row3Rects[i].contains(tap.x, tap.y)) {
                        char s[2] = {NROW3[i], 0};
                        appendUtf8(buf, len, CAP, s);
                        handled = true;
                    }
                }
            } else {
                for (uint8_t r = 0; r < specialRowCount && !handled; r++) {
                    uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
                    uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
                    for (uint8_t i = 0; i < n && !handled; i++) {
                        if (specRects[r][i].contains(tap.x, tap.y)) {
                            appendUtf8(buf, len, CAP, specials[r * SPECIAL_PER_ROW + i]);
                            handled = true;
                        }
                    }
                }
            }
            if (!handled && spaceBtn.contains(tap.x, tap.y)) { appendUtf8(buf, len, CAP, " "); handled = true; }
            if (!handled && backspaceBtn.contains(tap.x, tap.y)) { backspaceUtf8(buf, len); handled = true; }
            if (!handled && toggleBtn.contains(tap.x, tap.y)) { specialPage = !specialPage; handled = true; }
            if (!handled && skipBtn.contains(tap.x, tap.y)) { buf[0] = 0; len = 0; done = true; handled = true; }
            if (!handled && confirmBtn.contains(tap.x, tap.y)) { done = true; handled = true; }

            if (handled) redraw();
        }

        return String(buf);
    }

    // Prozentkodiert einen UTF-8-String fuer die Nominatim-Query (jedes
    // Roh-Byte einzeln - funktioniert damit auch fuer die mehrbytigen
    // Sonderzeichen).
    String urlEncode(const String& s) {
        String out;
        char hex[4];
        for (size_t i = 0; i < s.length(); i++) {
            uint8_t c = (uint8_t)s[i];
            bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                               (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
            if (unreserved) {
                out += (char)c;
            } else {
                snprintf(hex, sizeof(hex), "%%%02X", c);
                out += hex;
            }
        }
        return out;
    }

    // Fuer staerker lokalisierte display_name-Ergebnisse wird Nominatim
    // um Ergebnisse in der aktuell gewaehlten UI-Sprache gebeten
    // (accept-language-Parameter) - analog zum NOMINATIM_LANG_CODES[]-
    // Array im Flightradar-Original, hier aber nur 4 statt 8 Sprachen
    // (siehe i18n.h).
    const char* nominatimLangCode() {
        switch (I18n::language()) {
            case I18n::LANG_EN: return "en";
            case I18n::LANG_ES: return "es";
            case I18n::LANG_FR: return "fr";
            default: return "de";
        }
    }

    enum class GeocodeResult { Ok, NoResults, NetworkError };

    // Bis zu MAX_GEOCODE_CANDIDATES Treffer statt blind nur den ersten zu
    // uebernehmen (Alex' Bugmeldung "Cambridge" - Nominatim kennt mehrere
    // gleichnamige Orte weltweit, z.B. Cambridge UK/MA-USA/Ontario-Kanada,
    // vorher entschied ausschliesslich Nominatims eigenes Ranking, ohne
    // dass der Nutzer das sehen oder korrigieren konnte). Bei genau einem
    // Treffer bleibt der Ablauf wie bisher (direkt zum Bestaetigungs-
    // Screen), bei mehreren zeigt runCandidatePickScreen() (siehe unten)
    // eine Auswahlliste.
    constexpr uint8_t MAX_GEOCODE_CANDIDATES = 5;
    struct GeocodeCandidate {
        double lat = 0;
        double lon = 0;
        String displayName;
        // 2-Buchstaben-ISO-Laendercode (Nominatim liefert das nur mit
        // "&addressdetails=1" in address.country_code, Alex' Auftrag,
        // 07.10. - fuer die standortabhaengige Waehrung im Krypto-Ticker,
        // siehe currency.h). Leer, falls Nominatim es ausnahmsweise nicht
        // liefert.
        String countryCode;
    };

    // Kostenloser, anmeldefreier Geokodierungs-Dienst (OpenStreetMap
    // Nominatim, siehe Config::NOMINATIM_HOST). Blockierender Aufruf,
    // ausgeloest durch Nutzer-Interaktion (Tap auf "Suchen") - laeuft im
    // Vordergrund auf Core 1, exakt wie der bestehende Flugzeug-Detail-
    // Abruf in aircraft_details.cpp.
    GeocodeResult geocode(const String& query, GeocodeCandidate* candidates, uint8_t& count) {
        count = 0;
        if (WiFi.status() != WL_CONNECTED) return GeocodeResult::NetworkError;

        WiFiClientSecure client;
        client.setInsecure();
        client.setTimeout(Config::HTTP_TIMEOUT_MS);

        String url = "https://" + String(Config::NOMINATIM_HOST) +
                     "/search?format=json&limit=" + String(MAX_GEOCODE_CANDIDATES) +
                     "&q=" + urlEncode(query);
        url += "&accept-language=" + String(nominatimLangCode());
        // addressdetails=1 liefert zusaetzlich address.country_code (Alex'
        // Auftrag, 07.10. - fuer die standortabhaengige Waehrung im
        // Krypto-Ticker, siehe GeocodeCandidate::countryCode oben).
        url += "&addressdetails=1";

        HTTPClient http;
        http.setTimeout(Config::HTTP_TIMEOUT_MS);
        if (!http.begin(client, url)) return GeocodeResult::NetworkError;
        http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

        int code = http.GET();
        if (code != HTTP_CODE_OK) {
            http.end();
            return GeocodeResult::NetworkError;
        }

        // Body erst komplett einsammeln statt direkt aus http.getStream()
        // zu parsen - siehe weather.cpp, gleicher Grund (Chunked-Transfer-
        // Encoding fuehrte dort sonst zu ArduinoJson-"InvalidInput").
        String body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);
        if (err) return GeocodeResult::NetworkError;

        JsonArray arr = doc.as<JsonArray>();
        if (arr.isNull() || arr.size() == 0) return GeocodeResult::NoResults;

        for (JsonObject obj : arr) {
            if (count >= MAX_GEOCODE_CANDIDATES) break;
            const char* latStr = obj["lat"] | "";
            const char* lonStr = obj["lon"] | "";
            if (!latStr[0] || !lonStr[0]) continue;

            candidates[count].lat = atof(latStr);
            candidates[count].lon = atof(lonStr);
            const char* dn = obj["display_name"] | "";
            candidates[count].displayName = String(dn);
            const char* cc = obj["address"]["country_code"] | "";
            candidates[count].countryCode = String(cc);
            count++;
        }
        if (count == 0) return GeocodeResult::NoResults;
        return GeocodeResult::Ok;
    }

    // Kuerzt Nominatims oft sehr langen display_name (z.B. "Cambridge,
    // Cambridgeshire, Cambridgeshire and Peterborough, England, United
    // Kingdom") auf "Ort, Land" (erstes und letztes Komma-Segment) fuer
    // die Auswahlliste - der volle Text bleibt auf dem nachfolgenden
    // Bestaetigungs-Screen (runConfirmScreen()) weiterhin sichtbar.
    // Zusaetzlich zeichenweise gekuerzt, falls selbst diese kurze Fassung
    // die Zeilenbreite sprengt (beliebiger, nicht uebersetzter Ortsname -
    // CLAUDE.md-Pflicht: darf nie ungeprueft ueberlaufen), gleiches
    // Prinzip wie drawTruncatedLeft() in radar_screen.cpp.
    String shortCandidateLabel(TFT_eSPI& tft, const String& displayName, int16_t maxWidth) {
        int32_t firstComma = displayName.indexOf(',');
        int32_t lastComma = displayName.lastIndexOf(',');
        String label;
        if (firstComma < 0) {
            label = displayName;
        } else {
            String first = displayName.substring(0, firstComma);
            first.trim();
            if (lastComma > firstComma) {
                String last = displayName.substring(lastComma + 1);
                last.trim();
                label = last.length() > 0 ? (first + ", " + last) : first;
            } else {
                label = first;
            }
        }
        while (label.length() > 1 && tft.textWidth(label) > maxWidth) {
            label.remove(label.length() - 1);
        }
        return label;
    }

    // Fehler-/Kein-Ergebnis-Screen mit "Erneut versuchen"/"Abbrechen".
    // Gibt true zurueck, wenn der Nutzer es erneut versuchen will.
    bool showErrorRetry(TFT_eSPI& tft, StringId msgId) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        layoutWrapped(tft, 10, 40, (int16_t)(Config::SCREEN_WIDTH - 20), 18, I18n::t(msgId));

        Rect retryBtn  = {10, (int16_t)(Config::SCREEN_HEIGHT - 96), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect cancelBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        drawButton(tft, retryBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
        drawButton(tft, cancelBtn, I18n::t(StringId::ADDRESS_SEARCH_CANCEL), false, true);

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
            if (retryBtn.contains(tap.x, tap.y)) return true;
            if (cancelBtn.contains(tap.x, tap.y)) return false;
        }
    }

    // Bestaetigungs-Screen mit dem von Nominatim gefundenen Ort. Gibt 1
    // zurueck ("verwenden"), 0 fuer "erneut versuchen" (zurueck zur
    // Adress-Tastatur).
    int runConfirmScreen(TFT_eSPI& tft, const String& displayName) {
        MenuStars::reset();

        constexpr int16_t textMaxWidth = Config::SCREEN_WIDTH - 20;
        constexpr int16_t LINE_H = 18;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t TRY_AGAIN_Y = (int16_t)(Config::SCREEN_HEIGHT - 96);
        constexpr int16_t USE_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(TRY_AGAIN_Y - 10);

        // Volle Adresse OHNE Kuerzung anzeigen (frueher wurde bei > 150
        // Zeichen mit "..." abgeschnitten) - stattdessen wird der Absatz bei
        // Bedarf vertikal scrollbar, exakt dasselbe Muster wie beim "Wie
        // funktionieren Presets"-Infoscreen (location_presets_screen.cpp
        // ::runInfoScreen). Bewusst KEIN Marquee hier: das ist ein
        // mehrzeiliger, wortumgebrochener Absatz, keine einzelne Zeile, die
        // in der Breite nicht passt - horizontales Scrollen waere hier die
        // falsche Loesung.
        int16_t totalH = layoutWrapped(tft, 10, VIEW_TOP, textMaxWidth, LINE_H, displayName, 0, 0, 0, false);
        int16_t maxScroll = (int16_t)(totalH - VIEW_BOTTOM);
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect tryAgainBtn = {10, TRY_AGAIN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect useBtn = scrollable
            ? Rect{10, USE_Y, 130, 40}
            : Rect{10, USE_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, USE_Y, 38, 40};
        Rect downBtn = {190, USE_Y, 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ADDRESS_SEARCH_CONFIRM_TITLE));

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            layoutWrapped(tft, 10, VIEW_TOP, textMaxWidth, LINE_H, displayName, scrollY, VIEW_TOP, VIEW_BOTTOM, true);

            drawButton(tft, tryAgainBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));
            drawButton(tft, useBtn, I18n::t(StringId::ADDRESS_SEARCH_USE_THIS));
            if (scrollable) {
                drawButton(tft, upBtn, "^");
                drawButton(tft, downBtn, "v");
            }
        };

        redraw();

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (tryAgainBtn.contains(tap.x, tap.y)) return 0;
                if (useBtn.contains(tap.x, tap.y)) return 1;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY -= SCROLL_STEP;
                    if (scrollY < 0) scrollY = 0;
                    redraw();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY += SCROLL_STEP;
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    redraw();
                }
            }
            MenuStars::update(tft);
            delay(20);
        }
    }
}

// Auswahl-Screen bei mehreren Nominatim-Treffern (siehe geocode()-
// Kommentar oben) - gleiches dynamisches Zeilenlayout wie
// LanguageScreen::run() (ROW_H haengt von der tatsaechlichen Zeilenzahl
// ab: bis zu MAX_GEOCODE_CANDIDATES Treffer + ein "Erneut versuchen"-
// Button). Jede Zeile zeigt eine gekuerzte "Ort, Land"-Fassung
// (shortCandidateLabel()) statt des oft sehr langen vollen display_name -
// der wird erst auf dem nachfolgenden Bestaetigungs-Screen
// (runConfirmScreen()) komplett angezeigt, genau wie beim bisherigen
// Einzeltreffer-Ablauf. Gibt den gewaehlten Index zurueck, oder -1 bei
// "Erneut versuchen"/Timeout (fuehrt zurueck zur Adress-Tastatur, wie
// showErrorRetry()).
int runCandidatePickScreen(TFT_eSPI& tft, const GeocodeCandidate* candidates, uint8_t count) {
    MenuStars::reset();

    constexpr int16_t ROW_GAP = 6;
    constexpr int16_t START_Y = 30;
    constexpr int16_t BOTTOM_MARGIN = 10;
    uint8_t rowCount = (uint8_t)(count + 1); // Treffer + "Erneut versuchen"
    int16_t rowH = (int16_t)((Config::SCREEN_HEIGHT - START_Y - BOTTOM_MARGIN - (rowCount - 1) * ROW_GAP) / rowCount);

    auto rowRect = [&](uint8_t index) -> Rect {
        return {10, (int16_t)(START_Y + index * (rowH + ROW_GAP)),
                (int16_t)(Config::SCREEN_WIDTH - 20), rowH};
    };

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(10, 14);
    tft.println(I18n::t(StringId::ADDRESS_SEARCH_MULTIPLE_TITLE));

    tft.setTextSize(1);
    int16_t labelMaxWidth = (int16_t)(Config::SCREEN_WIDTH - 20 - 16); // Rand innerhalb der Buttons
    for (uint8_t i = 0; i < count; i++) {
        drawButton(tft, rowRect(i), shortCandidateLabel(tft, candidates[i].displayName, labelMaxWidth));
    }
    Rect tryAgainBtn = rowRect(count);
    drawButton(tft, tryAgainBtn, I18n::t(StringId::ADDRESS_SEARCH_TRY_AGAIN));

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            for (uint8_t i = 0; i < count; i++) {
                if (rowRect(i).contains(tap.x, tap.y)) return (int)i;
            }
            if (tryAgainBtn.contains(tap.x, tap.y)) return -1;
        }
        // Inaktivitaets-Timeout - siehe Config::MENU_IDLE_TIMEOUT_MS.
        if (TouchInput::msSinceLastTap() >= Config::MENU_IDLE_TIMEOUT_MS) return -1;
        MenuStars::update(tft);
        delay(20);
    }
}

namespace {
    // Gemeinsamer Kern von run()/runForCoordinates() (Alex' Auftrag,
    // 06.10. - OEPNV-Screen braucht eine Adresseingabe, die NICHT den
    // allgemeinen Toolbox-Standort ueberschreibt, siehe transit_screen.cpp):
    // Adresse eingeben, geokodieren, bei mehreren Treffern auswaehlen,
    // bestaetigen. Gibt true + die bestaetigten Koordinaten zurueck, false
    // bei Abbruch. Speichert NICHTS selbst - das entscheidet der jeweilige
    // Aufrufer (run(): als neues Preset; runForCoordinates(): gar nicht).
    bool findCoordinates(TFT_eSPI& tft, double& outLat, double& outLon, String& outCountryCode) {
        while (true) {
            String address = runAddressKeyboard(tft);
            if (address.length() == 0) return false;

            tft.fillScreen(TFT_BLACK);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(I18n::t(StringId::ADDRESS_SEARCH_SEARCHING), Config::SCREEN_WIDTH / 2, Config::SCREEN_HEIGHT / 2);
            tft.setTextDatum(TL_DATUM);

            GeocodeCandidate candidates[MAX_GEOCODE_CANDIDATES];
            uint8_t candidateCount = 0;
            GeocodeResult result = geocode(address, candidates, candidateCount);

            if (result == GeocodeResult::NetworkError) {
                if (!showErrorRetry(tft, StringId::ADDRESS_SEARCH_ERROR)) return false;
                continue;
            }
            if (result == GeocodeResult::NoResults) {
                if (!showErrorRetry(tft, StringId::ADDRESS_SEARCH_NO_RESULTS)) return false;
                continue;
            }

            uint8_t selected = 0;
            if (candidateCount > 1) {
                int picked = runCandidatePickScreen(tft, candidates, candidateCount);
                if (picked < 0) continue;
                selected = (uint8_t)picked;
            }
            double lat = candidates[selected].lat;
            double lon = candidates[selected].lon;
            const String& displayName = candidates[selected].displayName;
            const String& countryCode = candidates[selected].countryCode;

            int choice = runConfirmScreen(tft, displayName);
            if (choice == 0) continue;

            outLat = lat;
            outLon = lon;
            outCountryCode = countryCode;
            return true;
        }
    }
}

bool run(TFT_eSPI& tft) {
    double lat = 0, lon = 0;
    String countryCode;
    if (!findCoordinates(tft, lat, lon, countryCode)) return false;

    String name = runNameKeypad(tft);
    if (!LocationPresets::addPreset(lat, lon, name)) return false;
    // Neu angelegtes Preset sofort aktivieren - sonst blieb z.B. der
    // automatische IP-Standort aktiv, obwohl gerade extra eine genauere
    // Adresse eingegeben wurde (siehe Alex' Feedback).
    LocationPresets::setActiveIndex((int8_t)(LocationPresets::count() - 1));
    // Fuer die standortabhaengige Waehrung im Krypto-Ticker (Alex'
    // Auftrag, 07.10., siehe currency.h) - NUR hier gesetzt, NICHT in
    // runForCoordinates() unten (das OEPNV-"Genauere Adresse"-Ergebnis
    // darf die allgemeine Toolbox-Position/-Waehrung nicht beeinflussen,
    // gleiche Begruendung wie beim Nicht-Speichern der Koordinaten dort).
    LocationPresets::setCountryCode(countryCode);
    return true;
}

bool runForCoordinates(TFT_eSPI& tft, double& outLat, double& outLon) {
    String unusedCountryCode;
    return findCoordinates(tft, outLat, outLon, unusedCountryCode);
}

bool backfillCountryCodeIfMissing() {
    // Nur noetig fuer Standorte, die VOR Einfuehrung des countryCode-
    // Felds (07.10.) gespeichert wurden - deren locations.txt hat kein
    // ",country=.." im ersten Zeilenfeld, LocationPresets::countryCode()
    // bleibt dann dauerhaft leer. Der Laendercode wird fuer die
    // laenderspezifischen Feiertage im Kalender-Screen gebraucht (siehe
    // holiday_client.cpp) - der Krypto-Ticker nutzt ihn seit 13.10. NICHT
    // mehr (dortige Automatik wurde entfernt, siehe currency.h).
    if (LocationPresets::count() == 0) return false;
    if (LocationPresets::countryCode().length() == 2) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    int8_t active = LocationPresets::activeIndex();
    uint8_t idx = (active >= 0) ? (uint8_t)active : 0;
    double lat = 0, lon = 0;
    LocationPresets::getLatLon(idx, lat, lon);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(Config::HTTP_TIMEOUT_MS);

    char latBuf[16];
    char lonBuf[16];
    snprintf(latBuf, sizeof(latBuf), "%.6f", lat);
    snprintf(lonBuf, sizeof(lonBuf), "%.6f", lon);

    // zoom=3 (Laender-Ebene) - wir brauchen nur address.country_code,
    // keine Strassen-/Ortsdetails, haelt die Antwort klein.
    String url = "https://" + String(Config::NOMINATIM_HOST) +
                 "/reverse?format=json&lat=" + String(latBuf) + "&lon=" + String(lonBuf) +
                 "&addressdetails=1&zoom=3";

    HTTPClient http;
    http.setTimeout(Config::HTTP_TIMEOUT_MS);
    if (!http.begin(client, url)) return false;
    http.addHeader("User-Agent", Config::NOMINATIM_USER_AGENT);

    int code = http.GET();
    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body)) return false;

    const char* cc = doc["address"]["country_code"] | "";
    if (!cc[0]) return false;

    LocationPresets::setCountryCode(String(cc));
    LocationPresets::commit();
    return true;
}

}
