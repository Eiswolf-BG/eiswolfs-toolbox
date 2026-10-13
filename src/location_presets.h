#pragma once
#include <Arduino.h>

namespace LocationPresets {
    constexpr uint8_t MAX_PRESETS = 3;

    void init();

    uint8_t count();
    void getLatLon(uint8_t index, double& lat, double& lon);
    // Vom Nutzer vergebener Name (z.B. "Zuhause") - leerer String, wenn
    // keiner gesetzt wurde (dann zeigt der Screen stattdessen die
    // Koordinaten an).
    String getName(uint8_t index);

    // WICHTIG (Alex' Auftrag, 05.10. - "erst committen, dann speichern"):
    // addPreset()/removePreset()/setActiveIndex() aendern NUR noch den
    // RAM-Zustand, schreiben NICHT mehr automatisch auf die SD-Karte.
    // Der Aufrufer MUSS explizit commit() rufen, wenn die Aenderung
    // dauerhaft werden soll. Aktuell tut das ausschliesslich der
    // Wisch-Erklaer-Screen am Ende der Ersteinrichtung (main.cpp).
    bool addPreset(double lat, double lon, const String& name = String());
    void removePreset(uint8_t index);

    int8_t activeIndex();
    void setActiveIndex(int8_t index);

    // Woher die AKTUELL aktive Position stammt (Alex' Auftrag, 06.10. -
    // fuer die neue "Adresse aendern"-Einstellungsseite: zeigt "ungefaehre
    // WLAN-Position" an, wenn keine Adresse eingegeben wurde). Persistiert
    // als Zusatzfeld in derselben Datei (siehe location_presets.cpp) -
    // faellt bei aelteren/fremden Dateien ohne dieses Feld (z.B. einer
    // 1:1 kopierten Flightradar-locations.txt) auf Unknown zurueck.
    enum class Source : uint8_t { Unknown, Address, Wifi };
    void setSource(Source s);
    Source source();

    // 2-Buchstaben-ISO-Laendercode (klein geschrieben, z.B. "de"/"gb"/"us")
    // der AKTUELLEN Toolbox-Position - Alex' Auftrag, 07.10., fuer die
    // standortabhaengige Waehrung im Krypto-Ticker (siehe currency.h).
    // Leerer String = unbekannt (z.B. aelteres/fremdes locations.txt ohne
    // dieses Feld, oder noch keine Position gesetzt) - Aufrufer fallen
    // dann auf USD zurueck. Wird von AddressSearchScreen::run()
    // (Nominatim liefert address.country_code) bzw.
    // first_run_location_screen.cpp::fetchIpLocation() (ip-api.com
    // liefert countryCode) gesetzt, genau wie setSource() NICHT
    // automatisch gespeichert - der jeweilige Aufrufer committed wie
    // gehabt selbst.
    void setCountryCode(const String& code);
    String countryCode();

    // Schreibt den aktuellen RAM-Zustand auf die SD-Karte (siehe oben).
    void commit();
    // Laedt Standort-Presets aus einer BELIEBIGEN Datei (gleiches Format
    // wie Config::SD_LOCATIONS_FILE) in den RAM-Zustand, OHNE irgendwas
    // auf die SD-Karte zu schreiben - fuer die Uebernahme-Erkennung aus
    // dem Flightradar-Ordner (siehe main.cpp). Ueberschreibt den
    // bisherigen RAM-Zustand komplett.
    void loadFromPath(const char* path);
}
