#pragma once
#include <Arduino.h>
#include "config.h"

namespace WifiMgr {
    enum class State {
        Idle, Connecting, Connected, Failed, NoCredentials
    };

    void init();
    void beginConnect();
    void connectTo(const char* ssid, const char* password);

    void update();
    State getState();
    const char* getIP();

    uint8_t networkCount();
    String networkSsid(uint8_t index);
    // hidden: true, wenn die SSID ueber den "Andere/versteckte SSID"-
    // Eingabepfad hinzugefuegt wurde (nicht aus einer Scan-Auswahl) -
    // siehe wifi_setup_screen.cpp. Steuert, ob beginConnect()/
    // tryReconnectAsync() beim (Wieder-)Verbinden einen Treffer im
    // normalen Netzwerk-Scan voraussetzen (versteckte SSIDs erscheinen
    // dort nie) oder direkt per WiFi.begin() mit explizitem SSID-
    // Parameter verbinden. Default false, damit bestehende Aufrufer
    // unveraendert funktionieren.
    //
    // WICHTIG (Alex' Auftrag, 05.10. - "erst committen, dann speichern"):
    // addNetwork()/removeNetwork() aendern NUR noch den RAM-Zustand,
    // schreiben NICHT mehr automatisch auf die SD-Karte. Das war bis
    // dahin anders (jede Aenderung wurde sofort gespeichert) - jetzt
    // MUSS der Aufrufer explizit commit() rufen, wenn die Aenderung
    // dauerhaft werden soll. Aktuell tut das ausschliesslich der
    // Wisch-Erklaer-Screen am Ende der Ersteinrichtung (main.cpp).
    bool addNetwork(const char* ssid, const char* password, bool hidden = false);
    void removeNetwork(uint8_t index);
    // Schreibt den aktuellen RAM-Zustand auf die SD-Karte (siehe oben).
    void commit();
    // Laedt WLAN-Zugangsdaten aus einer BELIEBIGEN Datei (gleiches Format
    // wie Config::SD_WIFI_CREDENTIALS_FILE) in den RAM-Zustand, OHNE
    // irgendwas auf die SD-Karte zu schreiben - fuer die Uebernahme-
    // Erkennung aus dem Flightradar-Ordner (siehe main.cpp), die vor dem
    // Commit-Zeitpunkt noch nichts auf der Toolbox-eigenen SD-Struktur
    // anlegen darf. Ueberschreibt den bisherigen RAM-Zustand komplett.
    void loadFromPath(const char* path);

    void beginScan();
    bool isScanComplete();
    int  getScanResultCount();
    String getScanResultSSID(int index);
    int32_t getScanResultRSSI(int index);
}