#include "transit_screen.h"
#include <time.h>
#include <SD.h>
#include "net_task.h"
#include "address_search_screen.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"

namespace TransitScreen {

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

    // Wortweiser Umbruch anhand echter Pixelbreite - gleiches Muster wie
    // in rocket_screen.cpp/iss_radar_screen.cpp, hier lokal dupliziert
    // statt geteilt (Projekt-Konvention, siehe CLAUDE.md). Optionale
    // Scroll-Unterstuetzung (scrollY/viewTop/viewBottom/draw, Defaults =
    // bisheriges Verhalten ohne Scrollen) - 1:1 dasselbe Muster wie
    // address_search_screen.cpp::runConfirmScreen(), fuer showDetailScreen()
    // unten gebraucht (Alex' Auftrag, 06.10.: "alle sinnvollen API-Daten
    // anzeigen" macht den Detail-Screen potenziell laenger als der
    // Bildschirm - siehe CLAUDE.md "Pflicht: jedes Layout muss auf den
    // Bildschirm passen").
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

    // Wie layoutWrapped() oben, zeichnet aber Label (Akzentfarbe) und Wert
    // (Weiss) getrennt (Alex' Auftrag, 07.10., siehe CLAUDE.md "Pflicht:
    // Label/Wert-Farbkontrast" - bislang bei ISS-Radar/Rocket-Screen
    // umgesetzt, hier auf Alex' ausdruecklichen Hinweis fuer den OEPNV-
    // Screen nachgezogen) - lokal dupliziert statt geteilt (gleiche Logik
    // wie in iss_radar_screen.cpp, Projekt-Konvention).
    int16_t layoutWrappedLabelValue(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                                     int16_t lineHeight, const String& label, const String& value,
                                     uint16_t labelColor, uint16_t valueColor) {
        String text = label + value;
        int32_t labelLen = (int32_t)label.length();
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

            int32_t labelCharsHere = labelLen - start;
            if (labelCharsHere < 0) labelCharsHere = 0;
            if (labelCharsHere > (int32_t)line.length()) labelCharsHere = (int32_t)line.length();

            tft.setCursor(x, y);
            if (labelCharsHere > 0) {
                tft.setTextColor(labelColor, TFT_BLACK);
                tft.print(line.substring(0, labelCharsHere));
            }
            if (labelCharsHere < (int32_t)line.length()) {
                tft.setTextColor(valueColor, TFT_BLACK);
                tft.print(line.substring(labelCharsHere));
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

    // Kuerzt zeichenweise, bis der String in maxWidth passt - fuer die
    // Linien-Spalte (immer kurz, z.B. "N42") in der Abfahrten-Liste.
    String truncateToWidth(TFT_eSPI& tft, const String& s, int16_t maxWidth) {
        String out = s;
        while (out.length() > 1 && tft.textWidth(out) > maxWidth) {
            out.remove(out.length() - 1);
        }
        return out;
    }

    // Countdown-Baustein ("NN Min"/"Xh YYm"/"jetzt") - seit 06.10. nur noch
    // ZUSAETZLICHE Information neben der Uhrzeit (siehe formatClockTime()
    // unten), nicht mehr die Hauptanzeige (Alex' Auftrag: "man soll sofort
    // sehen, wann der Bus tatsaechlich kommt, ohne im Kopf nachzurechnen").
    // Format unveraendert: unter 60 Minuten "NN Min", ab 60 Minuten "Xh YYm".
    String formatCompactTime(time_t epoch) {
        long secs = (long)epoch - (long)time(nullptr);
        if (secs < 30) return String(I18n::t(StringId::TRANSIT_DEPARTURE_NOW));
        long totalMins = (secs + 30) / 60;
        if (totalMins < 60) {
            return String(totalMins) + I18n::t(StringId::TRANSIT_UNIT_MIN);
        }
        long hours = totalMins / 60;
        long mins = totalMins % 60;
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld%s %02ld%s", hours, I18n::t(StringId::TRANSIT_UNIT_HOUR_SHORT),
                 mins, I18n::t(StringId::TRANSIT_UNIT_MIN_SHORT));
        return String(buf);
    }

    // "in 12 Min"/"in 2h 55m"/"jetzt" - Praefix "in " wird NICHT vor
    // "jetzt" gesetzt (klingt sonst falsch: "in jetzt"). Nur dort
    // verwendet, wo neben der Uhrzeit noch Platz ist (Detail-Screen) - in
    // der engen Listenzeile wuerde ein zweiter Wert das Breitenbudget
    // sprengen (siehe CLAUDE.md "Pflicht: Elemente muessen immer in ihren
    // Platz passen").
    String formatRelativeSuffix(time_t epoch) {
        String compact = formatCompactTime(epoch);
        if (compact == I18n::t(StringId::TRANSIT_DEPARTURE_NOW)) return compact;
        return String(I18n::t(StringId::TRANSIT_IN_PREFIX)) + compact;
    }

    // ---- Lokale Uhrzeit statt UTC (Alex' Auftrag, 06.10.) ----
    // Das Geraet selbst laeuft komplett in UTC (main.cpp::configTime(0,0,...),
    // kein Zeitzonen-Offset) - die Transitous-API liefert aber pro
    // Haltestelle die IANA-Zeitzone mit (place.tz, z.B. "Europe/Berlin",
    // live bestaetigt, siehe transit_client.cpp). Eine vollstaendige
    // IANA-Zeitzonendatenbank ist auf dem ESP32 nicht sinnvoll umsetzbar;
    // da Transitous ausschliesslich europaeische Haltestellen abdeckt und
    // alle europaeischen Laender dieselbe EU-weit EINHEITLICHE Sommerzeit-
    // Umstellung verwenden (letzter Sonntag Maerz/Oktober, 01:00 UTC -
    // unterscheidet sich nur die Standardzeit-Basis je Land), reicht eine
    // kleine Basis-Offset-Tabelle + eine gemeinsame DST-Berechnung. Eine
    // unbekannte/neue Zeitzone faellt auf Mitteleuropa (+1h, Transitous'
    // bisherige Hauptabdeckung DACH) zurueck - falls das fuer eine andere
    // Region spuerbar falsch sein sollte, bitte zurueckmelden statt es
    // weiter zu verfeinern/zu raten.
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    // Letzter Sonntag eines Monats, als Tage seit 1970-01-01 (1970-01-01
    // war ein Donnerstag = Wochentag 4 bei Sonntag=0).
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

    struct TzBaseOffset { const char* name; int8_t hours; };
    // Standardzeit-Basis (Winterzeit) je IANA-Zeitzone - live gegen die
    // echte API ermittelte Namensform ("Europe/Berlin" usw.).
    const TzBaseOffset TZ_BASE_OFFSETS[] = {
        {"Europe/London", 0}, {"Europe/Dublin", 0}, {"Europe/Lisbon", 0},
        {"Europe/Berlin", 1}, {"Europe/Paris", 1}, {"Europe/Madrid", 1},
        {"Europe/Rome", 1}, {"Europe/Amsterdam", 1}, {"Europe/Brussels", 1},
        {"Europe/Vienna", 1}, {"Europe/Zurich", 1}, {"Europe/Warsaw", 1},
        {"Europe/Prague", 1}, {"Europe/Budapest", 1}, {"Europe/Stockholm", 1},
        {"Europe/Oslo", 1}, {"Europe/Copenhagen", 1}, {"Europe/Luxembourg", 1},
        {"Europe/Ljubljana", 1}, {"Europe/Zagreb", 1}, {"Europe/Bratislava", 1},
        {"Europe/Helsinki", 2}, {"Europe/Athens", 2}, {"Europe/Bucharest", 2},
        {"Europe/Sofia", 2}, {"Europe/Tallinn", 2}, {"Europe/Riga", 2},
        {"Europe/Vilnius", 2}, {"Europe/Chisinau", 2}, {"Europe/Kyiv", 2},
    };

    int8_t baseOffsetHoursForTz(const String& tz) {
        for (const TzBaseOffset& e : TZ_BASE_OFFSETS) {
            if (tz == e.name) return e.hours;
        }
        return 1; // Fallback Mitteleuropa, siehe Kommentar oben
    }

    // Groblaeufige Jahreszahl (um max. +/-1 Tag ungenau) - reicht als
    // Eingabe fuer isEuDst(), da deren eigentliche Grenze die exakte
    // epoch-Berechnung der Umstellungszeitpunkte selbst ist; die Jahreszahl
    // bestimmt nur, fuer welches Jahr diese Grenzen ueberhaupt berechnet
    // werden (ein Tag Ungenauigkeit faellt nie in die Naehe von Maerz/
    // Oktober, wenn man gerade an Neujahr ist).
    int yearOfEpochApprox(time_t epoch) {
        return 1970 + (int)(epoch / 31556952L);
    }

    // "HH:MM" in der lokalen Zeit der Haltestelle - jetzt die
    // HAUPTANZEIGE in Liste und Detail-Screen (Alex' Auftrag, 06.10.).
    String formatClockTime(time_t epoch, const String& tz) {
        int8_t baseHours = baseOffsetHoursForTz(tz);
        int year = yearOfEpochApprox(epoch);
        int offsetHours = baseHours + (isEuDst(epoch, year) ? 1 : 0);
        time_t localEpoch = epoch + (time_t)offsetHours * 3600;
        long secOfDay = ((long)(localEpoch % 86400) + 86400) % 86400;
        char buf[8];
        snprintf(buf, sizeof(buf), "%02ld:%02ld", secOfDay / 3600, (secOfDay % 3600) / 60);
        return String(buf);
    }

    // Config::HEADER_HEIGHT statt eines eigenen CHROME_H-Werts (Alex'
    // Auftrag, 06.10. - EINE zentrale Kopfbereich-Hoehe fuer JEDEN Screen,
    // siehe CLAUDE.md "Pflicht: einheitliche grosse Ueberschrift").
    // CONTENT_TOP im gleichen Abstand (6px) mitverschoben.
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 6);
    constexpr int16_t REFINE_BTN_H = 36;
    constexpr int16_t REFINE_BTN_Y = (int16_t)(Config::SCREEN_HEIGHT - REFINE_BTN_H - 8);

    // CLAUDE.md-Mindestgroesse fuer Tap-Flaechen (Alex' Auftrag, 06.10.,
    // "Finger-only"-Regel) - gilt jetzt auch fuer die Abfahrten-Zeilen,
    // seit sie antippbar sind (siehe drawDepartures()/showDetailScreen()).
    constexpr int16_t ROW_MIN_H = 31;
    constexpr int16_t ROW_MAX_H = 40;

    // 31x31 (26+5) - "?"-Buttons systemweit vergroessert.
    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }
    Rect refineBtnRect() { return Rect{10, REFINE_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), REFINE_BTN_H}; }

    // Zentrierung des Titels auf der Mitte der vom "?"-Button FREIEN Zone,
    // nicht auf der wahren Bildschirmmitte (Alex' Fund, 06.10. bei
    // "Raketenstart" - der 7px-Monospace-Font braucht dort 168px bei
    // Groesse 2, bei echter Mittel-Zentrierung blieben wegen des Buttons
    // aber nur ~160px). So verbleiben 197px echter Spielraum.
    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.).
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::TRANSIT_TITLE), TFT_GREEN);
    }

    void clearContentArea(TFT_eSPI& tft) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(REFINE_BTN_Y - 4 - CHROME_H), TFT_BLACK);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        clearContentArea(tft);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::TRANSIT_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2),
                             (int16_t)((CHROME_H + REFINE_BTN_Y) / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    // Ordnet den NetTask-Fehlergrund der passenden, lokalisierten Meldung
    // zu (Alex' Auftrag: "keine Verbindung"/"keine Haltestelle gefunden"/
    // "keine Abfahrten" sollen unterschiedliche Meldungen zeigen).
    StringId errorStringId(NetTask::TransitError err) {
        switch (err) {
            case NetTask::TransitError::NoStopFound: return StringId::TRANSIT_FETCH_ERROR_NO_STOP;
            case NetTask::TransitError::NoDepartures: return StringId::TRANSIT_FETCH_ERROR_NO_DEPARTURES;
            default: return StringId::TRANSIT_FETCH_ERROR_NETWORK;
        }
    }

    // ---- Spalten-Layout, von drawDepartures()/updateHeadsignMarquees()/
    // handleTap() gemeinsam gebraucht. ----
    constexpr int16_t LINE_COL_W = 42;
    constexpr int16_t TIME_COL_W = 64; // reicht fuer "23h 59m" o.ae. mit Reserve
    int16_t headsignX() { return (int16_t)(10 + LINE_COL_W + 4); }
    int16_t headsignW() { return (int16_t)(Config::SCREEN_WIDTH - 20 - LINE_COL_W - TIME_COL_W - 8); }
    int16_t timeX() { return (int16_t)(Config::SCREEN_WIDTH - 10 - TIME_COL_W); }

    // Zeilen-Rechtecke + wie viele tatsaechlich angezeigt werden - von
    // drawDepartures() befuellt, von handleTap() fuer die Tap-Erkennung
    // gebraucht (Alex' Auftrag, 06.10.: jede Abfahrtszeile antippbar).
    Rect departureRowRects[Config::TRANSIT_MAX_DEPARTURES];
    uint8_t departureRowCount = 0;
    MarqueeLabel::State headsignMarquee[Config::TRANSIT_MAX_DEPARTURES];
    // Nur fuer ausgefallene Fahrten gebraucht (Alex' Auftrag, 06.10.: Ausfall
    // soll "deutlich sichtbar" direkt in der Liste stehen) - der
    // Ausfall-Text ("Fällt aus"/"Cancelled"/...) muss in allen 4 Sprachen in
    // die feste TIME_COL_W-Spalte passen, dafuer wird hier wie bei der
    // Ziel-Spalte die zentrale Marquee-Komponente wiederverwendet statt die
    // Spaltenbreite zu raten.
    MarqueeLabel::State timeColMarquee[Config::TRANSIT_MAX_DEPARTURES];

    void drawDepartures(TFT_eSPI& tft, const NetTask::TransitData& data) {
        clearContentArea(tft);
        int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        int16_t stopEndY = layoutWrappedLabelValue(tft, 10, CONTENT_TOP, maxW, 15,
            I18n::t(StringId::TRANSIT_STOP_LABEL), data.stopName, TFT_GREEN, TFT_WHITE);

        int16_t listTop = (int16_t)(stopEndY + 6);
        int16_t listBottom = (int16_t)(REFINE_BTN_Y - 6);
        departureRowCount = 0;

        if (data.departureCount == 0) {
            drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)((listTop + listBottom) / 2),
                                 maxW, 16, I18n::t(StringId::TRANSIT_FETCH_ERROR_NO_DEPARTURES));
            return;
        }

        // Lieber WENIGER Abfahrten GUT antippbar zeigen als mehr Abfahrten
        // unter der Mindest-Tap-Hoehe (ROW_MIN_H, Alex' Auftrag, 06.10. -
        // CLAUDE.md-Mindestgroesse fuer Tap-Flaechen) - displayCount ist
        // deshalb ggf. kleiner als data.departureCount.
        int16_t availableH = (int16_t)(listBottom - listTop);
        uint8_t maxRowsThatFit = (uint8_t)(availableH / ROW_MIN_H);
        if (maxRowsThatFit == 0) maxRowsThatFit = 1;
        uint8_t displayCount = (data.departureCount < maxRowsThatFit) ? data.departureCount : maxRowsThatFit;

        int16_t rowH = (int16_t)(availableH / displayCount);
        if (rowH > ROW_MAX_H) rowH = ROW_MAX_H;

        int16_t hX = headsignX();
        int16_t hW = headsignW();
        int16_t tX = timeX();

        tft.setTextDatum(ML_DATUM);
        for (uint8_t i = 0; i < displayCount; i++) {
            int16_t rowY = (int16_t)(listTop + i * rowH);
            int16_t rowCy = (int16_t)(rowY + rowH / 2);
            const TransitClient::Departure& dep = data.departures[i];
            bool cancelled = dep.cancelled;
            uint16_t rowColor = cancelled ? TFT_RED : TFT_GREEN;
            int16_t rowLineH = (int16_t)(rowH - 2);

            departureRowRects[i] = {4, rowY, (int16_t)(Config::SCREEN_WIDTH - 8), rowH};
            // Leichte Antipp-Markierung (duenner Rahmen) - macht sichtbar,
            // dass die Zeile eine Tap-Flaeche ist (Alex' Auftrag: jede
            // Abfahrtszeile oeffnet einen Detail-Screen). Bei einer
            // ausgefallenen Fahrt wird der ganze Rahmen rot statt gedimmt -
            // zusammen mit der roten Linie/Zielanzeige "deutlich sichtbar"
            // direkt in der Liste (Alex' Auftrag, 06.10.).
            tft.drawRoundRect(departureRowRects[i].x, departureRowRects[i].y,
                               departureRowRects[i].w, departureRowRects[i].h, 3,
                               cancelled ? TFT_RED : tft.color565(0, (uint8_t)(255 * 0.35f), 0));

            tft.setTextColor(rowColor, TFT_BLACK);
            tft.drawString(truncateToWidth(tft, dep.line, LINE_COL_W), 10, rowCy);

            MarqueeLabel::reset(headsignMarquee[i]);
            MarqueeLabel::draw(tft, headsignMarquee[i], hX, rowCy, hW, rowLineH,
                                dep.headsign, cancelled ? TFT_RED : TFT_WHITE, TFT_BLACK);

            if (cancelled) {
                MarqueeLabel::reset(timeColMarquee[i]);
                MarqueeLabel::draw(tft, timeColMarquee[i], tX, rowCy, TIME_COL_W, rowLineH,
                                    I18n::t(StringId::TRANSIT_CANCELLED_LABEL), TFT_RED, TFT_BLACK);
            } else {
                // Uhrzeit statt Countdown als Hauptanzeige (Alex' Auftrag,
                // 06.10.) - immer kurz genug fuer TIME_COL_W, kein Marquee
                // noetig.
                tft.setTextColor(rowColor, TFT_BLACK);
                tft.drawString(formatClockTime(dep.epoch, dep.stopTz), tX, rowCy);
            }
        }
        tft.setTextDatum(TL_DATUM);
        departureRowCount = displayCount;
    }

    // Nur die Ziel-Spalte (und bei Ausfall die Zeit-Spalte) neu zeichnen
    // (fuer laufende Marquee-Animation, jeden update()-Durchlauf
    // aufgerufen) - Linie/Rahmen bleiben unveraendert stehen,
    // MarqueeLabel::draw() loescht/zeichnet nur seinen eigenen
    // Spaltenbereich neu. Die normale Uhrzeit-Anzeige (nicht ausgefallene
    // Fahrten) braucht keine Aktualisierung hier, sie aendert sich nicht
    // zwischen zwei Minuten-Tickwechseln und wird ohnehin bei jeder neuen
    // NetTask-Version komplett per drawDepartures() neu gezeichnet.
    void updateHeadsignMarquees(TFT_eSPI& tft, const NetTask::TransitData& data) {
        int16_t hX = headsignX();
        int16_t hW = headsignW();
        int16_t tX = timeX();
        for (uint8_t i = 0; i < departureRowCount; i++) {
            const TransitClient::Departure& dep = data.departures[i];
            int16_t rowCy = (int16_t)(departureRowRects[i].y + departureRowRects[i].h / 2);
            int16_t rowLineH = (int16_t)(departureRowRects[i].h - 2);
            MarqueeLabel::draw(tft, headsignMarquee[i], hX, rowCy, hW, rowLineH,
                                dep.headsign, dep.cancelled ? TFT_RED : TFT_WHITE, TFT_BLACK);
            if (dep.cancelled) {
                MarqueeLabel::draw(tft, timeColMarquee[i], tX, rowCy, TIME_COL_W, rowLineH,
                                    I18n::t(StringId::TRANSIT_CANCELLED_LABEL), TFT_RED, TFT_BLACK);
            }
        }
    }

    // ---- Detail-Screen (Alex' Auftrag, 06.10.) - zeigt die von
    // Transitous zusaetzlich mitgelieferten, in der Liste ungenutzten
    // Felder (inzwischen: Verspaetung, Ausfall, volle Linienbezeichnung,
    // Rollstuhl-/Fahrradmitnahme - "alle sinnvollen API-Daten anzeigen",
    // siehe CLAUDE.md "Pflicht: vollstaendige API-Daten nutzen"). Ein
    // "alerts"-Feld existiert im stoptimes-Endpunkt NICHT (live mehrfach
    // gegen die echte API geprueft, siehe transit_client.h) - deshalb
    // bewusst NICHT umgesetzt, statt ein nicht vorhandenes Feld zu
    // erfinden. Mit so vielen Feldern passt der Screen nicht mehr
    // garantiert auf den Bildschirm - deshalb (wie runConfirmScreen() in
    // address_search_screen.cpp) scrollbar, mit Hoch/Runter-Pfeilen nur
    // wenn tatsaechlich noetig. ----
    void showDetailScreen(TFT_eSPI& tft, const TransitClient::Departure& dep) {
        MenuStars::reset();

        constexpr int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t LINE_H = 16;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 10);

        // Rundfahrt (z.B. Linie 963 "Geislingen (Steige)" -> "Geislingen
        // (Steige)", live gegen die echte API bestaetigt, 06.10.): Start
        // der Fahrt (tripFrom.name) ist identisch mit dem Fahrtziel
        // (headsign) - echte Rohdaten, kein Mapping-Fehler. Ohne
        // Kennzeichnung sieht das wie ein Bug aus, siehe CLAUDE.md.
        bool isLoop = dep.tripFromName.length() > 0 && dep.tripFromName == dep.headsign;

        // Zeichnet (oder vermisst nur, draw=false) alle Detailzeilen -
        // wird zweimal aufgerufen: einmal zum Ermitteln der Gesamthoehe
        // (fuer die Scroll-Entscheidung), einmal fuer die eigentliche
        // Anzeige. "Nach:" zeigt bewusst headsign statt tripTo.name, damit
        // Liste (die immer headsign zeigt) und Detail-Screen beim Ziel
        // IMMER denselben Wert zeigen (Alex' Auftrag, 06.10.) - beide
        // Felder koennen bei manchen Fahrten theoretisch abweichen
        // (headsign ist der fahrgast-sichtbare Richtungstext, tripTo.name
        // der GTFS-technische Fahrtendpunkt).
        auto drawContent = [&](int16_t scrollY, bool draw) -> int16_t {
            int16_t y = VIEW_TOP;
            int16_t vTop = draw ? VIEW_TOP : -32000;
            int16_t vBottom = draw ? VIEW_BOTTOM : 32000;

            if (dep.cancelled) {
                if (draw) tft.setTextColor(TFT_RED, TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, I18n::t(StringId::TRANSIT_CANCELLED_LABEL),
                                   scrollY, vTop, vBottom, draw);
                y = (int16_t)(y + 4);
            }

            if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
            String lineHeadsign = dep.line + " " + I18n::t(StringId::TRANSIT_STOP_LABEL) + dep.headsign;
            y = layoutWrapped(tft, 10, y, maxW, LINE_H, lineHeadsign, scrollY, vTop, vBottom, draw);

            if (isLoop) {
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_LOOP_LABEL)) + dep.tripFromName,
                    scrollY, vTop, vBottom, draw);
            } else {
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_FROM_LABEL)) + dep.tripFromName,
                    scrollY, vTop, vBottom, draw);
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_TO_LABEL)) + dep.headsign,
                    scrollY, vTop, vBottom, draw);
            }

            y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                String(I18n::t(StringId::TRANSIT_DETAIL_AGENCY_LABEL)) + dep.agencyName,
                scrollY, vTop, vBottom, draw);

            if (dep.routeLongName.length() > 0) {
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_ROUTE_LONGNAME_LABEL)) + dep.routeLongName,
                    scrollY, vTop, vBottom, draw);
            }
            if (dep.track.length() > 0) {
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_TRACK_LABEL)) + dep.track,
                    scrollY, vTop, vBottom, draw);
            }

            y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                String(I18n::t(StringId::TRANSIT_DETAIL_STATUS_LABEL)) +
                I18n::t(dep.realTime ? StringId::TRANSIT_DETAIL_REALTIME_YES : StringId::TRANSIT_DETAIL_REALTIME_NO),
                scrollY, vTop, vBottom, draw);

            // Verspaetung nur anzeigen, wenn ueberhaupt Echtzeitdaten
            // vorliegen - ohne Echtzeittracking waere "puenktlich" eine
            // unbegruendete Behauptung (Alex' Auftrag, 06.10.).
            if (dep.realTime) {
                long delaySecs = (long)dep.epoch - (long)dep.scheduledEpoch;
                long delayMin = (delaySecs + (delaySecs >= 0 ? 30 : -30)) / 60;
                String delayText = (delayMin == 0)
                    ? String(I18n::t(StringId::TRANSIT_DETAIL_ON_TIME))
                    : String(delayMin > 0 ? "+" : "") + String(delayMin) + I18n::t(StringId::TRANSIT_UNIT_MIN);
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_DELAY_LABEL)) + delayText,
                    scrollY, vTop, vBottom, draw);
            }

            if (dep.wheelchairAccessible.length() > 0) {
                StringId wcId = StringId::TRANSIT_DETAIL_WHEELCHAIR_UNKNOWN;
                if (dep.wheelchairAccessible == "ACCESSIBLE") wcId = StringId::TRANSIT_DETAIL_WHEELCHAIR_YES;
                else if (dep.wheelchairAccessible == "NOT_ACCESSIBLE") wcId = StringId::TRANSIT_DETAIL_WHEELCHAIR_NO;
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::TRANSIT_DETAIL_WHEELCHAIR_LABEL)) + I18n::t(wcId),
                    scrollY, vTop, vBottom, draw);
            }

            y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                String(I18n::t(StringId::TRANSIT_DETAIL_BIKES_LABEL)) +
                I18n::t(dep.bikesAllowed ? StringId::TRANSIT_DETAIL_BIKES_YES : StringId::TRANSIT_DETAIL_BIKES_NO),
                scrollY, vTop, vBottom, draw);

            // Uhrzeit als Hauptangabe, Countdown klein ergaenzend dahinter
            // (Alex' Auftrag, 06.10.) - hier ist genug Platz dafuer, anders
            // als in der engen Listenzeile (siehe formatRelativeSuffix()).
            y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                String(I18n::t(StringId::TRANSIT_DETAIL_DEPARTS_LABEL)) + formatClockTime(dep.epoch, dep.stopTz) +
                " (" + formatRelativeSuffix(dep.epoch) + ")",
                scrollY, vTop, vBottom, draw);

            return y;
        };

        int16_t totalH = drawContent(0, false);
        int16_t maxScroll = (int16_t)(totalH - VIEW_BOTTOM);
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;

        Rect backBtn = scrollable
            ? Rect{10, BACK_Y, 130, 40}
            : Rect{10, BACK_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, BACK_Y, 38, 40};
        Rect downBtn = {190, BACK_Y, 38, 40};
        constexpr int16_t SCROLL_STEP = 48;

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::TRANSIT_DETAIL_TITLE));

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

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::TRANSIT_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::TRANSIT_INFO_BODY));

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

    // Separater Standort NUR fuer diesen Screen (Config::
    // SD_TRANSIT_LOCATION_FILE) - UEBERSCHREIBT NICHT die allgemeine
    // Toolbox-Position (LocationPresets). Fehlt die Datei, wird false
    // zurueckgegeben (Aufrufer faellt dann auf die allgemeine Position
    // zurueck).
    bool loadLocationOverride(double& lat, double& lon) {
        if (!SD.exists(Config::SD_TRANSIT_LOCATION_FILE)) return false;
        File f = SD.open(Config::SD_TRANSIT_LOCATION_FILE, FILE_READ);
        if (!f) return false;
        String line = f.readStringUntil('\n');
        f.close();
        line.trim();
        int comma = line.indexOf(',');
        if (comma < 0) return false;
        lat = line.substring(0, comma).toDouble();
        lon = line.substring(comma + 1).toDouble();
        return true;
    }

    void saveLocationOverride(double lat, double lon) {
        File f = SD.open(Config::SD_TRANSIT_LOCATION_FILE, FILE_WRITE);
        if (!f) return;
        f.printf("%.6f,%.6f\n", lat, lon);
        f.close();
    }

    // Ermittelt die effektive Position (Override falls vorhanden, sonst
    // die allgemeine aktive Toolbox-Position, gleiches Muster wie
    // iss_radar_screen.cpp::enter()) und meldet sie bei NetTask an.
    bool resolveAndSetLocation() {
        double lat = 0, lon = 0;
        if (loadLocationOverride(lat, lon)) {
            NetTask::setTransitLocation(lat, lon);
            return true;
        }
        if (LocationPresets::count() > 0) {
            int8_t active = LocationPresets::activeIndex();
            uint8_t idx = (active >= 0) ? (uint8_t)active : 0;
            LocationPresets::getLatLon(idx, lat, lon);
            NetTask::setTransitLocation(lat, lon);
            return true;
        }
        return false;
    }

    // Persistiert ueber mehrere enter()/leave()-Zyklen hinweg (gleiches
    // Prinzip wie rocket_screen.cpp/iss_radar_screen.cpp). Der eigentliche
    // Netzwerk-Abruf laeuft ausschliesslich in NetTask auf Core 0.
    bool haveData = false;
    bool hasLocation = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    NetTask::TransitData lastData;

    void redraw(TFT_eSPI& tft) {
        drawChrome(tft);
        drawButton(tft, refineBtnRect(), I18n::t(StringId::TRANSIT_REFINE_ADDRESS_BTN));

        if (!hasLocation) {
            drawCenteredHint(tft, StringId::ISS_RADAR_NO_LOCATION);
        } else if (haveData) {
            drawDepartures(tft, lastData);
        } else {
            drawCenteredHint(tft, StringId::TRANSIT_LOADING);
        }
    }
}

void enter(TFT_eSPI& tft) {
    hasLocation = resolveAndSetLocation();
    redraw(tft);
}

void update(TFT_eSPI& tft) {
    // NetTask (Core 0) liefert die Daten im Hintergrund - hier (Core 1)
    // nur pruefen, ob sich die Versionsnummer seit dem letzten Durchlauf
    // geaendert hat. Kein eigener Netzwerk-Abruf mehr in diesem Screen.
    if (!hasLocation) return;

    uint32_t v = NetTask::transitVersion();
    if (v != lastSeenVersion) {
        NetTask::TransitData newData;
        if (NetTask::getTransitData(newData)) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawDepartures(tft, lastData);
        }
    } else {
        NetTask::Status st = NetTask::transitStatus();
        if (!haveData && st != lastSeenStatus) {
            lastSeenStatus = st;
            if (st == NetTask::Status::Error) {
                drawCenteredHint(tft, errorStringId(NetTask::transitError()));
            }
        }
    }

    // Lauftext-Animation fuer zu lange Ziel-Labels (Alex' Auftrag, 06.10. -
    // siehe CLAUDE.md "Pflicht: automatisches Scrollen bei zu langem
    // Text") - MUSS jeden Durchlauf aufgerufen werden, nicht nur bei
    // neuen Daten.
    if (haveData) {
        updateHeadsignMarquees(tft, lastData);
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        redraw(tft);
        return;
    }

    if (refineBtnRect().contains(x, y)) {
        double lat = 0, lon = 0;
        if (AddressSearchScreen::runForCoordinates(tft, lat, lon)) {
            saveLocationOverride(lat, lon);
            NetTask::setTransitLocation(lat, lon);
            hasLocation = true;
            haveData = false;
            lastSeenStatus = NetTask::Status::Pending;
        }
        MenuStars::reset();
        redraw(tft);
        return;
    }

    if (haveData) {
        for (uint8_t i = 0; i < departureRowCount; i++) {
            if (departureRowRects[i].contains(x, y)) {
                showDetailScreen(tft, lastData.departures[i]);
                MenuStars::reset();
                redraw(tft);
                return;
            }
        }
    }
}

}
