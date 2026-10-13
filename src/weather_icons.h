#pragma once
#include <TFT_eSPI.h>

// Farbige Wetter-Icons nach WMO-Wettercode (Open-Meteo liefert den
// international genormten Code, siehe weather_client.h) - EINE zentrale
// Zeichenfunktion, auf dem Hauptscreen, der 3-Stunden-Leiste und der
// Wochenuebersicht gleich verwendet (Alex' Auftrag, 06.10.), damit sich
// die Icons nie zwischen den Stellen unterscheiden. Alle Formen werden
// mit TFT_eSPI-Grundformen gezeichnet (keine eingebetteten Bitmaps) -
// skaliert proportional zu "size" (siehe weather_icons.cpp fuer die
// WMO-Code->Kategorie-Zuordnung).
namespace WeatherIcons {
    // (cx, cy) = Mittelpunkt, size = ungefaehre Gesamtbreite/-hoehe in
    // Pixeln (die Funktion selbst zeichnet nichts ausserhalb eines
    // size x size grossen Quadrats um (cx, cy)) - Aufrufer muss dafuer
    // genug Platz reservieren (CLAUDE.md "Pflicht: Elemente muessen immer
    // in ihren Platz passen").
    void draw(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t size, uint8_t wmoCode, bool isDay);

    // Einzelnes Sonnen-/Mondsichel-Symbol in einer FEST uebergebenen
    // Farbe, unabhaengig vom WMO-Code-Icon-Satz oben (Alex' Auftrag,
    // 07.10.) - z.B. fuer Sonnenaufgang/-untergang, wo die Form selbst
    // (nicht der Wettercode) die Bedeutung traegt.
    void drawSunIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color);
    void drawMoonIcon(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t r, uint16_t color, uint16_t bgColor);
}
