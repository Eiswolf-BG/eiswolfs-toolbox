#pragma once
#include <Arduino.h>
#include "config.h"

// Ein-/Ausschalter fuer die LED-Benachrichtigungen jedes LED-faehigen
// Screens (Alex' Auftrag, 12.10.) - persistiert auf der SD-Karte (Config::
// SD_LED_NOTIFICATIONS_FILE), ueberlebt einen Neustart. Ein zusaetzlicher
// Haupt-Toggle schaltet ALLE LED-Benachrichtigungen auf einmal stumm, ohne
// die einzelnen Toggle-Zustaende zu veraendern (die bleiben gespeichert,
// sind nur inaktiv). Standard bei der Ersteinrichtung: ALLES aus.
//
// Neue LED-faehige Screens (z.B. die geplante Kalender-Termin-Erinnerung)
// werden einfach als weiterer Eintrag in ScreenId ergaenzt (vor COUNT) -
// die Speicherung/Laden-Logik in led_notifications.cpp skaliert automatisch
// mit COUNT mit, keine weiteren Codeaenderungen noetig.
namespace LedNotifications {
    enum class ScreenId : uint8_t {
        Iss = 0,
        Rocket,
        Reminder, // Termin-Erinnerungen (Alex' Auftrag, 12.10., zweiter Teil)
        COUNT
    };

    // Laedt von der SD-Karte - einmal beim Boot aufrufen (main.cpp::setup()).
    void init();

    bool masterEnabled();
    void setMasterEnabled(bool on); // speichert sofort

    bool screenEnabled(ScreenId id);
    void setScreenEnabled(ScreenId id, bool on); // speichert sofort

    // Bequemlichkeits-Funktion fuer die LED-Trigger der einzelnen Screens
    // (iss_radar_screen.cpp/rocket_screen.cpp) - true nur, wenn SOWOHL der
    // Haupt-Toggle ALS AUCH der Einzel-Toggle dieses Screens an sind.
    bool shouldBlink(ScreenId id);
}
