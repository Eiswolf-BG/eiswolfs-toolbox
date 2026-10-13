#include "app_navigation.h"
#include "swipe_input.h"
#include "menu_stars.h"
#include "iss_radar_screen.h"
#include "rocket_screen.h"
#include "transit_screen.h"
#include "weather_screen.h"
#include "calendar_screen.h"
#include "natural_events_screen.h"
#include "f1_screen.h"
#include "crypto_screen.h"
#include "earthquake_screen.h"
#include "settings_hub_screen.h"
#include "settings_display_screen.h"
#include "led_control.h"
#include "appointment_reminders.h"

// Wisch-Navigationskonzept (Alex' Auftrag vom 05.10., Einstellungs-Teil am
// 06.10. zweimal ueberarbeitet - zuletzt auf einen zentralen Button-Hub
// umgestellt statt wischbarer Themenseiten, siehe CLAUDE.md "Navigation:
// Wisch-Gesten statt Menue"):
// - Links/Rechts wischen schaltet zwischen den Feature-Bildschirmen um
//   (Karussell, siehe FEATURES[] unten). Wischen nach LINKS geht einen
//   Schritt WEITER (naechster Eintrag), nach RECHTS einen Schritt
//   ZURUECK (voriger Eintrag) - uebliche Konvention (z.B. Smartphone-
//   Homescreens). Links/Rechts hat INNERHALB der Einstellungen keine
//   Funktion mehr (dort nur noch Tippen, siehe settings_hub_screen.h).
// - Hoch ODER runter wischen (von einem Feature-Bildschirm aus) oeffnet
//   den Einstellungs-Hub (settings_hub_screen.h) - EIN einziger Screen
//   mit 6 Buttons, die jeweils ihr eigenes Untermenue oeffnen.
// - Vom Einstellungs-Hub aus fuehrt Hoch- oder Runterwischen zurueck zu
//   GENAU DEM Feature-Bildschirm, von dem aus man urspruenglich in die
//   Einstellungen gewischt hat (letzten Zustand merken).
namespace AppNavigation {

namespace {
    struct FeatureScreen {
        void (*enter)(TFT_eSPI&);
        void (*update)(TFT_eSPI&);
        void (*handleTap)(TFT_eSPI&, int16_t, int16_t);
    };

    // Neue Feature-Screens (Raketenstart, Bahn, Formel 1, ...) werden
    // hier einfach als weiterer Eintrag ergaenzt - sie brauchen dieselbe
    // enter()/update()/handleTap()-Schnittstelle wie IssRadarScreen.
    // Wetter bewusst an ERSTER Stelle (Alex' Auftrag, 06.10.) - das ist
    // der Screen, der direkt nach dem Boot zu sehen ist (currentFeature
    // startet bei Index 0). Reihenfolge der uebrigen drei ist beliebig.
    constexpr FeatureScreen FEATURES[] = {
        { WeatherScreen::enter, WeatherScreen::update, WeatherScreen::handleTap },
        { CalendarScreen::enter, CalendarScreen::update, CalendarScreen::handleTap },
        { NaturalEventsScreen::enter, NaturalEventsScreen::update, NaturalEventsScreen::handleTap },
        { IssRadarScreen::enter, IssRadarScreen::update, IssRadarScreen::handleTap },
        { RocketScreen::enter, RocketScreen::update, RocketScreen::handleTap },
        { TransitScreen::enter, TransitScreen::update, TransitScreen::handleTap },
        { F1Screen::enter, F1Screen::update, F1Screen::handleTap },
        { CryptoScreen::enter, CryptoScreen::update, CryptoScreen::handleTap },
        { EarthquakeScreen::enter, EarthquakeScreen::update, EarthquakeScreen::handleTap },
    };
    constexpr uint8_t FEATURE_COUNT = sizeof(FEATURES) / sizeof(FEATURES[0]);

    uint8_t currentFeature = 0;
    uint8_t featureBeforeSettings = 0;
    bool inSettings = false;

    void enterFeature(TFT_eSPI& tft, uint8_t index) {
        // LED IMMER zuerst aus (Alex' Auftrag, 12.10.: ISS-Rot-Blinken nur
        // "solange dieser Screen aktiv angezeigt wird") - der jeweilige
        // Screen entscheidet in seinem eigenen enter() danach selbst, ob er
        // sie wieder einschaltet (aktuell nur ISS-Radar). Zentral hier statt
        // in jedem einzelnen Feature-Screen, da es sonst keinen generischen
        // "Screen verlassen"-Hook gibt.
        LedControl::stopBlink();
        currentFeature = index;
        MenuStars::reset();
        FEATURES[index].enter(tft);
    }

    void enterSettings(TFT_eSPI& tft) {
        LedControl::stopBlink();
        featureBeforeSettings = currentFeature;
        inSettings = true;
        MenuStars::reset();
        SettingsHubScreen::enter(tft);
    }

    void leaveSettingsBackToFeature(TFT_eSPI& tft) {
        inSettings = false;
        enterFeature(tft, featureBeforeSettings);
    }

    // Verarbeitet eine einzelne Wisch-Geste (Links/Rechts/Hoch/Runter) -
    // ausgelagert, damit dieselbe Logik sowohl fuer eine ganz normale
    // Geste in der Hauptschleife als auch fuer die "initialGesture" aus
    // run() gilt (siehe dort - der Wisch, mit dem der einmalige Wisch-
    // Erklaer-Screen verlassen wurde, soll exakt genauso wirken wie jede
    // spaetere Geste dieser Richtung).
    void handleDirectionalGesture(TFT_eSPI& tft, SwipeInput::Direction dir) {
        switch (dir) {
            case SwipeInput::Direction::Left:
                // Innerhalb der Einstellungen bewusst ohne Wirkung - der
                // Hub-Screen und seine Untermenues kennen keine Wisch-
                // Navigation mehr, nur noch Tippen (Alex' Auftrag, 06.10.).
                if (!inSettings && FEATURE_COUNT > 1) {
                    enterFeature(tft, (uint8_t)((currentFeature + 1) % FEATURE_COUNT));
                }
                break;

            case SwipeInput::Direction::Right:
                if (!inSettings && FEATURE_COUNT > 1) {
                    enterFeature(tft, (uint8_t)(currentFeature == 0 ? FEATURE_COUNT - 1 : currentFeature - 1));
                }
                break;

            case SwipeInput::Direction::Up:
            case SwipeInput::Direction::Down:
                if (inSettings) {
                    leaveSettingsBackToFeature(tft);
                } else {
                    enterSettings(tft);
                }
                break;

            case SwipeInput::Direction::None:
                break;
        }
    }
}

void run(TFT_eSPI& tft, SwipeInput::Direction initialGesture) {
    if (initialGesture == SwipeInput::Direction::None) {
        enterFeature(tft, 0);
    } else {
        // currentFeature/inSettings stehen noch auf ihren Default-Werten
        // (0/false) - die erste echte Geste wird dadurch so behandelt,
        // als waere der Nutzer regulaer auf Feature 0 gestartet und haette
        // dann sofort gewischt (siehe handleDirectionalGesture()).
        handleDirectionalGesture(tft, initialGesture);
    }

    // Bildschirm-Timeout (settings_display_screen.cpp, Alex' Auftrag,
    // 06.10.) - backlightOn/suppressNextGesture leben bewusst HIER (nicht
    // in settings_display_screen.cpp selbst), weil nur diese zentrale
    // Schleife Zugriff auf jede einzelne Beruehrung VOR ihrer Auswertung
    // als Tap/Wisch hat.
    bool backlightOn = true;
    // Verhindert, dass genau die Beruehrung, die den Bildschirm aus dem
    // Timeout aufweckt, zusaetzlich noch als normaler Tap/Wisch verarbeitet
    // wird (sonst koennte ein Aufweck-Tap z.B. versehentlich einen Button
    // treffen) - bleibt gesetzt, bis dieselbe Beruehrung per Loslassen
    // regulaer abgeschlossen wurde.
    bool suppressNextGesture = false;

    while (true) {
        bool rawTouched = TouchInput::rawPoint().touched;

        if (!backlightOn) {
            if (rawTouched) {
                SettingsDisplayScreen::applyBacklight();
                backlightOn = true;
                suppressNextGesture = true;
            }
            MenuStars::update(tft);
            LedControl::update();
            AppointmentReminders::update();
            delay(20);
            continue;
        }

        SwipeInput::Direction dir;
        TouchInput::Point tap;
        bool gestureHappened = SwipeInput::poll(dir, tap);

        if (suppressNextGesture) {
            if (gestureHappened) suppressNextGesture = false;
        } else if (gestureHappened) {
            if (dir == SwipeInput::Direction::None) {
                if (inSettings) {
                    SettingsHubScreen::handleTap(tft, tap.x, tap.y);
                } else {
                    FEATURES[currentFeature].handleTap(tft, tap.x, tap.y);
                }
            } else {
                handleDirectionalGesture(tft, dir);
            }
        }

        if (inSettings) {
            SettingsHubScreen::update(tft);
        } else {
            FEATURES[currentFeature].update(tft);
        }

        MenuStars::update(tft);
        LedControl::update();
        AppointmentReminders::update();

        uint32_t timeoutMs = SettingsDisplayScreen::screenTimeoutMs();
        if (!suppressNextGesture && timeoutMs > 0 && TouchInput::msSinceLastTap() >= timeoutMs) {
            SettingsDisplayScreen::turnBacklightOff();
            backlightOn = false;
        }

        delay(20);
    }
}

}
