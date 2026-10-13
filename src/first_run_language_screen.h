#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace FirstRunLanguageScreen {
    // Einmaliger Sprachauswahl-Screen beim allerersten Start - laeuft
    // NACH dem WLAN-Verbindungsaufbau und VOR der Standort-Einrichtung
    // (exakt dieselbe Reihenfolge wie bei Eiswolfs Flightradar CYD,
    // siehe main.cpp). Kein Zurueck-Button - ein Tap auf eine Sprache
    // speichert sie sofort und schliesst den Screen.
    void run(TFT_eSPI& tft);
}
