#include "natural_events_screen.h"
#include <time.h>
#include "touch_input.h"
#include "menu_stars.h"
#include "marquee_label.h"
#include "ui_header.h"
#include "config.h"
#include "i18n.h"
#include "natural_events_client.h"
#include "net_task.h"
#include "network_retry_view.h"

namespace NaturalEventsScreen {

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
    // draw-Schalter (nur messen, nicht zeichnen) - gleiches Muster wie
    // earthquake_screen.cpp, lokal dupliziert (Projekt-Konvention).
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
    // (Weiss) getrennt (CLAUDE.md "Pflicht: Label/Wert-Farbkontrast").
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

    // ---- Lokale Zeit des GERAETS (wie earthquake_screen.cpp/f1_screen.cpp
    // ::formatDeviceLocalDateTime(), 1:1 dupliziert, Projekt-Konvention). ----
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

    StringId categoryLabelId(NaturalEventsClient::Category c) {
        switch (c) {
            case NaturalEventsClient::Category::Volcano: return StringId::NATURAL_EVENTS_CATEGORY_VOLCANO;
            case NaturalEventsClient::Category::Storm: return StringId::NATURAL_EVENTS_CATEGORY_STORM;
            case NaturalEventsClient::Category::Flood: return StringId::NATURAL_EVENTS_CATEGORY_FLOOD;
            case NaturalEventsClient::Category::Landslide: return StringId::NATURAL_EVENTS_CATEGORY_LANDSLIDE;
            default: return StringId::NATURAL_EVENTS_CATEGORY_VOLCANO; // sollte nie vorkommen
        }
    }

    constexpr int16_t CHROME_H = Config::HEADER_HEIGHT;
    constexpr int16_t CONTENT_TOP = (int16_t)(CHROME_H + 10);
    constexpr int16_t TILE_X = 10;
    constexpr int16_t TILE_W = (int16_t)(Config::SCREEN_WIDTH - 20);
    constexpr int16_t MORE_BTN_H = 36;
    constexpr int16_t MORE_BTN_GAP = 10;

    // Haupt-Bildschirm zeigt jetzt direkt eine Liste einzelner Ereignisse
    // (Kategorie+Datum+Lauftext-Titel, gleiches Zeilenmuster wie
    // showMoreEventsScreen() unten) statt nur einer Zusammenfassung -
    // Alex' Auftrag, 13.10.: "Bildschirm voll mit Ereignissen, DANACH der
    // Button". VISIBLE_ROWS wird aus den uebrigen Layout-Konstanten
    // hergeleitet (kein Magic Number, passt sich automatisch an, falls
    // CHROME_H/MORE_BTN_H sich mal aendern - CLAUDE.md "jedes Layout muss
    // auf den Bildschirm passen").
    constexpr int16_t ROW_H = 40;
    constexpr int16_t ROW_GAP = 4;
    constexpr int16_t LIST_BOTTOM_MARGIN = 10;
    constexpr int16_t VISIBLE_AREA_H = (int16_t)(Config::SCREEN_HEIGHT - CONTENT_TOP - MORE_BTN_H -
                                                  MORE_BTN_GAP - LIST_BOTTOM_MARGIN);
    constexpr uint8_t VISIBLE_ROWS = (uint8_t)(VISIBLE_AREA_H / (ROW_H + ROW_GAP));

    Rect infoBtnRect() { return Rect{(int16_t)(Config::SCREEN_WIDTH - 35), 4, 31, 31}; }

    constexpr int16_t HEADER_FREE_LEFT = 4;
    constexpr int16_t HEADER_FREE_RIGHT = (int16_t)(Config::SCREEN_WIDTH - 35 - 4);
    constexpr int16_t HEADER_TITLE_CX = (int16_t)((HEADER_FREE_LEFT + HEADER_FREE_RIGHT) / 2);
    constexpr int16_t HEADER_TITLE_MAX_W = (int16_t)(HEADER_FREE_RIGHT - HEADER_FREE_LEFT);

    void drawChrome(TFT_eSPI& tft) {
        tft.fillScreen(TFT_BLACK);
        drawButton(tft, infoBtnRect(), "?");
        UiHeader::drawCentered(tft, HEADER_TITLE_CX, (int16_t)(CHROME_H / 2), HEADER_TITLE_MAX_W,
                                I18n::t(StringId::NATURAL_EVENTS_TITLE), TFT_GREEN);
    }

    void drawCenteredHint(TFT_eSPI& tft, StringId id) {
        tft.fillRect(0, CHROME_H, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - CHROME_H), TFT_BLACK);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        // Geduld-Hinweis (Alex' Auftrag, 13.10.) - nur beim Erst-Ladezustand
        // angehaengt, nicht bei sonstigen Hinweisen ueber diesen Helfer.
        String text = I18n::t(id);
        if (id == StringId::NATURAL_EVENTS_LOADING) { text += " "; text += I18n::t(StringId::LOADING_PATIENCE_HINT); }
        drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2),
                             (int16_t)(Config::SCREEN_WIDTH - 40), 16, text);
    }

    // Dynamisch, da die Button-Position von der tatsaechlich gezeichneten
    // Zeilenzahl abhaengt (kann < VISIBLE_ROWS sein, wenn insgesamt
    // weniger Ereignisse vorliegen) - gleiches Prinzip wie
    // earthquake_screen.cpp::lastMoreBtnRect.
    Rect lastMoreBtnRect = {0, 0, 0, 0};

    // Persistente Lauftext-Zustaende je sichtbarer Zeile (Alex' Auftrag,
    // 13.10.: "Bildschirm voll mit Ereignissen") - werden in update() bei
    // JEDEM Tick weitergedreht, nicht nur bei neuen Daten, damit lange
    // Titel wie im "Weitere Ereignisse"-Screen durchlaufen.
    MarqueeLabel::State rowMarquee[VISIBLE_ROWS];
    uint8_t lastVisibleRowCount = 0;

    int16_t rowTopFor(uint8_t rowIndex) {
        return (int16_t)(CONTENT_TOP + rowIndex * (ROW_H + ROW_GAP));
    }

    void drawMainTile(TFT_eSPI& tft, const NetTask::NaturalEventsData& data) {
        tft.fillRect(0, CONTENT_TOP, Config::SCREEN_WIDTH,
                     (int16_t)(Config::SCREEN_HEIGHT - CONTENT_TOP), TFT_BLACK);

        uint8_t rowsToShow = (data.eventCount < VISIBLE_ROWS) ? data.eventCount : VISIBLE_ROWS;
        lastVisibleRowCount = rowsToShow;

        for (uint8_t i = 0; i < rowsToShow; i++) {
            MarqueeLabel::reset(rowMarquee[i]);
            int16_t rowTop = rowTopFor(i);

            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(TFT_GREEN, TFT_BLACK);
            tft.drawString(I18n::t(categoryLabelId(data.events[i].category)), TILE_X, (int16_t)(rowTop + 11));

            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            tft.drawString(formatDeviceLocalDateTime(data.events[i].epoch),
                            (int16_t)(Config::SCREEN_WIDTH - TILE_X), (int16_t)(rowTop + 11));
            tft.setTextDatum(TL_DATUM);
        }

        int16_t listBottom = rowTopFor(rowsToShow);
        lastMoreBtnRect = {TILE_X, (int16_t)(listBottom + MORE_BTN_GAP), TILE_W, MORE_BTN_H};
        if (data.eventCount > rowsToShow) {
            drawButton(tft, lastMoreBtnRect, I18n::t(StringId::NATURAL_EVENTS_MORE_BTN));
        }
    }

    void showInfoScreen(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        int16_t y = layoutWrapped(tft, 10, 20, (int16_t)(Config::SCREEN_WIDTH - 20), 18,
                                   I18n::t(StringId::NATURAL_EVENTS_INFO_TITLE));

        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        layoutWrapped(tft, 10, (int16_t)(y + 10), (int16_t)(Config::SCREEN_WIDTH - 20), 16,
                       I18n::t(StringId::NATURAL_EVENTS_INFO_BODY));

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

    // ---- "Weitere Ereignisse" - Liste der auf der Hauptseite NICHT schon
    // sichtbaren Ereignisse (startIndex = Anzahl der dort bereits
    // gezeigten Zeilen, Alex' Auftrag 13.10. "Bildschirm voll mit
    // Ereignissen" - die Hauptseite zeigt jetzt selbst schon die ersten
    // VISIBLE_ROWS Eintraege, keine Dopplung hier). Zwei Zeilen je
    // Eintrag (Kategorie+Datum, Titel per Lauftext), scrollbar mit
    // strenger Vollstaendigkeits-Pruefung (siehe earthquake_screen.cpp-
    // Kommentar/Bugfix, 12.10. - eine Zeile wird nur gezeichnet, wenn sie
    // GANZ innerhalb des sichtbaren Bereichs liegt, sonst wuerde sie in
    // die Button-Leiste hineinragen koennen). ----
    void showMoreEventsScreen(TFT_eSPI& tft, const NetTask::NaturalEventsData& data, uint8_t startIndex) {
        MenuStars::reset();
        uint8_t count = (uint8_t)(data.eventCount - startIndex);

        static MarqueeLabel::State titleMarquee[Config::NATURAL_EVENTS_MAX_COUNT];
        for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(titleMarquee[i]);

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

        auto drawStaticRows = [&]() {
            tft.fillRect(0, VIEW_TOP, Config::SCREEN_WIDTH, (int16_t)(VIEW_BOTTOM - VIEW_TOP), TFT_BLACK);
            for (uint8_t i = 0; i < count; i++) {
                int16_t rowTop = (int16_t)(VIEW_TOP + i * (ROW_H + ROW_GAP) - scrollY);
                if (rowTop < VIEW_TOP || rowTop + ROW_H > VIEW_BOTTOM) continue;

                tft.setTextDatum(MR_DATUM);
                tft.setTextColor(TFT_WHITE, TFT_BLACK);
                tft.drawString(formatDeviceLocalDateTime(data.events[startIndex + i].epoch), TIME_RIGHT_X, (int16_t)(rowTop + 11));

                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(TFT_GREEN, TFT_BLACK);
                tft.drawString(I18n::t(categoryLabelId(data.events[startIndex + i].category)), NAME_X, (int16_t)(rowTop + 11));

                tft.setTextDatum(TL_DATUM);
            }
        };

        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setCursor(10, 14);
        tft.println(I18n::t(StringId::NATURAL_EVENTS_MORE_TITLE));

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
                    for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(titleMarquee[i]);
                    drawStaticRows();
                } else if (scrollable && downBtn.contains(tap.x, tap.y) && scrollY < maxScroll) {
                    scrollY = (int16_t)(scrollY + SCROLL_STEP);
                    if (scrollY > maxScroll) scrollY = maxScroll;
                    for (uint8_t i = 0; i < count; i++) MarqueeLabel::reset(titleMarquee[i]);
                    drawStaticRows();
                }
            }

            for (uint8_t i = 0; i < count; i++) {
                int16_t rowTop = (int16_t)(VIEW_TOP + i * (ROW_H + ROW_GAP) - scrollY);
                if (rowTop < VIEW_TOP || rowTop + ROW_H > VIEW_BOTTOM) continue;
                MarqueeLabel::draw(tft, titleMarquee[i], NAME_X, (int16_t)(rowTop + 30), NAME_W, 16,
                                    data.events[startIndex + i].title, TFT_WHITE, TFT_BLACK);
            }
            MenuStars::update(tft);
            delay(20);
        }
    }

    bool haveData = false;
    uint32_t lastSeenVersion = 0;
    NetTask::Status lastSeenStatus = NetTask::Status::Pending;
    NetTask::NaturalEventsData lastData;

    // Gueltig nur, solange der "Keine Verbindung"-Fehlerzustand sichtbar
    // ist (Alex' Auftrag, 12.10., network_retry_view.h).
    NetworkRetryView::Rect retryBtnRect = {0, 0, 0, 0};
}

void enter(TFT_eSPI& tft) {
    drawChrome(tft);
    if (haveData) {
        drawMainTile(tft, lastData);
    } else {
        drawCenteredHint(tft, StringId::NATURAL_EVENTS_LOADING);
    }
}

void update(TFT_eSPI& tft) {
    uint32_t v = NetTask::naturalEventsVersion();
    if (v != lastSeenVersion) {
        NetTask::NaturalEventsData newData;
        if (NetTask::getNaturalEventsData(newData)) {
            lastSeenVersion = v;
            lastData = newData;
            haveData = true;
            drawMainTile(tft, lastData);
        }
    } else if (!haveData) {
        NetTask::Status st = NetTask::naturalEventsStatus();
        if (st != lastSeenStatus) {
            lastSeenStatus = st;
            if (st == NetTask::Status::Error) {
                retryBtnRect = NetworkRetryView::draw(tft, CHROME_H, I18n::t(StringId::NATURAL_EVENTS_FETCH_ERROR));
            }
        }
    }

    // Lauftext fuer lange Titel in den sichtbaren Hauptbildschirm-Zeilen
    // weiterdrehen (Alex' Auftrag, 13.10.) - laeuft bei JEDEM Tick, nicht
    // nur bei neuen Daten, update() wird dafuer in app_navigation.cpp
    // ohnehin jede Hauptschleifen-Runde aufgerufen.
    if (haveData) {
        for (uint8_t i = 0; i < lastVisibleRowCount; i++) {
            int16_t rowTop = rowTopFor(i);
            MarqueeLabel::draw(tft, rowMarquee[i], TILE_X, (int16_t)(rowTop + 30), TILE_W, 16,
                                lastData.events[i].title, TFT_WHITE, TFT_BLACK);
        }
    }
}

void handleTap(TFT_eSPI& tft, int16_t x, int16_t y) {
    // "Nochmal versuchen" (Alex' Auftrag, 12.10., network_retry_view.h) -
    // siehe rocket_screen.cpp fuer die identische Begruendung.
    if (!haveData && retryBtnRect.contains(x, y)) {
        drawCenteredHint(tft, StringId::NATURAL_EVENTS_LOADING);
        lastSeenStatus = NetTask::Status::Pending;
        NetTask::forceNaturalEventsRefetch();
        return;
    }

    if (infoBtnRect().contains(x, y)) {
        showInfoScreen(tft);
        MenuStars::reset();
        drawChrome(tft);
        if (haveData) {
            drawMainTile(tft, lastData);
        } else {
            drawCenteredHint(tft, StringId::NATURAL_EVENTS_LOADING);
        }
        return;
    }

    if (haveData && lastData.eventCount > lastVisibleRowCount && lastMoreBtnRect.contains(x, y)) {
        showMoreEventsScreen(tft, lastData, lastVisibleRowCount);
        MenuStars::reset();
        drawChrome(tft);
        drawMainTile(tft, lastData);
        return;
    }
}

}
