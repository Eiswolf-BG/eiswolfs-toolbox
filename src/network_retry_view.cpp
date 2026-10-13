#include "network_retry_view.h"
#include "config.h"
#include "i18n.h"

namespace NetworkRetryView {

namespace {
    // Wortweiser, zentrierter Umbruch anhand echter Pixelbreite (wie
    // ueberall sonst im Projekt, hier lokal fuer diese EINE gemeinsame
    // Komponente - CLAUDE.md "Pflicht: Textbreite in allen 4 Sprachen
    // pruefen" gilt dafuer trotzdem, siehe Aufrufer-Texte).
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
}

Rect draw(TFT_eSPI& tft, int16_t chromeH, const String& errorText) {
    tft.fillRect(0, chromeH, Config::SCREEN_WIDTH, (int16_t)(Config::SCREEN_HEIGHT - chromeH), TFT_BLACK);

    // 190x44 - deutlich ueber der 31x31-Mindestgroesse, komfortabel mit
    // dem Daumen treffbar (Alex' Auftrag: "gern groesser, gut antippbar").
    constexpr int16_t BTN_W = 190;
    constexpr int16_t BTN_H = 44;
    Rect btn = {(int16_t)((Config::SCREEN_WIDTH - BTN_W) / 2),
                (int16_t)(Config::SCREEN_HEIGHT / 2 + 30), BTN_W, BTN_H};

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    drawWrappedCentered(tft, (int16_t)(Config::SCREEN_WIDTH / 2), (int16_t)(Config::SCREEN_HEIGHT / 2 - 10),
                         (int16_t)(Config::SCREEN_WIDTH - 40), 16, errorText);

    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 4, TFT_BLACK);
    tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 4, TFT_GREEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString(I18n::t(StringId::NETWORK_RETRY_BTN), (int16_t)(btn.x + btn.w / 2), (int16_t)(btn.y + btn.h / 2));
    tft.setTextDatum(TL_DATUM);

    return btn;
}

}
