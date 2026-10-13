#pragma once
#include <TFT_eSPI.h>

// Formel-1-Screen (Alex' Auftrag, 06.10., siehe CLAUDE.md "Formel 1") -
// letzter Feature-Screen im Wisch-Karussell (FEATURES[] in
// app_navigation.cpp). Gleiche enter()/update()/handleTap()-Schnittstelle
// wie jeder andere Feature-Screen.
namespace F1Screen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
