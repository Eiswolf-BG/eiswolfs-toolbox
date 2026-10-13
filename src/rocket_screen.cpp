#include "rocket_screen.h"
#include <time.h>
#include "touch_input.h"
#include "menu_stars.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"
#include "rocket_client.h"
#include "net_task.h"
#include "led_control.h"
#include "led_notifications.h"
#include "network_retry_view.h"

namespace RocketScreen {

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
    // in iss_radar_screen.cpp, hier lokal dupliziert statt geteilt
    // (Projekt-Konvention, siehe CLAUDE.md). Optionale Scroll-Unterstuetzung
    // (scrollY/viewTop/viewBottom/draw, Defaults = bisheriges Verhalten
    // ohne Scrollen) - 1:1 dasselbe Muster wie transit_screen.cpp/
    // address_search_screen.cpp, fuer showLaunchDetailScreen() unten
    // gebraucht (Alex' Auftrag, 06.10.: "alle sinnvollen API-Daten
    // anzeigen" macht den Detail-Screen potenziell laenger als der
    // Bildschirm).
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
    // Label/Wert-Farbkontrast") - gleiche Logik wie in iss_radar_screen.cpp
    // (lokal dupliziert, Projekt-Konvention), hier zusaetzlich mit
    // Scroll-Unterstuetzung fuer den Detail-Screen.
    int16_t layoutWrappedLabelValue(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                                     int16_t lineHeight, const String& label, const String& value,
                                     uint16_t labelColor, uint16_t valueColor,
                                     int16_t scrollY = 0, int16_t viewTop = -32000, int16_t viewBottom = 32000,
                                     bool draw = true) {
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
                int16_t screenY = (int16_t)(y - scrollY);
                if (screenY >= viewTop && screenY <= viewBottom) {
                    int32_t labelCharsHere = labelLen - start;
                    if (labelCharsHere < 0) labelCharsHere = 0;
                    if (labelCharsHere > (int32_t)line.length()) labelCharsHere = (int32_t)line.length();

                    tft.setCursor(x, screenY);
                    if (labelCharsHere > 0) {
                        tft.setTextColor(labelColor, TFT_BLACK);
                        tft.print(line.substring(0, labelCharsHere));
                    }
                    if (labelCharsHere < (int32_t)line.length()) {
                        tft.setTextColor(valueColor, TFT_BLACK);
                        tft.print(line.substring(labelCharsHere));
                    }
                }
            }

            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    // "2026-10-08T07:54:00Z" -> "08.10. 07:54" (UTC, kein Zeitzonen-Offset -
    // das Geraet kennt nur UTC, siehe CLAUDE.md "OEPNV-Abfahrtstafel") -
    // reine Substring-Extraktion, KEINE Epoch-Umrechnung noetig (anders als
    // beim Live-Countdown, der echte Sekunden braucht).
    String formatDateTimeUtc(const String& iso) {
        if (iso.length() < 16) return iso;
        return iso.substring(8, 10) + "." + iso.substring(5, 7) + ". " + iso.substring(11, 16);
    }

    // Wie layoutWrapped(), aber zentriert und als Textblock vertikal um
    // cy herum ausgerichtet - fuer kurze Hinweistexte (Laden/Fehler).
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
    // siehe CLAUDE.md "Pflicht: einheitliche grosse Ueberschrift"). Abstand
    // zum Textanfang von 6 auf 16 erhoeht (Alex' Auftrag, 06.10.: "10px
    // mehr Platz nach unten, damit die Ueberschrift abgesetzt wirkt").
    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 16);
    // Von 234 auf 190 nach oben verschoben (Alex' Auftrag, 06.10.: Platz
    // fuer den neuen "Weitere Starts"-Button darunter schaffen) - das
    // Info-Textblock darueber ist auch mit den neuen Agentur-/Achtungs-
    // Zeilen (siehe drawInfoBlock()) hoechstens 6 Zeilen lang (endet bei
    // y~160), bleibt also weiterhin klar davor.
    constexpr int16_t COUNTDOWN_AREA_TOP = 190;
    // Feste Bandhoehe statt bis zum Bildschirmende (Alex' Auftrag, 06.10.):
    // clearCountdownArea() darf den NEUEN "Weitere Starts"-Button darunter
    // nicht bei jedem Sekunden-Tick mitloeschen.
    constexpr int16_t COUNTDOWN_AREA_H = 70;
    constexpr int16_t MORE_BTN_Y = (int16_t)(COUNTDOWN_AREA_TOP + COUNTDOWN_AREA_H + 8);
    constexpr int16_t MORE_BTN_H = 36;
    constexpr int16_t TEXT_LINE_H = 15;

    // 31x31 (26+5) - CLAUDE.md-Mindestgroesse fuer Tap-Flaechen (Alex'
    // Auftrag, 06.10., "Finger-only"-Regel), "?"-Buttons systemweit
    // vergroessert.
    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    // Zentrierung des Titels auf der Mitte der vom "?"-Button FREIEN Zone,
    // nicht auf der wahren Bildschirmmitte (Alex' Fund, 06.10.:
    // "Raketenstart" wurde bei echter Mittel-Zentrierung faelschlich auf
    // Groesse 1 zurueckgestuft - der 7px-Monospace-Font braucht dafuer
    // exakt 168px bei Groesse 2, bei Zentrierung auf x=120 blieben
    // wegen des Buttons rechts aber nur ~160px). So verbleiben 197px
    // echter Spielraum, in dem auch "Rocket Launch" (182px) passt.
    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");

        // Grosse, einheitliche Ueberschrift (Alex' Auftrag, 06.10.).
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::ROCKET_TITLE), TFT_GREEN);
    }

    void clearContentArea(TFT_eSPI& tft) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
    }

    void clearCountdownArea(TFT_eSPI& tft) {
        tft.fillRect(0, COUNTDOWN_AREA_TOP, Config::SCREEN_WIDTH, COUNTDOWN_AREA_H, TFT_BLACK);
    }

    // Button fuer Start Nr. 2/3 (Alex' Auftrag, 06.10.) - nur sichtbar/
    // antippbar, wenn tatsaechlich mehr als 1 Start bekannt ist (siehe
    // redraw() unten).
    Rect moreBtnRect() { return Rect{10, MORE_BTN_Y, (int16_t)(Config::SCREEN_WIDTH - 20), MORE_BTN_H}; }

    // Feste Uebersetzungstabelle der Launch-Library-2-API-Statuswerte
    // (Alex' Auftrag, 07.10., gleiches Prinzip wie die Session-/DNF-
    // Tabellen im F1-Screen) - die API hat aktuell exakt 9 feste
    // Statuswerte mit stabilen numerischen IDs (status.id, 1-9), die
    // deutlich zuverlaessiger sind als der Text in status.abbrev/
    // status.name (Namen/Abkuerzungen koennten sich theoretisch aendern,
    // die ID nicht). Primaerer Schluessel ist deshalb status.id - der
    // textbasierte Abgleich dient nur als Rueckfallebene fuer den
    // unwahrscheinlichen Fall, dass eine Antwort mal ohne status.id
    // kommt (Feld fehlt -> Launch::statusId bleibt -1).
    StringId rocketStatusStringIdById(int16_t id) {
        switch (id) {
            case 1: return StringId::ROCKET_STATUS_GO;
            case 2: return StringId::ROCKET_STATUS_TBD;
            case 3: return StringId::ROCKET_STATUS_SUCCESS;
            case 4: return StringId::ROCKET_STATUS_FAILURE;
            case 5: return StringId::ROCKET_STATUS_HOLD;
            case 6: return StringId::ROCKET_STATUS_IN_FLIGHT;
            case 7: return StringId::ROCKET_STATUS_PARTIAL_FAILURE;
            case 8: return StringId::ROCKET_STATUS_TBC;
            case 9: return StringId::ROCKET_STATUS_PAYLOAD_DEPLOYED;
            default: return StringId::COUNT;
        }
    }

    // Textbasierte Rueckfallebene (status.abbrev) - nur relevant, wenn
    // status.id ausnahmsweise fehlt. Deckt dieselben 9 Statuswerte ab wie
    // rocketStatusStringIdById() oben.
    StringId rocketStatusStringIdByAbbrev(const String& abbrev) {
        if (abbrev == "Go") return StringId::ROCKET_STATUS_GO;
        if (abbrev == "TBD") return StringId::ROCKET_STATUS_TBD;
        if (abbrev == "Success") return StringId::ROCKET_STATUS_SUCCESS;
        if (abbrev == "Failure") return StringId::ROCKET_STATUS_FAILURE;
        if (abbrev == "Hold") return StringId::ROCKET_STATUS_HOLD;
        if (abbrev == "In Flight") return StringId::ROCKET_STATUS_IN_FLIGHT;
        if (abbrev == "Partial Failure") return StringId::ROCKET_STATUS_PARTIAL_FAILURE;
        if (abbrev == "TBC") return StringId::ROCKET_STATUS_TBC;
        if (abbrev == "Deployed") return StringId::ROCKET_STATUS_PAYLOAD_DEPLOYED;
        return StringId::COUNT;
    }

    // Liefert den uebersetzten Status-Text fuer einen Start: zuerst ueber
    // status.id (stabil), sonst ueber status.abbrev (Text-Fallback),
    // sonst defensiv der rohe API-Text aus status.name (Alex' Auftrag:
    // "lieber den rohen API-Text anzeigen statt abzustuerzen oder nichts
    // anzuzeigen", falls die API mal einen wirklich neuen, noch nicht in
    // dieser Tabelle gefuehrten Statuswert liefert).
    String rocketStatusText(const RocketClient::Launch& l) {
        StringId id = rocketStatusStringIdById(l.statusId);
        if (id == StringId::COUNT) id = rocketStatusStringIdByAbbrev(l.statusAbbrev);
        return (id != StringId::COUNT) ? String(I18n::t(id)) : l.statusName;
    }

    // Label in Akzentfarbe, Wert in Weiss (Alex' Auftrag, 07.10., siehe
    // CLAUDE.md "Pflicht: Label/Wert-Farbkontrast") - gilt NICHT fuer die
    // Hold-/Fehlgrund-Zeile unten, die bewusst komplett in ihrer eigenen
    // Warnfarbe bleibt (muss "deutlich sichtbar" sein, siehe Kommentar
    // dort - ein weisser Wertanteil wuerde diese Auffaelligkeit mindern).
    void drawInfoBlock(TFT_eSPI& tft, const RocketClient::Launch& launch) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH,
                     (int16_t)(COUNTDOWN_AREA_TOP - CHROME_H), TFT_BLACK);
        uint16_t accent = TFT_GREEN;
        int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        int16_t y = CONTENT_TOP;

        y = layoutWrappedLabelValue(tft, 10, y, maxW, TEXT_LINE_H,
            I18n::t(StringId::ROCKET_MISSION_LABEL), launch.missionName, accent, TFT_WHITE);
        // Agentur neu (Alex' Auftrag, 06.10. - "launch_service_provider.name...
        // fehlt aktuell komplett") - und die Raketen-Zeile zeigt jetzt die
        // VOLLE Bezeichnung statt nur des kurzen Namens (z.B. "KSLV-2 Nuri"
        // statt nur "Nuri").
        y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
            I18n::t(StringId::ROCKET_AGENCY_LABEL), launch.agencyName, accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
            I18n::t(StringId::ROCKET_ROCKET_LABEL), launch.rocketFullName, accent, TFT_WHITE);
        y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
            I18n::t(StringId::ROCKET_SITE_LABEL), launch.padName, accent, TFT_WHITE);

        String statusText = rocketStatusText(launch);
        y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
            I18n::t(StringId::ROCKET_STATUS_LABEL), statusText, accent, TFT_WHITE);

        // Hold-/Fehlgrund MUSS deutlich sichtbar sein (Alex' Auftrag,
        // 06.10. - "das ist eine kritische Status-Info") - deshalb direkt
        // hier auf dem Hauptscreen, nicht erst im Detail-Screen versteckt.
        // Fehlschlag hat Vorrang vor Hold, falls (theoretisch) beide
        // gleichzeitig gesetzt waeren. Live gegen die echte API geprueft
        // (06.10.): bei keinem der naechsten 50 Starts aktuell gesetzt,
        // das Feld existiert aber laut Schema und kann jederzeit befuellt
        // werden (z.B. kurz vor einem echten Start-Scrub).
        if (launch.failReason.length() > 0) {
            tft.setTextColor(TFT_RED, TFT_BLACK);
            layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
                String(I18n::t(StringId::ROCKET_CRITICAL_LABEL)) + launch.failReason);
        } else if (launch.holdReason.length() > 0) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
            layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, TEXT_LINE_H,
                String(I18n::t(StringId::ROCKET_CRITICAL_LABEL)) + launch.holdReason);
        }
    }

    void drawCountdown(TFT_eSPI& tft, time_t netEpoch) {
        clearCountdownArea(tft);

        long secondsRemaining = (long)netEpoch - (long)time(nullptr);
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

        // Countdown literal GELB statt Akzentfarbe (Alex' Auftrag, 07.10.:
        // "sämtliche Countdowns sind gelb") - eigene, themenunabhaengige
        // Signalfarbe wie bei AQI-Kategorien/Kp-Index/Hold-Fail, siehe
        // CLAUDE.md "Pflicht: Countdown-Anzeigen in Gelb".
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.setTextSize(1);
        tft.drawString(I18n::t(StringId::ROCKET_COUNTDOWN_LABEL),
                        (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(COUNTDOWN_AREA_TOP + 16));

        // BUGFIX (13.10., Alex' Auftrag: "komplettes System auf
        // Text-ueberlaeuft-Button/Flaeche pruefen") - bei einem weit in
        // der Zukunft liegenden Start (dreistellige Tageszahl, z.B. "365
        // Tage  12:34:56") lief der Countdown bei Groesse 2 ueber die
        // Bildschirmbreite hinaus. Faellt wie UiHeader/der Krypto-Preis
        // auf Groesse 1 zurueck, wenn er nicht passt.
        tft.setTextSize(2);
        if (tft.textWidth(countdownStr) > (Config::SCREEN_WIDTH - 10)) {
            tft.setTextSize(1);
        }
        tft.drawString(countdownStr, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(COUNTDOWN_AREA_TOP + 48));
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        clearContentArea(tft);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::ROCKET_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::ROCKET_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::ROCKET_INFO_BODY));

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

    // ---- Detail-Screen (Alex' Auftrag, 06.10.) - fuer JEDEN der bis zu
    // 3 Starts gueltig (Haupt-Start wird aktuell nicht darueber geoeffnet,
    // nur die ueber "Weitere Starts" erreichbaren Starts Nr. 2/3 - siehe
    // CLAUDE.md "Raketenstart-Countdown"). Scrollbar wie der OEPNV-/
    // Wetter-Detail-Screen, da mit allen optionalen Feldern (Missionsart/
    // Orbit/Hold-Fehlgrund/Go-Chance/Wetter-Hinweis/Beschreibung) die
    // Gesamthoehe variieren kann. ----
    void showLaunchDetailScreen(TFT_eSPI& tft, const RocketClient::Launch& l) {
        MenuStars::reset();
        uint16_t accent = TFT_GREEN;

        constexpr int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t LINE_H = 16;
        constexpr int16_t VIEW_TOP = 40;
        constexpr int16_t BACK_Y = (int16_t)(Config::SCREEN_HEIGHT - 50);
        constexpr int16_t VIEW_BOTTOM = (int16_t)(BACK_Y - 10);

        auto drawContent = [&](int16_t scrollY, bool draw) -> int16_t {
            int16_t y = VIEW_TOP;
            int16_t vTop = draw ? VIEW_TOP : -32000;
            int16_t vBottom = draw ? VIEW_BOTTOM : 32000;

            // Missionsname wie bisher ohne Label (Titelzeile) - Akzentfarbe
            // statt Weiss, damit er als Ueberschrift dieses Starts auch
            // optisch hervorsticht (einziger reiner "Wert ohne Label"
            // hier, deshalb bewusst keine Farbaufteilung).
            if (draw) tft.setTextColor(accent, TFT_BLACK);
            y = layoutWrapped(tft, 10, y, maxW, LINE_H, l.missionName, scrollY, vTop, vBottom, draw);

            y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                I18n::t(StringId::ROCKET_AGENCY_LABEL), l.agencyName, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                I18n::t(StringId::ROCKET_ROCKET_LABEL), l.rocketFullName, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                I18n::t(StringId::ROCKET_SITE_LABEL), l.padName, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);

            String statusText = rocketStatusText(l);
            y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                I18n::t(StringId::ROCKET_STATUS_LABEL), statusText, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);

            if (l.missionType.length() > 0) {
                y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    I18n::t(StringId::ROCKET_DETAIL_TYPE_LABEL), l.missionType, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            }
            if (l.missionOrbitName.length() > 0) {
                y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    I18n::t(StringId::ROCKET_DETAIL_ORBIT_LABEL), l.missionOrbitName, accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            }

            if (l.failReason.length() > 0) {
                if (draw) tft.setTextColor(TFT_RED, TFT_BLACK);
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::ROCKET_DETAIL_FAIL_LABEL)) + l.failReason, scrollY, vTop, vBottom, draw);
                if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
            } else if (l.holdReason.length() > 0) {
                if (draw) tft.setTextColor(TFT_YELLOW, TFT_BLACK);
                y = layoutWrapped(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    String(I18n::t(StringId::ROCKET_DETAIL_HOLD_LABEL)) + l.holdReason, scrollY, vTop, vBottom, draw);
                if (draw) tft.setTextColor(TFT_WHITE, TFT_BLACK);
            }

            if (l.probability >= 0) {
                y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    I18n::t(StringId::ROCKET_DETAIL_PROBABILITY_LABEL), String(l.probability) + "%",
                    accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            }
            if (l.weatherConcerns.length() > 0) {
                y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                    I18n::t(StringId::ROCKET_DETAIL_WEATHER_LABEL), l.weatherConcerns,
                    accent, TFT_WHITE, scrollY, vTop, vBottom, draw);
            }

            y = layoutWrappedLabelValue(tft, 10, (int16_t)(y + 4), maxW, LINE_H,
                I18n::t(StringId::ROCKET_DETAIL_DATE_LABEL), formatDateTimeUtc(l.netIso) + " UTC",
                accent, TFT_WHITE, scrollY, vTop, vBottom, draw);

            // Missionsbeschreibung NUR hier im Detail-Screen (Alex'
            // Auftrag, 06.10. - "nicht auf dem Hauptscreen, da zu lang").
            if (l.missionDescription.length() > 0) {
                y = layoutWrapped(tft, 10, (int16_t)(y + 8), maxW, LINE_H,
                    l.missionDescription, scrollY, vTop, vBottom, draw);
            }

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
            tft.println(I18n::t(StringId::ROCKET_DETAIL_TITLE));

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

    // ---- "Weitere Starts" (Alex' Auftrag, 06.10.) - Liste der Starts
    // Nr. 2/3 (launches zeigt auf lastData.launches[1], count = bis zu 2).
    // Jede Zeile antippbar, oeffnet denselben Detail-Screen wie oben.
    // Nur 2 Zeilen maximal - bewusst OHNE eigene Scroll-Infrastruktur
    // (passt immer bequem auf den Bildschirm). ----
    void showMoreLaunchesScreen(TFT_eSPI& tft, const RocketClient::Launch* launches, uint8_t count) {
        MenuStars::reset();

        Rect rowRects[Config::ROCKET_MAX_LAUNCHES];
        Rect backBtn = {10, (int16_t)(Config::SCREEN_HEIGHT - 50), (int16_t)(Config::SCREEN_WIDTH - 20), 40};

        auto redraw = [&]() {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.setCursor(10, 14);
            tft.println(I18n::t(StringId::ROCKET_MORE_LAUNCHES_TITLE));

            int16_t maxW = (int16_t)(Config::SCREEN_WIDTH - 20);
            int16_t y = 40;
            for (uint8_t i = 0; i < count; i++) {
                // 12px Ascent-Reserve ueber der ersten Zeilen-Baseline (wie
                // iss_radar_screen.cpp::DATA_AREA_TOP) statt eines
                // geratenen "-4"-Versatzes - der war zu klein fuer den
                // tatsaechlichen Ascent unseres grundlinien-verankerten
                // Fonts und liess die obere Rahmenlinie mitten durch den
                // Mission-Titel laufen (Alex' Fund, 07.10.).
                int16_t rowTop = (int16_t)(y - 12);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, 16, launches[i].missionName);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                y = layoutWrapped(tft, 10, y, maxW, 15,
                    launches[i].rocketFullName + " - " + formatDateTimeUtc(launches[i].netIso) + " UTC");
                rowRects[i] = {4, rowTop, (int16_t)(Config::SCREEN_WIDTH - 8), (int16_t)(y - rowTop + 2)};
                tft.drawRoundRect(rowRects[i].x, rowRects[i].y, rowRects[i].w, rowRects[i].h, 3,
                                   tft.color565(0, (uint8_t)(255 * 0.35f), 0));
                y += 14;
            }

            drawButton(tft, backBtn, I18n::t(StringId::BACK_BTN));
        };
        redraw();

        while (true) {
            TouchInput::Point tap;
            if (!TouchInput::wasTapped(tap)) { MenuStars::update(tft); delay(20); continue; }
            if (backBtn.contains(tap.x, tap.y)) return;

            bool handled = false;
            for (uint8_t i = 0; i < count && !handled; i++) {
                if (rowRects[i].contains(tap.x, tap.y)) {
                    showLaunchDetailScreen(tft, launches[i]);
                    MenuStars::reset();
                    redraw();
                    handled = true;
                }
            }
        }
    }

    // Persistiert ueber mehrere enter()/leave()-Zyklen hinweg (gleiches
    // Prinzip wie iss_radar_screen.cpp) - ein erneutes Betreten zeigt
    // sofort die zuletzt bekannten Daten. Der eigentliche Netzwerk-Abruf
    // laeuft ausschliesslich in NetTask auf Core 0 (siehe net_task.h).
    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    NetTask::RocketData lastData;
    long lastDisplayedSeconds = -1;

    // Gueltig nur, solange der "Keine Verbindung"-Fehlerzustand sichtbar
    // ist (Alex' Auftrag, 12.10., network_retry_view.h) - handleTap()
    // prueft diesen Rect nur, wenn !haveData, siehe dort.
    NetworkRetryView::Rect retryBtnRect = {0, 0, 0, 0};

    // Rotes LED-Blinken in den letzten 5 Minuten vor einem Start (Alex'
    // Auftrag, 12.10.) - eigener Merker (NICHT einfach bei jedem Aufruf
    // blind LedControl::startBlink() aufrufen), da applyLedForCountdown()
    // hier anders als bei iss_radar_screen.cpp bei JEDEM update()-Tick
    // laeuft (nicht nur bei neuen Daten): ein wiederholtes startBlink()
    // waehrend bereits aktivem Blinken wuerde dessen Umschalt-Timer jedes
    // Mal zuruecksetzen und das Blinken faktisch einfrieren. Nur bei einer
    // tatsaechlichen ZustandsAENDERUNG wird start-/stopBlink() aufgerufen.
    bool ledBlinking = false;

    // "5 Minuten oder weniger, aber Countdown laeuft noch echt" (Alex'
    // Auftrag: "die 5-Minuten-Bedingung darf sich nur auf einen echten,
    // noch bevorstehenden Start beziehen") - bewusst die ROHE (nicht bei 0
    // gekappte) Restzeit verwendet, nicht die bereits auf >=0 geklammerte
    // secondsRemaining-Variable von drawCountdown()/update() weiter unten:
    // ein abgelaufener Countdown (0 oder negativ, bevor der naechste Start
    // nachgeladen wurde) soll explizit NICHT als "<=5 Minuten" zaehlen.
    void applyLedForCountdown() {
        bool shouldBlink = false;
        if (haveData) {
            long rawRemaining = (long)lastData.launches[0].netEpoch - (long)time(nullptr);
            shouldBlink = (rawRemaining > 0 && rawRemaining <= 300);
        }
        // Nur blinken, wenn ZUSAETZLICH sowohl der Haupt-Toggle als auch
        // der Raketenstart-Einzel-Toggle in den LED-Einstellungen an sind
        // (Alex' Auftrag, 12.10., siehe led_notifications.h).
        shouldBlink = shouldBlink && LedNotifications::shouldBlink(LedNotifications::ScreenId::Rocket);
        if (shouldBlink != ledBlinking) {
            ledBlinking = shouldBlink;
            if (shouldBlink) {
                LedControl::startBlink(255, 0, 0, 500);
            } else {
                LedControl::stopBlink();
            }
        }
    }

    // Zeichnet den "Weitere Starts"-Button nur, wenn es tatsaechlich mehr
    // als 1 bekannten Start gibt - loescht seinen Bereich aber IMMER erst
    // (liegt ausserhalb des von drawInfoBlock() geleerten Bereichs), damit
    // kein veralteter Button stehen bleibt, falls launchCount zwischen
    // zwei Abrufen von >1 auf <=1 zurueckfaellt (theoretisch moeglich,
    // falls die API kurzfristig weniger als 3 Starts liefert).
    void drawMoreButtonIfNeeded(TFT_eSPI& tft) {
        Rect r = moreBtnRect();
        tft.fillRect(r.x, r.y, r.w, r.h, TFT_BLACK);
        if (lastData.launchCount > 1) {
            drawButton(tft, r, I18n::t(StringId::ROCKET_MORE_LAUNCHES_BTN));
        }
    }

    // Zweite, defensive Sicherung gegen denselben "bereits gestartete
    // Mission steht noch vorn"-Fall, den net_task.cpp direkt nach jedem
    // API-Abruf behebt (promoteNextFutureLaunch()) - hier zusaetzlich bei
    // JEDEM update()-Durchlauf, falls der Countdown fuer launches[0]
    // zwischen zwei Abrufen (alle 12 Minuten) bereits auf 0 gefallen ist,
    // waehrend ein spaeterer Eintrag im selben, schon etwas aelteren
    // Snapshot noch in der Zukunft liegt (Alex' Auftrag, 07.10.) - lokal
    // dupliziert statt geteilt (Projekt-Konvention). true, wenn Index 0
    // dadurch gewechselt hat (Aufrufer muss dann neu zeichnen).
    bool promoteNextFutureLaunchIfNeeded(NetTask::RocketData& data) {
        if (data.launchCount < 2) return false;
        time_t now = time(nullptr);
        if (data.launches[0].netEpoch > now) return false;

        for (uint8_t i = 1; i < data.launchCount; i++) {
            if (data.launches[i].netEpoch > now) {
                RocketClient::Launch promoted = data.launches[i];
                for (uint8_t j = i; j > 0; j--) data.launches[j] = data.launches[j - 1];
                data.launches[0] = promoted;
                return true;
            }
        }
        return false; // Fallback: alle verstrichen, Reihenfolge unveraendert.
    }
}

void enter(TFT_eSPI& tft) {
    // app_navigation.cpp hat die LED beim Betreten bereits zentral
    // ausgeschaltet (siehe dortiger Kommentar) - der lokale Merker muss
    // das nachvollziehen, sonst haelt applyLedForCountdown() ihn faelschlich
    // weiter fuer "an" und schaltet beim naechsten Tick nicht neu ein.
    ledBlinking = false;

    drawChrome(tft);
    if (haveData) {
        drawInfoBlock(tft, lastData.launches[0]);
        drawMoreButtonIfNeeded(tft);
        lastDisplayedSeconds = -1; // erzwingt sofortigen Countdown-Redraw
        drawCountdown(tft, lastData.launches[0].netEpoch);
        applyLedForCountdown();
    } else {
        drawCenteredHint(tft, StringId::ROCKET_LOADING);
    }
}

void update(TFT_eSPI& tft) {
    // NetTask (Core 0) liefert die Daten im Hintergrund - hier (Core 1)
    // nur pruefen, ob sich die Versionsnummer seit dem letzten Durchlauf
    // geaendert hat. Kein eigener Netzwerk-Abruf mehr in diesem Screen.
    uint32_t v = NetTask::rocketVersion();
    if (v != lastSeenVersion) {
        NetTask::RocketData newData;
        if (NetTask::getRocketData(newData) && newData.launchCount > 0) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawInfoBlock(tft, lastData.launches[0]);
            drawMoreButtonIfNeeded(tft);
            lastDisplayedSeconds = -1;
        }
    } else {
        NetTask::Status st = NetTask::rocketStatus();
        if (!haveData && st != lastSeenStatus) {
            lastSeenStatus = st;
            if (st == NetTask::Status::Error) {
                retryBtnRect = NetworkRetryView::draw(tft, CHROME_H, I18n::t(StringId::ROCKET_FETCH_ERROR));
            }
        }
    }

    if (!haveData) return;

    if (promoteNextFutureLaunchIfNeeded(lastData)) {
        drawInfoBlock(tft, lastData.launches[0]);
        drawMoreButtonIfNeeded(tft);
        lastDisplayedSeconds = -1;
    }

    long secondsRemaining = (long)lastData.launches[0].netEpoch - (long)time(nullptr);
    if (secondsRemaining < 0) secondsRemaining = 0;
    if (secondsRemaining != lastDisplayedSeconds) {
        lastDisplayedSeconds = secondsRemaining;
        drawCountdown(tft, lastData.launches[0].netEpoch);
    }

    // Jeden Tick pruefen (nicht nur bei Sekundenwechsel) - billig (eine
    // Subtraktion+Vergleich) und sorgt dafuer, dass die LED exakt in dem
    // Moment angeht/ausgeht, in dem die 5-Minuten-Schwelle ueberschritten
    // wird bzw. der Start beginnt, ohne auf den naechsten Countdown-
    // Sekundenwechsel warten zu muessen.
    applyLedForCountdown();
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    // "Nochmal versuchen" (Alex' Auftrag, 12.10., network_retry_view.h) -
    // nur relevant, solange der Fehlerzustand sichtbar ist. Setzt sofort
    // auf den Lade-Zustand zurueck und stoesst ueber NetTask::
    // forceRocketRefetch() denselben Abruf an, den sonst nur der
    // reguaere Poll-Timer ausloest - der Timer startet dabei automatisch
    // neu (siehe net_task.cpp, kein doppelter Nachlauf-Abruf).
    if (!haveData && retryBtnRect.contains(x, y)) {
        drawCenteredHint(tft, StringId::ROCKET_LOADING);
        lastSeenStatus = NetTask::Status::Pending;
        NetTask::forceRocketRefetch();
        return;
    }

    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        drawChrome(tft);
        if (haveData) {
            drawInfoBlock(tft, lastData.launches[0]);
            drawMoreButtonIfNeeded(tft);
            lastDisplayedSeconds = -1;
            drawCountdown(tft, lastData.launches[0].netEpoch);
        } else {
            drawCenteredHint(tft, StringId::ROCKET_LOADING);
        }
        return;
    }

    if (haveData && lastData.launchCount > 1 && moreBtnRect().contains(x, y)) {
        showMoreLaunchesScreen(tft, &lastData.launches[1], (uint8_t)(lastData.launchCount - 1));
        MenuStars::reset();
        drawChrome(tft);
        drawInfoBlock(tft, lastData.launches[0]);
        drawMoreButtonIfNeeded(tft);
        lastDisplayedSeconds = -1;
        drawCountdown(tft, lastData.launches[0].netEpoch);
        return;
    }
}

}
