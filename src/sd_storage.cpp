#include "sd_storage.h"
#include "config.h"
#include "sd_mutex.h"
#include <SD.h>
#include <SPI.h>

namespace SdStorage {

namespace {
    bool mounted = false;
    SPIClass sdSpi(HSPI);

    bool ensureDir(const char* path) {
        if (SD.exists(path)) return true;
        return SD.mkdir(path);
    }
}

bool init() {
    SdMutex::init();
    SdMutex::Guard guard;

    sdSpi.begin(Config::SD_SPI_CLK_PIN, Config::SD_SPI_MISO_PIN,
                Config::SD_SPI_MOSI_PIN, Config::SD_SPI_CS_PIN);
    mounted = SD.begin(Config::SD_SPI_CS_PIN, sdSpi, 4000000);
    return mounted;
}

bool isMounted() { return mounted; }

void createStructure() {
    if (!mounted) return;
    SdMutex::Guard guard;
    ensureDir(Config::SD_ROOT_DIR);
}

void seedDefaultDataFiles() {
    // Aktuell nichts zu seeden - dieses Projekt hat (anders als
    // Flightradar mit seinen Airline-/Flugzeugtyp-Lookup-CSVs) noch keine
    // eigenen Standard-Datendateien. Bleibt als Funktion bestehen, damit
    // der uebernommene Ersteinrichtungs-Ablauf unveraendert funktioniert.
}

}
