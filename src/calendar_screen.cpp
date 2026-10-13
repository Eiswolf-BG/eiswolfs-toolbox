#include "calendar_screen.h"
#include <time.h>
#include <cstring>
#include "touch_input.h"
#include "menu_stars.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"
#include "net_task.h"
#include "location_presets.h"
#include "appointments.h"

namespace CalendarScreen {

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

    // Wortweiser Umbruch anhand echter Pixelbreite - gleiches Muster wie
    // in allen anderen Screens, lokal dupliziert (Projekt-Konvention).
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

    // ---- Lokale Zeit des GERAETS (wie earthquake_screen.cpp/f1_screen.cpp
    // ::formatDeviceLocalDateTime(), 1:1 dupliziert, Projekt-Konvention) -
    // feste Mitteleuropa-DST-Basis statt einer Zeitzonen-Nachschlagtabelle.
    // ----
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    long lastSundayOfMonth(int year, unsigned month, unsigned lastDay) {
        long days = daysFromCivil(year, month, lastDay);
        long dow = ((days % 7) + 4 + 7) % 7;
        return days - dow;
    }

    bool isEuDst(time_t epoch, int year) {
        time_t dstStart = (time_t)(lastSundayOfMonth(year, 3, 31) * 86400L + 1 * 3600L);
        time_t dstEnd = (time_t)(lastSundayOfMonth(year, 10, 31) * 86400L + 1 * 3600L);
        return epoch >= dstStart && epoch < dstEnd;
    }

    int yearOfEpochApprox(time_t epoch) {
        return 1970 + (int)(epoch / 31556952L);
    }

    bool isLeapYear(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

    unsigned daysInMonth(int y, unsigned m) {
        static const unsigned DIM[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (m == 2 && isLeapYear(y)) return 29;
        return DIM[m - 1];
    }

    // 0=Montag..6=Sonntag (ISO-8601-Konvention, siehe i18n.h-Kommentar).
    int weekdayOf(int y, unsigned m, unsigned d) {
        long days = daysFromCivil(y, m, d);
        return (int)(((days + 3) % 7 + 7) % 7);
    }

    struct LocalDate { int year; unsigned month; unsigned day; };

    LocalDate todayLocal() {
        time_t epoch = time(nullptr);
        int year = yearOfEpochApprox(epoch);
        int offsetHours = 1 + (isEuDst(epoch, year) ? 1 : 0);
        time_t localEpoch = epoch + (time_t)offsetHours * 3600;
        struct tm t;
        gmtime_r(&localEpoch, &t);
        LocalDate d;
        d.year = t.tm_year + 1900;
        d.month = (unsigned)(t.tm_mon + 1);
        d.day = (unsigned)t.tm_mday;
        return d;
    }

    String isoDate(int year, unsigned month, unsigned day) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%04d-%02u-%02u", year, month, day);
        return String(buf);
    }

    constexpr StringId WEEKDAY_IDS[7] = {
        StringId::CALENDAR_WEEKDAY_MON, StringId::CALENDAR_WEEKDAY_TUE,
        StringId::CALENDAR_WEEKDAY_WED, StringId::CALENDAR_WEEKDAY_THU,
        StringId::CALENDAR_WEEKDAY_FRI, StringId::CALENDAR_WEEKDAY_SAT,
        StringId::CALENDAR_WEEKDAY_SUN,
    };
    constexpr StringId MONTH_IDS[12] = {
        StringId::CALENDAR_MONTH_JAN, StringId::CALENDAR_MONTH_FEB, StringId::CALENDAR_MONTH_MAR,
        StringId::CALENDAR_MONTH_APR, StringId::CALENDAR_MONTH_MAY, StringId::CALENDAR_MONTH_JUN,
        StringId::CALENDAR_MONTH_JUL, StringId::CALENDAR_MONTH_AUG, StringId::CALENDAR_MONTH_SEP,
        StringId::CALENDAR_MONTH_OCT, StringId::CALENDAR_MONTH_NOV, StringId::CALENDAR_MONTH_DEC,
    };

    // Laenderabhaengiger Wochenstart (Alex' Auftrag, 12.10.) - basiert auf
    // LocationPresets::countryCode() (gleiche Quelle wie Feiertage/Krypto-
    // Waehrung), AUSDRUECKLICH NICHT auf der UI-Sprache (I18n::language()) -
    // ein englischsprachiger Nutzer in Grossbritannien soll weiterhin
    // Montag-Start sehen. Rueckgabewert ist der WEEKDAY_IDS[]-Index (0=Mo,
    // Montag-basierte Nummerierung wie im Rest dieser Datei), der die ERSTE
    // Spalte des Rasters werden soll - rotierend statt hart Montag/Sonntag,
    // da auch Samstag-Start (AF/IR/SO) unterstuetzt werden muss.
    int weekStartForCountry(const String& countryCode) {
        String cc = countryCode;
        cc.toLowerCase();
        constexpr const char* SUNDAY_START[] = {
            "us", "ca", "mx", "br", "jp", "cn", "kr", "in", "pk", "bd", "il", "ph",
        };
        constexpr const char* SATURDAY_START[] = {"af", "ir", "so"};
        for (const char* c : SUNDAY_START) if (cc == c) return 6; // Sonntag
        for (const char* c : SATURDAY_START) if (cc == c) return 5; // Samstag
        return 0; // Montag (Standard, ISO 8601)
    }

    // Kopfbereich wieder kompakt (Alex' Auftrag, 12.10., zweite Runde: die
    // Monat/Jahr-Ueberschrift war zu gross geraten - zurueck auf Groesse 1,
    // NUR der Screen-Titel "Kalender" oben bleibt bei der einheitlichen
    // Groesse, siehe "Pflicht: einheitliche Screen-Ueberschriftengroesse"
    // in CLAUDE.md). Der dadurch frei werdende Platz geht vollstaendig an
    // das Raster (siehe currentCellH unten).
    // BUGFIX (12.10., Alex' Fund per Foto): "Oktober 2026" wurde UNTEN
    // abgeschnitten - derselbe Baseline/Unterlaengen-Fehler wie zuvor bei
    // ISS-Radar/Rocket-Liste/Krypto-F&G-Kachel (siehe CLAUDE.md
    // "WICHTIGSTE FALLE"). Der vorherige 11px-Abstand zwischen
    // MONTH_LABEL_Y und WEEKDAY_ROW_Y liess der Monat/Jahr-Zeile keine
    // Reserve fuer ihre eigene Unterlaengen-/Descent-Zone. Fix: Abstand auf
    // 16px vergroessert (gleiches Prinzip wie die an anderer Stelle bereits
    // bewaehrten Abstands-Korrekturen), GRID_TOP im gleichen Zug mitverschoben.
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t MONTH_LABEL_Y = (int16_t)(CHROME_H + 10);
    constexpr int16_t WEEKDAY_ROW_Y = (int16_t)(CHROME_H + 26);
    constexpr int16_t GRID_TOP = (int16_t)(CHROME_H + 38);
    constexpr int16_t GRID_X = 10;
    // Breite bleibt bei 31px (7 Spalten * 31px = 217px von 220px
    // verfuegbarer Breite bei den projektweit ueblichen 10px Seitenraendern
    // - mehr geht bei 7 gleich breiten Spalten praktisch nicht). Die Hoehe
    // ist dagegen NICHT mehr fest, sondern wird in drawGrid() JEDES MAL neu
    // berechnet (currentCellH) - so gross wie der tatsaechlich verfuegbare
    // Platz fuer den jeweiligen Monat (unterschiedlich viele Zeilen) UND
    // den aktuellen Detailbereich-Zustand (eingeklappt/ausgeklappt) es
    // zulassen, nie kleiner als diese Mindestgroesse (Alex' Auftrag, 12.10.,
    // zweite Runde: "31x31 war als MINDESTgroesse gedacht, nicht als
    // Zielgroesse - so gross wie moeglich machen").
    constexpr int16_t CELL_W = 31;
    constexpr int16_t CELL_MIN_H = 31;
    int16_t currentCellH = CELL_MIN_H;

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    Rect cellRect(uint8_t row, uint8_t col) {
        return Rect{(int16_t)(GRID_X + col * CELL_W), (int16_t)(GRID_TOP + row * currentCellH),
                    CELL_W, currentCellH};
    }

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::CALENDAR_TITLE), TFT_GREEN);
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::CALENDAR_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::CALENDAR_INFO_BODY));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
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

    // ---- Eigene Termine: lokal dupliziertes Tastatur-Layout (Alex'
    // Auftrag, 12.10.: "bereits vorhandene Bildschirmtastatur aus der
    // Ersteinrichtung/WLAN-Konfiguration wiederverwenden" - Projekt-
    // Konvention ist dabei immer lokale Duplikation statt eines geteilten
    // Moduls, siehe z.B. address_search_screen.cpp::runNameKeypad(), das
    // seinerseits location_presets_screen.cpp::runPresetNameKeypad()
    // spiegelt. Diese Version hier spiegelt dieselbe Struktur). ----
    void appendUtf8(char* buf, uint8_t& len, uint8_t cap, const char* s) {
        size_t sl = strlen(s);
        if (len + sl >= cap) return;
        memcpy(buf + len, s, sl);
        len += (uint8_t)sl;
        buf[len] = 0;
    }

    void backspaceUtf8(char* buf, uint8_t& len) {
        if (len == 0) return;
        len--;
        while (len > 0 && (((uint8_t)buf[len]) & 0xC0) == 0x80) len--;
        buf[len] = 0;
    }

    String visibleTail(TFT_eSPI& tft, const char* buf, int16_t maxWidth) {
        String s(buf);
        while (s.length() > 0 && tft.textWidth(s) > maxWidth) {
            int32_t cut = ((((uint8_t)s[0]) & 0xE0) == 0xC0) ? 2 : 1;
            if (cut > (int32_t)s.length()) cut = s.length();
            s = s.substring(cut);
        }
        return s;
    }

    constexpr uint8_t SPECIAL_MAX = 16;
    constexpr uint8_t SPECIAL_PER_ROW = 6;
    constexpr uint8_t SPECIAL_ROW_MAX = 3;

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

    const char* specialToggleLabel() {
        switch (I18n::language()) {
            case I18n::LANG_DE: return "ÄÖÜ";
            case I18n::LANG_ES: return "ÑÁ";
            case I18n::LANG_FR: return "ÀÇ";
            default: return "/.-";
        }
    }

    // Gibt den eingegebenen Termintext zurueck, oder einen leeren String
    // bei Abbruch.
    String runAppointmentKeyboard(TFT_eSPI& tft) {
        MenuStars::reset();
        constexpr const char* NDIGITS = "1234567890";
        constexpr const char* NROW1 = "QWERTYUIOP";
        constexpr const char* NROW2 = "ASDFGHJKL";
        constexpr const char* NROW3 = "ZXCVBNM,.-";

        constexpr uint8_t CAP = Config::APPOINTMENT_TEXT_CAP;
        char buf[CAP] = {0};
        uint8_t len = 0;
        bool specialPage = false;

        const char* specials[SPECIAL_MAX];
        uint8_t specialCount = specialCharsForLanguage(specials);
        uint8_t specialRowCount = (uint8_t)((specialCount + SPECIAL_PER_ROW - 1) / SPECIAL_PER_ROW);

        constexpr int16_t KEY_GAP = 3;
        constexpr int16_t FIELD_H = 34;

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::CALENDAR_APPOINTMENT_PROMPT));
        int16_t fieldY = (int16_t)(tft.getCursorY() + 4);
        int16_t ROW0_Y = (int16_t)(fieldY + FIELD_H + 8);

        constexpr uint8_t LETTER_ROW_COUNT = 4;
        constexpr uint8_t TOTAL_ROWS =
            (SPECIAL_ROW_MAX > LETTER_ROW_COUNT ? SPECIAL_ROW_MAX : LETTER_ROW_COUNT) + 2;
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableH = (int16_t)(Config::SCREEN_HEIGHT - ROW0_Y - BOTTOM_MARGIN);
        int16_t KEY_H = (int16_t)((availableH - (TOTAL_ROWS - 1) * KEY_GAP) / TOTAL_ROWS);

        auto layoutRow = [&](const char* row, int16_t y, Rect* outRects, uint8_t n) {
            (void)row;
            int16_t usableW = Config::SCREEN_WIDTH - 8;
            int16_t keyW = (usableW - (n - 1) * KEY_GAP) / n;
            int16_t x = 4;
            for (uint8_t i = 0; i < n; i++) {
                outRects[i] = {x, y, keyW, KEY_H};
                x += keyW + KEY_GAP;
            }
        };

        Rect digitRects[10], row1Rects[10], row2Rects[9], row3Rects[10];
        layoutRow(NDIGITS, ROW0_Y, digitRects, 10);
        layoutRow(NROW1, (int16_t)(ROW0_Y + (KEY_H + KEY_GAP)), row1Rects, 10);
        layoutRow(NROW2, (int16_t)(ROW0_Y + 2 * (KEY_H + KEY_GAP)), row2Rects, 9);
        layoutRow(NROW3, (int16_t)(ROW0_Y + 3 * (KEY_H + KEY_GAP)), row3Rects, 10);

        Rect specRects[SPECIAL_ROW_MAX][SPECIAL_PER_ROW];
        for (uint8_t r = 0; r < specialRowCount; r++) {
            uint8_t remaining = (uint8_t)(specialCount - r * SPECIAL_PER_ROW);
            uint8_t n = remaining < SPECIAL_PER_ROW ? remaining : SPECIAL_PER_ROW;
            layoutRow(nullptr, (int16_t)(ROW0_Y + r * (KEY_H + KEY_GAP)), specRects[r], n);
        }

        Rect spaceBtn, backspaceBtn, toggleBtn, cancelBtn, confirmBtn;

        auto layoutTrailingButtons = [&]() {
            uint8_t contentRows = specialPage ? specialRowCount : 4;
            int16_t spaceRowY = (int16_t)(ROW0_Y + contentRows * (KEY_H + KEY_GAP));
            int16_t btnRowY = (int16_t)(ROW0_Y + (contentRows + 1) * (KEY_H + KEY_GAP));
            spaceBtn     = {4, spaceRowY, 150, KEY_H};
            backspaceBtn = {158, spaceRowY, (int16_t)(Config::SCREEN_WIDTH - 8 - 154), KEY_H};
            toggleBtn = {4, btnRowY, 80, KEY_H};
            cancelBtn = {(int16_t)(4 + 80 + KEY_GAP), btnRowY, 90, KEY_H};
            confirmBtn = {(int16_t)(4 + 80 + KEY_GAP + 90 + KEY_GAP), btnRowY,
                          (int16_t)(Config::SCREEN_WIDTH - 8 - (80 + KEY_GAP + 90 + KEY_GAP)), KEY_H};
        };
        layoutTrailingButtons();

        bool done = false;
        bool cancelled = false;

        auto redraw = [&]() {
            layoutTrailingButtons();
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::CALENDAR_APPOINTMENT_PROMPT));

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
                for (uint8_t i = 0; i < 10; i++) drawButton(tft, row3Rects[i], String(NROW3[i]));
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
            drawButton(tft, cancelBtn, I18n::t(StringId::BACK_BTN));
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
                for (uint8_t i = 0; i < 10 && !handled; i++) {
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
            if (!handled && cancelBtn.contains(tap.x, tap.y)) { cancelled = true; done = true; handled = true; }
            if (!handled && confirmBtn.contains(tap.x, tap.y)) { done = true; handled = true; }

            if (handled) redraw();
        }

        return cancelled ? String() : String(buf);
    }

    // ---- Optionale Uhrzeit-Auswahl (Alex' Auftrag, 12.10., erster
    // Teilschritt Richtung Termin-Erinnerung - noch OHNE Erinnerungs-Logik
    // selbst) - folgt nach der Termintext-Tastatur, bevor der Termin
    // gespeichert wird. Einfacher Plus/Minus-Stepper (24h-Format, Minute
    // in 5er-Schritten - gaengige, schnell bedienbare Granularitaet fuer
    // Termine), PLUS deutlicher "Ohne Uhrzeit speichern"-Button zum
    // Ueberspringen. Alle Buttons >= 31x31 (CLAUDE.md-Mindest-Tapflaeche),
    // hier grosszuegiger (44px) fuer komfortables wiederholtes Antippen. ----
    struct TimePickerResult {
        bool hasTime;
        uint8_t hour;
        uint8_t minute;
    };

    TimePickerResult runTimePicker(TFT_eSPI& tft) {
        MenuStars::reset();

        uint8_t hour = 12;
        uint8_t minute = 0;

        constexpr int16_t STEP_BTN = 44;
        constexpr int16_t DISPLAY_Y = 76;
        constexpr int16_t LABEL_Y = 110;
        constexpr int16_t ROW_Y = 130;
        constexpr int16_t SKIP_H = 40;
        constexpr int16_t OK_H = 40;
        constexpr int16_t HALF_CX_LEFT = (int16_t)(Config::SCREEN_WIDTH / 4);
        constexpr int16_t HALF_CX_RIGHT = (int16_t)(Config::SCREEN_WIDTH * 3 / 4);

        Rect hourMinus = {(int16_t)(HALF_CX_LEFT - STEP_BTN - 10), ROW_Y, STEP_BTN, STEP_BTN};
        Rect hourPlus  = {(int16_t)(HALF_CX_LEFT + 10), ROW_Y, STEP_BTN, STEP_BTN};
        Rect minMinus  = {(int16_t)(HALF_CX_RIGHT - STEP_BTN - 10), ROW_Y, STEP_BTN, STEP_BTN};
        Rect minPlus   = {(int16_t)(HALF_CX_RIGHT + 10), ROW_Y, STEP_BTN, STEP_BTN};

        Rect skipBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - SKIP_H - OK_H - 14),
                         (int16_t)(Config::SCREEN_WIDTH - 20), SKIP_H};
        Rect okBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - OK_H - 6),
                      (int16_t)(Config::SCREEN_WIDTH - 20), OK_H};

        bool done = false;
        bool skipped = false;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::CALENDAR_TIME_PROMPT));

            char buf[6];
            snprintf(buf, sizeof(buf), "%02u:%02u", hour, minute);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.setTextSize(3);
            tft.drawString(String(buf), (int16_t)(Config::SCREEN_WIDTH / 2), DISPLAY_Y);
            tft.setTextSize(1);

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(I18n::t(StringId::CALENDAR_HOUR_LABEL), HALF_CX_LEFT, LABEL_Y);
            tft.drawString(I18n::t(StringId::CALENDAR_MINUTE_LABEL), HALF_CX_RIGHT, LABEL_Y);
            tft.setTextDatum(TL_DATUM);

            drawButton(tft, hourMinus, "-");
            drawButton(tft, hourPlus, "+");
            drawButton(tft, minMinus, "-");
            drawButton(tft, minPlus, "+");

            drawButton(tft, skipBtn, I18n::t(StringId::CALENDAR_SAVE_WITHOUT_TIME));
            drawButton(tft, okBtn, I18n::t(StringId::OK));
        };
        redraw();

        while (!done) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }

            bool handled = false;
            if (hourMinus.contains(tap.x, tap.y)) { hour = (hour == 0) ? 23 : (uint8_t)(hour - 1); handled = true; }
            else if (hourPlus.contains(tap.x, tap.y)) { hour = (uint8_t)((hour + 1) % 24); handled = true; }
            else if (minMinus.contains(tap.x, tap.y)) { minute = (minute < 5) ? 55 : (uint8_t)(minute - 5); handled = true; }
            else if (minPlus.contains(tap.x, tap.y)) { minute = (uint8_t)((minute + 5) % 60); handled = true; }
            else if (skipBtn.contains(tap.x, tap.y)) { skipped = true; done = true; }
            else if (okBtn.contains(tap.x, tap.y)) { done = true; }

            if (handled) redraw();
        }

        TimePickerResult result;
        result.hasTime = !skipped;
        result.hour = hour;
        result.minute = minute;
        return result;
    }

    // Standard-Uhrzeit fuer "1 Tag vorher", wenn der Termin selbst KEINE
    // Uhrzeit hat (Alex' Auftrag, 12.10., zweiter Teil: "z.B. 09:00 Uhr,
    // das bitte auch so in der Button-Beschriftung sichtbar machen") - MUSS
    // exakt zu appointment_reminders.cpp::computeReminderEpoch() passen.
    constexpr uint8_t DAY1_DEFAULT_HOUR = 9;
    constexpr uint8_t DAY1_DEFAULT_MINUTE = 0;

    // ---- Erinnerungs-Auswahl (Alex' Auftrag, 12.10., zweiter/letzter Teil
    // der Termin-Erinnerung) - folgt direkt nach der Uhrzeit-Auswahl. Ohne
    // gesetzte Uhrzeit ergeben die minuten-/stunden-basierten Optionen
    // keinen Sinn (kein fester Zeitpunkt vorhanden) und werden gar nicht
    // erst angeboten - nur "Keine" und "1 Tag vorher" (mit der festen
    // Standard-Uhrzeit in der Beschriftung, siehe oben). ----
    Appointments::ReminderType runReminderPicker(TFT_eSPI& tft, bool hasTime) {
        MenuStars::reset();

        struct Option {
            Appointments::ReminderType type;
            String label;
        };
        Option options[5];
        uint8_t optionCount = 0;
        options[optionCount++] = {Appointments::ReminderType::None, String(I18n::t(StringId::CALENDAR_REMINDER_NONE))};
        if (hasTime) {
            options[optionCount++] = {Appointments::ReminderType::Min10, String(I18n::t(StringId::CALENDAR_REMINDER_MIN10))};
            options[optionCount++] = {Appointments::ReminderType::Min30, String(I18n::t(StringId::CALENDAR_REMINDER_MIN30))};
            options[optionCount++] = {Appointments::ReminderType::Hour1, String(I18n::t(StringId::CALENDAR_REMINDER_HOUR1))};
            options[optionCount++] = {Appointments::ReminderType::Day1, String(I18n::t(StringId::CALENDAR_REMINDER_DAY1))};
        } else {
            char buf[8];
            snprintf(buf, sizeof(buf), "%02u:%02u", DAY1_DEFAULT_HOUR, DAY1_DEFAULT_MINUTE);
            options[optionCount++] = {Appointments::ReminderType::Day1,
                String(I18n::t(StringId::CALENDAR_REMINDER_DAY1)) + " (" + buf + ")"};
        }

        constexpr int16_t ROW_H = 40;
        constexpr int16_t ROW_GAP = 8;
        constexpr int16_t TOP = 50;

        auto rowRect = [&](uint8_t i) -> Rect {
            return {10, (int16_t)(TOP + i * (ROW_H + ROW_GAP)), (int16_t)(Config::SCREEN_WIDTH - 20), ROW_H};
        };

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::CALENDAR_REMINDER_PROMPT));
            for (uint8_t i = 0; i < optionCount; i++) {
                drawButton(tft, rowRect(i), options[i].label);
            }
        };
        redraw();

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
            for (uint8_t i = 0; i < optionCount; i++) {
                if (rowRect(i).contains(tap.x, tap.y)) return options[i].type;
            }
        }
    }

    // Zeigt "HH:MM - Text" wenn eine Uhrzeit gesetzt ist, sonst nur den
    // Text (Alex' Auftrag, 12.10.).
    String appointmentDisplayText(const Appointments::Entry& e) {
        if (!e.hasTime) return String(e.text);
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u:%02u", e.hour, e.minute);
        return String(buf) + " - " + String(e.text);
    }

    // ---- Feiertage/Termine-Anzeige unterhalb des Rasters ----

    String holidayNameForDay(const NetTask::HolidayData& data, int year, unsigned month, unsigned day) {
        String target = isoDate(year, month, day);
        for (uint8_t i = 0; i < data.holidayCount; i++) {
            if (data.holidays[i].date == target) return data.holidays[i].name;
        }
        return String();
    }

    bool dayHasAppointment(int year, unsigned month, unsigned day) {
        String target = isoDate(year, month, day);
        for (uint8_t i = 0; i < Appointments::count(); i++) {
            if (target == Appointments::get(i).date) return true;
        }
        return false;
    }

    // ---- Eigener Tages-Detail-Screen (Alex' Auftrag, 12.10., zweite
    // Runde: komplett neuer, eigener Screen statt Detailzeile unterhalb
    // des Rasters - JEDER Tag ist antippbar, nicht nur markierte. Der
    // vorherige "+"-Button auf dem Hauptscreen entfaellt dadurch
    // vollstaendig). "year"/"month"/"day" werden als LOKALE Parameter
    // uebergeben statt (wie vorher) in einem modulweiten "selectedDate"
    // zwischengespeichert - das war vermutlich die Ursache fuer den von
    // Alex gemeldeten Bug ("neue Termine landeten faelschlich auf
    // 'morgen'"): ein geteilter, mutierbarer Zustand ist fehleranfaelliger
    // als ein Wert, der einmal beim OEffnen des Screens fest uebergeben
    // wird und sich danach nicht mehr aendern kann. ----
    void showDayDetailScreen(TFT_eSPI& tft, int year, unsigned month, unsigned day,
                              const NetTask::HolidayData& holidayData, bool haveHolidayData) {
        MenuStars::reset();

        const String dateStr = isoDate(year, month, day);
        char dateDisplay[11];
        snprintf(dateDisplay, sizeof(dateDisplay), "%02u.%02u.%04d", day, month, year);
        const String holidayName = haveHolidayData ? holidayNameForDay(holidayData, year, month, day) : String();

        constexpr uint8_t MAX_VISIBLE_APPT_ROWS = 8;
        uint8_t apptIndices[Config::APPOINTMENT_MAX_COUNT]; // globale Appointments-Indizes dieses Tages
        uint8_t apptCount = 0;
        uint8_t scrollOffset = 0;
        MarqueeLabel::State apptRowMarquee[MAX_VISIBLE_APPT_ROWS];
        for (uint8_t i = 0; i < MAX_VISIBLE_APPT_ROWS; i++) MarqueeLabel::reset(apptRowMarquee[i]);

        auto refreshAppointments = [&]() {
            apptCount = 0;
            for (uint8_t i = 0; i < Appointments::count() && apptCount < Config::APPOINTMENT_MAX_COUNT; i++) {
                if (dateStr == Appointments::get(i).date) apptIndices[apptCount++] = i;
            }
            if (scrollOffset >= apptCount) scrollOffset = 0;
        };
        refreshAppointments();

        constexpr int16_t TITLE_Y = 14;
        const int16_t holidayY = 34;
        const int16_t listTop = (int16_t)(holidayName.length() > 0 ? 54 : 36);
        constexpr int16_t BACK_H = 40;
        const int16_t backY = (int16_t)(Config::SCREEN_HEIGHT - BACK_H - 6);
        constexpr int16_t ADD_H = 40;
        constexpr int16_t ROW_GAP = 8;
        const int16_t addY = (int16_t)(backY - ROW_GAP - ADD_H);
        const int16_t listBottom = (int16_t)(addY - 8);

        Rect backBtn = {10, backY, (int16_t)(Config::SCREEN_WIDTH - 20), BACK_H};
        Rect addBtn = {10, addY, (int16_t)(Config::SCREEN_WIDTH - 20), ADD_H};

        constexpr int16_t APPT_ROW_H = 24;
        constexpr int16_t DELETE_BTN_W = 26;
        Rect apptDeleteBtnRects[MAX_VISIBLE_APPT_ROWS];
        uint8_t apptDeleteGlobalIndex[MAX_VISIBLE_APPT_ROWS];
        uint8_t visibleApptRowCount = 0;
        Rect scrollUpBtnRect, scrollDownBtnRect;
        bool scrollControlsVisible = false;

        auto drawApptList = [&](bool resetMarquees) {
            tft.fillRect(0, listTop, Config::SCREEN_WIDTH, (int16_t)(listBottom - listTop), TFT_BLACK);
            visibleApptRowCount = 0;
            scrollControlsVisible = false;
            if (apptCount == 0) return;

            int16_t availForList = (int16_t)(listBottom - listTop);
            uint8_t maxRows = (uint8_t)(availForList / APPT_ROW_H);
            if (maxRows > MAX_VISIBLE_APPT_ROWS) maxRows = MAX_VISIBLE_APPT_ROWS;
            if (maxRows == 0) return;

            bool needsScroll = apptCount > maxRows;
            uint8_t rowsToShow = needsScroll ? (uint8_t)(maxRows - 1) : maxRows;
            if (rowsToShow > apptCount) rowsToShow = apptCount;

            if (resetMarquees) {
                for (uint8_t i = 0; i < MAX_VISIBLE_APPT_ROWS; i++) MarqueeLabel::reset(apptRowMarquee[i]);
            }

            for (uint8_t i = 0; i < rowsToShow; i++) {
                uint8_t apptPos = (uint8_t)(scrollOffset + i);
                if (apptPos >= apptCount) break;
                uint8_t globalIdx = apptIndices[apptPos];

                int16_t rowTop = (int16_t)(listTop + i * APPT_ROW_H);
                int16_t rowCy = (int16_t)(rowTop + APPT_ROW_H / 2);

                Rect delBtn = {(int16_t)(Config::SCREEN_WIDTH - 10 - DELETE_BTN_W), (int16_t)(rowTop + 1),
                               DELETE_BTN_W, (int16_t)(APPT_ROW_H - 2)};
                apptDeleteBtnRects[i] = delBtn;
                apptDeleteGlobalIndex[i] = globalIdx;
                drawButton(tft, delBtn, "x");

                int16_t textMaxW = (int16_t)(Config::SCREEN_WIDTH - 20 - DELETE_BTN_W - 4);
                MarqueeLabel::draw(tft, apptRowMarquee[i], 10, rowCy, textMaxW, 18,
                                    appointmentDisplayText(Appointments::get(globalIdx)), TFT_WHITE, TFT_BLACK);
            }
            visibleApptRowCount = rowsToShow;

            if (needsScroll) {
                int16_t scrollRowY = (int16_t)(listTop + rowsToShow * APPT_ROW_H);
                scrollUpBtnRect = {10, scrollRowY, 110, (int16_t)(APPT_ROW_H - 2)};
                scrollDownBtnRect = {130, scrollRowY, 100, (int16_t)(APPT_ROW_H - 2)};
                drawButton(tft, scrollUpBtnRect, "^");
                drawButton(tft, scrollDownBtnRect, "v");
                scrollControlsVisible = true;
            }
        };

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, TITLE_Y);
        tft.println(dateDisplay);
        if (holidayName.length() > 0) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            tft.setCursor(10, holidayY);
            tft.println(holidayName);
        }
        drawApptList(true);
        drawButton(tft, addBtn, I18n::t(StringId::CALENDAR_ADD_APPOINTMENT_BTN));
        drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;

                if (addBtn.contains(tap.x, tap.y)) {
                    // "dateStr" ist ein lokaler, seit dem Oeffnen dieses
                    // Screens unveraenderlicher Wert - der neue Termin wird
                    // IMMER genau dem hier angezeigten Datum zugeordnet,
                    // unabhaengig davon, welcher Tag zwischenzeitlich sonst
                    // irgendwo angetippt wurde (behebt den gemeldeten
                    // "landet auf morgen"-Bug strukturell).
                    String text = runAppointmentKeyboard(tft);
                    if (text.length() > 0) {
                        TimePickerResult tr = runTimePicker(tft);
                        Appointments::ReminderType reminder = runReminderPicker(tft, tr.hasTime);
                        Appointments::add(dateStr.c_str(), text, tr.hasTime, tr.hour, tr.minute, reminder);
                        refreshAppointments();
                        scrollOffset = 0;
                    }
                    MenuStars::reset();
                    tft.fillScreen(TFT_BLACK);
                    tft.setTextColor(TFT_GREEN, TFT_BLACK);
                    tft.setCursor(10, TITLE_Y);
                    tft.println(dateDisplay);
                    if (holidayName.length() > 0) {
                        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                        tft.setCursor(10, holidayY);
                        tft.println(holidayName);
                    }
                    drawApptList(true);
                    drawButton(tft, addBtn, I18n::t(StringId::CALENDAR_ADD_APPOINTMENT_BTN));
                    drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
                    continue;
                }

                if (scrollControlsVisible && scrollUpBtnRect.contains(tap.x, tap.y) && scrollOffset > 0) {
                    scrollOffset--;
                    drawApptList(true);
                    continue;
                }
                if (scrollControlsVisible && scrollDownBtnRect.contains(tap.x, tap.y) &&
                    scrollOffset + visibleApptRowCount < apptCount) {
                    scrollOffset++;
                    drawApptList(true);
                    continue;
                }

                bool deleted = false;
                for (uint8_t i = 0; i < visibleApptRowCount; i++) {
                    if (apptDeleteBtnRects[i].contains(tap.x, tap.y)) {
                        Appointments::removeAt(apptDeleteGlobalIndex[i]);
                        refreshAppointments();
                        drawApptList(true);
                        deleted = true;
                        break;
                    }
                }
                if (deleted) continue;
            }

            // Lauftext-Animation fuer zu lange Termintexte (ohne Reset, wie
            // an anderer Stelle im Projekt etabliert).
            for (uint8_t i = 0; i < visibleApptRowCount; i++) {
                uint8_t apptPos = (uint8_t)(scrollOffset + i);
                if (apptPos >= apptCount) break;
                int16_t rowCy = (int16_t)(listTop + i * APPT_ROW_H + APPT_ROW_H / 2);
                int16_t textMaxW = (int16_t)(Config::SCREEN_WIDTH - 20 - DELETE_BTN_W - 4);
                MarqueeLabel::draw(tft, apptRowMarquee[i], 10, rowCy, textMaxW, 18,
                                    appointmentDisplayText(Appointments::get(apptIndices[apptPos])), TFT_WHITE, TFT_BLACK);
            }
            MenuStars::update(tft);
            delay(20);
        }
    }

    uint8_t lastGridRows = 0;
    int lastGridYear = 0;
    unsigned lastGridMonth = 0;

    // Letztes gezeichnetes Raster-Rect je Zelle gemerkt (handleTap() braucht
    // das fuer die Tap-Erkennung) - plus die zugehoerigen Kalenderdaten
    // (Tag/Monat/Jahr je Zelle), damit handleTap() nicht erneut rechnen muss.
    constexpr uint8_t MAX_GRID_ROWS = 6;
    Rect lastCellRects[MAX_GRID_ROWS][7];
    unsigned lastCellDay[MAX_GRID_ROWS][7];
    bool lastCellValid[MAX_GRID_ROWS][7];

    // Laenderabhaengiger Wochenstart (Alex' Auftrag, 12.10., siehe
    // weekStartForCountry() oben) - einmal pro enter() ermittelt, NICHT
    // von NetTask abhaengig (reiner Core-1-Zugriff auf LocationPresets,
    // kein Netzwerk-Abruf noetig).
    int weekStart = 0;

    void drawGrid(TFT_eSPI& tft, const NetTask::HolidayData& data, bool haveHolidayData) {
        LocalDate today = todayLocal();
        int year = today.year;
        unsigned month = today.month;

        // Spalte, in der der 1. des Monats im rotierten Raster landet -
        // ersetzt das vorherige, hart Montag-first angenommene "firstWd"
        // direkt als Spaltenindex.
        int firstCol = ((weekdayOf(year, month, 1) - weekStart) % 7 + 7) % 7;
        unsigned numDays = daysInMonth(year, month);
        uint8_t rows = (uint8_t)((firstCol + (int)numDays + 6) / 7);
        if (rows > MAX_GRID_ROWS) rows = MAX_GRID_ROWS; // theoretisch nie noetig (max. 6)
        lastGridRows = rows;
        lastGridYear = year;
        lastGridMonth = month;

        // Zellhoehe neu berechnet - JEDES Mal, wenn das Raster neu
        // gezeichnet wird (Alex' Auftrag, 12.10.: "so gross wie moeglich",
        // 31x31 ist nur die Mindestgroesse). Seit dem Umbau auf einen
        // eigenen Tages-Detail-Screen (zweite Runde, 12.10.) muss unterhalb
        // des Rasters kein Platz mehr fuer eine Detailzeile reserviert
        // werden - nur noch ein kleiner Rand zum Bildschirmrand.
        constexpr int16_t BOTTOM_MARGIN = 6;
        int16_t availableForGrid = (int16_t)(Config::SCREEN_HEIGHT - GRID_TOP - BOTTOM_MARGIN);
        currentCellH = (int16_t)(availableForGrid / rows);
        if (currentCellH < CELL_MIN_H) currentCellH = CELL_MIN_H;

        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH,
                     (int16_t)(GRID_TOP + rows * currentCellH - CHROME_H), TFT_BLACK);

        // Monat/Jahr wieder kompakt, Groesse 1 (Alex' Auftrag, 12.10.,
        // zweite Runde: die vorherige Groesse 2 war zu gross geraten) - der
        // dadurch freie Platz kommt stattdessen komplett dem Raster zugute
        // (siehe currentCellH oben). Nur der Screen-Titel "Kalender" oben
        // bleibt bei der einheitlichen Screen-Ueberschriftengroesse.
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        String monthYear = String(I18n::t(MONTH_IDS[month - 1])) + " " + String(year);
        tft.drawString(monthYear, (int16_t)(Config::SCREEN_WIDTH / 2), MONTH_LABEL_Y);

        for (uint8_t col = 0; col < 7; col++) {
            int wd = (weekStart + col) % 7;
            tft.drawString(I18n::t(WEEKDAY_IDS[wd]), (int16_t)(GRID_X + col * CELL_W + CELL_W / 2),
                            WEEKDAY_ROW_Y);
        }
        tft.setTextDatum(TL_DATUM);

        for (uint8_t r = 0; r < MAX_GRID_ROWS; r++) {
            for (uint8_t c = 0; c < 7; c++) lastCellValid[r][c] = false;
        }

        for (unsigned day = 1; day <= numDays; day++) {
            int cellIdx = firstCol + (int)day - 1;
            uint8_t row = (uint8_t)(cellIdx / 7);
            uint8_t col = (uint8_t)(cellIdx % 7);
            if (row >= MAX_GRID_ROWS) continue;

            Rect r = cellRect(row, col);
            lastCellRects[row][col] = r;
            lastCellDay[row][col] = day;
            lastCellValid[row][col] = true;

            bool isToday = (day == today.day);
            String holidayName = haveHolidayData ? holidayNameForDay(data, year, month, day) : String();
            bool isHoliday = holidayName.length() > 0;
            bool hasAppt = dayHasAppointment(year, month, day);
            // Tatsaechlicher (Montag-basierter) Wochentag dieser Zelle -
            // aus Spalte+Wochenstart zurueckgerechnet (siehe weekStart
            // oben), NICHT aus der Spaltenposition allein, da das Raster
            // laenderabhaengig rotiert ist. Index 6 = Sonntag (WEEKDAY_IDS-
            // Reihenfolge Mo..So).
            bool isSunday = ((weekStart + col) % 7) == 6;

            if (isToday) {
                tft.drawRoundRect(r.x, r.y, r.w, r.h, 3, TFT_GREEN);
            }

            // Feiertag, Sonntag UND eigener Termin nutzen jetzt EINE
            // gemeinsame rote Farbe als einfaches "hier ist etwas
            // Besonderes"-Signal (Alex' Auftrag, 12.10. - ersetzt die
            // vorherige Grün/Gelb/weisser-Punkt-Kombination). Welcher Fall
            // genau zutrifft, zeigt sich erst nach dem Antippen (Feiertags-
            // name/Terminliste in der Detailzeile).
            bool isSpecial = isHoliday || isSunday || hasAppt;
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(isSpecial ? TFT_RED : TFT_WHITE, TFT_BLACK);
            tft.drawString(String(day), (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
            tft.setTextDatum(TL_DATUM);
        }
    }

    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    NetTask::HolidayData lastData;
}

void enter(TFT_eSPI& tft) {
    // NetTask (Core 0) liest LocationPresets NIEMALS selbst - Laendercode
    // wird hier einmal pro enter() weitergereicht (gleiches Prinzip wie
    // CryptoScreen::enter()/setCryptoCurrency()).
    NetTask::setHolidayCountry(LocationPresets::countryCode().c_str());
    weekStart = weekStartForCountry(LocationPresets::countryCode());

    drawChrome(tft);
    drawGrid(tft, lastData, haveData);
}

void update(TFT_eSPI& tft) {
    uint32_t v = NetTask::holidayVersion();
    if (v != lastSeenVersion) {
        NetTask::HolidayData newData;
        if (NetTask::getHolidayData(newData)) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawGrid(tft, lastData, true);
        }
    } else if (!haveData) {
        NetTask::Status st = NetTask::holidayStatus();
        if (st != lastSeenStatus) {
            lastSeenStatus = st;
            // Fehlerfall bewusst NICHT vollflaechig ueberdecken - das
            // Monatsraster selbst ist auch OHNE Feiertagsdaten schon
            // nuetzlich (heutiger Tag sichtbar, eigene Termine
            // funktionieren unabhaengig davon).
        }
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        drawChrome(tft);
        drawGrid(tft, lastData, haveData);
        return;
    }

    // JEDER Tag ist antippbar (Alex' Auftrag, 12.10., zweite Runde), nicht
    // nur rot markierte - oeffnet den eigenen Tages-Detail-Screen statt
    // einer Detailzeile hier auf dem Hauptscreen.
    for (uint8_t r = 0; r < lastGridRows; r++) {
        for (uint8_t c = 0; c < 7; c++) {
            if (!lastCellValid[r][c]) continue;
            if (!lastCellRects[r][c].contains(x, y)) continue;

            showDayDetailScreen(tft, lastGridYear, lastGridMonth, lastCellDay[r][c], lastData, haveData);
            MenuStars::reset();
            drawChrome(tft);
            // Volles drawGrid() (nicht nur ein Teil-Redraw) - ein neu
            // angelegter/geloeschter Termin auf dem Detail-Screen kann den
            // roten "hier ist was Besonderes"-Status dieses Tages
            // veraendert haben.
            drawGrid(tft, lastData, haveData);
            return;
        }
    }
}

}
