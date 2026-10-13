#pragma once
#include <TFT_eSPI.h>

// "WLAN"-Untermenue des Einstellungs-Hubs (settings_hub_screen.h) - vom
// Konzept 1:1 aus Eiswolfs Flightradar CYD uebernommen (dortige
// wifi_manage_screen.cpp): zeigt bis zu Config::MAX_WIFI_NETWORKS (3)
// gespeicherte Netzwerk-Slots, erlaubt das Entfernen einzelner Eintraege
// und (falls noch Platz ist) das Hinzufuegen eines neuen ueber den
// bestehenden Scan+Passwort-Dialog (wifi_setup_screen.h, derselbe wie in
// der Ersteinrichtung). Blockierend, kehrt per Tap auf den Zurueck-
// Button zum Hub zurueck. Speichert Aenderungen SOFORT ueber
// WifiMgr::commit() (anders als wifi_setup_screen.h selbst, das seit der
// "erst committen, dann speichern"-Umstellung nicht mehr automatisch
// speichert).
namespace SettingsWifiManageScreen {
    void run(TFT_eSPI& tft);
}
