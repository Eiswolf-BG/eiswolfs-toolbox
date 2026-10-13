#pragma once
#include <Arduino.h>

// Reduzierte Variante des Flightradar-Originals - nur die Funktionen,
// die der uebernommene Ersteinrichtungs-Ablauf tatsaechlich braucht.
// Flightradar-spezifisches (Airline-/Flugzeugtyp-CSV-Seeding, Logbuch-
// Ordner) bewusst NICHT mit uebernommen, siehe sd_storage.cpp.
namespace SdStorage {
    bool init();
    bool isMounted();

    // Legt den Toolbox-SD-Ordner an (siehe Config::SD_ROOT_DIR) -
    // wird beim Start der Ersteinrichtung (first_run_welcome_screen.cpp)
    // aufgerufen, NICHT schon beim allerersten Boot, damit vor dem
    // Antippen von "Start" nichts auf der Karte angelegt wird.
    void createStructure();

    // Aktuell ein No-Op (siehe sd_storage.cpp) - Platzhalter fuer
    // kuenftige Standard-Datendateien, falls das Projekt mal welche
    // braucht. Bleibt als Aufruf bestehen, damit der uebernommene
    // Ersteinrichtungs-Ablauf (first_run_welcome_screen.cpp) unveraendert
    // funktioniert.
    void seedDefaultDataFiles();
}
