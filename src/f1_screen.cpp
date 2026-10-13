#include "f1_screen.h"
#include <time.h>
#include <math.h>
#include "touch_input.h"
#include "menu_stars.h"
#include "ui_header.h"
#include "marquee_label.h"
#include "config.h"
#include "i18n.h"
#include "f1_client.h"
#include "net_task.h"

namespace F1Screen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    void drawButton(TFT_eSPI& tft, const Rect& r, const String& label) {
        uint16_t accent = TFT_GREEN;
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(accent, TFT_BLACK);
        tft.drawString(label, (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
        tft.setTextDatum(TL_DATUM);
    }

    // Wortweiser Umbruch anhand echter Pixelbreite, mit optionaler
    // Scroll-Unterstuetzung - 1:1 dasselbe Muster wie in rocket_screen.cpp/
    // transit_screen.cpp, hier lokal dupliziert (Projekt-Konvention).
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

    void drawWrappedCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                              int16_t lineHeight, const String& text) {
        constexpr uint8_t MAX_LINES = 4;
        String lines[MAX_LINES];
        uint8_t lineCount = 0;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len && lineCount < MAX_LINES) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;
            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }
            lines[lineCount++] = line;
            start += line.length();
        }
        tft.setTextDatum(MC_DATUM);
        int16_t top = (int16_t)(cy - ((lineCount - 1) * lineHeight) / 2);
        for (uint8_t i = 0; i < lineCount; i++) {
            tft.drawString(lines[i], cx, (int16_t)(top + i * lineHeight));
        }
        tft.setTextDatum(TL_DATUM);
    }

    // ---- Lokale Zeit des GERAETS fuer die Sessionzeiten-Liste im
    // Detail-Screen (Alex' Auftrag, 06.10.: "lokale Zeitzone des Geraets,
    // nicht die EU-Pauschalregel"). Das Geraet selbst laeuft komplett in
    // UTC (main.cpp::configTime(0,0,...)) und kennt keine eigene
    // Zeitzonen-Einstellung (nur lat/lon ueber LocationPresets, siehe
    // CLAUDE.md) - anders als beim OEPNV-Screen gibt es hier auch KEINE
    // API-gelieferte Ziel-Zeitzone (ein Rennkalender hat keinen
    // "Streckentyp"-tz-Wert, der fuer Alex' eigenen Standort relevant
    // waere). Deshalb (Interpretation des Auftrags, bitte zurueckmelden
    // falls falsch gedeutet): dieselbe DST-Berechnung wie beim OEPNV-
    // Screen (isEuDst(), 1:1 dupliziert), aber mit einer FESTEN
    // Mitteleuropa-Basis (+1h Winterzeit) statt einer Tabelle, da keine
    // Ziel-Zeitzone zum Nachschlagen vorliegt - deckt Alex' eigenen
    // Standort (Deutschland) korrekt ab.
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

    // "DD.MM. HH:MM" in Geraete-Lokalzeit (siehe Kommentar oben).
    String formatDeviceLocalDateTime(time_t epoch) {
        if (epoch <= 0) return "--";
        int year = yearOfEpochApprox(epoch);
        int offsetHours = 1 + (isEuDst(epoch, year) ? 1 : 0);
        time_t localEpoch = epoch + (time_t)offsetHours * 3600;
        struct tm t;
        gmtime_r(&localEpoch, &t);
        char buf[20];
        snprintf(buf, sizeof(buf), "%02d.%02d. %02d:%02d", t.tm_mday, t.tm_mon + 1, t.tm_hour, t.tm_min);
        return String(buf);
    }

    // ---- Uebersetzung der rohen OpenF1/Ergast-Texte (Alex' Auftrag,
    // 06.10.: "ich will kein englisches Wort sehen" - siehe CLAUDE.md
    // "Weltraumwetter"-Lehre). Es gibt bewusst KEINEN Rohtext-Fallback -
    // ein unbekannter Wert landet immer bei einem uebersetzten
    // Sammelbegriff, nie beim rohen englischen API-Wert. ----
    StringId sessionNameStringId(const String& raw) {
        if (raw == "Practice 1") return StringId::F1_SESSION_PRACTICE1;
        if (raw == "Practice 2") return StringId::F1_SESSION_PRACTICE2;
        if (raw == "Practice 3") return StringId::F1_SESSION_PRACTICE3;
        if (raw == "Qualifying") return StringId::F1_SESSION_QUALIFYING;
        if (raw == "Sprint Qualifying" || raw == "Sprint Shootout") return StringId::F1_SESSION_SPRINT_QUALIFYING;
        if (raw == "Sprint") return StringId::F1_SESSION_SPRINT;
        if (raw == "Race") return StringId::F1_SESSION_RACE;
        return StringId::F1_SESSION_GENERIC;
    }

    // Erkennt "+N Lap"/"+N Laps" (Ergast-"status"-Konvention fuer
    // ueberrundete, aber noch klassifizierte Fahrer) und baut daraus einen
    // uebersetzten Text - alle anderen bekannten Werte ueber eine feste
    // Tabelle, unbekannte Werte ueber den uebersetzten Sammelbegriff
    // F1_STATUS_DNF_GENERIC (NIE der rohe englische Text selbst).
    String formatResultStatus(const String& raw) {
        if (raw == "Finished") return I18n::t(StringId::F1_STATUS_FINISHED);
        if (raw == "Retired") return I18n::t(StringId::F1_STATUS_RETIRED);
        if (raw == "Accident") return I18n::t(StringId::F1_STATUS_ACCIDENT);
        if (raw == "Collision") return I18n::t(StringId::F1_STATUS_COLLISION);
        if (raw == "Engine") return I18n::t(StringId::F1_STATUS_ENGINE);
        if (raw == "Gearbox") return I18n::t(StringId::F1_STATUS_GEARBOX);
        if (raw == "Transmission") return I18n::t(StringId::F1_STATUS_TRANSMISSION);
        if (raw == "Hydraulics") return I18n::t(StringId::F1_STATUS_HYDRAULICS);
        if (raw == "Electrical") return I18n::t(StringId::F1_STATUS_ELECTRICAL);
        if (raw == "Brakes") return I18n::t(StringId::F1_STATUS_BRAKES);
        if (raw == "Suspension") return I18n::t(StringId::F1_STATUS_SUSPENSION);
        if (raw == "Puncture") return I18n::t(StringId::F1_STATUS_PUNCTURE);
        if (raw == "Spun off") return I18n::t(StringId::F1_STATUS_SPUN_OFF);
        if (raw == "Disqualified") return I18n::t(StringId::F1_STATUS_DISQUALIFIED);
        if (raw == "Not classified") return I18n::t(StringId::F1_STATUS_NOT_CLASSIFIED);
        if (raw == "Did not start") return I18n::t(StringId::F1_STATUS_DID_NOT_START);
        if (raw == "Did not qualify") return I18n::t(StringId::F1_STATUS_DID_NOT_QUALIFY);
        if (raw == "Withdrew") return I18n::t(StringId::F1_STATUS_WITHDREW);

        if (raw.startsWith("+") && raw.endsWith("Lap")) {
            int laps = raw.substring(1, raw.indexOf(' ')).toInt();
            if (laps > 0) return "+" + String(laps) + " " + I18n::t(StringId::F1_GAP_LAP_SINGULAR);
        }
        if (raw.startsWith("+") && raw.endsWith("Laps")) {
            int laps = raw.substring(1, raw.indexOf(' ')).toInt();
            if (laps > 0) return "+" + String(laps) + " " + I18n::t(StringId::F1_GAP_LAP_PLURAL);
        }
        return I18n::t(StringId::F1_STATUS_DNF_GENERIC);
    }

    String formatGapText(bool hasGapSeconds, float gapSeconds, bool hasGapLaps, int16_t gapLaps) {
        if (hasGapLaps && gapLaps > 0) {
            String unit = I18n::t(gapLaps == 1 ? StringId::F1_GAP_LAP_SINGULAR : StringId::F1_GAP_LAP_PLURAL);
            return "+" + String(gapLaps) + " " + unit;
        }
        if (hasGapSeconds) {
            if (gapSeconds <= 0.001f) return "-";
            char buf[16];
            snprintf(buf, sizeof(buf), "+%.3f", (double)gapSeconds);
            return String(buf);
        }
        return "";
    }

    // "M:SS.sss", z.B. 83.417s -> "1:23.417" (Alex' Auftrag, 06.10.) -
    // uebliche Rundenzeit-Schreibweise, keine Uebersetzung noetig (reines
    // Zahlenformat, kein Wort).
    String formatLapTime(float seconds) {
        if (seconds <= 0) return "-";
        int totalMs = (int)lroundf(seconds * 1000.0f);
        int minutes = totalMs / 60000;
        int secPart = (totalMs / 1000) % 60;
        int msPart = totalMs % 1000;
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d.%03d", minutes, secPart, msPart);
        return String(buf);
    }

    // Config::HEADER_HEIGHT statt eines eigenen CHROME_H-Werts (siehe
    // CLAUDE.md "Pflicht: einheitliche grosse Ueberschrift").
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 16);
    constexpr int16_t MORE_BTN_H = 36;
    constexpr int16_t MORE_BTN_Y = (int16_t)(Config::SCREEN_HEIGHT - MORE_BTN_H - 10);

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }
    Rect moreBtnRect() { return Rect{10, MORE_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), MORE_BTN_H}; }

    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::F1_TITLE), TFT_GREEN);
    }

    void clearContentArea(TFT_eSPI& tft) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        clearContentArea(tft);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::F1_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::F1_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::F1_INFO_BODY));

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

    // ---- Persistenter Zustand (ueber mehrere enter()/leave()-Zyklen
    // hinweg, gleiches Prinzip wie rocket_screen.cpp/weather_screen.cpp).
    // Der eigentliche Netzwerk-Abruf laeuft ausschliesslich in NetTask auf
    // Core 0. ----
    bool haveSeason = false;
    uint32_t lastSeasonVersion = 0;
    NetTask::Status lastSeasonStatus = NetTask::Status::Pending;
    F1Client::SeasonData lastSeason;

    bool haveLiveInfo = false; // true, sobald NetTask mind. einmal geprueft hat, ob live gefahren wird
    uint32_t lastLiveVersion = 0;
    NetTask::F1LiveBoard lastLive;

    long lastDisplayedCountdownSeconds = -1;

    MarqueeLabel::State sessionNameMarquee;
    MarqueeLabel::State raceNameMarquee;
    MarqueeLabel::State raceLocationMarquee;
    MarqueeLabel::State top3Marquee[3];

    constexpr int16_t TEXT_LINE_H = 15;
    constexpr int16_t LIVE_ROW_H = 20;

    // Zeichnet die Live-Rangliste (Fall A) - so viele Zeilen wie passen,
    // Rest im Detail-Screen ("Mehr", Alex' Auftrag, 06.10.).
    void drawLiveBoard(TFT_eSPI& tft, const NetTask::F1LiveBoard& board) {
        clearContentArea(tft);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        MarqueeLabel::reset(sessionNameMarquee);
        MarqueeLabel::draw(tft, sessionNameMarquee, 10, (int16_t)(CONTENT_TOP + 6),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                            I18n::t(sessionNameStringId(board.sessionName)), TFT_GREEN, TFT_BLACK);

        int16_t listTop = (int16_t)(CONTENT_TOP + 26);
        int16_t listBottom = (int16_t)(MORE_BTN_Y - 6);
        int16_t availableH = (int16_t)(listBottom - listTop);
        uint8_t maxRows = (uint8_t)(availableH / LIVE_ROW_H);
        if (maxRows == 0) maxRows = 1;
        uint8_t displayCount = (board.rowCount < maxRows) ? board.rowCount : maxRows;

        tft.setTextDatum(ML_DATUM);
        for (uint8_t i = 0; i < displayCount; i++) {
            const NetTask::F1LiveRow& row = board.rows[i];
            int16_t rowCy = (int16_t)(listTop + i * LIVE_ROW_H + LIVE_ROW_H / 2);

            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            String posText = row.position > 0 ? (String(row.position) + ".") : "-";
            tft.drawString(posText, 10, rowCy);

            tft.fillCircle(42, rowCy, 4, row.teamColour565);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(row.acronym, 54, rowCy);

            tft.setTextDatum(MR_DATUM);
            String lapTime = row.hasLapTime ? formatLapTime(row.lapTimeSec) : "";
            tft.drawString(lapTime, 172, rowCy);

            String gap = formatGapText(row.hasGapSeconds, row.gapSeconds, row.hasGapLaps, row.gapLaps);
            tft.drawString(gap, (int16_t)(Config::SCREEN_WIDTH - 10), rowCy);
            tft.setTextDatum(ML_DATUM);
        }
        tft.setTextDatum(TL_DATUM);
    }

    void animateLiveBoard(TFT_eSPI& tft, const NetTask::F1LiveBoard& board) {
        MarqueeLabel::draw(tft, sessionNameMarquee, 10, (int16_t)(CONTENT_TOP + 6),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                            I18n::t(sessionNameStringId(board.sessionName)), TFT_GREEN, TFT_BLACK);
    }

    // Feste Bandhoehe fuer NUR den Countdown-Block (Label + Uhrzeit) -
    // deckt sich mit den y-Schritten in drawNextRace() unten
    // (Rennname +22, Ort +20 = 42, danach der Countdown-Block mit Hoehe
    // 64). Eigene, bewusst KLEINE Loeschflaeche statt des gesamten
    // Inhaltsbereichs - noetig, weil der Countdown anders als bei den
    // uebrigen Feature-Screens JEDE SEKUNDE neu gezeichnet wird (Alex'
    // Fund, 06.10.: das volle `clearContentArea()` + Neuzeichnen von
    // Rennname/Ort/Top-3 bei JEDEM Sekundentick verursachte sichtbares
    // Flimmern auf dem Hauptbildschirm im Nicht-Live-Zustand - gleiches
    // Grundprinzip wie `rocket_screen.cpp`s `COUNTDOWN_AREA_TOP`/
    // `clearCountdownArea()`, hier lokal nachgebildet).
    constexpr int16_t COUNTDOWN_AREA_TOP = (int16_t)(CONTENT_TOP + 42);
    constexpr int16_t COUNTDOWN_AREA_H = 64;

    // Countdown literal GELB statt Akzentfarbe (Alex' Auftrag, 07.10.:
    // "sämtliche Countdowns sind gelb") - eigene, themenunabhaengige
    // Signalfarbe, siehe CLAUDE.md "Pflicht: Countdown-Anzeigen in Gelb".
    void drawCountdownBlock(TFT_eSPI& tft, time_t raceEpoch) {
        tft.fillRect(0, COUNTDOWN_AREA_TOP, Config::SCREEN_WIDTH, COUNTDOWN_AREA_H, TFT_BLACK);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString(I18n::t(StringId::F1_COUNTDOWN_LABEL), (int16_t)(Config::SCREEN_WIDTH / 2),
                        (int16_t)(COUNTDOWN_AREA_TOP + 14));

        long secondsRemaining = (long)raceEpoch - (long)time(nullptr);
        if (secondsRemaining < 0) secondsRemaining = 0;
        long days = secondsRemaining / 86400;
        long rem = secondsRemaining % 86400;
        long hours = rem / 3600;
        rem %= 3600;
        long minutes = rem / 60;
        long seconds = rem % 60;
        char clock[12];
        snprintf(clock, sizeof(clock), "%02ld:%02ld:%02ld", hours, minutes, seconds);
        String countdownStr = String(days) + " " + I18n::t(StringId::ROCKET_UNIT_DAYS) + "  " + clock;

        // BUGFIX (13.10., Alex' Auftrag: "komplettes System auf
        // Text-ueberlaeuft-Button/Flaeche pruefen") - gleicher Fix wie
        // rocket_screen.cpp::drawCountdown(): bei dreistelliger Tageszahl
        // lief der Countdown bei Groesse 2 ueber die Bildschirmbreite
        // hinaus, faellt jetzt auf Groesse 1 zurueck.
        tft.setTextSize(2);
        if (tft.textWidth(countdownStr) > (Config::SCREEN_WIDTH - 10)) {
            tft.setTextSize(1);
        }
        tft.drawString(countdownStr, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(COUNTDOWN_AREA_TOP + 44));
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    // Zeichnet naechstes Rennen + Countdown + Top-3-Fahrerwertung (Fall B) -
    // NUR bei neuen Saison-Daten oder beim ersten Betreten aufgerufen (der
    // Countdown selbst wird danach ausschliesslich ueber
    // drawCountdownBlock() aktualisiert, siehe update() unten).
    void drawNextRace(TFT_eSPI& tft, const F1Client::SeasonData& season) {
        clearContentArea(tft);

        int16_t y = CONTENT_TOP;
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        MarqueeLabel::reset(raceNameMarquee);
        MarqueeLabel::draw(tft, raceNameMarquee, 10, (int16_t)(y + 7),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 18, season.nextRace.raceName,
                            TFT_GREEN, TFT_BLACK);
        y += 22;

        String location = season.nextRace.locality.length() > 0
            ? (season.nextRace.locality + ", " + season.nextRace.country)
            : season.nextRace.country;
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        MarqueeLabel::reset(raceLocationMarquee);
        MarqueeLabel::draw(tft, raceLocationMarquee, 10, (int16_t)(y + 6),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 15, location, TFT_WHITE, TFT_BLACK);
        y += 20; // y == COUNTDOWN_AREA_TOP

        drawCountdownBlock(tft, season.nextRace.race.epoch);
        y += COUNTDOWN_AREA_H;

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, (int16_t)(y + 11));
        tft.print(I18n::t(StringId::F1_TOP_DRIVERS_LABEL));
        y += 20;

        uint8_t topCount = (season.driverStandingCount < 3) ? season.driverStandingCount : 3;
        for (uint8_t i = 0; i < topCount; i++) {
            const F1Client::DriverStanding& d = season.driverStandings[i];
            String line = String(d.position) + ". " + d.familyName + " (" + d.code + ") - " +
                          String((int)d.points) + " " + I18n::t(StringId::F1_POINTS_SHORT);
            int16_t rowCy = (int16_t)(y + i * 18 + 9);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            MarqueeLabel::reset(top3Marquee[i]);
            MarqueeLabel::draw(tft, top3Marquee[i], 10, rowCy,
                                (int16_t)(Config::SCREEN_WIDTH - 20), 16, line, TFT_WHITE, TFT_BLACK);
        }
    }

    void animateNextRace(TFT_eSPI& tft, const F1Client::SeasonData& season) {
        MarqueeLabel::draw(tft, raceNameMarquee, 10, (int16_t)(CONTENT_TOP + 7),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 18, season.nextRace.raceName,
                            TFT_GREEN, TFT_BLACK);
        String location = season.nextRace.locality.length() > 0
            ? (season.nextRace.locality + ", " + season.nextRace.country)
            : season.nextRace.country;
        MarqueeLabel::draw(tft, raceLocationMarquee, 10, (int16_t)(CONTENT_TOP + 28),
                            (int16_t)(Config::SCREEN_WIDTH - 20), 15, location, TFT_WHITE, TFT_BLACK);

        uint8_t topCount = (season.driverStandingCount < 3) ? season.driverStandingCount : 3;
        int16_t y = (int16_t)(COUNTDOWN_AREA_TOP + COUNTDOWN_AREA_H + 20);
        for (uint8_t i = 0; i < topCount; i++) {
            const F1Client::DriverStanding& d = season.driverStandings[i];
            String line = String(d.position) + ". " + d.familyName + " (" + d.code + ") - " +
                          String((int)d.points) + " " + I18n::t(StringId::F1_POINTS_SHORT);
            int16_t rowCy = (int16_t)(y + i * 18 + 9);
            MarqueeLabel::draw(tft, top3Marquee[i], 10, rowCy,
                                (int16_t)(Config::SCREEN_WIDTH - 20), 16, line, TFT_WHITE, TFT_BLACK);
        }
    }

    void drawMoreButton(TFT_eSPI& tft) {
        drawButton(tft, moreBtnRect(), I18n::t(StringId::F1_MORE_BTN));
    }

    // ---- Detail-Screen (scrollbar, Alex' Auftrag, 06.10.) - zeigt, in
    // dieser Reihenfolge: bei laufender Session die VOLLSTAENDIGE
    // Live-Rangliste (das ist das "Rest im Detailscreen" aus dem Auftrag),
    // danach IMMER die komplette Fahrer-/Konstrukteurswertung, das Ergebnis
    // des letzten Rennens und alle Sessionzeiten des naechsten
    // Rennwochenends (das sind die 4 explizit aufgezaehlten Punkte). Beide
    // Teile im selben Screen statt einer Fallunterscheidung, damit bei
    // "Mehr" nie Inhalt fehlt. ----
    void showDetailScreen(TFT_eSPI& tft, const NetTask::F1LiveBoard& live, bool haveLiveData,
                           const F1Client::SeasonData& season, bool haveSeasonData) {
        MenuStars::reset();

        constexpr int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t LINE_H = 16;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 10);

        auto sectionTitle = [&](int16_t y, int16_t vTop, int16_t vBottom, int16_t scrollY, bool draw, const char* text) -> int16_t {
            if (draw) tft.setTextColor(TFT_GREEN, TFT_BLACK);
            int16_t ny = layoutWrapped(tft, 10, y, maxW, LINE_H, String(text), scrollY, vTop, vBottom, draw);
            if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
            return (int16_t)(ny + 4);
        };

        auto drawContent = [&](int16_t scrollY, bool draw) -> int16_t {
            int16_t y = VIEW_TOP;
            int16_t vTop = draw ? VIEW_TOP : -32000;
            int16_t vBottom = draw ? VIEW_BOTTOM : 32000;

            if (haveLiveData && live.live) {
                y = sectionTitle(y, vTop, vBottom, scrollY, draw, I18n::t(StringId::F1_DETAIL_LIVE_TITLE));
                for (uint8_t i = 0; i < live.rowCount; i++) {
                    const NetTask::F1LiveRow& row = live.rows[i];
                    if (draw) {
                        int16_t screenY = (int16_t)(y - scrollY);
                        if (screenY >= vTop && screenY <= vBottom) {
                            tft.fillCircle(14, (int16_t)(screenY + 5), 4, row.teamColour565);
                        }
                    }
                    String posText = row.position > 0 ? (String(row.position) + ".") : "-";
                    String gap = formatGapText(row.hasGapSeconds, row.gapSeconds, row.hasGapLaps, row.gapLaps);
                    String lapTime = row.hasLapTime ? formatLapTime(row.lapTimeSec) : "";
                    String line = posText + " " + row.acronym + " " + row.teamName +
                                  (gap.length() > 0 ? (" " + gap) : "") +
                                  (lapTime.length() > 0 ? (" " + lapTime) : "");
                    y = layoutWrapped(tft, 24, y, (int16_t)(maxW - 14), LINE_H, line, scrollY, vTop, vBottom, draw);
                    y += 2;
                }
                y += 6;
            }

            y = sectionTitle(y, vTop, vBottom, scrollY, draw, I18n::t(StringId::F1_DETAIL_DRIVER_STANDINGS_TITLE));
            if (!haveSeasonData) {
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, I18n::t(StringId::F1_LOADING), scrollY, vTop, vBottom, draw);
            }
            for (uint8_t i = 0; i < season.driverStandingCount; i++) {
                const F1Client::DriverStanding& d = season.driverStandings[i];
                String line1 = String(d.position) + ". " + d.givenName + " " + d.familyName + " (" + d.code + ")";
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line1, scrollY, vTop, vBottom, draw);
                String line2 = d.constructorName + " - " + String((int)d.points) + " " +
                               I18n::t(StringId::F1_POINTS_SHORT);
                if (d.wins > 0) line2 += " - " + String(d.wins) + " " + I18n::t(StringId::F1_WINS_SHORT);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line2, scrollY, vTop, vBottom, draw);
                y += 4;
            }
            y += 4;

            y = sectionTitle(y, vTop, vBottom, scrollY, draw, I18n::t(StringId::F1_DETAIL_CONSTRUCTOR_STANDINGS_TITLE));
            for (uint8_t i = 0; i < season.constructorStandingCount; i++) {
                const F1Client::ConstructorStanding& c = season.constructorStandings[i];
                String line = String(c.position) + ". " + c.name + " - " + String((int)c.points) + " " +
                              I18n::t(StringId::F1_POINTS_SHORT) + " - " + String(c.wins) + " " +
                              I18n::t(StringId::F1_WINS_SHORT);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line, scrollY, vTop, vBottom, draw);
                y += 2;
            }
            y += 4;

            y = sectionTitle(y, vTop, vBottom, scrollY, draw,
                (String(I18n::t(StringId::F1_DETAIL_LAST_RESULT_LABEL)) + season.lastRaceName).c_str());
            for (uint8_t i = 0; i < season.lastResultCount; i++) {
                const F1Client::RaceResult& r = season.lastResults[i];
                String line1 = r.positionText + ". " + r.givenName + " " + r.familyName + " (" + r.driverCode + ")";
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line1, scrollY, vTop, vBottom, draw);
                String detail = r.constructorName + " - ";
                detail += (r.time.length() > 0) ? r.time : formatResultStatus(r.status);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, detail, scrollY, vTop, vBottom, draw);
                y += 4;
            }
            y += 4;

            y = sectionTitle(y, vTop, vBottom, scrollY, draw, I18n::t(StringId::F1_DETAIL_NEXT_SESSIONS_TITLE));
            auto sessionLine = [&](StringId label, const F1Client::SessionTime& st) {
                if (!st.valid) return;
                String line = String(I18n::t(label)) + ": " + formatDeviceLocalDateTime(st.epoch);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line, scrollY, vTop, vBottom, draw);
            };
            sessionLine(StringId::F1_SESSION_PRACTICE1, season.nextRace.firstPractice);
            sessionLine(StringId::F1_SESSION_PRACTICE2, season.nextRace.secondPractice);
            sessionLine(StringId::F1_SESSION_PRACTICE3, season.nextRace.thirdPractice);
            sessionLine(StringId::F1_SESSION_SPRINT_QUALIFYING, season.nextRace.sprintQualifying);
            sessionLine(StringId::F1_SESSION_SPRINT, season.nextRace.sprint);
            sessionLine(StringId::F1_SESSION_QUALIFYING, season.nextRace.qualifying);
            sessionLine(StringId::F1_SESSION_RACE, season.nextRace.race);

            return y;
        };

        int16_t totalH = drawContent(0, false);
        int16_t maxScroll = (int16_t)(totalH - VIEW_BOTTOM);
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;
        constexpr int16_t SCROLL_STEP = 48;

        Rect backBtn = scrollable
            ? Rect{10, BACK_Y, 130, 40}
            : Rect{10, BACK_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, BACK_Y, 38, 40};
        Rect downBtn = {190, BACK_Y, 38, 40};

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::F1_DETAIL_TITLE));

            drawContent(scrollY, true);

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

    void redraw(TFT_eSPI& tft) {
        drawChrome(tft);
        if (!haveLiveInfo) {
            drawCenteredHint(tft, StringId::F1_LOADING);
            return;
        }
        if (lastLive.live) {
            drawLiveBoard(tft, lastLive);
            drawMoreButton(tft);
        } else if (haveSeason) {
            drawNextRace(tft, lastSeason);
            drawMoreButton(tft);
            lastDisplayedCountdownSeconds = -1;
        } else {
            drawCenteredHint(tft, StringId::F1_LOADING);
        }
    }
}

void enter(TFT_eSPI& tft) {
    redraw(tft);
}

void update(TFT_eSPI& tft) {
    // NetTask (Core 0) liefert die Daten im Hintergrund - hier (Core 1) nur
    // pruefen, ob sich eine der beiden Versionsnummern geaendert hat. Kein
    // eigener Netzwerk-Abruf mehr in diesem Screen.
    bool needRedraw = false;

    uint32_t sv = NetTask::f1SeasonVersion();
    if (sv != lastSeasonVersion) {
        F1Client::SeasonData newData;
        if (NetTask::getF1SeasonData(newData)) {
            lastSeasonVersion = sv;
            lastSeason = newData;
            haveSeason = true;
            needRedraw = true;
        }
    }

    uint32_t lv = NetTask::f1LiveVersion();
    if (lv != lastLiveVersion) {
        NetTask::F1LiveBoard newBoard;
        if (NetTask::getF1LiveBoard(newBoard)) {
            lastLiveVersion = lv;
            lastLive = newBoard;
            haveLiveInfo = true;
            needRedraw = true;
        }
    }

    if (needRedraw) {
        redraw(tft);
        return;
    }

    if (!haveLiveInfo) return;

    if (lastLive.live) {
        animateLiveBoard(tft, lastLive);
    } else if (haveSeason) {
        // Nur noch der Countdown-Block selbst wird pro Sekunde neu
        // gezeichnet (Alex' Fund, 06.10.: ein volles drawNextRace() bei
        // JEDEM Sekundentick loeschte/zeichnete den kompletten
        // Inhaltsbereich neu - sichtbares Flimmern). drawCountdownBlock()
        // loescht vorher IMMER die komplette Bandflaeche (volle Breite/
        // Hoehe, nicht nur den schmaleren neuen Text), dadurch bleiben
        // auch bei wechselnder Ziffernbreite (z.B. Tage-Anzahl) keine
        // Reste der alten Ziffern stehen. Die Marquee-Animation (Rennname/
        // Ort/Top-3) laeuft JETZT unabhaengig davon JEDEN Tick, nicht nur
        // wenn sich die Sekunde zufaellig nicht geaendert hat (vorher ein
        // if/else, das animateNextRace() dadurch faktisch nie erreichte).
        long secondsRemaining = (long)lastSeason.nextRace.race.epoch - (long)time(nullptr);
        if (secondsRemaining < 0) secondsRemaining = 0;
        if (secondsRemaining != lastDisplayedCountdownSeconds) {
            lastDisplayedCountdownSeconds = secondsRemaining;
            drawCountdownBlock(tft, lastSeason.nextRace.race.epoch);
        }
        animateNextRace(tft, lastSeason);
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        redraw(tft);
        return;
    }

    if (haveLiveInfo && (lastLive.live || haveSeason) && moreBtnRect().contains(x, y)) {
        showDetailScreen(tft, lastLive, haveLiveInfo, lastSeason, haveSeason);
        MenuStars::reset();
        redraw(tft);
        return;
    }
}

}
