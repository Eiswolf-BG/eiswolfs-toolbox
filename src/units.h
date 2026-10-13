#pragma once
#include <Arduino.h>

// Globale Einheiten-Einstellung (Metrisch/Imperial) - Alex' Auftrag,
// 07.10.: die App laeuft weltweit (oeffentliches GitHub-Repo), km/h bzw.
// °C bedeuten vielen Nutzern (z.B. in den USA) nichts. EXAKT nach
// demselben Muster wie I18n (i18n.h/.cpp) - eine zentrale, global
// auslesbare Funktion statt einer Pro-Screen-Loesung, damit jeder
// kuenftige neue Screen mit physikalischen Einheiten sie einfach mit
// abfragen kann (siehe CLAUDE.md "Pflicht: globale Einheiten-
// Einstellung beachten" - STANDARDREGEL fuer alle kuenftigen Screens).
namespace Units {
    enum class System : uint8_t { Metric = 0, Imperial = 1 };

    // Laedt die zuletzt gespeicherte Einheit von der SD-Karte (siehe
    // Config::SD_UNITS_FILE) - faellt auf Metrisch zurueck, falls keine
    // Datei existiert oder sie ungueltig ist. Einmal beim Boot aufrufen,
    // vor dem ersten Netzwerk-Abruf, der Einheiten braucht.
    void init();

    System current();

    // Setzt NUR den RAM-Zustand, OHNE zu speichern (Alex' Auftrag,
    // 05.10.-Konvention - "erst committen, dann speichern") - fuer die
    // Ersteinrichtung (first_run_units_screen.cpp), wo noch nichts auf
    // die SD-Karte geschrieben werden darf.
    void setPreview(System s);
    // Schreibt den aktuellen (ggf. nur per setPreview() gesetzten)
    // Zustand auf die SD-Karte - der eigentliche "Commit"-Schritt.
    void save();
    // Setzt die Einheit UND speichert sie sofort - fuer die Verwendung
    // NACH der Ersteinrichtung (settings_units_screen.cpp, Einstellungen
    // speichern grundsaetzlich sofort statt erst bei einem spaeteren
    // Commit wie in der Ersteinrichtung).
    void set(System s);

    // ---- Open-Meteo-API-Parameterwerte (live per curl verifiziert,
    // 07.10.) - direkt als Query-Parameter verwendbar, siehe
    // weather_client.cpp. Open-Meteo rechnet serverseitig um, der Client
    // rechnet selbst NICHTS um (Alex' Auftrag). ----
    const char* temperatureUnitParam();   // "celsius" / "fahrenheit"
    const char* windSpeedUnitParam();     // "kmh" / "mph"
    const char* precipitationUnitParam(); // "mm" / "inch"

    // ---- Anzeige-Suffixe/-Kuerzel fuer Screens, die ihre Werte selbst
    // als Text zusammenbauen (API liefert nur die Zahl in der
    // gewaehlten Einheit, keinen passenden Einheiten-Text mit). Reine
    // Einheitenzeichen (wie "km/h"/"mm" anderswo im Projekt), keine
    // Woerter - brauchen deshalb keine eigene Uebersetzung/StringId. ----
    const char* temperatureLetter();   // "C" / "F" (hinter das Gradzeichen)
    const char* windSpeedSuffix();     // "km/h" / "mph"
    const char* precipitationSuffix(); // "mm" / "in"
    const char* distanceSuffix();      // "km" / "mi" (fuer kuenftige Screens mit Entfernungsangaben)

    // ---- Lokale Umrechnung fuer Screens OHNE Server-Unterstuetzung
    // (z.B. iss_radar_screen.cpp - die ISS-Position kommt aus einer
    // eigenen Bahnmechanik-Berechnung, nicht von einer API mit
    // Einheiten-Parameter wie bei Open-Meteo). Gibt den Wert bei
    // Metrisch unveraendert zurueck, bei Imperial umgerechnet. Zwei
    // separat benannte Funktionen fuer Lesbarkeit am Aufrufort, obwohl
    // der Umrechnungsfaktor (km->mi) fuer beide identisch ist. ----
    double distanceKmToDisplay(double km);
    double speedKmhToDisplay(double kmh);
}
