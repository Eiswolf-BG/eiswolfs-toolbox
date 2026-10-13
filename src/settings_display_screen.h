#pragma once
#include <TFT_eSPI.h>

// "Anzeige"-Untermenue des Einstellungs-Hubs (settings_hub_screen.h) -
// Alex' Auftrag, 06.10.: entspricht inhaltlich der fruehreren "System"-
// Themenseite (Helligkeit, Screenflip, Bildschirm-Timeout), nur
// umbenannt und als Tap-only-Untermenue statt wischbarer Themenseite
// (die WLAN-Zeile ist dabei entfallen - WLAN ist jetzt ein eigener
// Hub-Button, siehe settings_wifi_manage_screen.h). Blockierend, kehrt
// per Tap auf den Zurueck-Button zum Hub zurueck - KEINE Wisch-
// Navigation mehr (Alex' Auftrag).
namespace SettingsDisplayScreen {
    void run(TFT_eSPI& tft);

    // Oeffentlich, da main.cpp (Boot) und app_navigation.cpp (Bildschirm-
    // Timeout-Ueberwachung in der zentralen Schleife) sie brauchen - siehe
    // dortige Kommentare.
    void loadSettings();
    void applyBacklight();
    void turnBacklightOff();
    uint32_t screenTimeoutMs();
}
