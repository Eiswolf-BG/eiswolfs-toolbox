#include "location_presets.h"
#include "config.h"
#include "sd_mutex.h"
#include "sd_storage.h"
#include <SD.h>
#include <cstring>

namespace LocationPresets {

namespace {
    // BUGFIX (05.10., Alex' Meldung "Kein Standort eingerichtet" trotz
    // vorhandenem Flightradar-Ordner): Diese Konstante war beim Portieren
    // aus Eiswolfs Flightradar CYD versehentlich auf DESSEN eigenen SD-
    // Pfad hartkodiert stehengeblieben (Kopierfehler), statt wie
    // wifi_manager.cpp korrekt Config::SD_LOCATIONS_FILE ("/Toolbox_cyd/
    // locations.txt") zu verwenden. Dadurch las LocationPresets::init()
    // bei JEDEM Start direkt aus Flightradars eigenem Ordner, statt aus
    // der eigens dafuer angelegten Toolbox-Kopie - und wenn Flightradars
    // locations.txt selbst keine (gueltigen) Presets enthielt, blieb
    // count() bei 0, obwohl die Uebernahme eigentlich serviceable
    // Koordinaten haette liefern sollen.
    constexpr const char* PRESETS_FILE = Config::SD_LOCATIONS_FILE;

    struct Preset {
        double lat = 0;
        double lon = 0;
        // Optionaler, vom Nutzer vergebener Name (z.B. "Zuhause") oder vom
        // naechstgelegenen Flughafen uebernommen - leer, wenn keiner gesetzt
        // wurde. 16 Zeichen + Nullterminierung reichen fuer eine gut lesbare
        // Zeile in der Preset-Liste.
        char name[17] = {0};
    };

    Preset presets[MAX_PRESETS];
    uint8_t presetCount = 0;
    int8_t activeIdx = -1;
    Source locationSource = Source::Unknown;
    // 2 Zeichen + Nullterminierung - siehe location_presets.h-Kommentar.
    char countryCodeBuf[3] = {0};

    const char* sourceToStr(Source s) {
        switch (s) {
            case Source::Address: return "address";
            case Source::Wifi: return "wifi";
            default: return "unknown";
        }
    }

    Source sourceFromStr(const String& s) {
        if (s == "address") return Source::Address;
        if (s == "wifi") return Source::Wifi;
        return Source::Unknown;
    }

    void saveToSd() {
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        File f = SD.open(PRESETS_FILE, FILE_WRITE);
        if (!f) return;
        f.printf("active=%d,source=%s,country=%s\n", (int)activeIdx, sourceToStr(locationSource), countryCodeBuf);
        for (uint8_t i = 0; i < presetCount; i++) {
            f.printf("%.6f,%.6f,%s\n", presets[i].lat, presets[i].lon, presets[i].name);
        }
        f.close();
    }

    // path: beliebiger Dateipfad im gleichen Format - oeffentlich ueber
    // LocationPresets::loadFromPath() nutzbar (Flightradar-Uebernahme-
    // Erkennung), intern auch fuer den eigenen Toolbox-Pfad genutzt.
    void loadFromPathInternal(const char* path) {
        presetCount = 0;
        activeIdx = -1;
        locationSource = Source::Unknown;
        countryCodeBuf[0] = 0;
        if (!SdStorage::isMounted()) return;
        SdMutex::Guard guard;

        if (!SD.exists(path)) return;
        File f = SD.open(path, FILE_READ);
        if (!f) return;

        bool firstLine = true;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() == 0) continue;

            if (firstLine) {
                firstLine = false;
                if (line.startsWith("active=")) {
                    // Zusatzfeld ",source=..." optional (fehlt z.B. bei einer
                    // 1:1 kopierten Flightradar-locations.txt) - dann bleibt
                    // locationSource auf Unknown (bereits oben zurueckgesetzt).
                    int commaPos = line.indexOf(',');
                    String activePart = (commaPos < 0) ? line.substring(7) : line.substring(7, commaPos);
                    activeIdx = (int8_t)activePart.toInt();
                    int srcPos = line.indexOf("source=");
                    if (srcPos >= 0) locationSource = sourceFromStr(line.substring(srcPos + 7));
                    // Drittes Zusatzfeld ",country=.." optional (fehlt bei
                    // aelteren/fremden Dateien ohne dieses Feld) - dann
                    // bleibt countryCodeBuf leer (bereits oben zurueckgesetzt).
                    int countryPos = line.indexOf("country=");
                    if (countryPos >= 0) {
                        String cc = line.substring(countryPos + 8);
                        int ccComma = cc.indexOf(',');
                        if (ccComma >= 0) cc = cc.substring(0, ccComma);
                        strncpy(countryCodeBuf, cc.c_str(), 2);
                        countryCodeBuf[2] = 0;
                    }
                    continue;
                }
            }

            int comma1 = line.indexOf(',');
            if (comma1 < 0) continue;
            if (presetCount >= MAX_PRESETS) break;

            // Der Name ist ein optionales drittes Feld - aeltere,
            // gespeicherte Presets (vor diesem Feature) haben nur
            // "lat,lon" ohne zweites Komma, dann bleibt der Name leer.
            int comma2 = line.indexOf(',', comma1 + 1);
            presets[presetCount].lat = line.substring(0, comma1).toDouble();
            if (comma2 < 0) {
                presets[presetCount].lon = line.substring(comma1 + 1).toDouble();
                presets[presetCount].name[0] = 0;
            } else {
                presets[presetCount].lon = line.substring(comma1 + 1, comma2).toDouble();
                String nm = line.substring(comma2 + 1);
                strncpy(presets[presetCount].name, nm.c_str(), sizeof(presets[presetCount].name) - 1);
            }
            presetCount++;
        }
        f.close();

        if (activeIdx >= (int8_t)presetCount) activeIdx = -1;
    }
}

void init() {
    loadFromPathInternal(PRESETS_FILE);
}

void loadFromPath(const char* path) {
    loadFromPathInternal(path);
}

void commit() {
    saveToSd();
}

uint8_t count() { return presetCount; }

void getLatLon(uint8_t index, double& lat, double& lon) {
    if (index >= presetCount) return;
    lat = presets[index].lat;
    lon = presets[index].lon;
}

String getName(uint8_t index) {
    if (index >= presetCount) return String();
    return String(presets[index].name);
}

bool addPreset(double lat, double lon, const String& name) {
    if (presetCount >= MAX_PRESETS) return false;
    presets[presetCount].lat = lat;
    presets[presetCount].lon = lon;
    strncpy(presets[presetCount].name, name.c_str(), sizeof(presets[presetCount].name) - 1);
    presetCount++;
    return true;
}

void removePreset(uint8_t index) {
    if (index >= presetCount) return;
    for (uint8_t i = index; i < presetCount - 1; i++) {
        presets[i] = presets[i + 1];
    }
    presetCount--;
    presets[presetCount] = Preset{};

    if (activeIdx == (int8_t)index) {
        activeIdx = -1;
    } else if (activeIdx > (int8_t)index) {
        activeIdx--;
    }
}

int8_t activeIndex() { return activeIdx; }

void setActiveIndex(int8_t index) {
    if (index >= (int8_t)presetCount) return;
    activeIdx = index;
}

void setSource(Source s) { locationSource = s; }
Source source() { return locationSource; }

void setCountryCode(const String& code) {
    String lower = code;
    lower.toLowerCase();
    strncpy(countryCodeBuf, lower.c_str(), 2);
    countryCodeBuf[2] = 0;
}
String countryCode() { return String(countryCodeBuf); }

}
