#include "touch_input.h"
#include "config.h"
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <SD.h>

namespace TouchInput {

namespace {
    SPIClass touchSpi(VSPI);
    XPT2046_Touchscreen touch(Config::TOUCH_CS_PIN, Config::TOUCH_IRQ_PIN);

    constexpr int16_t MARGIN_PX = 24; // Abstand der Kalibrierungspunkte vom Rand

    // Rohe Min/Max-Werte aus der Kalibrierung. Bis zur ersten Kalibrierung
    // grosszuegige Standardwerte, damit auch ohne eigene Kalibrierungs-
    // Routine schon grob nutzbare Koordinaten herauskommen.
    int16_t calXmin = 200;
    int16_t calXmax = 3900;
    int16_t calYmin = 200;
    int16_t calYmax = 3900;
    bool calibrationLoaded = false;

    bool lastTouched = false;
    Point lastRaw;

    bool rotated180 = false;

    uint32_t lastTapMs = 0;

    int16_t clampi(int16_t v, int16_t lo, int16_t hi) {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }
}

void begin() {
    touchSpi.begin(Config::TOUCH_CLK_PIN, Config::TOUCH_MISO_PIN,
                    Config::TOUCH_MOSI_PIN, Config::TOUCH_CS_PIN);
    touch.begin(touchSpi);
    touch.setRotation(0);
    lastTapMs = millis();
}

void setRotated180(bool rotated) {
    rotated180 = rotated;
}

bool isRotated180() { return rotated180; }

bool loadRotated180() {
    if (!SD.exists(Config::SD_DISPLAY_ROTATED_FILE)) return false;
    File f = SD.open(Config::SD_DISPLAY_ROTATED_FILE, FILE_READ);
    if (!f) return false;
    String line = f.readStringUntil('\n');
    f.close();
    line.trim();
    bool rotated = (line.toInt() != 0);
    setRotated180(rotated);
    return rotated;
}

void saveRotated180() {
    File f = SD.open(Config::SD_DISPLAY_ROTATED_FILE, FILE_WRITE);
    if (!f) return;
    f.println(rotated180 ? "1" : "0");
    f.close();
}

bool hasCalibration() { return calibrationLoaded; }

void setCalibration(int16_t rawXmin, int16_t rawXmax, int16_t rawYmin, int16_t rawYmax) {
    calXmin = rawXmin;
    calXmax = rawXmax;
    calYmin = rawYmin;
    calYmax = rawYmax;
    calibrationLoaded = true;
}

// Pfad-parametrisierter Kern - von loadCalibration() (eigener Pfad) UND
// loadFromPath() (Flightradar-Uebernahme-Erkennung, siehe main.cpp
// detectAndLoadFlightradarData()) genutzt, gleiches Muster wie
// wifi_manager.cpp::loadFromPathInternal().
bool loadCalibrationFromPath(const char* path) {
    if (!SD.exists(path)) return false;

    File f = SD.open(path, FILE_READ);
    if (!f) return false;

    String line = f.readStringUntil('\n');
    f.close();
    line.trim();

    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    if (p1 < 0 || p2 < 0 || p3 < 0) return false;

    int16_t xmin = line.substring(0, p1).toInt();
    int16_t xmax = line.substring(p1 + 1, p2).toInt();
    int16_t ymin = line.substring(p2 + 1, p3).toInt();
    int16_t ymax = line.substring(p3 + 1).toInt();

    if (xmax <= xmin || ymax <= ymin) return false;

    setCalibration(xmin, xmax, ymin, ymax);
    return true;
}

bool loadCalibration() {
    return loadCalibrationFromPath(Config::SD_CALIBRATION_FILE);
}

// Liest Kalibrierungswerte von einem beliebigen Pfad (Flightradar-
// Uebernahme) direkt in den RAM-Zustand, OHNE irgendwas zu speichern -
// identisches Prinzip wie WifiMgr::loadFromPath()/LocationPresets::
// loadFromPath() (siehe main.cpp "Ersteinrichtung: Uebernahme von
// Flightradar-Daten"). Touch-Kalibrierung ist eine Eigenschaft der
// physischen Hardware, nicht der Firmware - auf demselben Geraet bereits
// per Flightradar ermittelt, kann sie 1:1 uebernommen werden, ohne die
// eigene (boot-zeit-rauschanfaellige) interaktive Kalibrierung zu
// durchlaufen.
bool loadFromPath(const char* path) {
    return loadCalibrationFromPath(path);
}

void saveCalibration() {
    File f = SD.open(Config::SD_CALIBRATION_FILE, FILE_WRITE);
    if (!f) return;
    f.printf("%d,%d,%d,%d\n", calXmin, calXmax, calYmin, calYmax);
    f.close();
}

Point rawPoint() {
    Point p;
    p.touched = touch.touched();
    if (p.touched) {
        TS_Point raw = touch.getPoint();
        p.x = raw.x;
        p.y = raw.y;
    }
    return p;
}

Point mappedPoint() {
    Point raw = rawPoint();
    Point out;
    out.touched = raw.touched;
    if (!raw.touched) return out;

    long mx = map((long)raw.x, calXmin, calXmax, MARGIN_PX, Config::SCREEN_WIDTH - MARGIN_PX);
    long my = map((long)raw.y, calYmin, calYmax, MARGIN_PX, Config::SCREEN_HEIGHT - MARGIN_PX);

    if (rotated180) {
        mx = (Config::SCREEN_WIDTH - 1) - mx;
        my = (Config::SCREEN_HEIGHT - 1) - my;
    }

    out.x = clampi((int16_t)mx, 0, Config::SCREEN_WIDTH - 1);
    out.y = clampi((int16_t)my, 0, Config::SCREEN_HEIGHT - 1);
    return out;
}

bool wasTapped(Point& outPoint) {
    Point cur = mappedPoint();

    bool fired = false;
    if (!cur.touched && lastTouched) {
        outPoint = lastRaw;
        fired = true;
    }

    lastTouched = cur.touched;
    if (cur.touched) lastRaw = cur;

    if (fired) lastTapMs = millis();

    return fired;
}

uint32_t msSinceLastTap() {
    return millis() - lastTapMs;
}

}
