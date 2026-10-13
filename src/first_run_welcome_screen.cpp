#include "first_run_welcome_screen.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "config.h"
#include "toolbox_logo_bitmap.h"

// Struktur/Ablauf 1:1 aus Eiswolfs Flightradar CYD uebernommen
// (first_run_welcome_screen.cpp dort), inklusive des Layout-Prinzips
// fuer den verfuegbaren Platz zwischen Titel und Start-Button (siehe
// dortige `drawRadarGroup`-Platzierung). Inhaltlich ersetzt: statt der
// flugradar-thematischen Vektorgrafik (Flugzeuge/Helikopter um ein
// Radar-Reticle) zeigt dieser Screen das von Alex gelieferte
// Werkzeugkisten-Foto (05.10., assets/toolbox_logo_source.png), per
// Skript in ein 1-Bit-PROGMEM-Bitmap umgewandelt (siehe
// toolbox_logo_bitmap.h) und ueber tft.drawBitmap() gezeichnet - kein
// selbst gezeichnetes Vektor-Icon mehr. Mit dem Schriftzug
// "Eiswolfs Toolbox" darunter. "Welcome!"/"Start" bleiben hier hart
// kodiert statt ueber StringId, weil dieser Screen laut Ablaufreihenfolge
// (siehe main.cpp) VOR der Sprachauswahl laeuft - anders als im
// Flightradar-Original (dort hart Deutsch) aber bewusst auf Englisch,
// da main.cpp bis zur Sprachauswahl konsequent Englisch erzwingt (Alex'
// Auftrag, 06.10.: "Englisch bis Sprachauswahl" gilt auch hier, nicht
// nur fuer Kalibrierung/WLAN).

namespace FirstRunWelcomeScreen {

namespace {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    constexpr const char* TITLE = "Welcome!";
    constexpr const char* LOGO_TEXT = "Eiswolfs\nToolbox";

    // Zentrierter, fetter (1px-Offset-Technik) wortweise umgebrochener
    // Text - gleiche Hilfsfunktion wie im Flightradar-Original, hier
    // zusaetzlich um "\n" als expliziten Zeilenumbruch erweitert (fuer
    // den zweizeiligen Schriftzug).
    int16_t drawCenteredWrappedBold(TFT_eSPI& tft, int16_t cx, int16_t startY,
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
            int32_t explicitBreak = text.indexOf('\n', start);
            int32_t segmentEnd = (explicitBreak >= 0) ? explicitBreak : len;
            String line = text.substring(start, segmentEnd);
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
            if (explicitBreak >= 0 && start == explicitBreak) start++;
        }
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
        return y;
    }

    // Button-Rahmen im ueblichen Stil (schwarze Fuellung, duenner
    // Akzentfarb-Rahmen, abgerundete Ecken) - 1:1 aus dem Flightradar-
    // Original.
    void drawButtonFrame(TFT_eSPI& tft, const Rect& r) {
        tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, TFT_BLACK);
        tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, TFT_GREEN);
    }

    constexpr int16_t START_TEXT_Y_OFFSET = -3;

    void drawStartText(TFT_eSPI& tft, const Rect& r, uint16_t color) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);
        tft.setTextSize(2);
        int16_t cx = r.x + r.w / 2;
        int16_t cy = (int16_t)(r.y + r.h / 2 + START_TEXT_Y_OFFSET);
        tft.drawString("Start", cx, cy);
        tft.drawString("Start", (int16_t)(cx + 1), cy);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);
    }

    uint8_t breathePhase = 0;
    uint32_t lastBreatheMs = 0;
    constexpr uint32_t BREATHE_INTERVAL_MS = 20;
    constexpr uint8_t BREATHE_SPEED = 2;
    constexpr uint8_t BREATHE_LOW_R = 0,   BREATHE_LOW_G = 170, BREATHE_LOW_B = 0;
    constexpr uint8_t BREATHE_HIGH_R = 110, BREATHE_HIGH_G = 255, BREATHE_HIGH_B = 110;

    void updateBreathe(TFT_eSPI& tft, const Rect& btn) {
        uint32_t now = millis();
        if (now - lastBreatheMs < BREATHE_INTERVAL_MS) return;
        lastBreatheMs = now;

        breathePhase = (uint8_t)(breathePhase + BREATHE_SPEED);
        uint8_t tri = (breathePhase < 128) ? (uint8_t)(breathePhase * 2)
                                            : (uint8_t)((255 - breathePhase) * 2);
        uint8_t r = (uint8_t)(BREATHE_LOW_R + ((BREATHE_HIGH_R - BREATHE_LOW_R) * (uint16_t)tri) / 255);
        uint8_t g = (uint8_t)(BREATHE_LOW_G + ((BREATHE_HIGH_G - BREATHE_LOW_G) * (uint16_t)tri) / 255);
        uint8_t b = (uint8_t)(BREATHE_LOW_B + ((BREATHE_HIGH_B - BREATHE_LOW_B) * (uint16_t)tri) / 255);
        drawStartText(tft, btn, tft.color565(r, g, b));
    }

    // Lade-Punkte: ersetzen den "Start"-Text im Button, waehrend die
    // SD-Karte im Hintergrund angefasst wird (Ordnerstruktur anlegen) -
    // 1:1 aus dem Flightradar-Original uebernommen, verhindert dort
    // dokumentierte Doppel-Tap-/Touch-Rest-Probleme.
    void drawLoadingDots(TFT_eSPI& tft, const Rect& r, uint8_t litCount) {
        tft.fillRect((int16_t)(r.x + 2), (int16_t)(r.y + 2), (int16_t)(r.w - 4), (int16_t)(r.h - 4), TFT_BLACK);
        int16_t cy = (int16_t)(r.y + r.h / 2);
        constexpr int16_t SPACING = 16;
        int16_t startX = (int16_t)(r.x + r.w / 2 - SPACING);
        for (uint8_t i = 0; i < 3; i++) {
            int16_t dx = (int16_t)(startX + i * SPACING);
            if (i < litCount) {
                tft.fillCircle(dx, cy, 4, TFT_GREEN);
            } else {
                tft.drawCircle(dx, cy, 4, TFT_GREEN);
            }
        }
    }

    void runStartSequence(TFT_eSPI& tft, const Rect& r) {
        // SdStorage::createStructure()/seedDefaultDataFiles() werden
        // bewusst NICHT mehr hier aufgerufen - siehe main.cpp "erst
        // committen, dann speichern" (Alex' Auftrag, 05.10.): waehrend
        // der gesamten Ersteinrichtung darf noch NICHTS auf die SD-Karte
        // geschrieben werden, auch nicht nur der leere Ordner. Beide
        // Aufrufe passieren jetzt als erster Schritt des finalen Commits
        // (siehe main.cpp, ausgeloest vom Wisch-Erklaer-Screen). Die
        // Lade-Punkte-Animation bleibt trotzdem (kurze Pausen statt
        // echter Arbeit), damit der Button optisch unveraendert wirkt.
        drawLoadingDots(tft, r, 0);
        delay(150);
        drawLoadingDots(tft, r, 1);
        delay(150);
        drawLoadingDots(tft, r, 2);
        delay(150);
        drawLoadingDots(tft, r, 3);
        delay(150);

        while (TouchInput::rawPoint().touched) {
            delay(10);
        }
        delay(150);
    }
}

void run(TFT_eSPI& tft) {
    MenuStars::reset();
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);

    int16_t textEndY = drawCenteredWrappedBold(tft, (int16_t)(Config::SCREEN_WIDTH / 2), 14,
                                                 (int16_t)(Config::SCREEN_WIDTH - 20),
                                                 24, 2, TITLE);

    tft.setTextSize(2);
    int16_t startTextW = tft.textWidth("Start");
    tft.setTextSize(1);
    constexpr int16_t BTN_PAD_X = 26;
    constexpr int16_t BTN_PAD_Y = 14;
    constexpr int16_t BTN_TEXT_H = 16;
    constexpr int16_t BTN_MARGIN_BOTTOM = 16;
    int16_t btnW = (int16_t)(startTextW + 1 + BTN_PAD_X * 2);
    int16_t btnH = (int16_t)(BTN_TEXT_H + BTN_PAD_Y * 2);
    int16_t btnY = (int16_t)(Config::SCREEN_HEIGHT - BTN_MARGIN_BOTTOM - btnH);
    int16_t btnX = (int16_t)((Config::SCREEN_WIDTH - btnW) / 2);
    Rect startBtn = {btnX, btnY, btnW, btnH};

    // Logo-Bitmap oben, Schriftzug "Eiswolfs Toolbox" darunter (siehe
    // Datei-Kommentar oben) - im verfuegbaren Platz zwischen Titel und
    // Start-Button, gleiches Platzierungsprinzip wie Flightradars
    // Radar-Grafik dort. Die Bitmap hat eine feste Pixelgroesse (siehe
    // toolbox_logo_bitmap.h) - anders als beim vorherigen Vektor-Icon
    // wird hier nichts mehr dynamisch skaliert, nur noch horizontal
    // zentriert und direkt unter dem Titel platziert. Schriftzug bewusst
    // kleiner (Size 1 statt 2) als beim alten Icon-Layout, da die Bitmap
    // (170x170) schon fast den gesamten verfuegbaren Platz braucht.
    int16_t availTop = (int16_t)(textEndY + 4);
    int16_t availBottom = (int16_t)(btnY - 8);

    int16_t bitmapX = (int16_t)(Config::SCREEN_WIDTH / 2 - ToolboxLogoBitmap::WIDTH / 2);
    tft.drawBitmap(bitmapX, availTop, ToolboxLogoBitmap::BITMAP,
                    ToolboxLogoBitmap::WIDTH, ToolboxLogoBitmap::HEIGHT,
                    TFT_GREEN);

    constexpr int16_t ICON_GAP = 6;
    constexpr int16_t LOGO_LINE_H = 16;
    int16_t textStartY = (int16_t)(availTop + ToolboxLogoBitmap::HEIGHT + ICON_GAP);
    if ((int16_t)(textStartY + 2 * LOGO_LINE_H) > availBottom) {
        textStartY = (int16_t)(availBottom - 2 * LOGO_LINE_H);
    }
    drawCenteredWrappedBold(tft, (int16_t)(Config::SCREEN_WIDTH / 2), textStartY,
                             (int16_t)(Config::SCREEN_WIDTH - 20), LOGO_LINE_H, 1, LOGO_TEXT);

    drawButtonFrame(tft, startBtn);
    drawStartText(tft, startBtn, TFT_GREEN);

    while (true) {
        TouchInput::Point tap;
        if (TouchInput::wasTapped(tap)) {
            if (startBtn.contains(tap.x, tap.y)) {
                runStartSequence(tft, startBtn);
                return;
            }
            continue;
        }
        MenuStars::update(tft);
        updateBreathe(tft, startBtn);
        delay(20);
    }
}

}
