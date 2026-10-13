#pragma once
#include <stdint.h>

// Sehr stark vereinfachte Weltkarte (Kontinent-Umrisse) als 64x32-Raster-
// Bitmap, generiert aus Natural-Earth-Daten (1:110m-Aufloesung) - 1:1
// identische Daten wie bei Eiswolfs Flightradar CYD (dortige
// world_map.h/.cpp, siehe CLAUDE.md "ISS-Radar: gepunktete Weltkarte"),
// hier uebernommen, weil Alex' Auftrag (06.10.) ausdruecklich auf das
// dortige Vorbild verweist. Jede Zeile ist ein 64-Bit-Wert; Bit (63-i)
// gesetzt = Landflaeche an Spalte i. Equirektangulare Rohdaten: Zeile 0 =
// Nordpol, Zeile 31 = Suedpol, Spalte 0 = -180 Grad Laenge, Spalte 63 =
// +180 Grad Laenge - ANDERS als bei Flightradar wird diese Rohdaten-
// Tabelle hier aber NICHT flach in den Kreis gequetscht, sondern per
// echter Azimutal-Projektion um den Beobachterstandort gezeichnet (siehe
// iss_radar_screen.cpp::drawWorldMap()), damit sie zur bestehenden
// Azimut/Elevation-Darstellung (Mitte=Zenit, Rand=Horizont) passt.
namespace WorldMap {
    constexpr uint8_t GRID_W = 64;
    constexpr uint8_t GRID_H = 32;
    extern const uint64_t ROWS[GRID_H];
}
