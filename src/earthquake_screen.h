#pragma once
#include <TFT_eSPI.h>

// Erdbeben-Screen (Alex' Auftrag, 07.10.) - zeigt das aktuell staerkste
// Erdbeben weltweit ab Magnitude 4,5 (US Geological Survey) mit Ort,
// Tiefe und Zeitpunkt. Sechster Feature-Screen im Wisch-Karussell (siehe
// app_navigation.h) - gleiches enter()/update()/handleTap()-Muster wie
// die uebrigen Feature-Screens.
namespace EarthquakeScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
