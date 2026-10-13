#include "weather_screen.h"
#include <math.h>
#include "net_task.h"
#include "weather_client.h"
#include "location_presets.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "weather_icons.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"
#include "units.h"

namespace WeatherScreen {

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

    // Wortweiser Umbruch anhand echter Pixelbreite - gleiches Muster wie in
    // den anderen Feature-Screens, hier lokal dupliziert (Projekt-
    // Konvention, siehe CLAUDE.md). Optionale Scroll-Unterstuetzung
    // (scrollY/viewTop/viewBottom/draw, Defaults = bisheriges Verhalten
    // ohne Scrollen) - 1:1 dasselbe Muster wie rocket_screen.cpp/
    // f1_screen.cpp, hier fuer den neuen Luftqualitaets-Detail-Screen
    // gebraucht (Alex' Auftrag, 06.10.).
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

    // Tageszahl seit 1970-01-01 (Howard Hinnant) - 1:1 dasselbe Muster wie
    // transit_client.cpp/transit_screen.cpp, hier lokal dupliziert (siehe
    // CLAUDE.md-Konvention). NUR fuer den Wochentag in der Wochenuebersicht
    // gebraucht - KEINE Zeitzonen-/Sommerzeit-Berechnung noetig, Open-Meteo
    // liefert mit "timezone=auto" bereits lokale Datums-/Uhrzeit-Strings
    // (siehe CLAUDE.md "Wetter-Screen" - grosser Unterschied zum
    // OEPNV-Screen).
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    // "YYYY-MM-DD" -> Wochentags-StringId (Sonntag=0, da 1970-01-01 ein
    // Donnerstag war = Wochentag 4 bei Sonntag=0).
    StringId weekdayStringId(const String& dateLocal) {
        int y, m, d;
        if (sscanf(dateLocal.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return StringId::WEEKDAY_SUN;
        long days = daysFromCivil(y, (unsigned)m, (unsigned)d);
        long dow = ((days % 7) + 4 + 7) % 7;
        static const StringId IDS[7] = {
            StringId::WEEKDAY_SUN, StringId::WEEKDAY_MON, StringId::WEEKDAY_TUE, StringId::WEEKDAY_WED,
            StringId::WEEKDAY_THU, StringId::WEEKDAY_FRI, StringId::WEEKDAY_SAT,
        };
        return IDS[dow];
    }

    // "YYYY-MM-DDTHH:MM" -> "HH:MM" (schon lokale Wanduhrzeit, kein Offset
    // noetig - siehe CLAUDE.md).
    String timePart(const String& dateTimeLocal) {
        if (dateTimeLocal.length() < 16) return dateTimeLocal;
        return dateTimeLocal.substring(11, 16);
    }

    // Tag/Nacht fuer eine stuendliche Vorhersage-Spalte - vereinfachend
    // immer gegen Sonnenauf-/-untergang von HEUTE (daily[0]) verglichen
    // (die Vorhersageleiste deckt nur die kommenden ~18h ab, nicht
    // mehrere Tage - geringfuegige Ungenauigkeit an den Raendern ist fuer
    // eine kleine Leiste unerheblich).
    bool isHourDay(const String& timeLocal, const WeatherClient::Forecast& data) {
        if (data.dailyCount == 0) return true;
        return timeLocal >= data.daily[0].sunrise && timeLocal < data.daily[0].sunset;
    }

    String formatTemp(float t) {
        return String((long)lroundf(t)) + "\xC2\xB0"; // Grad-Zeichen, UTF-8
    }

    // MIT Einheiten-Buchstabe (C/F, Alex' Auftrag, 07.10. - siehe units.h
    // "Globale Einheiten-Einstellung") - bewusst NUR fuer die prominente
    // Haupttemperaturanzeige, NICHT fuer die kompakten Spalten (stuendliche
    // Leiste/Wochenuebersicht): dort ist der Platz zu knapp (z.B. "-12°C"
    // waere 5 Zeichen = 35px in einer nur ~36px breiten Spalte, praktisch
    // ohne Reserve) UND die Einheit ist durch die Hauptanzeige bereits
    // einmal klar - wiederholte Buchstaben in jeder kleinen Spalte waeren
    // nur zusaetzliches Ueberlauf-Risiko ohne echten Zugewinn an Klarheit.
    String formatTempWithUnit(float t) {
        return formatTemp(t) + Units::temperatureLetter();
    }

    // ---- Kleine Icons fuer die kompakte Zeile/Sonnenauf-/-untergang -
    // bewusst OHNE Text-Label (nur Icon + Wert), da Einheiten (%, km/h,
    // mm, Uhrzeit) universell sind und keine Uebersetzung brauchen - laut
    // CLAUDE.md immer ein sicherer Weg, das Breiten-Risiko mehrsprachiger
    // Labels in engen Zeilen von vornherein zu vermeiden. ----
    void drawDroplet(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        tft.fillCircle(cx, (int16_t)(cy + r / 3), r, color);
        tft.fillTriangle(cx, (int16_t)(cy - r), (int16_t)(cx - r), (int16_t)(cy + r / 3),
                          (int16_t)(cx + r), (int16_t)(cy + r / 3), color);
    }

    // Windrichtungs-Pfeil statt Text (Alex' Auftrag, 06.10. - loest das
    // Breiten-Problem von "wind_direction_10m" als Text komplett, ohne auf
    // die Information zu verzichten). "fromDeg" ist die Richtung, AUS der
    // der Wind kommt (API-Konvention) - der Pfeil zeigt bewusst in die
    // Richtung, in die der Wind WEHT (fromDeg+180), dieselbe Konvention
    // wie bei den meisten Wetter-Apps (z.B. Windy). 0 Grad = Norden = nach
    // oben auf dem Bildschirm.
    void drawWindArrow(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, float fromDeg, uint16_t color) {
        float toRad = (fromDeg + 180.0f) * 0.017453292519943295f;
        int16_t tipX = (int16_t)(cx + sinf(toRad) * r);
        int16_t tipY = (int16_t)(cy - cosf(toRad) * r);
        int16_t tailX = (int16_t)(cx - sinf(toRad) * r);
        int16_t tailY = (int16_t)(cy + cosf(toRad) * r);
        tft.drawLine(tailX, tailY, tipX, tipY, color);
        // Pfeilspitze: zwei kurze Linien, je 150 Grad zur Pfeilrichtung versetzt.
        int16_t h = (int16_t)(r * 0.55f);
        float back1 = toRad + 2.6179938780f;
        float back2 = toRad - 2.6179938780f;
        tft.drawLine(tipX, tipY, (int16_t)(tipX + sinf(back1) * h), (int16_t)(tipY - cosf(back1) * h), color);
        tft.drawLine(tipX, tipY, (int16_t)(tipX + sinf(back2) * h), (int16_t)(tipY - cosf(back2) * h), color);
    }

    // Kleines Sonnen-Icon mit Strahlen fuer den UV-Index (Alex' Auftrag,
    // 06.10.) - bewusst weniger/kuerzere Strahlen als das grosse
    // Wetter-Icon (weather_icons.cpp), damit es auf den ersten Blick klar
    // von diesem unterscheidbar bleibt.
    void drawUvIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        tft.fillCircle(cx, cy, (int16_t)(r * 0.55f), color);
        for (int i = 0; i < 6; i++) {
            float ang = i * 1.0471975512f; // 6 Strahlen, 60 Grad Schritte
            int16_t x1 = (int16_t)(cx + cosf(ang) * r * 0.75f);
            int16_t y1 = (int16_t)(cy + sinf(ang) * r * 0.75f);
            int16_t x2 = (int16_t)(cx + cosf(ang) * r * 1.25f);
            int16_t y2 = (int16_t)(cy + sinf(ang) * r * 1.25f);
            tft.drawLine(x1, y1, x2, y2, color);
        }
    }

    // Feste Farben fuer Sonnenaufgang (Sonnensymbol)/-untergang
    // (Mondsichel) - Alex' Auftrag, 07.10.: "nicht die Akzentfarbe des
    // Themes", damit auf den ersten Blick klar ist, welches der beiden
    // Symbole gemeint ist, unabhaengig vom gewaehlten Farbthema. Gleiche
    // literal/status-gebundene Farbkonvention wie bei den AQI-Kategorie-
    // Farben oben (siehe CLAUDE.md Farbschema-Abschnitt). Gelb identisch
    // zum Sonnen-Icon in weather_icons.cpp (color565(255,200,0)) - kann
    // hier nicht als constexpr vorberechnet werden (color565() braucht
    // eine tft-Instanz), deshalb als kleine Hilfsfunktion statt eines von
    // Hand ausgerechneten Hex-Werts (keine Rateversuche bei Farbwerten).
    uint16_t sunriseColor(TFT_eSPI& tft) { return tft.color565(255, 200, 0); }
    uint16_t sunsetColor(TFT_eSPI&) { return TFT_DARKGREY; }

    // ---- Luftqualitaet (Alex' Auftrag, 06.10., siehe CLAUDE.md
    // "Wetter-Screen: Luftqualitaet") - Farbskala/Kategorie fuer den
    // europaeischen AQI, EXAKT nach Alex' vorgegebener Standard-Skala.
    // Literal/status-gebundene Farben (NICHT das UI-Theme) wie bei
    // Flightradars Hoehenfarben/Kp-Index/Rakete-Hold-Fail - tragen eine
    // eigene Sicherheits-/Status-Bedeutung, keine Deko-Farbe (siehe
    // CLAUDE.md Farbschema-Abschnitt). ----
    uint16_t aqiColor(TFT_eSPI& tft, int16_t aqi) {
        if (aqi < 0) return TFT_DARKGREY;
        if (aqi <= 20) return TFT_GREEN;
        if (aqi <= 40) return tft.color565(170, 230, 60);   // hellgruen
        if (aqi <= 60) return TFT_YELLOW;
        if (aqi <= 80) return TFT_ORANGE;
        if (aqi <= 100) return TFT_RED;
        return tft.color565(130, 0, 60);                     // dunkelrot/violett
    }

    StringId aqiCategoryStringId(int16_t aqi) {
        if (aqi <= 20) return StringId::WEATHER_AQI_CAT_GOOD;
        if (aqi <= 40) return StringId::WEATHER_AQI_CAT_MODERATE;
        if (aqi <= 60) return StringId::WEATHER_AQI_CAT_MEDIUM;
        if (aqi <= 80) return StringId::WEATHER_AQI_CAT_POOR;
        if (aqi <= 100) return StringId::WEATHER_AQI_CAT_VERY_POOR;
        return StringId::WEATHER_AQI_CAT_EXTREME;
    }

    // US-EPA-AQI-Farbskala (offizielle Stufen-Grenzen 0-50/51-100/101-150/
    // 151-200/201-300/301-500) - EIGENSTAENDIG neben der europaeischen
    // Skala oben, NICHT deren Stufen-Grenzen wiederverwendet, da die
    // US-Skala andere Grenzen UND eine zusaetzliche Stufe ("Unhealthy for
    // Sensitive Groups") hat, die es in der europaeischen Skala nicht
    // gibt (Alex' Auftrag, 07.10.: "bei Imperial den us_aqi anzeigen" -
    // der europaeischen Farbskala auf den US-Wert anzuwenden waere
    // fachlich falsch gewesen, siehe CLAUDE.md "Globale Einheiten-
    // Einstellung").
    uint16_t usAqiColor(TFT_eSPI& tft, int16_t aqi) {
        if (aqi < 0) return TFT_DARKGREY;
        if (aqi <= 50) return TFT_GREEN;
        if (aqi <= 100) return TFT_YELLOW;
        if (aqi <= 150) return TFT_ORANGE;
        if (aqi <= 200) return TFT_RED;
        if (aqi <= 300) return tft.color565(130, 0, 200); // violett
        return tft.color565(120, 0, 40);                   // dunkelrot
    }

    StringId usAqiCategoryStringId(int16_t aqi) {
        if (aqi <= 50) return StringId::WEATHER_AQI_US_CAT_GOOD;
        if (aqi <= 100) return StringId::WEATHER_AQI_US_CAT_MODERATE;
        if (aqi <= 150) return StringId::WEATHER_AQI_US_CAT_UNHEALTHY_SENSITIVE;
        if (aqi <= 200) return StringId::WEATHER_AQI_US_CAT_UNHEALTHY;
        if (aqi <= 300) return StringId::WEATHER_AQI_US_CAT_VERY_UNHEALTHY;
        return StringId::WEATHER_AQI_US_CAT_HAZARDOUS;
    }

    // Waehlt je nach globaler Einheiten-Einstellung den europaeischen oder
    // amerikanischen AQI samt passender Farbe/Kategorie aus - EIN
    // gemeinsamer Satz, damit drawAqiTile() und die Lauftext-Animation in
    // update() garantiert dieselbe Kombination verwenden.
    int16_t displayedAqi(const WeatherClient::AirQuality& aq) {
        return (Units::current() == Units::System::Metric) ? aq.europeanAqi : aq.usAqi;
    }
    uint16_t displayedAqiColor(TFT_eSPI& tft, const WeatherClient::AirQuality& aq) {
        int16_t v = aq.valid ? displayedAqi(aq) : (int16_t)-1;
        return (Units::current() == Units::System::Metric) ? aqiColor(tft, v) : usAqiColor(tft, v);
    }
    StringId displayedAqiCategoryStringId(const WeatherClient::AirQuality& aq) {
        int16_t v = displayedAqi(aq);
        return (Units::current() == Units::System::Metric) ? aqiCategoryStringId(v) : usAqiCategoryStringId(v);
    }

    // Config::HEADER_HEIGHT statt eines eigenen CHROME_H-Werts (Alex'
    // Auftrag, 06.10. - EINE zentrale Kopfbereich-Hoehe fuer JEDEN Screen,
    // siehe CLAUDE.md "Pflicht: einheitliche grosse Ueberschrift").
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 6);
    constexpr int16_t WEEK_BTN_H = 36;
    // Einziger Button am unteren Rand (Alex' Auftrag, 06.10.: der
    // Wetter-Screen bekommt KEINEN eigenen "Genauere Adresse"-Button mehr,
    // nutzt ausschliesslich die allgemeine Toolbox-Position wie der
    // ISS-Radar - siehe CLAUDE.md "Eine allgemeine Toolbox-Position").
    constexpr int16_t WEEK_BTN_Y = (int16_t)(Config::SCREEN_HEIGHT - WEEK_BTN_H - 8);

    // Luftqualitaets-Kachel (Alex' Auftrag, 06.10., siehe CLAUDE.md
    // "Wetter-Screen: Luftqualitaet") - bodenverankert direkt oberhalb des
    // "Wochenübersicht"-Buttons, wie dieser selbst auch schon bodenverankert
    // ist (WEEK_BTN_Y), statt sich auf die exakte Endposition des
    // vorherigen Inhalts (Sonnenauf-/-untergang) zu verlassen - dadurch
    // bleibt die Kachel immer exakt am unteren Rand des Inhaltsbereichs,
    // unabhaengig davon, ob sich die Hoehe des Inhalts darueber mal
    // aendert. Zwei kleine Abstaende weiter oben im Hauptinhalt wurden
    // dafuer leicht gestrafft (row2Cy+14->+8, stripPercentY+12->+6, je 6px),
    // OHNE dass dadurch irgendein bestehendes Element kleiner/unleserlicher
    // wird oder verdraengt wuerde - reiner Abstands-Feinschliff, der genau
    // den noetigen Platz fuer die neue Kachel schafft (Alex' Vorgabe:
    // "ohne dass andere Elemente verdraengt werden").
    constexpr int16_t AQI_ROW_H = 36;
    constexpr int16_t AQI_ROW_TOP = (int16_t)(WEEK_BTN_Y - 4 - AQI_ROW_H);

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }
    Rect weekBtnRect() { return Rect{10, WEEK_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), WEEK_BTN_H}; }
    Rect aqiTileRect() { return Rect{4, AQI_ROW_TOP, (int16_t)(Config::SCREEN_WIDTH - 8), AQI_ROW_H}; }

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
                                I18n::t(StringId::WEATHER_TITLE), TFT_GREEN);
    }

    void clearContentArea(TFT_eSPI& tft) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(WEEK_BTN_Y - 4 - CHROME_H), TFT_BLACK);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        clearContentArea(tft);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::WEATHER_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2),
                             (int16_t)((CHROME_H + WEEK_BTN_Y) / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    // Laufschrift fuer das Luftqualitaets-Kategoriewort auf der Kachel
    // (Alex' Auftrag, 06.10.: "Laufschrift nutzen, falls eine Zeile nicht
    // in ihre Spaltenbreite passt") - "Extrêmement mauvaise" (FR) passt
    // z.B. NICHT in die verfuegbare Spaltenbreite, andere Sprachen/
    // Kategorien schon - deshalb grundsaetzlich ueber MarqueeLabel statt
    // nur fuer den einen bekannten Problemfall eine Ausnahme zu bauen.
    MarqueeLabel::State aqiCategoryMarquee;

    // Gemeinsame Spalten-Geometrie fuer das Kategoriewort - von
    // drawAqiTile() (Neuzeichnen) UND der Lauftext-Animation in update()
    // gebraucht (beide muessen exakt dieselbe Position/Breite verwenden).
    // Feste Spalte NACH der groesstmoeglichen Zahlenbreite (3-stellig, der
    // europaeische AQI ist theoretisch nach oben offen) - macht die
    // Position unabhaengig von der tatsaechlichen Ziffernzahl.
    constexpr int16_t AQI_NUM_X = 56;      // relativ zu r.x, Beginn der Zahl
    constexpr int16_t AQI_CAT_X = 112;     // relativ zu r.x, Beginn des Kategorieworts
    int16_t aqiCategoryX() { return (int16_t)(aqiTileRect().x + AQI_CAT_X); }
    int16_t aqiCategoryW() { Rect r = aqiTileRect(); return (int16_t)(r.x + r.w - 8 - aqiCategoryX()); }
    int16_t aqiRowCy() { Rect r = aqiTileRect(); return (int16_t)(r.y + r.h / 2); }

    // ---- Luftqualitaets-Kachel (Alex' Auftrag, 06.10., Layout-Fix 07.10.
    // nach Alex' Live-Foto) - bodenverankert (AQI_ROW_TOP/AQI_ROW_H oben),
    // tappbar (siehe handleTap() unten, Mindestgroesse 220x36px, deutlich
    // ueber der 31x31-Vorgabe). Zeigt den europaeischen AQI gross und nach
    // Alex' Standard-Skala farbcodiert, mit "AQI"-Beschriftung davor (Alex'
    // Fund, 07.10.: "niemand weiss, was die 42 da zu suchen hat" - "AQI"
    // ist wie "UV" ein international gebraeuchliches, nicht zu
    // uebersetzendes Kuerzel, siehe UV-Zeile oben im selben Screen), plus
    // das Kategoriewort per Laufschrift (passt nicht in allen Sprachen in
    // die Spalte, siehe aqiCategoryMarquee-Kommentar oben).
    //
    // WICHTIG zur Zahl: NICHT per drawString()+Datum vertikal zentriert,
    // sondern im selben bewaehrten Baseline-Stil wie die
    // Haupttemperaturanzeige oben (setCursor()+print(), siehe CLAUDE.md-
    // Font-Falle) - Alex' Live-Foto zeigte, dass die Zahl bei Size 2 bei
    // automatischer ML_DATUM-Zentrierung in dieser nur 36px hohen Kachel
    // unten ueber den Rahmen hinauslief (die tatsaechliche Glyphenhoehe
    // dieses Fonts bei Size 2 ist offenbar groesser als die ~16-18px, die
    // fuer eine symmetrische Zentrierung angenommen wurden). Mit einer
    // Baseline nahe der Kachel-Unterkante waechst der Text zuverlaessig
    // NACH OBEN in den vorhandenen Platz hinein, exakt wie bei der
    // bereits bewaehrten Haupttemperaturanzeige. ----
    void drawAqiTile(TFT_eSPI& tft, const WeatherClient::AirQuality& aq) {
        Rect r = aqiTileRect();
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, tft.color565(0, (uint8_t)(255 * 0.35f), 0));

        int16_t rowCy = aqiRowCy();
        uint16_t col = displayedAqiColor(tft, aq);

        tft.setTextDatum(ML_DATUM);
        tft.fillCircle((int16_t)(r.x + 10), rowCy, 7, col);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("AQI", (int16_t)(r.x + 22), rowCy);

        // Vertikal zentriert ueber setTextDatum(ML_DATUM) statt eines
        // rohen setCursor()-Versatzes (Alex' Fund, 07.10.: die Zahl sass
        // zu tief, nahe der Box-Unterkante statt mittig) - drawString()
        // mit gesetztem Datum rechnet die Zentrierung korrekt anhand der
        // Font-Metriken aus, unabhaengig von der Grundlinien-Falle unseres
        // Fonts (siehe CLAUDE.md "WICHTIGSTE FALLE"). TRANSPARENTER
        // Hintergrund (nur `col`, kein zweites TFT_BLACK-Argument) statt
        // wie zuvor explizit schwarz hinterlegt (Alex' zweiter Fund,
        // 07.10.: bei zentrierter Position erstreckte sich TFT_eSPIs
        // automatisches Hintergrund-Loeschrechteck fuer Groesse 2 bis in
        // die gerade erst gezeichnete untere Rahmenlinie hinein und riss
        // ein Stueck davon heraus) - unproblematisch, da `drawAqiTile()`
        // die gesamte Kachel bei JEDEM Aufruf ohnehin komplett schwarz
        // vorfuellt (siehe fillRect() oben), ein manuelles Nach-Loeschen
        // ist hier also nie noetig.
        tft.setTextColor(col);
        tft.setTextSize(2);
        tft.drawString(aq.valid ? String(displayedAqi(aq)) : "--", (int16_t)(r.x + AQI_NUM_X), rowCy);
        tft.setTextSize(1);

        if (aq.valid) {
            MarqueeLabel::reset(aqiCategoryMarquee);
            MarqueeLabel::draw(tft, aqiCategoryMarquee, aqiCategoryX(), rowCy, aqiCategoryW(), 16,
                                I18n::t(displayedAqiCategoryStringId(aq)), col, TFT_BLACK);
        }
        tft.setTextDatum(TL_DATUM);
    }

    // ---- Hauptscreen-Inhalt: aktuelles Wetter, kompakte Zeile, 3-Stunden-
    // Vorhersageleiste, Sonnenauf-/-untergang. Jeder Abschnitt rechnet
    // seine Endposition aus der vorherigen aus (kein fest geratenes
    // Layout) - siehe CLAUDE.md "Pflicht: jedes Layout muss auf den
    // Bildschirm passen". ----
    void drawMain(TFT_eSPI& tft, const WeatherClient::Forecast& data) {
        clearContentArea(tft);
        uint16_t accent = TFT_GREEN;
        int16_t y = CONTENT_TOP;

        // Aktuelles Wetter: Icon links, Temperatur + gefuehlte Temperatur rechts.
        int16_t iconCx = 34;
        int16_t iconCy = (int16_t)(y + 24);
        WeatherIcons::draw(tft, iconCx, iconCy, 46, data.weatherCode, data.isDay);

        int16_t textX = 66;
        tft.setTextColor(accent, TFT_BLACK);
        tft.setTextSize(2);
        tft.setCursor(textX, (int16_t)(y + 20));
        tft.print(formatTempWithUnit(data.temperature));
        tft.setTextSize(1);

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.setCursor(textX, (int16_t)(y + 42));
        tft.print(String(I18n::t(StringId::WEATHER_FEELS_LIKE_LABEL)) + formatTempWithUnit(data.apparentTemperature));

        y = (int16_t)(y + 52);

        // Kompakte Werte als 2x2-Raster (Luftfeuchte/Wind oben,
        // Niederschlag/UV-Index unten) statt einer einzigen 3er-Zeile -
        // der Platz dafuer wurde frei, seit der eigene "Genauere
        // Adresse"-Button entfiel (Alex' Auftrag, 06.10.: "den dadurch
        // freiwerdenden Platz bitte fuer den UV-Index nutzen"). Mit 2
        // Spalten statt 3 hat jede Spalte ~110px statt ~73px - reicht
        // bequem auch fuer "23km/h" + Pfeil-Icon.
        int16_t halfW = (int16_t)((Config::SCREEN_WIDTH - 20) / 2);
        int16_t row1Cy = (int16_t)(y + 8);
        int16_t row2Cy = (int16_t)(row1Cy + 20);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        drawDroplet(tft, 17, row1Cy, 6, accent);
        tft.drawString(String(data.humidityPercent) + "%", 28, row1Cy);

        drawWindArrow(tft, (int16_t)(10 + halfW + 8), row1Cy, 8, (float)data.windDirectionDeg, accent);
        tft.drawString(String((long)lroundf(data.windSpeedKmh)) + Units::windSpeedSuffix(),
                        (int16_t)(10 + halfW + 20), row1Cy);

        drawDroplet(tft, 17, row2Cy, 6, tft.color565(70, 150, 255));
        tft.drawString(String(data.precipitationMm, 1) + Units::precipitationSuffix(), 28, row2Cy);

        drawUvIcon(tft, (int16_t)(10 + halfW + 8), row2Cy, 7, tft.color565(255, 160, 0));
        String uvStr = (data.dailyCount > 0)
            ? ("UV " + String(data.daily[0].uvIndexMax, 1))
            : "UV --";
        tft.drawString(uvStr, (int16_t)(10 + halfW + 20), row2Cy);

        tft.setTextDatum(TL_DATUM);
        // Von +14 auf +8 gestrafft (Alex' Auftrag, 06.10. - Platz fuer die
        // neue Luftqualitaets-Kachel schaffen, siehe AQI_ROW_TOP oben).
        y = (int16_t)(row2Cy + 8);

        // "Vorhersage"-Label + 3-Stunden-Leiste.
        tft.setTextColor(accent, TFT_BLACK);
        tft.setCursor(10, (int16_t)(y + 10));
        tft.print(I18n::t(StringId::WEATHER_FORECAST_LABEL));
        y = (int16_t)(y + 16);

        uint8_t stripCols = Config::WEATHER_STRIP_COLUMNS;
        int16_t stripColW = (int16_t)((Config::SCREEN_WIDTH - 20) / stripCols);

        // Ab der aktuellen Stunde starten statt ab Index 0 - "hourly"
        // beginnt bei Mitternacht des aktuellen Tages, nicht bei "jetzt"
        // (live bestaetigt). String-Vergleich funktioniert, da alle
        // Zeit-Strings dasselbe feste Format haben (chronologische
        // Reihenfolge = lexikografische Reihenfolge).
        uint8_t startIdx = 0;
        for (uint8_t i = 0; i < data.hourlyCount; i++) {
            if (data.hourly[i].timeLocal >= data.currentTimeLocal) { startIdx = i; break; }
        }

        int16_t stripHourY = (int16_t)(y + 8);
        int16_t stripIconCy = (int16_t)(y + 24);
        int16_t stripTempY = (int16_t)(y + 40);
        int16_t stripPercentY = (int16_t)(y + 54);

        tft.setTextDatum(MC_DATUM);
        for (uint8_t col = 0; col < stripCols; col++) {
            uint16_t idx = (uint16_t)(startIdx + col * Config::WEATHER_STRIP_STEP_HOURS);
            if (idx >= data.hourlyCount) break;
            const WeatherClient::HourlyPoint& hp = data.hourly[idx];
            int16_t colCx = (int16_t)(10 + stripColW * col + stripColW / 2);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(hp.timeLocal.substring(11, 13), colCx, stripHourY);
            WeatherIcons::draw(tft, colCx, stripIconCy, 20, hp.weatherCode, isHourDay(hp.timeLocal, data));
            tft.drawString(formatTemp(hp.temperature), colCx, stripTempY);
            tft.setTextColor(accent, TFT_BLACK);
            tft.drawString(String(hp.precipProbability) + "%", colCx, stripPercentY);
        }
        tft.setTextDatum(TL_DATUM);

        // Von +12 auf +6 gestrafft (gleicher Grund wie oben).
        y = (int16_t)(stripPercentY + 6);

        // Sonnenaufgang/-untergang (daily[0], erster Eintrag = heute) -
        // Sonnensymbol (gelb) fuer Aufgang, Mondsichel (dunkelgrau) fuer
        // Untergang statt der bisherigen gleichfarbigen Pfeil-Punkte
        // (Alex' Fund, 07.10.: "auf einen Blick erkennbar", feste Farben
        // statt Akzentfarbe - siehe sunriseColor()/sunsetColor() oben).
        // Radius bewusst klein gehalten (5px, bei drawSunIcon() reicht die
        // Strahlenspitze bis r+5=10px ueber den Mittelpunkt hinaus) - der
        // verfuegbare Platz bis zur bodenverankerten AQI-Kachel darunter
        // ist knapp (sunRowCy=222, AQI_ROW_TOP=236, nur 14px Abstand
        // insgesamt) - nachgerechnet, nicht geraten, siehe CLAUDE.md-Fund
        // zur AQI-Zahl weiter oben (dieselbe Fehlerklasse, hier bewusst
        // vermieden statt wiederholt).
        int16_t sunRowCy = (int16_t)(y + 8);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        WeatherIcons::drawSunIcon(tft, 20, sunRowCy, 5, sunriseColor(tft));
        tft.drawString(data.dailyCount > 0 ? timePart(data.daily[0].sunrise) : "--:--", 36, sunRowCy);

        WeatherIcons::drawMoonIcon(tft, (int16_t)(Config::SCREEN_WIDTH / 2 + 10), sunRowCy, 5,
                                    sunsetColor(tft), TFT_BLACK);
        tft.drawString(data.dailyCount > 0 ? timePart(data.daily[0].sunset) : "--:--",
                        (int16_t)(Config::SCREEN_WIDTH / 2 + 26), sunRowCy);
        tft.setTextDatum(TL_DATUM);

        drawAqiTile(tft, data.airQuality);
    }

    // ---- Wochenuebersicht (Alex' Auftrag, 06.10.) - eigener Detail-
    // Screen, pro Tag eine Zeile. 7 Zeilen passen nicht alle gleichzeitig
    // auf den Bildschirm - deshalb scrollbar (Hoch/Runter-Pfeile nur wenn
    // noetig), per TFT_eSPI-Viewport-Clipping statt manueller
    // Pro-Zeile-Sichtbarkeitspruefung (robuster gegen Rundungsfehler). ----
    void showWeeklyScreen(TFT_eSPI& tft, const WeatherClient::Forecast& data) {
        MenuStars::reset();

        constexpr int16_t LIST_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t LIST_BOTTOM = (int16_t)(BACK_Y - 10);
        constexpr int16_t ROW_H = 38;

        int16_t listH = (int16_t)(LIST_BOTTOM - LIST_TOP);
        int16_t totalH = (int16_t)(data.dailyCount * ROW_H);
        int16_t maxScroll = (int16_t)(totalH - listH);
        if (maxScroll < 0) maxScroll = 0;
        bool scrollable = maxScroll > 0;
        int16_t scrollY = 0;
        constexpr int16_t SCROLL_STEP = (int16_t)(ROW_H * 2);

        Rect backBtn = scrollable
            ? Rect{10, BACK_Y, 130, 40}
            : Rect{10, BACK_Y, (int16_t)(Config::SCREEN_WIDTH - 20), 40};
        Rect upBtn   = {146, BACK_Y, 38, 40};
        Rect downBtn = {190, BACK_Y, 38, 40};

        auto drawRows = [&]() {
            tft.setViewport(0, LIST_TOP, Config::SCREEN_WIDTH, listH);
            tft.fillRect(0, 0, Config::SCREEN_WIDTH, listH, TFT_BLACK);
            uint16_t accent = TFT_GREEN;

            for (uint8_t i = 0; i < data.dailyCount; i++) {
                int16_t rowY = (int16_t)(i * ROW_H - scrollY);
                if (rowY + ROW_H < 0 || rowY > listH) continue;
                int16_t rowCy = (int16_t)(rowY + ROW_H / 2);
                const WeatherClient::DailyPoint& d = data.daily[i];

                // Spalten-x-Positionen bewusst mit Reserve bemessen (Alex'
                // Auftrag, 06.10.: UV-Index als dritter Wert dazu) - auch
                // im seltenen Extremfall zweistelliger Minustemperaturen
                // auf beiden Seiten ("-40/-40°") bleibt noch Abstand zur
                // naechsten Spalte.
                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(accent, TFT_BLACK);
                tft.drawString(I18n::t(weekdayStringId(d.dateLocal)), 8, rowCy);

                WeatherIcons::draw(tft, 44, rowCy, 20, d.weatherCode, true);

                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString(formatTemp(d.tempMax) + "/" + formatTemp(d.tempMin), 60, rowCy);

                // UV-Index - "UV" ist in allen 4 Sprachen dieselbe, nicht
                // uebersetzte Abkuerzung (wie "WLAN"/"WiFi" anderswo im
                // Projekt), braucht deshalb keine eigene StringId.
                tft.drawString("UV " + String(d.uvIndexMax, 1), 150, rowCy);

                tft.setTextDatum(MR_DATUM);
                tft.setTextColor(accent, TFT_BLACK);
                tft.drawString(String(d.precipProbabilityMax) + "%", (int16_t)(Config::SCREEN_WIDTH - 8), rowCy);
                tft.setTextDatum(TL_DATUM);

                tft.drawFastHLine(10, (int16_t)(rowY + ROW_H - 1), (int16_t)(Config::SCREEN_WIDTH - 20),
                                   tft.color565(0, (uint8_t)(255 * 0.35f), 0));
            }
            tft.resetViewport();
        };

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::WEATHER_WEEKLY_TITLE));

            drawRows();

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

    // ---- Luftqualitaets-Detail-Screen (Alex' Auftrag, 06.10.) -
    // scrollbar, gleiches Grundmuster wie rocket_screen.cpp/f1_screen.cpp
    // (drawContent(scrollY, draw)-Lambda: erst vermessen, dann zeichnen).
    // "\xC2\xB5g/m\xC2\xB3" ist die UTF-8-Byte-Sequenz fuer "µg/m³" -
    // gleiches Vorgehen wie formatTemp()'s "\xC2\xB0" fuer das Grad-
    // Zeichen oben, bewusst nicht uebersetzt (Einheit, kein Wort, wie
    // "km/h"/"mm" anderswo im Projekt). ----
    void showAirQualityScreen(TFT_eSPI& tft, const WeatherClient::AirQuality& aq) {
        MenuStars::reset();

        constexpr int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t LINE_H = 16;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 10);
        const String unit = "\xC2\xB5g/m\xC2\xB3";

        auto drawContent = [&](int16_t scrollY, bool draw) -> int16_t {
            int16_t y = VIEW_TOP;
            int16_t vTop = draw ? VIEW_TOP : -32000;
            int16_t vBottom = draw ? VIEW_BOTTOM : 32000;

            if (!aq.valid) {
                if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H,
                    I18n::t(StringId::WEATHER_AQI_UNAVAILABLE), scrollY, vTop, vBottom, draw);
                return y;
            }

            if (draw) tft.setTextColor(TFT_GREEN, TFT_BLACK);
            y = layoutWrapped(tft, 10, y, maxW, LINE_H,
                String(I18n::t(StringId::WEATHER_AQI_EU_LABEL)) + String(aq.europeanAqi) +
                " (" + I18n::t(aqiCategoryStringId(aq.europeanAqi)) + ")", scrollY, vTop, vBottom, draw);
            y = layoutWrapped(tft, 10, (int16_t)(y + 2), maxW, LINE_H,
                String(I18n::t(StringId::WEATHER_AQI_US_LABEL)) + (aq.usAqi >= 0 ? String(aq.usAqi) : "--"),
                scrollY, vTop, vBottom, draw);
            y += 8;

            // Teilindex + Rohmesswert + Kurzerklaerung je Schadstoff
            // (Alex' Auftrag: "jeweils den Teil-Index UND den rohen
            // Messwert" sowie "kurze, laienverstaendliche Erklaerung").
            auto pollutant = [&](StringId label, StringId explain, int16_t partIdx, float raw) {
                if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, I18n::t(label), scrollY, vTop, vBottom, draw);
                String line = (partIdx >= 0
                    ? (String(I18n::t(StringId::WEATHER_AQI_INDEX_LABEL)) + String(partIdx) + "   ")
                    : String()) + String(raw, 1) + " " + unit;
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, line, scrollY, vTop, vBottom, draw);
                if (draw) tft.setTextColor(tft.color565(0, (uint8_t)(255 * 0.7f), 0), TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, LINE_H, I18n::t(explain), scrollY, vTop, vBottom, draw);
                y += 8;
            };

            pollutant(StringId::WEATHER_AQI_PM25_LABEL, StringId::WEATHER_AQI_PM25_EXPLAIN, aq.aqiPm25, aq.pm25);
            pollutant(StringId::WEATHER_AQI_PM10_LABEL, StringId::WEATHER_AQI_PM10_EXPLAIN, aq.aqiPm10, aq.pm10);
            pollutant(StringId::WEATHER_AQI_NO2_LABEL, StringId::WEATHER_AQI_NO2_EXPLAIN, aq.aqiNo2, aq.nitrogenDioxide);
            pollutant(StringId::WEATHER_AQI_O3_LABEL, StringId::WEATHER_AQI_O3_EXPLAIN, aq.aqiO3, aq.ozone);
            pollutant(StringId::WEATHER_AQI_SO2_LABEL, StringId::WEATHER_AQI_SO2_EXPLAIN, aq.aqiSo2, aq.sulphurDioxide);

            // Kohlenmonoxid/Feinstaub(Sand)/Ammoniak - nur Rohmesswerte,
            // keine eigene EAQI-Teilindex-Berechnung von Open-Meteo
            // geliefert (Alex' Auftrag: "als rohe Messwerte zusaetzlich
            // auflisten").
            if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
            y = layoutWrapped(tft, 10, y, maxW, LINE_H,
                String(I18n::t(StringId::WEATHER_AQI_CO_LABEL)) + String(aq.carbonMonoxide, 1) + " " + unit,
                scrollY, vTop, vBottom, draw);
            y = layoutWrapped(tft, 10, y, maxW, LINE_H,
                String(I18n::t(StringId::WEATHER_AQI_DUST_LABEL)) + String(aq.dust, 1) + " " + unit,
                scrollY, vTop, vBottom, draw);
            y = layoutWrapped(tft, 10, y, maxW, LINE_H,
                String(I18n::t(StringId::WEATHER_AQI_AMMONIA_LABEL)) + String(aq.ammonia, 1) + " " + unit,
                scrollY, vTop, vBottom, draw);

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
            tft.println(I18n::t(StringId::WEATHER_AQI_DETAIL_TITLE));

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
                                   I18n::t(StringId::WEATHER_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::WEATHER_INFO_BODY));

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

    // Nutzt AUSSCHLIESSLICH die allgemeine Toolbox-Position (Alex'
    // Auftrag, 06.10. - KEIN eigener Standort-Override mehr, siehe
    // CLAUDE.md "Eine allgemeine Toolbox-Position") - 1:1 dasselbe Muster
    // wie iss_radar_screen.cpp::enter(). Aendert sich die Position ueber
    // die neue Einstellungsseite (settings_tools_screen.cpp), wird das
    // beim naechsten Betreten dieses Screens automatisch uebernommen,
    // ohne dass hier etwas angepasst werden muss.
    bool resolveAndSetLocation() {
        if (LocationPresets::count() == 0) return false;
        int8_t active = LocationPresets::activeIndex();
        uint8_t idx = (active >= 0) ? (uint8_t)active : 0;
        double lat = 0, lon = 0;
        LocationPresets::getLatLon(idx, lat, lon);
        NetTask::setWeatherLocation(lat, lon);
        return true;
    }

    bool hasLocation = false;
    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    WeatherClient::Forecast lastData;

    void redraw(TFT_eSPI& tft) {
        drawChrome(tft);
        drawButton(tft, weekBtnRect(), I18n::t(StringId::WEATHER_WEEK_OVERVIEW_BTN));

        if (!hasLocation) {
            drawCenteredHint(tft, StringId::ISS_RADAR_NO_LOCATION);
        } else if (haveData) {
            drawMain(tft, lastData);
        } else {
            drawCenteredHint(tft, StringId::WEATHER_LOADING);
        }
    }
}

void enter(TFT_eSPI& tft) {
    hasLocation = resolveAndSetLocation();
    redraw(tft);
}

void update(TFT_eSPI& tft) {
    if (!hasLocation) return;

    uint32_t v = NetTask::weatherVersion();
    if (v != lastSeenVersion) {
        WeatherClient::Forecast newData;
        if (NetTask::getWeatherData(newData)) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawMain(tft, lastData);
        }
    } else {
        NetTask::Status st = NetTask::weatherStatus();
        if (!haveData && st != lastSeenStatus) {
            lastSeenStatus = st;
            if (st == NetTask::Status::Error) {
                drawCenteredHint(tft, StringId::WEATHER_FETCH_ERROR_NETWORK);
            }
        }
    }

    // Lauftext-Animation fuer das AQI-Kategoriewort - MUSS jeden
    // Durchlauf aufgerufen werden, nicht nur bei neuen Daten (siehe
    // CLAUDE.md "Pflicht: automatisches Scrollen bei zu langem Text").
    if (haveData && lastData.airQuality.valid) {
        uint16_t col = displayedAqiColor(tft, lastData.airQuality);
        MarqueeLabel::draw(tft, aqiCategoryMarquee, aqiCategoryX(), aqiRowCy(), aqiCategoryW(), 16,
                            I18n::t(displayedAqiCategoryStringId(lastData.airQuality)), col, TFT_BLACK);
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        redraw(tft);
        return;
    }

    if (haveData && weekBtnRect().contains(x, y)) {
        showWeeklyScreen(tft, lastData);
        MenuStars::reset();
        redraw(tft);
        return;
    }

    if (haveData && aqiTileRect().contains(x, y)) {
        showAirQualityScreen(tft, lastData.airQuality);
        MenuStars::reset();
        redraw(tft);
        return;
    }
}

}
