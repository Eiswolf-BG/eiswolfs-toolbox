#pragma once

// Hintergrund-Pruefung fuer Termin-Erinnerungen (Alex' Auftrag, 12.10.,
// zweiter/letzter Teil der Termin-Erinnerung) - UNABHAENGIG davon, welcher
// Screen gerade aktiv ist (laeuft zentral in app_navigation.cpp mit, nicht
// an einen bestimmten Screen gebunden wie ISS-Radar/Raketenstart). Reiner
// Core-1-Zugriff (Appointments:: ist ohnehin nur lokale SD-Daten, kein
// NetTask noetig).
namespace AppointmentReminders {
    // Bei jedem Hauptschleifen-Tick aufrufen (app_navigation.cpp) - prueft
    // intern selbst (gedrosselt auf ca. einmal pro Minute, Alex' Auftrag),
    // ob fuer einen Termin mit gesetzter, noch nicht ausgeloester
    // Erinnerung der berechnete Zeitpunkt erreicht wurde. Falls ja: startet
    // ein 30-Sekunden-Blinken (LedControl::startTimedBlink()) und markiert
    // die Erinnerung als ausgeloest (Appointments::markReminderFired()) -
    // jede Erinnerung feuert dadurch nur genau einmal.
    void update();
}
