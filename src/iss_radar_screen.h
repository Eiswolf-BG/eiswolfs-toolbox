#pragma once
#include <TFT_eSPI.h>

// ISS-Live-Radar - Polardiagramm mit Mitte=Zenit (Elevation 90°) und
// Rand=Horizont (Elevation 0°), siehe CLAUDE.md "ISS-Radar" fuer die
// Konvention (anders als ein klassischer Distanz-Radar, bei dem die Mitte
// der eigene Standort waere). Verlassen wird der Screen per Wisch-Geste
// (siehe app_navigation.h) - kein Zurueck-Button mehr.
namespace IssRadarScreen {
    // Einmalig beim Betreten (bzw. beim Zurueckkehren von den
    // Einstellungen) aufrufen - zeichnet den Screen mit dem zuletzt
    // bekannten Stand, ohne einen neuen Netzwerk-Abruf zu erzwingen.
    void enter(TFT_eSPI& tft);
    // In der zentralen Wisch-/Tap-Schleife (app_navigation.cpp) aufrufen,
    // solange dieser Screen aktiv ist - fragt die ISS-Position selbst
    // periodisch neu ab (alle Config::ISS_REFETCH_INTERVAL_MS). Das
    // kurze Blockieren waehrend des HTTPS-Requests ist in Kauf genommen
    // (kein Dual-Core-Netzwerk-Task wie im Flightradar-Projekt, siehe
    // CLAUDE.md "Tech-Stack").
    void update(TFT_eSPI& tft);
    // Tap auf den "?"-Infobutton oeffnet den Erklaer-Screen.
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
