#pragma once
#include <TFT_eSPI.h>

// "Sprache"-Untermenue des Einstellungs-Hubs (settings_hub_screen.h) -
// inhaltlich unveraendert gegenueber der fruehreren Sprache-Themenseite,
// jetzt aber ohne Wisch-Navigation/unteren Streifen (Alex' Auftrag,
// 06.10.) - die Sprach-Buttons duerfen wieder die volle Bildschirmhoehe
// nutzen. Blockierend, kehrt per Tap auf den Zurueck-Button zum Hub
// zurueck.
namespace SettingsLanguageScreen {
    void run(TFT_eSPI& tft);
}
