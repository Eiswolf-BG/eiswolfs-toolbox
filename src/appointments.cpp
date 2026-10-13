#include "appointments.h"
#include <SD.h>

namespace Appointments {

namespace {
    Entry entries[Config::APPOINTMENT_MAX_COUNT];
    uint8_t entryCount = 0;

    String timeLine(const Entry& e) {
        if (!e.hasTime) return String("--:--");
        char buf[6];
        snprintf(buf, sizeof(buf), "%02u:%02u", e.hour, e.minute);
        return String(buf);
    }

    void saveToSd() {
        File f = SD.open(Config::SD_APPOINTMENTS_FILE, FILE_WRITE);
        if (!f) return;
        for (uint8_t i = 0; i < entryCount; i++) {
            f.println(entries[i].date);
            f.println(entries[i].text);
            f.println(timeLine(entries[i]));
            f.println((int)entries[i].reminder);
        }
        f.close();
    }
}

void init() {
    entryCount = 0;
    File f = SD.open(Config::SD_APPOINTMENTS_FILE, FILE_READ);
    if (!f) return;

    while (f.available() && entryCount < Config::APPOINTMENT_MAX_COUNT) {
        String dateLine = f.readStringUntil('\n');
        dateLine.trim();
        if (dateLine.length() == 0) continue; // ueberspringt vereinzelte Leerzeilen
        if (!f.available()) break; // Datumszeile ohne zugehoerige Textzeile - verwerfen

        String textLine = f.readStringUntil('\n');
        textLine.trim();

        if (!f.available()) break; // Text ohne zugehoerige Uhrzeit-Zeile - verwerfen
        String timeLineStr = f.readStringUntil('\n');
        timeLineStr.trim();

        if (!f.available()) break; // Uhrzeit ohne zugehoerige Erinnerungs-Zeile - verwerfen
        String reminderLineStr = f.readStringUntil('\n');
        reminderLineStr.trim();

        if (dateLine.length() != 10) continue; // kein gueltiges "YYYY-MM-DD"

        Entry& e = entries[entryCount];
        strncpy(e.date, dateLine.c_str(), sizeof(e.date) - 1);
        e.date[sizeof(e.date) - 1] = 0;
        strncpy(e.text, textLine.c_str(), sizeof(e.text) - 1);
        e.text[sizeof(e.text) - 1] = 0;

        int colonPos = timeLineStr.indexOf(':');
        int h = (colonPos > 0) ? timeLineStr.substring(0, colonPos).toInt() : -1;
        int m = (colonPos > 0) ? timeLineStr.substring(colonPos + 1).toInt() : -1;
        if (colonPos > 0 && h >= 0 && h <= 23 && m >= 0 && m <= 59) {
            e.hasTime = true;
            e.hour = (uint8_t)h;
            e.minute = (uint8_t)m;
        } else {
            e.hasTime = false;
            e.hour = 0;
            e.minute = 0;
        }

        int r = reminderLineStr.toInt();
        e.reminder = (r >= 0 && r < (int)ReminderType::COUNT) ? (ReminderType)r : ReminderType::None;
        e.reminderFired = false; // RAM-only, nie aus der SD-Karte uebernommen

        entryCount++;
    }
    f.close();
}

uint8_t count() { return entryCount; }

const Entry& get(uint8_t index) { return entries[index]; }

bool add(const char* date, const String& text, bool hasTime, uint8_t hour, uint8_t minute,
         ReminderType reminder) {
    if (entryCount >= Config::APPOINTMENT_MAX_COUNT) return false;
    if (text.length() == 0) return false;

    Entry& e = entries[entryCount];
    strncpy(e.date, date, sizeof(e.date) - 1);
    e.date[sizeof(e.date) - 1] = 0;
    strncpy(e.text, text.c_str(), sizeof(e.text) - 1);
    e.text[sizeof(e.text) - 1] = 0;
    e.hasTime = hasTime;
    e.hour = hasTime ? hour : 0;
    e.minute = hasTime ? minute : 0;
    e.reminder = reminder;
    e.reminderFired = false;
    entryCount++;

    saveToSd();
    return true;
}

void removeAt(uint8_t index) {
    if (index >= entryCount) return;
    for (uint8_t i = index; i < entryCount - 1; i++) entries[i] = entries[i + 1];
    entryCount--;
    entries[entryCount] = Entry{};
    saveToSd();
}

void markReminderFired(uint8_t index) {
    if (index >= entryCount) return;
    entries[index].reminderFired = true;
}

}
