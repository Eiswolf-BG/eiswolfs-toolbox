#pragma once
#include <TFT_eSPI.h>

// "Einheiten"-Untermenue, eigener Hauptbutton im Einstellungs-Hub
// (Alex' Auftrag, 07.10., siehe CLAUDE.md "Globale Einheiten-
// Einstellung") - bis 07.10. ein Unterabschnitt in Einstellungen ->
// Tools, auf Alex' Wunsch dort entfernt und als eigenstaendiger
// 6. Hub-Button an die Stelle des zuvor entfernten "Farben"-Buttons
// gesetzt (siehe settings_hub_screen.cpp). Zwei grosse Optionen
// (Metrisch/Imperial), gleiches Grundmuster wie settings_language_
// screen.h. Blockierend, kehrt per Tap auf den Zurueck-Button direkt
// zum Einstellungs-Hub zurueck.
namespace SettingsUnitsScreen {
    void run(TFT_eSPI& tft);
}
