#include "swipe_intro_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "i18n.h"

namespace SwipeIntroScreen {

namespace {
    // Wortweiser Umbruch anhand echter Pixelbreite - gleiches Muster wie
    // in den anderen Screens, hier lokal dupliziert statt geteilt
    // (Projekt-Konvention, siehe CLAUDE.md).
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

    // Wie layoutWrapped(), aber zentriert und als Textblock vertikal um
    // cy herum ausgerichtet - fuer die kurzen Pfeil-Labels oben/unten.
    void drawWrappedCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                              int16_t lineHeight, const String& text) {
        constexpr uint8_t MAX_LINES = 3;
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

    // Zentrierter, fetter (1px-Offset-Technik) wortweise umgebrochener
    // Text - gleiche Hilfsfunktion wie in first_run_welcome_screen.cpp
    // (dort lokal, hier erneut lokal dupliziert statt geteilt).
    void drawCenteredWrappedBold(TFT_eSPI& tft, int16_t cx, int16_t startY,
                                  int16_t maxWidth, int16_t lineHeight,
                                  uint8_t textSize, const String& text) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(textSize);
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
            int16_t lineCy = (int16_t)(y + lineHeight / 2);
            tft.drawString(line, cx, lineCy);
            tft.drawString(line, (int16_t)(cx + 1), lineCy);
            y += lineHeight;
            start += line.length();
        }
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    // Vier flache, duenne Pfeil-Umrisse (keine Fuellung) nah am jeweiligen
    // Bildschirmrand - reine Linienform, gleicher gruener Stil wie der
    // Rest der UI.
    void drawArrowUp(TFT_eSPI& tft, int16_t cx, int16_t apexY, int16_t baseY, int16_t halfW, uint16_t color) {
        tft.drawLine(cx, apexY, (int16_t)(cx - halfW), baseY, color);
        tft.drawLine(cx, apexY, (int16_t)(cx + halfW), baseY, color);
        tft.drawLine((int16_t)(cx - halfW), baseY, (int16_t)(cx + halfW), baseY, color);
    }

    void drawArrowDown(TFT_eSPI& tft, int16_t cx, int16_t apexY, int16_t baseY, int16_t halfW, uint16_t color) {
        tft.drawLine(cx, apexY, (int16_t)(cx - halfW), baseY, color);
        tft.drawLine(cx, apexY, (int16_t)(cx + halfW), baseY, color);
        tft.drawLine((int16_t)(cx - halfW), baseY, (int16_t)(cx + halfW), baseY, color);
    }

    void drawArrowLeft(TFT_eSPI& tft, int16_t cy, int16_t apexX, int16_t baseX, int16_t halfH, uint16_t color) {
        tft.drawLine(apexX, cy, baseX, (int16_t)(cy - halfH), color);
        tft.drawLine(apexX, cy, baseX, (int16_t)(cy + halfH), color);
        tft.drawLine(baseX, (int16_t)(cy - halfH), baseX, (int16_t)(cy + halfH), color);
    }

    void drawArrowRight(TFT_eSPI& tft, int16_t cy, int16_t apexX, int16_t baseX, int16_t halfH, uint16_t color) {
        tft.drawLine(apexX, cy, baseX, (int16_t)(cy - halfH), color);
        tft.drawLine(apexX, cy, baseX, (int16_t)(cy + halfH), color);
        tft.drawLine(baseX, (int16_t)(cy - halfH), baseX, (int16_t)(cy + halfH), color);
    }

    void draw(TFT_eSPI& tft) {
        uint16_t accent = TFT_GREEN;
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(accent, TFT_BLACK);

        constexpr int16_t ARROW_HALF = 12;

        // Oben: Pfeil + "Einstellungen"-Label knapp darunter.
        drawArrowUp(tft, 120, 8, 20, ARROW_HALF, accent);
        drawWrappedCentered(tft, 120, 34, 200, 14, I18n::t(StringId::SETTINGS_TITLE));

        // Unten: Pfeil + "Einstellungen"-Label knapp darueber.
        drawArrowDown(tft, 120, 312, 300, ARROW_HALF, accent);
        drawWrappedCentered(tft, 120, 286, 200, 14, I18n::t(StringId::SETTINGS_TITLE));

        // Links/Rechts: Pfeil + "Bildschirme wechseln"-Label in schmaler
        // Spalte daneben (links-/rechtsbuendig je nach Seite, damit der
        // Text nicht unter den grossen Fliesstext in der Mitte reicht).
        drawArrowLeft(tft, 190, 8, 20, ARROW_HALF, accent);
        layoutWrapped(tft, 24, 178, 66, 14, I18n::t(StringId::SWIPE_INTRO_SWITCH_LABEL));

        drawArrowRight(tft, 190, 232, 220, ARROW_HALF, accent);
        layoutWrapped(tft, 150, 178, 66, 14, I18n::t(StringId::SWIPE_INTRO_SWITCH_LABEL));

        // Grosser, fetter Haupttext mittig oben im verbleibenden Platz -
        // bewusst oberhalb der Links/Rechts-Pfeile (y=190), damit sich
        // nichts ueberschneidet, auch wenn der Text in laengeren Sprachen
        // (Spanisch/Franzoesisch) auf mehr Zeilen umbricht.
        drawCenteredWrappedBold(tft, 120, 56, 200, 24, 2, I18n::t(StringId::SWIPE_INTRO_TEXT));

        // Start-Hinweis im freien Platz zwischen den Links/Rechts-Labels
        // (enden spaetestens ~y=220) und dem unteren "Einstellungen"-Label
        // (beginnt ~y=279) - normale (nicht fette) Schriftgroesse, um sich
        // optisch vom grossen Haupttext abzuheben.
        drawWrappedCentered(tft, 120, 244, 200, 16, I18n::t(StringId::SWIPE_INTRO_START_HINT));
    }
}

SwipeInput::Direction run(TFT_eSPI& tft) {
    MenuStars::reset();
    draw(tft);

    // Verlaesst sich NICHT auf Taps UND NICHT auf Hoch/Runter-Wischen -
    // nur ein Links- oder Rechts-Swipe beendet die Schleife (Alex'
    // Auftrag, 05.10.: "kein separates weiter-Tippen, das Wischen selbst
    // ist die Bestaetigung" UND "Hoch/Runter soll hier (noch) nichts
    // ausloesen", da vor dem Commit ohnehin keine Einstellungen
    // existieren, die man aendern koennte).
    while (true) {
        SwipeInput::Direction dir;
        TouchInput::Point tap;
        if (SwipeInput::poll(dir, tap) &&
            (dir == SwipeInput::Direction::Left || dir == SwipeInput::Direction::Right)) {
            return dir;
        }
        MenuStars::update(tft);
        delay(20);
    }
}

}
