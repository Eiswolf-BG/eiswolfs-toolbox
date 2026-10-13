#include "appointment_reminders.h"
#include <time.h>
#include "appointments.h"
#include "led_control.h"
#include "led_notifications.h"

namespace AppointmentReminders {

namespace {
    // ---- Lokale Zeit des GERAETS (wie calendar_screen.cpp/earthquake_
    // screen.cpp::formatDeviceLocalDateTime(), 1:1 dupliziert, Projekt-
    // Konvention) - feste Mitteleuropa-DST-Basis statt einer Zeitzonen-
    // Nachschlagtabelle. Hier aber in der UMGEKEHRTEN Richtung genutzt:
    // nicht UTC->lokal (wie anderswo), sondern lokaler Termin-Zeitpunkt
    // ->UTC-Epoch, um ihn mit time(nullptr) vergleichen zu koennen. ----
    long daysFromCivil(int y, unsigned m, unsigned d) {
        y -= (m <= 2) ? 1 : 0;
        long era = (y >= 0 ? y : y - 399) / 400;
        unsigned yoe = (unsigned)(y - era * 400);
        unsigned doy = (153 * (m > 2 ? (m - 3) : (m + 9)) + 2) / 5 + d - 1;
        unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + (long)doe - 719468;
    }

    long lastSundayOfMonth(int year, unsigned month, unsigned lastDay) {
        long days = daysFromCivil(year, month, lastDay);
        long dow = ((days % 7) + 4 + 7) % 7;
        return days - dow;
    }

    // "naiveEpoch" ist hier wie in den anderen Dateien ein Zeitstempel, der
    // so behandelt wird, als waere er UTC, obwohl er eigentlich lokale
    // Wanduhrzeit darstellt - konsistent mit der sonst im Projekt
    // etablierten Vereinfachung (Mitteleuropa-Naeherung statt echter
    // Zeitzonen-Bibliothek).
    bool isEuDst(long naiveEpoch, int year) {
        long dstStart = lastSundayOfMonth(year, 3, 31) * 86400L + 1 * 3600L;
        long dstEnd = lastSundayOfMonth(year, 10, 31) * 86400L + 1 * 3600L;
        return naiveEpoch >= dstStart && naiveEpoch < dstEnd;
    }

    // Berechnet den tatsaechlichen Erinnerungszeitpunkt (UTC-Epoch) eines
    // Termins, falls dessen Erinnerungs-Einstellung das zulaesst. false,
    // wenn keine Erinnerung gesetzt ist oder (bei den minuten-/stunden-
    // basierten Optionen) keine Uhrzeit vorhanden ist - das sollte die
    // Auswahl im Kalender-Screen zwar bereits verhindern, hier zusaetzlich
    // defensiv abgesichert.
    bool computeReminderEpoch(const Appointments::Entry& e, time_t& outEpoch) {
        if (e.reminder == Appointments::ReminderType::None) return false;

        int year = 0;
        unsigned month = 0, day = 0;
        if (sscanf(e.date, "%d-%u-%u", &year, &month, &day) != 3) return false;

        long daysSinceEpoch = daysFromCivil(year, month, day);
        long naiveEpoch;

        if (e.reminder == Appointments::ReminderType::Day1) {
            // Ohne gesetzte Uhrzeit faellt "1 Tag vorher" auf eine feste
            // Standard-Uhrzeit (Alex' Auftrag: "z.B. 09:00 Uhr") - muss
            // exakt zu der im Kalender-Screen angezeigten Beschriftung
            // passen (siehe calendar_screen.cpp::DAY1_DEFAULT_HOUR/MINUTE).
            uint8_t hour = e.hasTime ? e.hour : 9;
            uint8_t minute = e.hasTime ? e.minute : 0;
            naiveEpoch = (daysSinceEpoch - 1) * 86400L + (long)hour * 3600L + (long)minute * 60L;
        } else {
            if (!e.hasTime) return false; // sollte durch die UI-Auswahl nie vorkommen
            naiveEpoch = daysSinceEpoch * 86400L + (long)e.hour * 3600L + (long)e.minute * 60L;
            switch (e.reminder) {
                case Appointments::ReminderType::Min10: naiveEpoch -= 10 * 60; break;
                case Appointments::ReminderType::Min30: naiveEpoch -= 30 * 60; break;
                case Appointments::ReminderType::Hour1: naiveEpoch -= 3600; break;
                default: return false;
            }
        }

        bool dst = isEuDst(naiveEpoch, year);
        int offsetHours = 1 + (dst ? 1 : 0);
        outEpoch = (time_t)(naiveEpoch - (long)offsetHours * 3600L);
        return true;
    }

    uint32_t lastCheckMs = 0;
    constexpr uint32_t CHECK_INTERVAL_MS = 60000; // "einmal pro Minute", Alex' Auftrag
    constexpr uint32_t BLINK_DURATION_MS = 30000; // "ca. 30 Sekunden", Alex' Auftrag
}

void update() {
    uint32_t now = millis();
    if (lastCheckMs != 0 && now - lastCheckMs < CHECK_INTERVAL_MS) return;
    lastCheckMs = now;

    // Gleiches Gate wie ISS-Radar/Raketenstart (Alex' Auftrag, 12.10.:
    // "gleiches Muster") - nur wenn SOWOHL Haupt-Toggle ALS AUCH der
    // Reminder-Einzel-Toggle an sind.
    if (!LedNotifications::shouldBlink(LedNotifications::ScreenId::Reminder)) return;

    time_t nowEpoch = time(nullptr);

    for (uint8_t i = 0; i < Appointments::count(); i++) {
        const Appointments::Entry& e = Appointments::get(i);
        if (e.reminder == Appointments::ReminderType::None) continue;
        if (e.reminderFired) continue;

        time_t target;
        if (!computeReminderEpoch(e, target)) continue;

        if (nowEpoch >= target) {
            Appointments::markReminderFired(i);
            LedControl::startTimedBlink(255, 0, 0, 500, BLINK_DURATION_MS);
        }
    }
}

}
