#pragma once
#include <TFT_eSPI.h>

// Raketenstart-Countdown - zeigt den naechsten bevorstehenden Start
// (Mission, Traegerrakete, Startplatz, Status) mit einem live laufenden
// Countdown bis zum geplanten Start (Launch Library 2 API). Zweiter
// Feature-Screen im Wisch-Karussell (siehe app_navigation.h) - gleiches
// enter()/update()/handleTap()-Muster wie iss_radar_screen.h.
namespace RocketScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
