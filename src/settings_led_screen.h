#pragma once
#include <TFT_eSPI.h>

// "LED"-Untermenue des Einstellungs-Hubs (settings_hub_screen.h) - Alex'
// Auftrag, 06.10.: komplett neu, aktuell nur ein Platzhalter ohne echte
// Funktionen, wird spaeter gefuellt. Die eigentliche LED-Ansteuerung lebt
// seit 12.10. in led_control.h/.cpp (GPIO4/16/17, aktiv-LOW PWM, live am
// Geraet bestaetigt) - dieser Screen hier bekommt erst spaeter eigene
// Einstellungs-Funktionen (z.B. An/Aus-Schalter fuer einzelne Blink-
// Anlaesse). Blockierend, kehrt per Tap auf den Zurueck-Button zum Hub
// zurueck.
namespace SettingsLedScreen {
    void run(TFT_eSPI& tft);
}
