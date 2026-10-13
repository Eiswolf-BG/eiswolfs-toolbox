#pragma once
#include <Arduino.h>

// ISS-Positionsabfrage (wheretheiss.at) und Standard-Beobachter-
// Trigonometrie (Azimut/Elevation/Entfernung) fuer den ISS-Radar-Screen -
// siehe CLAUDE.md "ISS-Radar" und iss_radar_screen.h fuer die Darstellung.
namespace IssClient {
    struct Position {
        double latitude = 0.0;
        double longitude = 0.0;
        double altitudeKm = 0.0;
        double velocityKmh = 0.0;
        // Rohwert aus der API ("daylight"/"eclipsed") - Uebersetzung ins
        // Deutsche passiert im Radar-Screen selbst (siehe
        // ISS_RADAR_VISIBILITY_*-StringIds).
        String visibility;
    };

    struct LookAngles {
        double azimuthDeg = 0.0;   // 0-360, 0=Norden, im Uhrzeigersinn
        double elevationDeg = 0.0; // -90..+90, >0 = ueber dem Horizont
        double rangeKm = 0.0;
    };

    // Einmaliger blockierender HTTPS-Request - true bei Erfolg.
    bool fetch(Position& out);

    // Standard-ENU-Look-Angle-Berechnung: Beobachter (Oberflaeche,
    // Hoehe=0) und Satellit (Subpunkt + Hoehe) werden in ECEF-Koordinaten
    // umgerechnet, die Differenz wird ins lokale Ost-Nord-Oben-System des
    // Beobachters rotiert.
    LookAngles computeLookAngles(double observerLatDeg, double observerLonDeg,
                                  double satLatDeg, double satLonDeg, double satAltKm);
}
