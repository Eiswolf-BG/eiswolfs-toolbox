#pragma once
#include <TFT_eSPI.h>
#include "swipe_input.h"

// Zentrale Wisch-Navigation - ersetzt das fruehere scrollbare Hauptmenue
// (menu_screen.cpp, entfernt am 05.10. auf Alex' ausdruecklichen Wunsch).
// Siehe CLAUDE.md "Navigation: Wisch-Gesten statt Menue" fuer das
// komplette Konzept.
namespace AppNavigation {
    // Laeuft dauerhaft (kehrt nie zurueck). initialGesture: wird direkt
    // beim Start wie eine echte Wisch-Geste behandelt (statt immer mit
    // Feature 0 zu beginnen) - fuer den Fall, dass der Wisch-Erklaer-
    // Screen (swipe_intro_screen.cpp) gerade mit genau dieser Geste
    // verlassen wurde. Direction::None (Standardfall) startet wie bisher
    // auf Feature 0.
    void run(TFT_eSPI& tft, SwipeInput::Direction initialGesture = SwipeInput::Direction::None);
}
