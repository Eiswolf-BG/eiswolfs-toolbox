#include "earthquake_screen.h"
#include <time.h>
#include "touch_input.h"
#include "menu_stars.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "units.h"
#include "config.h"
#include "i18n.h"
#include "earthquake_client.h"
#include "net_task.h"

namespace EarthquakeScreen {

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

    // Wortweiser Umbruch anhand echter Pixelbreite, mit optionalem
    // draw-Schalter (nur messen, nicht zeichnen) - fuer den zweistufigen
    // Mess-dann-Zeichnen-Ablauf der Hauptkachel unten (gleiches Muster wie
    // rocket_screen.cpp::showLaunchDetailScreen(), lokal dupliziert,
    // Projekt-Konvention).
    int16_t layoutWrapped(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                           int16_t lineHeight, const String& text, bool draw = true) {
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
                tft.setCursor(x, y);
                tft.print(line);
            }
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    // Wie layoutWrapped() oben, zeichnet aber Label (Akzentfarbe) und Wert
    // (Weiss) getrennt (CLAUDE.md "Pflicht: Label/Wert-Farbkontrast") -
    // ebenfalls mit draw-Schalter fuer den Mess-Durchlauf.
    int16_t layoutWrappedLabelValue(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                                     int16_t lineHeight, const String& label, const String& value,
                                     uint16_t labelColor, uint16_t valueColor, bool draw = true) {
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

            if (draw) {
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

    // ---- Lokale Zeit des GERAETS (wie f1_screen.cpp::
    // formatDeviceLocalDateTime(), 1:1 dupliziert, Projekt-Konvention) -
    // das Geraet kennt keine eigene gespeicherte Zeitzone (nur lat/lon
    // ueber LocationPresets), und die USGS-Erdbebendaten liefern (anders
    // als OEPNV mit seinem Haltestellen-tz-Feld) ebenfalls KEINE
    // Ziel-Zeitzone. Deshalb dieselbe Interpretation wie beim F1-Screen:
    // DST-Berechnung mit fester Mitteleuropa-Basis (+1h Winterzeit) statt
    // einer Nachschlagtabelle. ----
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

    // Literal/status-gebundene Magnitude-Farbskala (Alex' Auftrag, exakte
    // Stufen vorgegeben) - NICHT das Akzentgruen, eigene Bedeutung wie bei
    // AQI-Kategorien/Hold-Fail.
    uint16_t magnitudeColor(float mag) {
        if (mag >= 7.0f) return TFT_RED;
        if (mag >= 6.0f) return TFT_ORANGE;
        if (mag >= 5.0f) return TFT_YELLOW;
        return TFT_GREEN;
    }

    String formatMagnitude(float mag) {
        return "M " + String(mag, 1);
    }

    // Globale Einheiten-Einstellung beachten (CLAUDE.md "Pflicht: globale
    // Einheiten-Einstellung beachten") - depthKm kommt roh von der API
    // (immer km), hier bei Imperial lokal nach mi umgerechnet (gleiches
    // Prinzip wie beim ISS-Radar-Fix).
    String formatDepth(float depthKm) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f %s", Units::distanceKmToDisplay(depthKm), Units::distanceSuffix());
        return String(buf);
    }

    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 10);
    constexpr int16_t TILE_X = 10;
    constexpr int16_t TILE_W = (int16_t)(Config::SCREEN_WIDTH - 20);
    // 14px statt 10px (Alex' Fund, 07.10., Foto): bei 10px wirkte der
    // Ort-Text am rechten Box-Rand zu knapp/abgeschnitten - 14px schafft
    // sichtbar mehr Luft zum Rahmen, ohne die Hauptkachel unnoetig breiter
    // zu machen.
    constexpr int16_t TILE_PAD = 14;
    constexpr int16_t LINE_H = 15;
    constexpr int16_t MORE_BTN_H = 36;
    constexpr int16_t MORE_BTN_GAP = 10;

    // Das staerkste Beben bleibt als "Hero"-Element gross/hervorgehoben
    // oben (Magnitude-Farbskala, Ort, Zeit, Tsunami-Warnung) - direkt
    // darunter folgt jetzt zusaetzlich eine kompakte Liste WEITERER Beben
    // (Magnitude+Zeit/Ort, gleiches Zeilenmuster wie showMoreQuakesScreen()
    // unten), so viele wie nach der (variablen) Hero-Hoehe noch Platz
    // haben (Alex' Auftrag, 13.10.: "erstes Beben groesser/rot
    // hervorheben, darunter aber die weiteren Ereignisse, bis der
    // Bildschirm voll ist" - siehe CLAUDE.md "Ergaenzung fuer Screens mit
    // einem Hero-Element"). Die Zeilenzahl wird ERST in drawMainTile()
    // berechnet, da sie von der tatsaechlich gemessenen Hero-Hoehe abhaengt
    // (nicht konstant wie bei natural_events_screen.cpp::VISIBLE_ROWS).
    //
    // BUGFIX (13.10., Alex' Fund per Foto): mit dem anfangs uebernommenen
    // ZWEIZEILIGEN Zeilenmuster von showMoreQuakesScreen() (ROW_H=40+GAP4
    // = 44px/Zeile) passte bei manchen Hero-Hoehen (z.B. ein Beben mit
    // zweizeiligem Ortstext) oft nicht mal mehr EINE Zusatzzeile in den
    // Restplatz, obwohl noch ~40px uebrig waren - der Bildschirm blieb
    // trotzdem nur zu einem Bruchteil gefuellt, genau der Fehler, den die
    // neue Regel verhindern soll. Deshalb jetzt EINZEILIGES Format
    // (Magnitude+Ort-Lauftext+Zeit alle in einer Zeile, gleiches Prinzip
    // wie crypto_screen.cpp::showMoreCoinsScreen()) - ROW_H=28+GAP4=32px/
    // Zeile nutzt den Restplatz deutlich dichter aus.
    constexpr int16_t ROW_H = 28;
    constexpr int16_t ROW_GAP = 4;
    constexpr int16_t LIST_BOTTOM_MARGIN = 10;
    // Reservierte Breiten links (Magnitude, z.B. "M 10.0") und rechts
    // (Zeit, "DD.MM. HH:MM") - der Ort laeuft dazwischen als Lauftext.
    constexpr int16_t ROW_MAG_W = 50;
    constexpr int16_t ROW_TIME_W = 90;

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    // Zentrierung des Titels auf der Mitte der vom "?"-Button FREIEN Zone
    // (gleiches Muster wie rocket_screen.cpp/crypto_screen.cpp, lokal
    // dupliziert, Projekt-Konvention).
    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::EARTHQUAKE_TITLE), TFT_GREEN);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::EARTHQUAKE_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    // Zweistufiger Mess-dann-Zeichnen-Ablauf (draw=false misst nur die
    // tatsaechlich benoetigte Hoehe, ohne zu zeichnen - gleiches Muster
    // wie rocket_screen.cpp::showLaunchDetailScreen()) - die Hauptkachel-
    // Hoehe haengt von der (variabel langen) Ortsbeschreibung und davon
    // ab, ob eine Tsunami-Warnzeile noetig ist, deshalb hier gemessen
    // statt eine feste Hoehe zu raten (CLAUDE.md "Pflicht: jedes Layout
    // muss auf den Bildschirm passen").
    int16_t drawMainTileContent(TFT_eSPI& tft, int16_t tileX, int16_t tileY, int16_t tileW,
                                 const EarthquakeClient::Quake& q, bool draw) {
        int16_t x = (int16_t)(tileX + TILE_PAD);
        int16_t maxW = (int16_t)(tileW - 2 * TILE_PAD);
        int16_t y = (int16_t)(tileY + TILE_PAD);

        // Magnitude gross und farblich abgestuft - ueber drawString()+
        // ML_DATUM statt rohem setCursor()+print() (Lehre aus dem
        // AQI-Zahlen-Fix im Wetter-Screen: Datum-basierte Zentrierung
        // rechnet die Font-Baseline korrekt ein, kein geratener Versatz
        // noetig, siehe CLAUDE.md "WICHTIGSTE FALLE").
        if (draw) {
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(magnitudeColor(q.magnitude));
            tft.setTextSize(2);
            String magText = formatMagnitude(q.magnitude);
            int16_t magW = tft.textWidth(magText);
            tft.drawString(magText, x, (int16_t)(y + 13));
            tft.setTextSize(1);

            // Dezente Erklaerung, was die Zahl neben dem "M" bedeutet
            // (Alex' Auftrag, 12.10.: "fehlt eine Erklaerung") - RECHTS
            // neben der Zahl statt in einer eigenen Zeile, damit sich am
            // bereits korrigierten vertikalen Layout (Innenabstand,
            // Abstand zur naechsten Zeile) nichts aendert. Gedimmtes Gruen
            // statt Weiss/Akzentgruen (eigene, zurueckhaltende Bedeutung,
            // keine weitere abgefragte Dateninformation).
            tft.setTextColor(tft.color565(0, (uint8_t)(255 * 0.45f), 0));
            tft.drawString(I18n::t(StringId::EARTHQUAKE_MAGNITUDE_LABEL),
                            (int16_t)(x + magW + 8), (int16_t)(y + 13));

            tft.setTextDatum(TL_DATUM);
        }
        // Alex' Fund, 07.10. (Foto): "M 6.6" ueberschnitt sich mit der
        // folgenden "Ort:"-Zeile - derselbe Baseline/Ascent-Fehler wie
        // schon mehrfach behoben, hier beim Uebergang von einer per
        // ML_DATUM zentrierten Groesse-2-Zahl zu einer rohen, baseline-
        // verankerten Groesse-1-Zeile direkt darunter. 13px Abstand vom
        // Zentrierungspunkt der Zahl reichten nicht fuer deren eigene
        // Resthoehe PLUS den Ascent der naechsten Zeile. Fix: derselbe,
        // bereits am F&G-Krypto-Kachel bewaehrte 22px-Abstand
        // (crypto_screen.cpp::drawFngTile(): Zahlen-Mittelpunkt bei y+36,
        // Klassifikationszeile bei y+58) statt der zu knappen 13px.
        // Alex' Fund, 12.10. (Foto): selbst mit dem obigen Fix wirkte der
        // Abstand zur "Ort:"-Zeile noch zu eng - zusaetzlich eine volle
        // Leerzeile (LINE_H) ergaenzt, PLUS die Zeilenabstaende darunter
        // (Ort/Tiefe/Zeit) von +4px auf +10px vergroessert, damit sich der
        // Inhalt gleichmaessiger ueber die Kachelhoehe verteilt statt oben
        // gedraengt zu stehen und unten viel Leerraum zu lassen.
        y = (int16_t)(y + 35 + LINE_H);

        y = layoutWrappedLabelValue(tft, x, y, maxW, LINE_H,
            I18n::t(StringId::EARTHQUAKE_PLACE_LABEL), q.place, TFT_GREEN, TFT_WHITE, draw);
        y = layoutWrappedLabelValue(tft, x, (int16_t)(y + 10), maxW, LINE_H,
            I18n::t(StringId::EARTHQUAKE_DEPTH_LABEL), formatDepth(q.depthKm), TFT_GREEN, TFT_WHITE, draw);
        y = layoutWrappedLabelValue(tft, x, (int16_t)(y + 10), maxW, LINE_H,
            I18n::t(StringId::EARTHQUAKE_TIME_LABEL), formatDeviceLocalDateTime(q.epoch), TFT_GREEN, TFT_WHITE, draw);

        // Tsunami-Warnung bewusst KOMPLETT in Rot, keine Label/Wert-
        // Aufteilung (gleiche Begruendung wie Rocket-Screens Hold-/
        // Fehlgrund-Zeile: traegt eine eigene, kritische Bedeutung, ein
        // weisser Wertanteil wuerde die geforderte "auffaellige"
        // Sichtbarkeit mindern).
        if (q.tsunami) {
            if (draw) tft.setTextColor(TFT_RED, TFT_BLACK);
            y = layoutWrapped(tft, x, (int16_t)(y + 10), maxW, LINE_H,
                I18n::t(StringId::EARTHQUAKE_TSUNAMI_WARNING), draw);
        }

        return (int16_t)(y + TILE_PAD);
    }

    // Vom letzten drawMainTile()-Aufruf gemerkt, da die Hoehe der
    // Hauptkachel variabel ist (haengt von Ortstext-Laenge/Tsunami-Zeile
    // ab) - handleTap() braucht das aktuelle Rect des sichtbaren "Weitere
    // Beben"-Buttons darunter (gleiches Prinzip wie rocket_screen.cpp::
    // moreBtnRect(), hier aber dynamisch statt fest, da die Rocket-
    // Countdown-Flaeche eine feste Hoehe hat, unsere Hauptkachel nicht).
    Rect lastMoreBtnRect = {0, 0, 0, 0};

    // Maximale Zeilenzahl der Zusatzliste unterhalb des Hero-Elements -
    // fester Obergrenze fuer das Marquee-Zustands-Array, die tatsaechlich
    // gezeichnete Zahl (lastListRowCount) wird dynamisch aus der nach dem
    // Hero verbleibenden Hoehe berechnet (siehe drawMainTile()).
    constexpr uint8_t MAX_LIST_ROWS = 6;
    MarqueeLabel::State listMarquee[MAX_LIST_ROWS];
    uint8_t lastListRowCount = 0;
    int16_t lastListTop = 0;

    int16_t listRowTopFor(uint8_t rowIndex) {
        return (int16_t)(lastListTop + rowIndex * (ROW_H + ROW_GAP));
    }

    // Zeichnet eine kompakte EINZEILIGE Zeile (Magnitude links, Zeit
    // rechts) - der Ort laeuft als Lauftext dazwischen, siehe
    // MarqueeLabel::draw()-Aufruf in update() (dort, weil Lauftext bei
    // JEDEM Tick weitergedreht werden muss, nicht nur beim Neuzeichnen).
    void drawListRowStatic(TFT_eSPI& tft, int16_t rowTop, const EarthquakeClient::Quake& q) {
        int16_t rowCy = (int16_t)(rowTop + ROW_H / 2);

        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(formatDeviceLocalDateTime(q.epoch), (int16_t)(Config::SCREEN_WIDTH - 10), rowCy);

        tft.setTextDatum(ML_DATUM);
        // Tsunami ueberschreibt die Magnitude-Farbe auf Rot (eigene,
        // kritische Bedeutung, siehe drawMainTileContent()-Kommentar oben).
        tft.setTextColor(q.tsunami ? TFT_RED : magnitudeColor(q.magnitude), TFT_BLACK);
        tft.drawString(formatMagnitude(q.magnitude), TILE_X, rowCy);

        tft.setTextDatum(TL_DATUM);
    }

    // Sichtbarer "Weitere Beben"-Button unterhalb der Hauptkachel statt
    // nur ueber einen unsichtbaren Tap auf die Hauptflaeche erreichbar
    // (Alex' Fund, 07.10., Foto: wirkte leer/wenig einladend) - gleiches
    // Muster wie rocket_screen.cpp::drawMoreButtonIfNeeded(), nur
    // zusaetzlich mit dynamischer Y-Position wegen der variablen
    // Kachelhoehe. Alex' Fund, 13.10. (Foto): unterhalb des Hero-Elements
    // blieb der Rest des Bildschirms ungenutzt leer, obwohl "Weitere
    // Beben" noch viele weitere Eintraege zeigte - jetzt wird dieser Platz
    // mit einer kompakten Zusatzliste gefuellt (siehe CLAUDE.md "Ergaenzung
    // fuer Screens mit einem Hero-Element").
    void drawMainTile(TFT_eSPI& tft, const EarthquakeClient::Quake* quakes, uint8_t quakeCount) {
        tft.fillRect(0, CONTENT_TOP, Config::SCREEN_WIDTH,
                     (int16_t)(Config::SCREEN_HEIGHT - CONTENT_TOP), TFT_BLACK);

        int16_t measuredBottom = drawMainTileContent(tft, TILE_X, CONTENT_TOP, TILE_W, quakes[0], false);
        int16_t tileH = (int16_t)(measuredBottom - CONTENT_TOP);

        tft.drawRoundRect(TILE_X, CONTENT_TOP, TILE_W, tileH, 4, TFT_GREEN);
        drawMainTileContent(tft, TILE_X, CONTENT_TOP, TILE_W, quakes[0], true);

        lastListTop = (int16_t)(CONTENT_TOP + tileH + MORE_BTN_GAP);
        int16_t availableForList = (int16_t)(Config::SCREEN_HEIGHT - lastListTop - MORE_BTN_H -
                                              MORE_BTN_GAP - LIST_BOTTOM_MARGIN);
        uint8_t remainingQuakes = (uint8_t)(quakeCount - 1);
        uint8_t rowsFit = (availableForList > 0) ? (uint8_t)(availableForList / (ROW_H + ROW_GAP)) : 0;
        if (rowsFit > MAX_LIST_ROWS) rowsFit = MAX_LIST_ROWS;
        uint8_t rowsToShow = (remainingQuakes < rowsFit) ? remainingQuakes : rowsFit;
        lastListRowCount = rowsToShow;

        for (uint8_t i = 0; i < rowsToShow; i++) {
            MarqueeLabel::reset(listMarquee[i]);
            drawListRowStatic(tft, listRowTopFor(i), quakes[1 + i]);
        }

        int16_t listBottom = listRowTopFor(rowsToShow);
        lastMoreBtnRect = {TILE_X, (int16_t)(listBottom + MORE_BTN_GAP), TILE_W, MORE_BTN_H};
        if (quakeCount > 1 + rowsToShow) {
            drawButton(tft, lastMoreBtnRect, I18n::t(StringId::EARTHQUAKE_MORE_BTN));
        }
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::EARTHQUAKE_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::EARTHQUAKE_INFO_BODY));

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

    // ---- "Weitere Beben" (Alex' Auftrag, 07.10., Layout komplett
    // ueberarbeitet 12.10.) - Liste der naechsten bis zu 9 Eintraege
    // (Index 1..Config::EARTHQUAKE_MAX_QUAKES-1, das staerkste steht schon
    // auf der Hauptkachel). Alex' Fund, 12.10. (Foto): die urspruengliche
    // EINZEILIGE Variante (Ort per Marquee in nur 92px links, Magnitude ab
    // 108px) war fuer die oft sehr langen USGS-Ortsbeschreibungen ("50 km
    // ESE of X, Papua New Guinea") zu eng - wirkte abgeschnitten/
    // ueberlappend. Jetzt ZWEI Zeilen pro Eintrag (Zeile 1: Magnitude +
    // Zeitstempel, Zeile 2: Ort per Marquee ueber die VOLLE Breite) PLUS
    // scrollbar (gleiches Hoch/Runter-Button-Muster wie f1_screen.cpp/
    // crypto_currency_screen.cpp) - behebt gleichzeitig Alex' zweiten Fund
    // (letzter Eintrag ueberlappte den Zurueck-Button): mit Scrollen+
    // Viewport-Clipping kann kein Eintrag mehr in den Button-Bereich
    // hineinragen, unabhaengig von der tatsaechlichen Anzahl Ergebnisse.
    void showMoreQuakesScreen(TFT_eSPI& tft, const EarthquakeClient::Quake* quakes, uint8_t count) {
        MenuStars::reset();

        static MarqueeLabel::State placeMarquee[Config::EARTHQUAKE_MAX_QUAKES];
        for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(placeMarquee[i]);

        constexpr int16_t ROW_H = 40;
        constexpr int16_t ROW_GAP = 4;
        constexpr int16_t NAME_X = 10;
        constexpr int16_t NAME_W = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t TIME_RIGHT_X = (int16_t)(Config::SCREEN_WIDTH - 10);
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 8);

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

        // Zeichnet nur Magnitude+Zeitstempel (Zeile 1 je Eintrag) - der
        // Ortsname (Zeile 2) laeuft als Marquee ueber eine eigene,
        // haeufigere Update-Kadenz unten in der Hauptschleife und wird
        // deshalb NICHT hier mitgezeichnet (sonst wuerde jeder Scroll-Schritt
        // den Marquee-Zustand/Fortschritt sichtbar zuruecksetzen).
        auto drawStaticRows = [&]() {
            tft.fillRect(0, VIEW_TOP, Config::SCREEN_WIDTH, (int16_t)(VIEW_BOTTOM - VIEW_TOP), TFT_BLACK);
            for (uint8_t i = 0; i < count; i++) {
                int16_t rowTop = (int16_t)(VIEW_TOP + i * (ROW_H + ROW_GAP) - scrollY);
                // Alex' Fund, 12.10. (Foto): die vorherige Pruefung liess
                // eine Zeile durch, sobald sie NUR TEILWEISE sichtbar war
                // (z.B. rowTop knapp vor VIEW_BOTTOM) - die Zeile wurde dann
                // trotzdem KOMPLETT gezeichnet und ragte mit ihrem unteren
                // Teil in die Button-Leiste hinein. Jetzt strenge
                // Vollstaendigkeits-Pruefung: eine Zeile wird nur gezeichnet,
                // wenn sie GANZ innerhalb von [VIEW_TOP, VIEW_BOTTOM] liegt.
                if (rowTop < VIEW_TOP || rowTop + ROW_H > VIEW_BOTTOM) continue;

                tft.setTextDatum(MR_DATUM);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString(formatDeviceLocalDateTime(quakes[i].epoch), TIME_RIGHT_X, (int16_t)(rowTop + 11));

                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(magnitudeColor(quakes[i].magnitude), TFT_BLACK);
                tft.drawString(formatMagnitude(quakes[i].magnitude), NAME_X, (int16_t)(rowTop + 11));

                tft.setTextDatum(TL_DATUM);
            }
        };

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::EARTHQUAKE_MORE_TITLE));

        drawStaticRows();
        drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
        if (scrollable) {
            drawButton(tft, upBtn, "^");
            drawButton(tft, downBtn, "v");
        }

        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) {
                if (backBtn.contains(tap.x, tap.y)) return;
                if (scrollable && upBtn.contains(tap.x, tap.y) && scrollY > 0) {
                    scrollY = (int16_t)(scrollY - SCROLL_STEP);
                    if (scrollY < 0) scrollY = 0;
                    for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(placeMarquee[i]);
                    drawStaticRows();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY = (int16_t)(scrollY + SCROLL_STEP);
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(placeMarquee[i]);
                    drawStaticRows();
                }
            }

            for (uint8_t i = 0; i < count; i++) {
                int16_t rowTop = (int16_t)(VIEW_TOP + i * (ROW_H + ROW_GAP) - scrollY);
                if (rowTop < VIEW_TOP || rowTop + ROW_H > VIEW_BOTTOM) continue;
                MarqueeLabel::draw(tft, placeMarquee[i], NAME_X, (int16_t)(rowTop + 30), NAME_W, 16,
                                    quakes[i].place, TFT_WHITE, TFT_BLACK);
            }
            MenuStars::update(tft);
            delay(20);
        }
    }

    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    NetTask::EarthquakeData lastData;
}

void enter(TFT_eSPI& tft) {
    drawChrome(tft);
    if (haveData && lastData.quakeCount > 0) {
        drawMainTile(tft, lastData.quakes, lastData.quakeCount);
    } else {
        drawCenteredHint(tft, StringId::EARTHQUAKE_LOADING);
    }
}

void update(TFT_eSPI& tft) {
    uint32_t v = NetTask::earthquakeVersion();
    if (v != lastSeenVersion) {
        NetTask::EarthquakeData newData;
        if (NetTask::getEarthquakeData(newData) && newData.quakeCount > 0) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawMainTile(tft, lastData.quakes, lastData.quakeCount);
        }
    } else if (!haveData) {
        NetTask::Status st = NetTask::earthquakeStatus();
        if (st != lastSeenStatus) {
            lastSeenStatus = st;
            if (st == NetTask::Status::Error) {
                drawCenteredHint(tft, StringId::EARTHQUAKE_FETCH_ERROR);
            }
        }
    }

    // Lauftext fuer lange Ortsnamen in der Zusatzliste unterhalb des
    // Hero-Elements weiterdrehen (Alex' Auftrag, 13.10.) - laeuft bei
    // JEDEM Tick, nicht nur bei neuen Daten (gleiches Prinzip wie
    // natural_events_screen.cpp).
    if (haveData) {
        constexpr int16_t MARQUEE_X = (int16_t)(TILE_X + ROW_MAG_W);
        constexpr int16_t MARQUEE_W = (int16_t)(Config::SCREEN_WIDTH - 20 - ROW_MAG_W - ROW_TIME_W);
        for (uint8_t i = 0; i < lastListRowCount; i++) {
            int16_t rowCy = (int16_t)(listRowTopFor(i) + ROW_H / 2);
            MarqueeLabel::draw(tft, listMarquee[i], MARQUEE_X, rowCy,
                                MARQUEE_W, 16,
                                lastData.quakes[1 + i].place, TFT_WHITE, TFT_BLACK);
        }
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        drawChrome(tft);
        if (haveData && lastData.quakeCount > 0) {
            drawMainTile(tft, lastData.quakes, lastData.quakeCount);
        } else {
            drawCenteredHint(tft, StringId::EARTHQUAKE_LOADING);
        }
        return;
    }

    if (haveData && lastData.quakeCount > (uint8_t)(1 + lastListRowCount) && lastMoreBtnRect.contains(x, y)) {
        uint8_t startIndex = (uint8_t)(1 + lastListRowCount);
        showMoreQuakesScreen(tft, &lastData.quakes[startIndex], (uint8_t)(lastData.quakeCount - startIndex));
        MenuStars::reset();
        drawChrome(tft);
        drawMainTile(tft, lastData.quakes, lastData.quakeCount);
        return;
    }
}

}
