#include "weather_icons.h"
#include <math.h>

namespace WeatherIcons {

namespace {
    // WMO-Wettercode -> grobe Darstellungs-Kategorie. Zuordnung nach Alex'
    // Vorgabe (06.10.): 0=klar, 1-2=teilweise bewoelkt (Sonne/Mond+Wolke),
    // 3=bedeckt (nur Wolke, keine Sonne/Mond - "Sonne+Wolke BZW. Wolke" aus
    // dem Auftrag woertlich genommen), 45/48=Nebel, 51-57=Niesel,
    // 61-67=Regen, 71-77=Schnee, 80-82=Regenschauer, 85/86=Schneeschauer,
    // 95-99=Gewitter. Unbekannte/neue Codes fallen auf eine schlichte
    // graue Wolke zurueck.
    enum Category : uint8_t {
        CLEAR, PARTLY, OVERCAST, FOG, DRIZZLE, RAIN, SNOW, RAIN_SHOWERS, SNOW_SHOWERS, THUNDERSTORM, UNKNOWN
    };

    Category categoryForCode(uint8_t code) {
        if (code == 0) return CLEAR;
        if (code == 1 || code == 2) return PARTLY;
        if (code == 3) return OVERCAST;
        if (code == 45 || code == 48) return FOG;
        if (code >= 51 && code <= 57) return DRIZZLE;
        if (code >= 61 && code <= 67) return RAIN;
        if (code >= 71 && code <= 77) return SNOW;
        if (code == 80 || code == 81 || code == 82) return RAIN_SHOWERS;
        if (code == 85 || code == 86) return SNOW_SHOWERS;
        if (code >= 95 && code <= 99) return THUNDERSTORM;
        return UNKNOWN;
    }

    // ---- Grundformen, alle relativ zu einem Mittelpunkt + Skalierung ----
    void drawSun(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        tft.fillCircle(cx, cy, r, color);
        for (int i = 0; i < 8; i++) {
            float ang = i * 0.7853981634f; // 8 Strahlen, 45 Grad Schritte
            int16_t x1 = (int16_t)(cx + cosf(ang) * (r + 2));
            int16_t y1 = (int16_t)(cy + sinf(ang) * (r + 2));
            int16_t x2 = (int16_t)(cx + cosf(ang) * (r + 5));
            int16_t y2 = (int16_t)(cy + sinf(ang) * (r + 5));
            tft.drawLine(x1, y1, x2, y2, color);
        }
    }

    // Mondsichel: voller Kreis, davon ein Teil mit der Hintergrundfarbe
    // "weggeschnitten" - einfacher als ein echtes Sichel-Polygon, auf
    // einem 240x320-TFT nicht unterscheidbar schlechter.
    void drawMoon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color, uint16_t bgColor) {
        tft.fillCircle(cx, cy, r, color);
        tft.fillCircle((int16_t)(cx + r * 0.55f), (int16_t)(cy - r * 0.35f), (int16_t)(r * 0.85f), bgColor);
    }

    // Klassische "3 ueberlappende Kreise + Basis-Rundrechteck"-Wolke.
    void drawCloud(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t w, int16_t h, uint16_t color) {
        int16_t r = (int16_t)(h / 2);
        tft.fillCircle((int16_t)(cx - w / 4), cy, r, color);
        tft.fillCircle((int16_t)(cx + w / 4), cy, r, color);
        tft.fillCircle(cx, (int16_t)(cy - r / 3), (int16_t)(r * 1.15f), color);
        tft.fillRoundRect((int16_t)(cx - w / 2), (int16_t)(cy - h / 5), w, (int16_t)(h * 0.7f),
                           (int16_t)(h / 3), color);
    }

    // Schraeger Doppelstrich - liest sich als Regentropfen-Streak.
    void drawRaindrop(TFT_eSPI& tft, int16_t x, int16_t y, int16_t len, uint16_t color) {
        tft.drawLine(x, y, (int16_t)(x - len / 3), (int16_t)(y + len), color);
        tft.drawLine((int16_t)(x + 1), y, (int16_t)(x - len / 3 + 1), (int16_t)(y + len), color);
    }

    // Kleiner 3-Linien-Stern (0/60/120 Grad) - einfache, klar erkennbare
    // Schneeflocke ohne Unicode-Sonderzeichen (siehe CLAUDE.md "Nur
    // ASCII-Ersatzzeichen statt Unicode").
    void drawSnowflake(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
        tft.drawLine((int16_t)(cx - r), cy, (int16_t)(cx + r), cy, color);
        tft.drawLine((int16_t)(cx - r * 0.7f), (int16_t)(cy - r * 0.7f),
                      (int16_t)(cx + r * 0.7f), (int16_t)(cy + r * 0.7f), color);
        tft.drawLine((int16_t)(cx - r * 0.7f), (int16_t)(cy + r * 0.7f),
                      (int16_t)(cx + r * 0.7f), (int16_t)(cy - r * 0.7f), color);
    }

    // Blitz als zwei gegenlaeufige Dreiecke - ergibt einen einfachen
    // Zickzack-Bolzen ohne Unicode-Blitzsymbol.
    void drawLightning(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t h, uint16_t color) {
        int16_t x0 = cx, y0 = (int16_t)(cy - h / 2);
        int16_t x1 = (int16_t)(cx - h / 6), y1 = cy;
        int16_t x2 = (int16_t)(cx + h / 6), y2 = cy;
        int16_t x3 = (int16_t)(cx - h / 8), y3 = (int16_t)(cy + h / 2);
        tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
        tft.fillTriangle(x1, y1, x2, y2, x3, y3, color);
    }

    inline int16_t sc(int16_t base, float factor) { return (int16_t)(base * factor); }
}

// Oeffentliche Huellen um drawSun()/drawMoon() oben (Alex' Auftrag,
// 07.10.) - fuer Stellen, die EXPLIZIT ein Sonnen-/Mondsymbol in einer
// FESTEN Farbe brauchen, losgeloest vom WMO-Code-Icon-Satz (z.B.
// Sonnenaufgang/-untergang im Wetter-Screen: dort bewusst NICHT die
// Akzentfarbe des Themes, damit auf den ersten Blick klar ist, welches
// Symbol Sonnenaufgang bzw. -untergang meint).
void drawSunIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color) {
    drawSun(tft, cx, cy, r, color);
}

void drawMoonIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color, uint16_t bgColor) {
    drawMoon(tft, cx, cy, r, color, bgColor);
}

void draw(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t size, uint8_t wmoCode, bool isDay) {
    float s = size / 32.0f;
    Category cat = categoryForCode(wmoCode);

    uint16_t sunColor = tft.color565(255, 200, 0);
    uint16_t moonColor = tft.color565(205, 210, 220);
    uint16_t cloudColor = tft.color565(225, 228, 232);
    uint16_t cloudDark = tft.color565(130, 140, 150);
    uint16_t rainColor = tft.color565(70, 150, 255);
    uint16_t snowColor = tft.color565(225, 238, 250);
    uint16_t stormCloud = tft.color565(95, 80, 115);
    uint16_t boltColor = tft.color565(255, 220, 40);

    switch (cat) {
        case CLEAR:
            if (isDay) drawSun(tft, cx, cy, sc(12, s), sunColor);
            else drawMoon(tft, cx, cy, sc(11, s), moonColor, TFT_BLACK);
            break;

        case PARTLY: {
            int16_t sx = (int16_t)(cx - sc(6, s));
            int16_t sy = (int16_t)(cy - sc(5, s));
            if (isDay) drawSun(tft, sx, sy, sc(8, s), sunColor);
            else drawMoon(tft, sx, sy, sc(7, s), moonColor, TFT_BLACK);
            drawCloud(tft, (int16_t)(cx + sc(3, s)), (int16_t)(cy + sc(4, s)), sc(26, s), sc(14, s), cloudColor);
            break;
        }

        case OVERCAST:
            drawCloud(tft, cx, cy, sc(30, s), sc(16, s), cloudColor);
            break;

        case FOG:
            drawCloud(tft, cx, (int16_t)(cy - sc(3, s)), sc(30, s), sc(14, s), cloudDark);
            for (int i = 0; i < 3; i++) {
                int16_t ly = (int16_t)(cy + sc(6, s) + i * sc(4, s));
                tft.drawFastHLine((int16_t)(cx - sc(13, s)), ly, sc(26, s), cloudDark);
            }
            break;

        case DRIZZLE:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(28, s), sc(14, s), cloudColor);
            drawRaindrop(tft, (int16_t)(cx - sc(6, s)), (int16_t)(cy + sc(6, s)), sc(5, s), rainColor);
            drawRaindrop(tft, (int16_t)(cx + sc(6, s)), (int16_t)(cy + sc(6, s)), sc(5, s), rainColor);
            break;

        case RAIN:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(30, s), sc(15, s), cloudColor);
            drawRaindrop(tft, (int16_t)(cx - sc(9, s)), (int16_t)(cy + sc(6, s)), sc(6, s), rainColor);
            drawRaindrop(tft, cx, (int16_t)(cy + sc(7, s)), sc(7, s), rainColor);
            drawRaindrop(tft, (int16_t)(cx + sc(9, s)), (int16_t)(cy + sc(6, s)), sc(6, s), rainColor);
            break;

        case SNOW:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(28, s), sc(14, s), snowColor);
            drawSnowflake(tft, (int16_t)(cx - sc(7, s)), (int16_t)(cy + sc(7, s)), sc(3, s), TFT_WHITE);
            drawSnowflake(tft, (int16_t)(cx + sc(7, s)), (int16_t)(cy + sc(7, s)), sc(3, s), TFT_WHITE);
            break;

        case RAIN_SHOWERS:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(30, s), sc(15, s), cloudDark);
            drawRaindrop(tft, (int16_t)(cx - sc(10, s)), (int16_t)(cy + sc(6, s)), sc(7, s), rainColor);
            drawRaindrop(tft, (int16_t)(cx - sc(2, s)), (int16_t)(cy + sc(8, s)), sc(8, s), rainColor);
            drawRaindrop(tft, (int16_t)(cx + sc(7, s)), (int16_t)(cy + sc(6, s)), sc(7, s), rainColor);
            drawRaindrop(tft, (int16_t)(cx + sc(12, s)), (int16_t)(cy + sc(8, s)), sc(6, s), rainColor);
            break;

        case SNOW_SHOWERS:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(30, s), sc(15, s), snowColor);
            drawSnowflake(tft, (int16_t)(cx - sc(9, s)), (int16_t)(cy + sc(7, s)), sc(3, s), TFT_WHITE);
            drawSnowflake(tft, cx, (int16_t)(cy + sc(9, s)), sc(4, s), TFT_WHITE);
            drawSnowflake(tft, (int16_t)(cx + sc(9, s)), (int16_t)(cy + sc(7, s)), sc(3, s), TFT_WHITE);
            break;

        case THUNDERSTORM:
            drawCloud(tft, cx, (int16_t)(cy - sc(4, s)), sc(30, s), sc(15, s), stormCloud);
            drawLightning(tft, cx, (int16_t)(cy + sc(7, s)), sc(16, s), boltColor);
            break;

        default:
            drawCloud(tft, cx, cy, sc(28, s), sc(14, s), cloudColor);
            break;
    }
}

}
