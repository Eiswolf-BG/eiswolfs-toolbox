#include "led_notifications.h"
#include <SD.h>

namespace LedNotifications {

namespace {
    constexpr uint8_t SCREEN_COUNT = (uint8_t)ScreenId::COUNT;

    bool master = false;
    bool perScreen[SCREEN_COUNT] = {};

    void saveToSd() {
        File f = SD.open(Config::SD_LED_NOTIFICATIONS_FILE, FILE_WRITE);
        if (!f) return;
        f.println(master ? "1" : "0");
        for (uint8_t i = 0; i < SCREEN_COUNT; i++) {
            f.println(perScreen[i] ? "1" : "0");
        }
        f.close();
    }
}

void init() {
    master = false;
    for (uint8_t i = 0; i < SCREEN_COUNT; i++) perScreen[i] = false;

    File f = SD.open(Config::SD_LED_NOTIFICATIONS_FILE, FILE_READ);
    if (!f) return;

    String line = f.readStringUntil('\n');
    line.trim();
    master = (line.toInt() != 0);

    for (uint8_t i = 0; i < SCREEN_COUNT && f.available(); i++) {
        line = f.readStringUntil('\n');
        line.trim();
        perScreen[i] = (line.toInt() != 0);
    }
    f.close();
}

bool masterEnabled() { return master; }

void setMasterEnabled(bool on) {
    master = on;
    saveToSd();
}

bool screenEnabled(ScreenId id) {
    uint8_t idx = (uint8_t)id;
    if (idx >= SCREEN_COUNT) return false;
    return perScreen[idx];
}

void setScreenEnabled(ScreenId id, bool on) {
    uint8_t idx = (uint8_t)id;
    if (idx >= SCREEN_COUNT) return;
    perScreen[idx] = on;
    saveToSd();
}

bool shouldBlink(ScreenId id) {
    return master && screenEnabled(id);
}

}
