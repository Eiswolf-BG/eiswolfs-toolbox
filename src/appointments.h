#pragma once
#include <Arduino.h>
#include "config.h"

// Eigene Termine im Kalender-Screen (Alex' Auftrag, 12.10.) - lokal auf
// der SD-Karte gespeichert (Config::SD_APPOINTMENTS_FILE), ueberlebt einen
// Neustart. Reiner Core-1-Zugriff (keine Netzwerk-Daten, kein NetTask
// noetig), gleiches Grundmuster wie units.h/currency.h (init() einmal beim
// Boot, danach synchron lesen/schreiben).
//
// Speicherformat: VIER Zeilen je Termin (Datum, Text, Uhrzeit, Erinnerung),
// KEIN Trennzeichen innerhalb einer Zeile noetig - das macht das Format
// robust gegen JEDES vom Nutzer eingegebene Zeichen (Alex' Auftrag: "robust
// gegen Sonderzeichen, falls zufaellig ein internes Trennzeichen
// getroffen wird"), da die virtuelle Tastatur ohnehin keine echte
// Zeilenumbruch-Taste anbietet - der Termintext kann also nie selbst eine
// Zeile beenden. Die Uhrzeit-Zeile ist entweder "HH:MM" oder das Kennzeichen
// "--:--" fuer "keine Uhrzeit gesetzt". Die Erinnerungs-Zeile ist die
// numerische ReminderType-Auspraegung (0-4) als Text.
//
// BUGFIX-HINWEIS: vor diesem Feld war das Format ZWEI (ohne Uhrzeit/
// Erinnerung) bzw. DREI Zeilen (ohne Erinnerung) je Termin - bereits
// gespeicherte Termine aus diesen frueheren Testphasen werden beim
// naechsten Laden falsch interpretiert (keine Migration eingebaut, da die
// Funktion selbst noch nicht veroeffentlicht war). Betroffen nur Alex'
// eigenes Testgeraet.
namespace Appointments {
    // Reihenfolge fest (wird numerisch gespeichert) - neue Optionen immer
    // VOR COUNT anhaengen, nie bestehende Werte umsortieren.
    enum class ReminderType : uint8_t {
        None = 0,
        Min10,
        Min30,
        Hour1,
        Day1,
        COUNT
    };

    struct Entry {
        char date[11] = {0}; // "YYYY-MM-DD"
        char text[Config::APPOINTMENT_TEXT_CAP] = {0};
        bool hasTime = false;
        uint8_t hour = 0;   // 0-23, nur gueltig wenn hasTime
        uint8_t minute = 0; // 0-59, nur gueltig wenn hasTime
        ReminderType reminder = ReminderType::None;
        // NUR im RAM, NIE auf der SD-Karte gespeichert/geladen (bleibt nach
        // jedem Boot false) - verhindert, dass dieselbe Erinnerung mehrfach
        // ausloest (Alex' Auftrag, 12.10.: "ein einfacher Merker im RAM
        // reicht, muss nicht auf der SD-Karte gespeichert werden"). Lebt
        // bewusst IM Entry (nicht in einem separaten, nach Array-Index
        // getrennten Array) - verschiebt sich dadurch automatisch korrekt
        // mit, wenn removeAt() vorherige Eintraege nach vorne rueckt.
        bool reminderFired = false;
    };

    // Laedt von der SD-Karte - einmal beim Boot aufrufen (main.cpp::setup()).
    void init();

    uint8_t count();
    const Entry& get(uint8_t index);

    // Fuegt einen neuen Termin hinzu und speichert SOFORT (Projekt-
    // Konvention: Einstellungen/Nutzerdaten speichern sofort, nicht erst
    // bei einem spaeteren Commit). hasTime=false ignoriert hour/minute.
    // false, wenn Config::APPOINTMENT_MAX_COUNT bereits erreicht ist.
    bool add(const char* date, const String& text, bool hasTime, uint8_t hour, uint8_t minute,
             ReminderType reminder);

    // Entfernt den Termin an "index" und speichert sofort.
    void removeAt(uint8_t index);

    // Markiert die Erinnerung des Termins an "index" als bereits
    // ausgeloest (RAM-only, siehe Entry::reminderFired - speichert NICHT
    // auf der SD-Karte).
    void markReminderFired(uint8_t index);
}
