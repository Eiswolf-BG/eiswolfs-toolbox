#pragma once
#include "touch_input.h"

// Erkennt Wisch-Gesten (links/rechts/hoch/runter) zusaetzlich zu reinen
// Taps, als Grundlage fuer die Wisch-Navigation (siehe CLAUDE.md
// "Navigation: Wisch-Gesten statt Menue"). Baut auf TouchInput auf,
// verfolgt aber selbst den Beruehrungs-START-Punkt (TouchInput kennt
// nur den Loslass-Punkt), um die Wisch-Richtung zu bestimmen.
namespace SwipeInput {
    enum class Direction { None, Left, Right, Up, Down };

    // Einmal pro Hauptschleifen-Durchlauf aufrufen. Liefert true genau
    // einmal, wenn eine Beruehrung beendet wurde (Loslassen) - dir gibt
    // dann an, ob es eine Wisch-Geste war (Links/Rechts/Hoch/Runter) oder
    // nur ein Tap (Direction::None, outPoint ist dann der Tap-Punkt fuer
    // z.B. Button-Treffer).
    bool poll(Direction& dir, TouchInput::Point& outPoint);
}
