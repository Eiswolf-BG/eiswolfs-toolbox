#pragma once
#include <TFT_eSPI.h>

// Pflichtschritt in der Ersteinrichtung, DIREKT NACH der Sprachauswahl
// (Alex' Auftrag, 07.10., siehe CLAUDE.md "Globale Einheiten-
// Einstellung") - zwei grosse Optionen (Metrisch/Imperial). Anders als
// first_run_language_screen.cpp kann hier bereits die gewaehlte Sprache
// genutzt werden (Sprachauswahl laeuft vorher im Ablaufplan, siehe
// main.cpp::buildSetupPlan()).
namespace FirstRunUnitsScreen {
    void run(TFT_eSPI& tft);
}
