#pragma once
#include <TFT_eSPI.h>
#include "swipe_input.h"

// Einmaliger Erklaer-Screen fuer die Wisch-Navigation, direkt nach
// Abschluss der Ersteinrichtung (siehe main.cpp, Config::
// SD_SWIPE_INTRO_DONE_FILE). Wird NICHT per Tap verlassen, nur durch
// eine echte Wisch-Geste - und zwar wird GENAU diese erste Geste vom
// Aufrufer (main.cpp/app_navigation.cpp) als die ERSTE echte
// Navigationsaktion behandelt, kein separates "weiter"-Antippen noetig.
namespace SwipeIntroScreen {
    // Blockiert, bis der Nutzer wischt - liefert die erkannte Richtung
    // zurueck (niemals Direction::None, reine Taps werden ignoriert).
    SwipeInput::Direction run(TFT_eSPI& tft);
}
