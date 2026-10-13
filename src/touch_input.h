#pragma once
#include <Arduino.h>

// 1:1 aus Eiswolfs Flightradar CYD uebernommen (gleiche Touch-Hardware,
// siehe CLAUDE.md) - einzige Aenderung: das dortige "TEST DIAG"-
// Rohdaten-Mitschreiben (lastRawSample()/RawSample), eine temporaere
// Diagnose-Ergaenzung fuer eine Flightradar-spezifische Fehlersuche,
// wurde hier nicht mit uebernommen.
namespace TouchInput {

    struct Point {
        int16_t x = 0;
        int16_t y = 0;
        bool touched = false;
    };

    void begin();

    // Muss gesetzt werden, wenn der Bildschirm um 180 Grad gedreht ist.
    void setRotated180(bool rotated);
    bool isRotated180();

    // Laedt/speichert den 180-Grad-Flip-Zustand (Config::
    // SD_DISPLAY_ROTATED_FILE, siehe settings_rotation_screen.cpp) - ruft
    // intern setRotated180() auf, betrifft aber NUR die Touch-Zuordnung.
    // Die eigentliche Bildschirmdrehung (tft.setRotation()) muss der
    // Aufrufer selbst setzen (main.cpp beim Boot, settings_rotation_screen.cpp
    // beim Umschalten) - TouchInput kennt das TFT-Objekt nicht.
    bool loadRotated180();
    void saveRotated180();

    // Laedt calibration.txt von der SD-Karte. Gibt false zurueck, wenn keine
    // vorhanden oder ungueltig ist.
    bool loadCalibration();
    void setCalibration(int16_t rawXmin, int16_t rawXmax, int16_t rawYmin, int16_t rawYmax);
    void saveCalibration();
    bool hasCalibration();

    // Laedt eine Kalibrierung von einem BELIEBIGEN Pfad direkt in den RAM-
    // Zustand, OHNE zu speichern (Flightradar-Uebernahme-Erkennung, siehe
    // main.cpp) - gleiches Prinzip wie WifiMgr::loadFromPath()/
    // LocationPresets::loadFromPath().
    bool loadFromPath(const char* path);

    // Aktueller Touch-Zustand, roh (unkalibriert).
    Point rawPoint();

    // Aktueller Touch-Zustand, auf Bildschirmkoordinaten umgerechnet und
    // geclampt.
    Point mappedPoint();

    // Liefert genau einmal pro physischer Beruehrung "true" (beim
    // Loslassen), mitsamt der zuletzt bekannten Position.
    bool wasTapped(Point& outPoint);

    // Millisekunden seit dem letzten abgeschlossenen Tap - fuer
    // Inaktivitaets-Timeouts in Vollbild-Screens.
    uint32_t msSinceLastTap();
}
