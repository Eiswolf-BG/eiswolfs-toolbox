#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

namespace AddressSearchScreen {
    // Adresse eingeben (mit Sonderzeichen-Tastatur) -> per Nominatim
    // (OpenStreetMap) geokodieren -> Ergebnis bestaetigen -> Namen vergeben
    // -> als neuen Standort-Preset speichern. Gibt true zurueck, wenn ein
    // Preset hinzugefuegt wurde, false bei Abbruch durch den Nutzer.
    bool run(TFT_eSPI& tft);

    // Wie run(), aber OHNE das Ergebnis als Standort-Preset zu speichern
    // oder zu aktivieren - liefert nur die bestaetigten Koordinaten zurueck
    // (Alex' Auftrag, 06.10. - fuer den OEPNV-Screen: "Genauere Adresse"
    // darf NICHT den allgemeinen Toolbox-Standort ueberschreiben, siehe
    // transit_screen.cpp). true bei Erfolg, false bei Abbruch.
    bool runForCoordinates(TFT_eSPI& tft, double& outLat, double& outLon);

    // Einmaliger Nachlade-Versuch fuer BESTEHENDE, bereits vor Einfuehrung
    // des countryCode-Felds gespeicherte Standorte (Alex' Auftrag, 07.10.:
    // "nicht jeder bestehende Nutzer soll den Standort neu einrichten
    // muessen") - fragt per Nominatim-Reverse-Geocoding anhand der
    // VORHANDENEN Koordinaten des aktiven Presets den Laendercode nach und
    // speichert ihn. Macht nichts, wenn bereits ein Laendercode vorhanden
    // ist, kein Preset existiert, oder kein WLAN verbunden ist - blockierend,
    // nur einmal beim Boot aus main.cpp::setup() aufgerufen (gleiche
    // Kategorie wie der bestehende NTP-Sync, kurzer Blockier-Call waehrend
    // des Boot-Vorgangs). Gibt true zurueck, wenn ein Laendercode
    // nachgetragen wurde.
    bool backfillCountryCodeIfMissing();
}
