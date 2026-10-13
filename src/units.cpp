#include "units.h"
#include <SD.h>
#include "config.h"

namespace Units {

namespace {
    System currentSystem = System::Metric;
    // 1 km = 0,621371 mi (gleicher Faktor fuer Geschwindigkeit, da reine
    // Verhaeltnis-Umrechnung) - amtlicher Umrechnungsfaktor, nicht
    // geraetet.
    constexpr double KM_TO_MI = 0.621371;
}

void init() {
    currentSystem = System::Metric;
    File f = SD.open(Config::SD_UNITS_FILE, FILE_READ);
    if (!f) return;
    String line = f.readStringUntil('\n');
    f.close();
    int idx = line.toInt();
    if (idx == 0 || idx == 1) {
        currentSystem = (System)idx;
    }
}

System current() { return currentSystem; }

void setPreview(System s) { currentSystem = s; }

void save() {
    File f = SD.open(Config::SD_UNITS_FILE, FILE_WRITE);
    if (!f) return;
    f.println((int)currentSystem);
    f.close();
}

void set(System s) {
    setPreview(s);
    save();
}

const char* temperatureUnitParam() {
    return currentSystem == System::Metric ? "celsius" : "fahrenheit";
}

const char* windSpeedUnitParam() {
    return currentSystem == System::Metric ? "kmh" : "mph";
}

const char* precipitationUnitParam() {
    return currentSystem == System::Metric ? "mm" : "inch";
}

const char* temperatureLetter() {
    return currentSystem == System::Metric ? "C" : "F";
}

const char* windSpeedSuffix() {
    return currentSystem == System::Metric ? "km/h" : "mph";
}

const char* precipitationSuffix() {
    return currentSystem == System::Metric ? "mm" : "in";
}

const char* distanceSuffix() {
    return currentSystem == System::Metric ? "km" : "mi";
}

double distanceKmToDisplay(double km) {
    return currentSystem == System::Metric ? km : km * KM_TO_MI;
}

double speedKmhToDisplay(double kmh) {
    return currentSystem == System::Metric ? kmh : kmh * KM_TO_MI;
}

}
