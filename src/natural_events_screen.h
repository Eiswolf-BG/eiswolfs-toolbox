#pragma once
#include <TFT_eSPI.h>

// Naturereignisse-Screen (Alex' Auftrag, 12.10.) - NASA EONET, direkt nach
// dem Kalender-Screen im Wisch-Karussell. Gleiche enter()/update()/
// handleTap()-Schnittstelle wie jeder andere Feature-Screen (siehe
// app_navigation.cpp::FEATURES[]).
namespace NaturalEventsScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
