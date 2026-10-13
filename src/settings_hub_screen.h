#pragma once
#include <TFT_eSPI.h>

// Zentraler Einstellungs-Hub "System" (Alex' Auftrag, 06.10. - kompletter
// Neubau: ersetzt die vorherigen wischbaren Einstellungs-Themenseiten).
// EIN Screen mit 6 grossen Buttons (2 Reihen zu 3), jeder oeffnet sein
// eigenes Untermenue als blockierenden Aufruf (siehe handleTap()) - KEINE
// Wisch-Navigation mehr innerhalb der Einstellungen, nur noch Tippen.
// Gleiches enter()/update()/handleTap()-Muster wie die Feature-Screens
// (siehe app_navigation.h), da app_navigation.cpp diesen EINEN Hub direkt
// anspricht (kein SETTINGS_SCREENS[]-Array mehr noetig).
namespace SettingsHubScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
