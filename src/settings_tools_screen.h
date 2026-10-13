#pragma once
#include <TFT_eSPI.h>

// "Tools"-Untermenue des Einstellungs-Hubs (settings_hub_screen.h) -
// Alex' Auftrag, 06.10.: komplett neu, aktuell nur ein Platzhalter ohne
// echte Funktionen, wird spaeter gefuellt. Blockierend, kehrt per Tap
// auf den Zurueck-Button zum Hub zurueck.
namespace SettingsToolsScreen {
    void run(TFT_eSPI& tft);
}
