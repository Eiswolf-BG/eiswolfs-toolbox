#include "iss_radar_screen.h"
#include <math.h>
#include "touch_input.h"
#include "menu_stars.h"
#include "ui_header.h"
#include "world_map.h"
#include "config.h"
#include "i18n.h"
#include "iss_client.h"
#include "net_task.h"
#include "location_presets.h"
#include "units.h"
#include "led_control.h"
#include "led_notifications.h"

namespace IssRadarScreen {

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

    // Wortweiser Umbruch anhand echter Pixelbreite, jede Zeile einzeln
    // geprueft (siehe CLAUDE.md "Pflichtpruefung Textbreite") - gleiches
    // Muster wie in first_run_welcome_screen.cpp etc., hier lokal
    // dupliziert statt geteilt (Projekt-Konvention, siehe CLAUDE.md).
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

    // Wie layoutWrapped() oben (identische wortweise Umbruch-Logik, jede
    // Zeile einzeln auf echte Pixelbreite geprueft - CLAUDE.md-Pflicht),
    // zeichnet aber Label und Wert in getrennten Farben (Alex' Auftrag,
    // 07.10.: "klarer Kontrast zwischen fester Bezeichnung und Live-
    // Wert") - label+value werden intern zu EINEM String verkettet und
    // genau wie bisher umgebrochen, beim Zeichnen jeder resultierenden
    // Zeile wird aber ermittelt, welcher Anteil noch zum Label gehoert
    // (faellt in der Praxis immer auf die erste Zeile, da die Labels
    // kurz sind - die Faller-Unterscheidung deckt aber auch den
    // theoretischen Fall ab, dass ein besonders langer Wert selbst
    // umbricht, ohne dafuer geraten zu muessen).
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

    // Wie layoutWrapped(), aber zentriert und als Textblock vertikal um
    // cy herum ausgerichtet - fuer kurze Hinweistexte innerhalb des
    // Radarkreises (z.B. "ISS aktuell unter dem Horizont"). MAX_LINES ist
    // bewusst grosszuegig bemessen (die beiden tatsaechlichen Aufrufer
    // sind kurze Hinweissaetze) - jede erzeugte Zeile wird trotzdem
    // einzeln auf ihre tatsaechliche Pixelbreite geprueft, kein fixes
    // Zeilenlimit ohne erneute Breitenpruefung.
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

    // Config::HEADER_HEIGHT statt eines eigenen CHROME_H-Werts (Alex'
    // Auftrag, 06.10. - EINE zentrale Kopfbereich-Hoehe fuer JEDEN Screen,
    // siehe CLAUDE.md "Pflicht: einheitliche grosse Ueberschrift"). Gross
    // genug fuer den "?"-Button (31x31, y=4) UND die auf UiHeader::
    // TEXT_SIZE vergroesserte Ueberschrift.
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t RADAR_CX = 120;
    // Kreis etwas kleiner (Alex' Auftrag, 06.10.: Platz fuer die groessere
    // Ueberschrift UND die jetzt nach innen gewanderten Kompass-
    // Buchstaben) - Mittelpunkt/Groesse neu berechnet, siehe Kommentar bei
    // TEXT_START_Y unten fuer das volle Hoehenbudget.
    constexpr int16_t RADAR_RADIUS = 65;
    constexpr int16_t RADAR_CY = (int16_t)(CHROME_H + 12 + RADAR_RADIUS);
    // 7 Datenzeilen statt vorher 6 (neue "Status:"-Zeile, siehe
    // drawDataLines()) - Start-Y aus dem Kreis-Ende + fester Abstand
    // berechnet, nicht mehr als Magic Number (CLAUDE.md "Pflicht: jedes
    // Layout muss auf den Bildschirm passen"). Abstand von 14 auf 24
    // erhoeht (Alex' Auftrag, 06.10.: "Schrift unterm Kreis 10px weiter
    // runter") - 7 Zeilen enden damit bei y=304, passt noch bequem vor
    // den unteren Bildschirmrand (320).
    constexpr int16_t TEXT_START_Y = (int16_t)(RADAR_CY + RADAR_RADIUS + 24);
    constexpr int16_t TEXT_LINE_H = 14;

    // 31x31 (26+5) - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen (Alex'
    // Auftrag, 06.10., "Finger-only"-Regel), "?"-Buttons systemweit
    // vergroessert.
    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    // Zentrierung des Titels auf der Mitte der vom "?"-Button FREIEN Zone,
    // nicht auf der wahren Bildschirmmitte (Alex' Fund, 06.10.: ein
    // Titel wie "Raketenstart" wurde bei echter Mittel-Zentrierung
    // faelschlich auf Groesse 1 zurueckgestuft, obwohl bei Zentrierung
    // auf die freie Zone noch 197px statt nur 160px Platz waeren - der
    // 7px-Monospace-Font braucht dafuer exakt 168px bei Groesse 2).
    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    // Alex' Flimmer-Fund, 07.10. (siehe CLAUDE.md "ISS-Radar: Flimmer-
    // Fix"): Kreis/Weltkarte/Kompass werden jetzt NUR NOCH einmal
    // gezeichnet (siehe drawStaticFrame()/ensureStaticFrame() unten),
    // nicht mehr bei jeder 12-Sekunden-Positionsaktualisierung.
    // haveStaticFrame haelt fest, ob dieser statische Rahmen aktuell
    // tatsaechlich auf dem Bildschirm steht - JEDE Stelle, die den
    // Bildschirm (teilweise) loescht, OHNE den Rahmen selbst gleich
    // wieder zu zeichnen (drawChrome()'s fillScreen(), drawCenteredHint()'s
    // clearContentArea()), MUSS dieses Flag zuruecksetzen, sonst wuerde
    // ensureStaticFrame() faelschlich annehmen, der Rahmen stehe noch.
    bool haveStaticFrame = false;
    // frameLat/frameLon = Standort, fuer den der aktuell sichtbare
    // statische Rahmen gezeichnet wurde (fuer den Standortwechsel-Edge-
    // Case in update() unten); lastDotVisible/lastDotPx/lastDotPy =
    // letzte gezeichnete ISS-Punkt-Position, fuer das gezielte
    // Wegradieren beim naechsten Positionswechsel (siehe updateIssDot()).
    double frameLat = 0.0, frameLon = 0.0;
    bool lastDotVisible = false;
    int16_t lastDotPx = 0, lastDotPy = 0;

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        haveStaticFrame = false;
        drawButton(tft, infoBtnRect(), "?");

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.).
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::ISS_RADAR_TITLE), TFT_GREEN);
    }

    // Leert nur den Inhaltsbereich unterhalb der Kopfzeile (Radar + Text) -
    // Info-Button und Titel bleiben stehen, damit sie bei jeder
    // Aktualisierung (alle paar Sekunden) nicht unnoetig neu gezeichnet
    // werden muessen.
    void clearContentArea(TFT_eSPI& tft) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
        haveStaticFrame = false;
    }

    constexpr double DEG2RAD = 0.017453292519943295;
    constexpr double PI_D = 3.14159265358979323846;

    // Gepunktete Weltkarte im Kreis (Alex' Auftrag, 06.10. - nach dem
    // Vorbild von Eiswolfs Flightradar CYDs Radar-Screen, siehe
    // world_map.h fuer die uebernommenen Daten). WICHTIG, anders als dort:
    // Flightradars drawWorldMap() quetscht die Rohdaten nur FLACH
    // (Equirektangular-Raster 1:1 auf das Kreis-Bounding-Box skaliert,
    // unabhaengig vom Beobachterstandort) in den Kreis - das wuerde NICHT
    // zur hier bereits bestehenden Azimut/Elevation-Darstellung passen
    // (Mitte=Zenit, Rand=Horizont). Diese Version projiziert deshalb
    // stattdessen jeden Landpunkt per echter Azimutal-Projektion UM DEN
    // BEOBACHTERSTANDORT (obsLat/obsLon): fuer jeden Punkt werden
    // Grosskreis-Distanz c und Peilung theta vom Beobachter aus berechnet
    // (Standardformeln der Kugel-Trigonometrie), dann genau wie der
    // ISS-Punkt selbst auf (px,py) abgebildet - nur dass der Radius hier
    // nicht aus der Elevation, sondern aus c/PI*RADAR_RADIUS kommt
    // (c=0 am Beobachter, c=PI am Antipodenpunkt = Kreisrand). Rotiert und
    // rezentriert sich dadurch automatisch, wenn sich die allgemeine
    // Toolbox-Position aendert (siehe "Eine allgemeine Toolbox-Position"
    // in CLAUDE.md) - KEIN fest verdrahteter Bezug auf echte Distanzen
    // (die Horizont-Entfernung eines Himmelsobjekts haengt von dessen
    // Hoehe ab, nicht von einer festen Erdoberflaechen-Distanz), sondern
    // bewusst eine dekorative/geografische Orientierungshilfe, kein
    // physikalisch exaktes zweites Koordinatensystem.
    void drawWorldMap(TFT_eSPI& tft, double obsLat, double obsLon) {
        uint16_t dim = tft.color565(0, (uint8_t)(255 * 0.4f), 0);
        double obsLatRad = obsLat * DEG2RAD;
        double obsLonRad = obsLon * DEG2RAD;

        for (uint8_t row = 0; row < WorldMap::GRID_H; row++) {
            uint64_t bits = WorldMap::ROWS[row];
            if (bits == 0) continue;
            double lat = 90.0 - ((double)row + 0.5) * (180.0 / WorldMap::GRID_H);
            double latRad = lat * DEG2RAD;

            for (uint8_t col = 0; col < WorldMap::GRID_W; col++) {
                if (!(bits & (1ULL << (WorldMap::GRID_W - 1 - col)))) continue;
                double lon = -180.0 + ((double)col + 0.5) * (360.0 / WorldMap::GRID_W);
                double dLon = lon * DEG2RAD - obsLonRad;

                double cosC = sin(obsLatRad) * sin(latRad) + cos(obsLatRad) * cos(latRad) * cos(dLon);
                if (cosC > 1.0) cosC = 1.0;
                else if (cosC < -1.0) cosC = -1.0;
                double c = acos(cosC);
                double theta = atan2(sin(dLon) * cos(latRad),
                                      cos(obsLatRad) * sin(latRad) - sin(obsLatRad) * cos(latRad) * cos(dLon));

                double r = (c / PI_D) * RADAR_RADIUS;
                if (r > (double)(RADAR_RADIUS - 2)) continue;

                int16_t px = (int16_t)(RADAR_CX + r * sin(theta));
                int16_t py = (int16_t)(RADAR_CY - r * cos(theta));
                tft.drawPixel(px, py, dim);
            }
        }
    }

    // Kompass-Buchstaben (N/S/O/W) am Kreisrand entfernt (Alex' Auftrag,
    // 07.10.) - der Kreis zeigt jetzt nur noch Umriss + gepunktete
    // Weltkarte + Mittelpunkt.
    void drawRadarFrame(TFT_eSPI& tft, double obsLat, double obsLon) {
        uint16_t accent = TFT_GREEN;
        tft.drawCircle(RADAR_CX, RADAR_CY, RADAR_RADIUS, accent);
        drawWorldMap(tft, obsLat, obsLon);
        tft.fillCircle(RADAR_CX, RADAR_CY, 2, accent);
    }

    StringId compassAbbrevId(double azimuthDeg) {
        static const StringId IDS[8] = {
            StringId::COMPASS_N, StringId::COMPASS_NE, StringId::COMPASS_E, StringId::COMPASS_SE,
            StringId::COMPASS_S, StringId::COMPASS_SW, StringId::COMPASS_W, StringId::COMPASS_NW,
        };
        int idx = ((int)lround(azimuthDeg / 45.0)) % 8;
        if (idx < 0) idx += 8;
        return IDS[idx];
    }

    StringId visibilityTextId(const String& raw) {
        if (raw == "daylight") return StringId::ISS_RADAR_VISIBILITY_DAYLIGHT;
        if (raw == "eclipsed") return StringId::ISS_RADAR_VISIBILITY_ECLIPSED;
        return StringId::ISS_RADAR_VISIBILITY_UNKNOWN;
    }

    // Label in Akzentfarbe, Wert in Weiss (Alex' Auftrag, 07.10.: "klarer
    // Kontrast zwischen fester Bezeichnung und Live-Wert" - gilt ab jetzt
    // als Standardregel fuer alle Screens, siehe CLAUDE.md "Pflicht:
    // Label/Wert-Farbkontrast").
    void drawDataLines(TFT_eSPI& tft, const IssClient::Position& pos, const IssClient::LookAngles& look) {
        uint16_t accent = TFT_GREEN;
        int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        int16_t y = TEXT_START_Y;

        // Neue Status-Zeile (Alex' Auftrag, 06.10.) - ersetzt den
        // bisherigen Hinweistext MITTEN IM KREIS, der jetzt komplett
        // entfernt ist (der Kreis zeigt bei unsichtbarer ISS nur noch die
        // gepunktete Weltkarte, seit 07.10. ohne Kompass-Buchstaben).
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_STATUS_LABEL),
            I18n::t(look.elevationDeg > 0.0 ? StringId::ISS_RADAR_STATUS_VISIBLE
                                             : StringId::ISS_RADAR_STATUS_BELOW_HORIZON),
            accent, TFT_WHITE);
        // Globale Einheiten-Einstellung beachten (Alex' Auftrag, 07.10. -
        // CLAUDE.md "Pflicht: globale Einheiten-Einstellung beachten"):
        // rangeKm/altitudeKm/velocityKmh kommen aus der eigenen
        // Bahnmechanik-Berechnung (iss_client.cpp), NICHT von einer API
        // mit Einheiten-Parameter wie bei Open-Meteo - deshalb hier lokal
        // ueber Units::distanceKmToDisplay()/speedKmhToDisplay() plus
        // passendem Suffix umgerechnet statt die Rohwerte fest in km
        // anzuzeigen.
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_DISTANCE_LABEL),
            String(Units::distanceKmToDisplay(look.rangeKm), 0) + " " + Units::distanceSuffix(), accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_ALTITUDE_LABEL),
            String(Units::distanceKmToDisplay(pos.altitudeKm), 0) + " " + Units::distanceSuffix(), accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_VELOCITY_LABEL),
            String(Units::speedKmhToDisplay(pos.velocityKmh), 0) + " " + Units::windSpeedSuffix(), accent, TFT_WHITE);
        // Auf 0-359 normalisiert anzeigen - ungerundet wuerde z.B. 359.6°
        // beim Runden auf 0 Nachkommastellen faelschlich als "360°"
        // erscheinen statt als "0°" (numerisch per Testrechnung
        // bestaetigt, siehe CLAUDE.md "ISS-Radar").
        long azRounded = ((lround(look.azimuthDeg) % 360) + 360) % 360;
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_AZIMUTH_LABEL),
            String(azRounded) + "\xC2\xB0 " + I18n::t(compassAbbrevId(look.azimuthDeg)), accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_ELEVATION_LABEL), String(look.elevationDeg, 1) + "\xC2\xB0", accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ISS_RADAR_VISIBILITY_LABEL), I18n::t(visibilityTextId(pos.visibility)), accent, TFT_WHITE);
    }

    // ---- Flimmer-Fix (Alex' Auftrag, 07.10., siehe CLAUDE.md "ISS-Radar:
    // Flimmer-Fix") - Kreis/Weltkarte/Kompass sind komplett statisch
    // (haengen nur vom Beobachterstandort ab, NICHT von der ISS-Position)
    // und werden deshalb nur noch EINMAL gezeichnet, nicht mehr bei jeder
    // 12-Sekunden-Positionsaktualisierung. ----

    // Zeichnet den statischen Rahmen frisch (voller Loeschvorgang davor) -
    // NUR aufgerufen, wenn er noch nie stand ODER sich der Beobachter-
    // standort geaendert hat (siehe ensureStaticFrame() unten). Setzt
    // lastDotVisible zurueck, da die Leinwand jetzt frisch ist - es gibt
    // keinen "alten Punkt" mehr zum gezielten Wegradieren, der naechste
    // updateIssDot()-Aufruf zeichnet den Punkt deshalb einfach direkt neu.
    void drawStaticFrame(TFT_eSPI& tft, double obsLatArg, double obsLonArg) {
        clearContentArea(tft);
        drawRadarFrame(tft, obsLatArg, obsLonArg);
        frameLat = obsLatArg;
        frameLon = obsLonArg;
        haveStaticFrame = true;
        lastDotVisible = false;
    }

    // Zeichnet den Rahmen NUR neu, wenn er noch nicht steht oder sich der
    // Standort seit dem letzten Zeichnen geaendert hat (z.B. ueber
    // Einstellungen -> Tools -> "Adresse ändern", waehrend der Screen
    // zwischenzeitlich verlassen war - siehe enter()/update() unten fuer
    // die beiden Stellen, die das abdecken).
    void ensureStaticFrame(TFT_eSPI& tft, double obsLatArg, double obsLonArg) {
        if (!haveStaticFrame || frameLat != obsLatArg || frameLon != obsLonArg) {
            drawStaticFrame(tft, obsLatArg, obsLonArg);
        }
    }

    // Loescht NUR den Textbereich unterhalb des Kreises (7 Datenzeilen) -
    // 12px Reserve ueber TEXT_START_Y fuer den Ascent der ersten Zeile
    // (unser Font waechst von y aus nach OBEN, siehe CLAUDE.md-Font-Falle),
    // bleibt dabei klar unterhalb des Kreis-Endes (RADAR_CY+RADAR_RADIUS),
    // ruehrt den Kreis/die Karte also nicht an.
    constexpr int16_t DATA_AREA_TOP = (int16_t)(TEXT_START_Y - 12);
    void clearDataArea(TFT_eSPI& tft) {
        tft.fillRect(0, DATA_AREA_TOP, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - DATA_AREA_TOP), TFT_BLACK);
    }

    void updateDataLines(TFT_eSPI& tft, const IssClient::Position& pos, const IssClient::LookAngles& look) {
        clearDataArea(tft);
        drawDataLines(tft, pos, look);
    }

    // Bewegt den ISS-Punkt + "ISS"-Beschriftung, OHNE den Kreis/die Karte
    // komplett neu zu zeichnen: die ALTE Punktposition (falls vorhanden
    // und sichtbar) wird zuerst gezielt "repariert" - ein kleines, per
    // tft.setViewport() geklipptes Rechteck um die alte Position wird
    // schwarz geleert und darin NUR drawRadarFrame() erneut aufgerufen
    // (vpDatum=false, Koordinaten bleiben absolut, TFT_eSPI verwirft
    // automatisch alles ausserhalb des kleinen Ausschnitts) - das stellt
    // Kreisbogen/Kartenpunkte/Kompass-Buchstaben an dieser Stelle korrekt
    // wieder her, ohne ein schwarzes Loch oder eine Punktspur zu
    // hinterlassen. Erst DANACH wird der neue Punkt an seiner neuen
    // Position gesetzt.
    void updateIssDot(TFT_eSPI& tft, const IssClient::LookAngles& look, double obsLatArg, double obsLonArg) {
        bool nowVisible = look.elevationDeg > 0.0;
        int16_t newPx = 0, newPy = 0;
        if (nowVisible) {
            double clampedElev = look.elevationDeg;
            if (clampedElev > 90.0) clampedElev = 90.0;
            double radius = RADAR_RADIUS * (1.0 - clampedElev / 90.0);
            double azRad = look.azimuthDeg * DEG2RAD;
            newPx = (int16_t)(RADAR_CX + radius * sin(azRad));
            newPy = (int16_t)(RADAR_CY - radius * cos(azRad));
        }

        bool positionChanged = !nowVisible || !lastDotVisible || newPx != lastDotPx || newPy != lastDotPy;
        if (lastDotVisible && positionChanged) {
            // Patch bewusst asymmetrisch (mehr Platz rechts) - die "ISS"-
            // Beschriftung reicht vom Punkt aus nur nach rechts (px+6 bis
            // ca. px+27 bei 3 Zeichen Breite), nicht nach links.
            constexpr int16_t PATCH_LEFT = 10, PATCH_RIGHT = 36, PATCH_VERT = 12;
            int16_t vx = (int16_t)(lastDotPx - PATCH_LEFT);
            int16_t vy = (int16_t)(lastDotPy - PATCH_VERT);
            int16_t vw = (int16_t)(PATCH_LEFT + PATCH_RIGHT);
            int16_t vh = (int16_t)(PATCH_VERT * 2);
            tft.setViewport(vx, vy, vw, vh, false);
            tft.fillRect(vx, vy, vw, vh, TFT_BLACK);
            drawRadarFrame(tft, obsLatArg, obsLonArg);
            tft.resetViewport();
        }

        if (nowVisible && positionChanged) {
            uint16_t accent = TFT_GREEN;
            tft.fillCircle(newPx, newPy, 4, accent);

            // "ISS"-Beschriftung neben dem Punkt (Alex' Auftrag, 06.10.) -
            // bewusst NICHT uebersetzt, wie schon beim Bildschirmtitel
            // ("Radar ISS" auch auf Spanisch/Franzoesisch), international
            // gebraeuchliche Abkuerzung.
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(accent, TFT_BLACK);
            tft.drawString("ISS", (int16_t)(newPx + 6), newPy);
            tft.setTextDatum(TL_DATUM);
        }

        lastDotVisible = nowVisible;
        lastDotPx = newPx;
        lastDotPy = newPy;
    }

    // Wird bei jeder erfolgreichen Positions-Aktualisierung aufgerufen -
    // zeichnet jetzt NUR NOCH die sich tatsaechlich aendernden Teile
    // (Punkt + Textzeilen), der statische Rahmen wird hoechstens bei
    // Standortwechsel mitgezeichnet (siehe ensureStaticFrame()).
    void drawRadarAndData(TFT_eSPI& tft, const IssClient::Position& pos, const IssClient::LookAngles& look,
                          double obsLat, double obsLon) {
        ensureStaticFrame(tft, obsLat, obsLon);
        updateIssDot(tft, look, obsLat, obsLon);
        updateDataLines(tft, pos, look);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        clearContentArea(tft);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::ISS_RADAR_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, RADAR_CX, RADAR_CY, (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::ISS_RADAR_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::ISS_RADAR_INFO_BODY));

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(I18n::t(StringId::BACK_BTN), (int16_t)(Config::SCREEN_WIDTH / 2),
                        (int16_t)(Config::SCREEN_HEIGHT - 20));
        tft.setTextDatum(TL_DATUM);

        // Modaler Erklaer-Screen, schliesst sich bei jedem Tap - blockiert
        // bewusst kurz (siehe iss_radar_screen.h), waehrenddessen laufen
        // keine Wisch-Gesten der zentralen Navigation (app_navigation.cpp).
        while (true) {
            TouchInput::Point tap;
            if (TouchInput::wasTapped(tap)) return;
            MenuStars::update(tft);
            delay(20);
        }
    }

    // Persistiert ueber mehrere enter()/leave()-Zyklen hinweg (z.B.
    // Hin- und Herwischen zu den Einstellungen) - so zeigt ein erneutes
    // Betreten sofort die zuletzt bekannten Daten, ohne selbst einen
    // Netzwerk-Abruf auszuloesen (das macht jetzt ausschliesslich
    // NetTask auf Core 0, siehe net_task.h).
    bool hasLocation = false;
    double obsLat = 0.0, obsLon = 0.0;
    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    IssClient::Position lastPos;
    IssClient::LookAngles lastLook;

    // Rotes LED-Blinken genau dann, wenn dieser Screen aktiv gezeigt wird
    // UND die ISS laut aktuellen Daten gerade ueber dem Horizont ist (Alex'
    // Auftrag, 12.10.) - dieselbe Sichtbarkeits-Bedingung wie der bereits
    // bestehende "Sichtbar"/"Nicht sichtbar"-Statustext und der ISS-Punkt
    // auf dem Radar (look.elevationDeg > 0.0, siehe drawDataLines()/
    // updateIssDot()). Das Verlassen des Screens schaltet die LED bereits
    // zentral in app_navigation.cpp::enterFeature()/enterSettings() aus -
    // hier nur noch dafuer sorgen, dass sie bei Sichtbarkeit (wieder)
    // angeht.
    void applyLedForVisibility() {
        bool visible = haveData && lastLook.elevationDeg > 0.0;
        // Nur blinken, wenn ZUSAETZLICH sowohl der Haupt-Toggle als auch
        // der ISS-Einzel-Toggle in den LED-Einstellungen an sind (Alex'
        // Auftrag, 12.10., siehe led_notifications.h).
        if (visible && LedNotifications::shouldBlink(LedNotifications::ScreenId::Iss)) {
            LedControl::startBlink(255, 0, 0, 500);
        } else {
            LedControl::stopBlink();
        }
    }
}

void enter(TFT_eSPI& tft) {
    hasLocation = (LocationPresets::count() > 0);
    if (hasLocation) {
        int8_t active = LocationPresets::activeIndex();
        uint8_t idx = (active >= 0) ? (uint8_t)active : 0;
        LocationPresets::getLatLon(idx, obsLat, obsLon);
    }

    drawChrome(tft);
    if (haveData) {
        drawRadarAndData(tft, lastPos, lastLook, obsLat, obsLon);
    } else if (hasLocation) {
        drawCenteredHint(tft, StringId::ISS_RADAR_LOADING);
    } else {
        drawCenteredHint(tft, StringId::ISS_RADAR_NO_LOCATION);
    }
    applyLedForVisibility();
}

void update(TFT_eSPI& tft) {
    if (!hasLocation) return;

    // Edge Case (Alex' Auftrag, 07.10.): den gespeicherten Standort bei
    // jedem Durchlauf leicht gegenpruefen, falls er sich AUSSERHALB eines
    // regulaeren enter()-Zyklus aendert. Im Normalfall ist das nicht
    // erreichbar - eine Standortaenderung geht nur ueber Einstellungen ->
    // Tools -> "Adresse ändern", was zwingend ein Verlassen dieses Screens
    // voraussetzt (Wisch zu den Einstellungen), und ein Rueckkehr-Wisch
    // loest ohnehin enter() aus, das obsLat/obsLon schon frisch liest.
    // Dieser Check ist trotzdem ein zusaetzliches Sicherheitsnetz (reiner
    // RAM-Zugriff, kein SD-Zugriff, siehe LocationPresets::getLatLon()) -
    // zeichnet bei einer erkannten Aenderung den statischen Rahmen sofort
    // neu, ohne auf die naechste ISS-Positionsaktualisierung zu warten.
    {
        int8_t active = LocationPresets::activeIndex();
        uint8_t idx = (active >= 0) ? (uint8_t)active : 0;
        double curLat, curLon;
        LocationPresets::getLatLon(idx, curLat, curLon);
        if (curLat != obsLat || curLon != obsLon) {
            obsLat = curLat;
            obsLon = curLon;
            if (haveData) {
                lastLook = IssClient::computeLookAngles(obsLat, obsLon, lastPos.latitude, lastPos.longitude, lastPos.altitudeKm);
                drawRadarAndData(tft, lastPos, lastLook, obsLat, obsLon);
                applyLedForVisibility();
            }
        }
    }

    // NetTask (Core 0) liefert die Positionsdaten im Hintergrund - hier
    // (Core 1) nur pruefen, ob sich die Versionsnummer seit dem letzten
    // Durchlauf geaendert hat, und falls ja, EINMAL kopieren und neu
    // zeichnen. Kein eigener Netzwerk-Abruf mehr in diesem Screen.
    uint32_t v = NetTask::issVersion();
    if (v != lastSeenVersion) {
        IssClient::Position newPos;
        if (NetTask::getIssPosition(newPos)) {
            lastSeenVersion = v;
            lastPos = newPos;
            lastLook = IssClient::computeLookAngles(obsLat, obsLon, lastPos.latitude, lastPos.longitude, lastPos.altitudeKm);
            haveData = true;
            drawRadarAndData(tft, lastPos, lastLook, obsLat, obsLon);
            applyLedForVisibility();
        }
        return;
    }

    NetTask::Status st = NetTask::issStatus();
    if (!haveData && st != lastSeenStatus) {
        lastSeenStatus = st;
        if (st == NetTask::Status::Error) {
            drawCenteredHint(tft, StringId::ISS_RADAR_FETCH_ERROR);
        }
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    if (!infoBtnRect().contains(x, y)) return;

    showInfoScreen(tft);
    MenuStars::reset();
    drawChrome(tft);
    if (haveData) {
        drawRadarAndData(tft, lastPos, lastLook, obsLat, obsLon);
    } else if (hasLocation) {
        drawCenteredHint(tft, StringId::ISS_RADAR_LOADING);
    } else {
        drawCenteredHint(tft, StringId::ISS_RADAR_NO_LOCATION);
    }
}

}
