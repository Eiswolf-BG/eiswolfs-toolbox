#pragma once
#include <TFT_eSPI.h>

// Wetter-Screen - aktuelles Wetter + stuendliche Kurzvorhersage +
// Sonnenaufgang/-untergang am Standort, mit einer "Wochenuebersicht"-
// Unterseite fuer die naechsten Tage (Open-Meteo API). Vierter
// Feature-Screen im Wisch-Karussell (siehe app_navigation.h) - gleiches
// enter()/update()/handleTap()-Muster wie die anderen drei.
namespace WeatherScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
