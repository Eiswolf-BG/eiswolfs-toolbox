#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SD.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "i18n.h"
#include "touch_input.h"
#include "menu_stars.h"
#include "ui_font.h"
#include "sd_storage.h"
#include "wifi_manager.h"
#include "wifi_setup_screen.h"
#include "calibration_screen.h"
#include "location_presets.h"
#include "address_search_screen.h"
#include "first_run_welcome_screen.h"
#include "first_run_language_screen.h"
#include "first_run_units_screen.h"
#include "units.h"
#include "currency.h"
#include "appointments.h"
#include "led_control.h"
#include "led_notifications.h"
#include "first_run_location_screen.h"
#include "app_navigation.h"
#include "toolbox_logo_bitmap.h"
#include "net_task.h"
#include "swipe_intro_screen.h"
#include "settings_display_screen.h"

// Ersteinrichtung - siehe CLAUDE.md "Ersteinrichtung: Uebernahme von
// Flightradar-Daten" fuer die vollstaendige Beschreibung. Kernprinzip
// seit 05.10. (Alex' Auftrag "erst committen, dann speichern"): WAEHREND
// der gesamten Ersteinrichtung (Kalibrierung, WLAN, Adresseingabe,
// Sprache) wird NICHTS auf die SD-Karte geschrieben - alles bleibt im
// RAM der jeweiligen Module (TouchInput/WifiMgr/LocationPresets/I18n,
// siehe deren setCalibration()/addNetwork()/addPreset()/
// setLanguagePreview(), die seitdem absichtlich NICHT mehr automatisch
// speichern). Erst wenn der Nutzer auf dem abschliessenden Wisch-
// Erklaer-Screen tatsaechlich nach LINKS oder RECHTS wischt (siehe
// swipe_intro_screen.h - einzige Wege, diesen Screen zu verlassen),
// werden alle gesammelten Daten in einem Rutsch committed (siehe
// commitFirstRunData() unten) und ERST DANACH geht es in den ersten
// Feature-Screen.
//
// Drei Ablauf-Varianten, abhaengig davon, was im Flightradar-Ordner
// gefunden wird (reine Erkennung per SD.exists()/Datei-Lesen - auch das
// schreibt nichts, siehe detectAndLoadFlightradarData() unten). Sprache
// kommt in ALLEN Varianten direkt nach Kalibrierung/WLAN und IMMER vor
// der Adresseingabe (Alex' Auftrag, 06.10. - Reihenfolge-Fix):
// 1. WLAN UND Koordinaten vorhanden: Willkommen -> Kalibrierung ->
//    Sprache -> Wisch-Erklaerung.
// 2. NUR WLAN vorhanden: Willkommen -> Kalibrierung -> Sprache ->
//    Adresseingabe -> Wisch-Erklaerung.
// 3. Nichts (brauchbares) vorhanden: Willkommen -> Kalibrierung -> WLAN-
//    Einrichtung -> Sprache -> Adresseingabe -> Wisch-Erklaerung.
// Touch-Kalibrierung laeuft IMMER (frueher ueberlebte die Uebernahme-
// Variante sie faelschlich komplett, siehe Kommentar an
// detectAndLoadFlightradarData() unten fuer die dadurch verursachten
// Bugs).
//
// Bis zum Sprachauswahl-Schritt (also waehrend Kalibrierung und
// WLAN-Einrichtung) steht die Anzeige IMMER fest auf Englisch, egal was
// spaeter gewaehlt wird (Alex' Auftrag, 06.10.) - siehe der
// I18n::setLanguagePreview(I18n::LANG_EN)-Aufruf gleich am Anfang des
// isFirstRun-Blocks in setup() unten. Ab der Sprachauswahl (inklusive)
// gilt dann die tatsaechlich gewaehlte Sprache.

TFT_eSPI tft = TFT_eSPI();

namespace {
    // Ergebnis der reinen Flightradar-Erkennung (siehe
    // detectAndLoadFlightradarData() unten).
    struct FlightradarDetection {
        bool hasWifi = false;
        bool hasCoords = false;
        bool hasCalibration = false;
    };

    // Prueft, ob im Flightradar-Ordner brauchbare WLAN-Zugangsdaten
    // und/oder Koordinaten liegen - und laedt sie, falls vorhanden,
    // DIREKT in den RAM-Zustand von WifiMgr/LocationPresets (deren
    // loadFromPath() liest von einem beliebigen Pfad, OHNE irgendwas zu
    // speichern - siehe dortiger Kommentar). Schreibt NICHTS auf die
    // SD-Karte (weder eine Kopie noch den Toolbox-Ordner selbst) - das
    // passiert erst bei commitFirstRunData().
    //
    // FRueHER (bis 05.10.) ueberlebte eine erfolgreiche Uebernahme die
    // GESAMTE eigene Ersteinrichtung inklusive Touch-Kalibrierung, und
    // Koordinaten wurden nur kopiert, wenn vorhanden, ohne das Ergebnis
    // zu pruefen - zusaetzlich verschaerft durch einen echten Bug in
    // location_presets.cpp (dortiger Kommentar: PRESETS_FILE zeigte
    // versehentlich direkt auf Flightradars eigenen SD-Pfad statt auf
    // die Toolbox-Kopie). Zusammen fuehrte das zu genau dem von Alex
    // beobachteten Verhalten: nur der Wisch-Erklaer-Screen erschien,
    // und die App landete danach mit "Kein Standort eingerichtet", weil
    // weder Kalibrierung noch Standort tatsaechlich uebernommen wurden.
    FlightradarDetection detectAndLoadFlightradarData() {
        FlightradarDetection d;

        if (SD.exists(Config::FLIGHTRADAR_WIFI_CREDENTIALS_FILE)) {
            WifiMgr::loadFromPath(Config::FLIGHTRADAR_WIFI_CREDENTIALS_FILE);
            d.hasWifi = (WifiMgr::networkCount() > 0);
        }
        if (SD.exists(Config::FLIGHTRADAR_LOCATIONS_FILE)) {
            LocationPresets::loadFromPath(Config::FLIGHTRADAR_LOCATIONS_FILE);
            d.hasCoords = (LocationPresets::count() > 0);
        }
        // Touch-Kalibrierung ist eine Eigenschaft der physischen Hardware,
        // nicht der Firmware - auf demselben Geraet bereits per Flightradar
        // ermittelt, direkt uebernehmbar (Alex' Auftrag, 13.10.: die
        // interaktive Kalibrierung lief zwar wie vorgesehen immer, war aber
        // auf einem zweiten Testgeraet nach dem Flashen offenbar durch einen
        // Boot-Zeit-Phantom-Touch kaputt/unbrauchbar geworden - siehe
        // CalibrationScreen::waitForTap()-Kommentar fuer die zusaetzliche
        // Haertung dort). Wenn vorhanden, entfaellt der riskante interaktive
        // Schritt dadurch komplett (siehe buildSetupPlan() unten).
        if (SD.exists(Config::FLIGHTRADAR_CALIBRATION_FILE)) {
            d.hasCalibration = TouchInput::loadFromPath(Config::FLIGHTRADAR_CALIBRATION_FILE);
        }

        if (d.hasWifi) Serial.println("[Setup] WLAN-Zugangsdaten von Flightradar erkannt (RAM, noch nicht gespeichert).");
        if (d.hasCoords) Serial.println("[Setup] Koordinaten von Flightradar erkannt (RAM, noch nicht gespeichert).");
        if (d.hasCalibration) Serial.println("[Setup] Touch-Kalibrierung von Flightradar erkannt (RAM, noch nicht gespeichert).");

        return d;
    }

    void markSetupDone() {
        File f = SD.open(Config::SD_SETUP_DONE_FILE, FILE_WRITE);
        if (!f) return;
        f.println("1");
        f.close();
    }

    void markSwipeIntroDone() {
        File f = SD.open(Config::SD_SWIPE_INTRO_DONE_FILE, FILE_WRITE);
        if (!f) return;
        f.println("1");
        f.close();
    }

    // Einziger Schreibzeitpunkt der gesamten Ersteinrichtung - wird genau
    // einmal aufgerufen, nachdem der Wisch-Erklaer-Screen tatsaechlich
    // per Links- oder Rechts-Swipe verlassen wurde (siehe setup()).
    //
    // BUGFIX (06.10., gefunden bei der Diagnose des "Ersteinrichtung
    // komplett uebersprungen"-Berichts): TouchInput::saveCalibration()
    // fehlte hier komplett - beim Umbau auf "erst committen, dann
    // speichern" (05.10.) wurde der fruehere Direkt-Speicher-Aufruf in
    // calibration_screen.cpp zwar korrekt entfernt (RAM-only waehrend des
    // Screens, siehe dortiger Kommentar), aber NICHT durch einen Aufruf
    // hier im finalen Commit ersetzt. Ergebnis: die Kalibrierung landete
    // nie auf der SD-Karte, selbst nach einem vollstaendigen, erfolgreich
    // committeten Ersteinrichtungs-Durchlauf - bei jedem naechsten Start
    // war calibration.txt weiterhin nicht vorhanden.
    void commitFirstRunData() {
        SdStorage::createStructure();
        SdStorage::seedDefaultDataFiles();
        TouchInput::saveCalibration();
        WifiMgr::commit();
        LocationPresets::commit();
        I18n::saveLanguage();
        Units::save();
        markSetupDone();
        markSwipeIntroDone();
    }

    // Lineare Zustandskette fuer die Ersteinrichtung (Alex' Auftrag,
    // 06.10. - Ersatz fuer mehrere verstreute, einzeln ausgefuehrte
    // if-Bloecke). Der komplette Ablaufplan wird EINMAL, VOR jedem
    // einzelnen interaktiven Screen, aus dem erkannten Zustand berechnet
    // und geloggt - und erst DANACH Schritt fuer Schritt abgearbeitet.
    // Das macht die Entscheidung selbst (welcher Schritt kommt dran)
    // unabhaengig von der Reihenfolge, in der sie bisher zufaellig im
    // Code standen, und an einer einzigen Stelle nachvollziehbar.
    // SetupStep::Units neu (Alex' Auftrag, 07.10. - Pflichtschritt,
    // direkt nach der Sprachauswahl, siehe CLAUDE.md "Globale Einheiten-
    // Einstellung").
    enum class SetupStep : uint8_t { Calibration, WifiSetup, Language, Units, Address, SwipeIntro };

    const char* setupStepName(SetupStep s) {
        switch (s) {
            case SetupStep::Calibration: return "Calibration";
            case SetupStep::WifiSetup:   return "WifiSetup";
            case SetupStep::Language:    return "Language";
            case SetupStep::Units:       return "Units";
            case SetupStep::Address:     return "Address";
            case SetupStep::SwipeIntro:  return "SwipeIntro";
        }
        return "?";
    }

    struct SetupPlan {
        SetupStep steps[6];
        uint8_t count = 0;
        void add(SetupStep s) { steps[count++] = s; }
    };

    // Baut den Ablaufplan NUR aus dem Erkennungsergebnis - keine
    // Seiteneffekte, rein lesend. Deckt genau die 3 Faelle a/b/c aus
    // Alex' Auftrag ab:
    //   a) nichts erkannt:        Calib, WifiSetup, Language, Units, Address, SwipeIntro
    //   b) nur WLAN erkannt:      Calib, Language, Units, Address, SwipeIntro
    //   c) WLAN UND Koordinaten:  Calib, Language, Units, SwipeIntro
    // AUSNAHME seit 13.10. (Alex' Auftrag): Calib entfaellt, wenn
    // fr.hasCalibration true ist, d.h. wenn eine Flightradar-Kalibrierung
    // fuer DIESES Geraet in DIESEM Boot-Vorgang frisch gefunden und
    // geladen wurde (siehe detectAndLoadFlightradarData()) - das ist NICHT
    // dasselbe wie das unten beschriebene 06.10.-Problem (dort ging es um
    // blindes Vertrauen auf eine moeglicherweise VERALTETE, bereits
    // vorhandene EIGENE Toolbox-calibration.txt auf einer wiederverwendeten
    // SD-Karte) - eine Flightradar-Kalibrierung wird stattdessen jedes Mal
    // frisch von dort gelesen und ist eine Eigenschaft der physischen
    // Hardware, nicht einer alten Firmware-Sitzung.
    // Sprachauswahl UND Einheiten-Auswahl laufen weiterhin IMMER
    // (Kalibrierung war bisher bewusst UNBEDINGT, nicht mehr abhaengig von
    // einer evtl. schon vorhandenen, aber eventuell veralteten
    // calibration.txt - siehe Diagnose-Befund 06.10.: genau dieses blinde
    // Vertrauen auf bereits vorhandene SD-Dateien war die eigentliche
    // Ursache dafuer, dass auf einer Karte mit Altlasten aus frueheren
    // Testlaeufen die
    // GESAMTE Ersteinrichtung faelschlich uebersprungen wurde).
    SetupPlan buildSetupPlan(const FlightradarDetection& fr) {
        SetupPlan plan;
        if (!fr.hasCalibration) plan.add(SetupStep::Calibration);
        if (!fr.hasWifi) plan.add(SetupStep::WifiSetup);
        plan.add(SetupStep::Language);
        plan.add(SetupStep::Units);
        if (!fr.hasCoords) plan.add(SetupStep::Address);
        plan.add(SetupStep::SwipeIntro);
        return plan;
    }

    // Vom Wisch-Erklaer-Screen (siehe unten) erkannte erste echte Geste -
    // wird 1:1 an AppNavigation::run() weitergereicht, damit genau diese
    // Geste die erste echte Navigationsaktion ist (Alex' Auftrag, 05.10.).
    // Direction::None (Standardfall - Screen wurde gar nicht gezeigt)
    // laesst AppNavigation wie bisher auf Feature 0 starten.
    SwipeInput::Direction g_initialGesture = SwipeInput::Direction::None;

    // Boot-Screen (Alex' Auftrag, 05.10., zuletzt vereinfacht): EIN
    // einziger Screen mit Logo-Bitmap + Markenname, OHNE Start-Button,
    // der zusaetzlich darunter den WLAN-Verbindungsstatus (und sobald
    // bekannt die IP) anzeigt - ersetzt den vorherigen separaten
    // "Verbinde..."-Screen (reiner Text ohne Logo) komplett, dessen
    // doppelte Anzeige Alex nicht mehr wollte ("nicht noch den 2.
    // Startscreen"). Laeuft bei JEDEM Start GENAU EINMAL, und zwar NACH
    // einer eventuellen Ersteinrichtung (dort sind WLAN-Zugangsdaten erst
    // ab da sicher bekannt) - "Eiswolfs"/"Toolbox" bleiben literal, da es
    // sich um den (sprachunabhaengigen) Produktnamen handelt.
    // Bitmap-Start um 20px nach unten verschoben (Alex' Wunsch, 05.10.:
    // mehr Abstand zum Titel, Logo weiter runter) - STATUS_AREA_TOP
    // entsprechend mitgewandert, damit der Statusbereich weiterhin direkt
    // unter der Bitmap beginnt, ohne sie zu ueberlappen.
    constexpr int16_t LOGO_BITMAP_Y = 90;
    constexpr int16_t STATUS_AREA_TOP = (int16_t)(LOGO_BITMAP_Y + ToolboxLogoBitmap::HEIGHT + 4);

    void drawBootLogoAndTitle(TFT_eSPI& tft) {
        MenuStars::reset();
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextSize(2);
        tft.drawString("Eiswolfs", (int16_t)(Config::SCREEN_WIDTH / 2), 24);
        tft.drawString("Toolbox", (int16_t)(Config::SCREEN_WIDTH / 2), 50);
        tft.setTextSize(1);
        tft.setTextDatum(TL_DATUM);

        int16_t bitmapX = (int16_t)(Config::SCREEN_WIDTH / 2 - ToolboxLogoBitmap::WIDTH / 2);
        tft.drawBitmap(bitmapX, LOGO_BITMAP_Y, ToolboxLogoBitmap::BITMAP,
                        ToolboxLogoBitmap::WIDTH, ToolboxLogoBitmap::HEIGHT,
                        TFT_GREEN);
    }

    // Leert nur den Statusbereich unterhalb der Bitmap und zeichnet ihn
    // neu - das Logo bleibt dabei die ganze Zeit unveraendert stehen,
    // waehrend sich der WLAN-Status aktualisiert (verbinde... -> verbunden
    // + IP, bzw. nicht verbunden). Zentriert und in Weiss (bzw. Rot bei
    // Fehlschlag) ueber drawString()+MC_DATUM statt setCursor()+print() -
    // exakt das Muster von Flightradars SplashScreen::setStatusLine()
    // (Alex' Wunsch, 05.10.: "genau wie beim Flightradar"). drawString()
    // mit MC_DATUM ist vom Grundlinien-Verhalten des eigenen Fonts nicht
    // betroffen, siehe CLAUDE.md.
    void drawBootStatus(TFT_eSPI& tft, StringId wifiStatusId, const String& ip,
                         uint16_t color = TFT_WHITE) {
        tft.fillRect(0, STATUS_AREA_TOP, Config::SCREEN_WIDTH,
                     (int16_t)(Config::SCREEN_HEIGHT - STATUS_AREA_TOP), TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(color, TFT_BLACK);

        int16_t cx = (int16_t)(Config::SCREEN_WIDTH / 2);
        String line1 = String(I18n::t(StringId::WIFI_LABEL_PREFIX)) + I18n::t(wifiStatusId);
        tft.drawString(line1, cx, (int16_t)(STATUS_AREA_TOP + 18));

        if (ip.length() > 0) {
            String line2 = String(I18n::t(StringId::STATUS_IP_LABEL)) + ip;
            tft.drawString(line2, cx, (int16_t)(STATUS_AREA_TOP + 40));
        }

        tft.setTextDatum(TL_DATUM);
    }

    // "SD-Karte erforderlich"-Screen - analog zum gleichnamigen Screen
    // bei Eiswolfs Flightradar CYD (main.cpp::haltWithSdRequiredScreen()
    // dort, direkt nachgesehen fuer Layout/Verhalten) - WICHTIG: dieser
    // Screen laeuft, BEVOR eine SD-Karte ueberhaupt erfolgreich
    // eingebunden werden konnte - I18n::init() kann also gar keine
    // gespeicherte Sprache laden. Alex' Auftrag, 06.10.: dieser Screen
    // erscheint deshalb IMMER explizit auf Englisch (siehe
    // setLanguagePreview()-Aufruf unten), nicht auf dem sonstigen
    // Deutsch-Standard/Fallback - Text liegt trotzdem als i18n-String vor
    // (Konsistenz fuer spaeter). Gleiche Pixel-Koordinaten wie im
    // Flightradar-Original uebernommen (live am echten Geraet getestet,
    // siehe dortiger Kommentar zu DE/FR/PT-Ueberlauf bei frueheren
    // Werten).
    //
    // Retry-Verhalten: Flightradars Original prueft NICHT automatisch
    // erneut, ob zwischenzeitlich eine Karte eingelegt wurde - es haelt
    // einfach dauerhaft an (while(true) delay(1000)), ein Neustart ist
    // noetig. 1:1 genauso hier uebernommen (bewusst KEIN Auto-Retry
    // erfunden, das es im Original gar nicht gibt).
    int16_t layoutWrappedSdRequired(TFT_eSPI& tft, int16_t x, int16_t startY, int16_t maxWidth,
                                     int16_t lineHeight, const String& text) {
        int16_t y = startY;
        int32_t start = 0;
        int32_t len = text.length();
        while (start < len) {
            while (start < len && text[start] == ' ') start++;
            if (start >= len) break;
            String line = text.substring(start, len);
            while (tft.textWidth(line) > maxWidth) {
                int32_t lastSpace = line.lastIndexOf(' ');
                if (lastSpace <= 0) break;
                line = line.substring(0, lastSpace);
            }
            tft.setCursor(x, y);
            tft.print(line);
            y += lineHeight;
            start += line.length();
        }
        return y;
    }

    void haltWithSdRequiredScreen(TFT_eSPI& tft) {
        // Siehe Kommentar oben - dieser Screen ist IMMER Englisch, auch
        // wenn spaeter (hier nie, da while(true) unten) eine andere
        // Sprache gewaehlt wuerde. I18n::init() laeuft wegen !sdOk gar
        // nicht erst, daher ist das hier der einzige Ort, der die Sprache
        // vorab festlegen muss.
        I18n::setLanguagePreview(I18n::LANG_EN);

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        int16_t cx = (int16_t)(Config::SCREEN_WIDTH / 2);

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE1), cx, 45);
        tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE2), cx, 72);
        tft.drawString(I18n::t(StringId::SD_REQUIRED_LINE3), cx, 99);

        tft.setTextDatum(TL_DATUM);
        tft.setTextSize(1);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        constexpr int16_t TEXT_X = 10;
        constexpr int16_t TEXT_MAX_W = (int16_t)(Config::SCREEN_WIDTH - 20);
        constexpr int16_t LINE_H = 14;
        int16_t y = layoutWrappedSdRequired(tft, TEXT_X, 132, TEXT_MAX_W, LINE_H,
                                             I18n::t(StringId::SD_REQUIRED_HINT));
        layoutWrappedSdRequired(tft, TEXT_X, (int16_t)(y + 6), TEXT_MAX_W, LINE_H,
                                 I18n::t(StringId::SD_REQUIRED_TROUBLESHOOTING));

        while (true) delay(1000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);

    tft.init();
    tft.setRotation(0);
    // 1:1 wie im Flightradar-Projekt - ohne diesen Aufruf stellt dieses
    // Panel Farben vertauscht dar (gruen wird zu lila, live bestaetigt).
    tft.invertDisplay(true);
    tft.setFreeFont(&UiFont11pt);
    tft.fillScreen(TFT_BLACK);

    // Hintergrundbeleuchtung per PWM uebernehmen (Config::BACKLIGHT_PWM_*,
    // settings_display_screen.cpp) - 1:1 dasselbe Muster wie bei Eiswolfs
    // Flightradar CYD. Vorerst volle Helligkeit, bis die gespeicherte
    // Einstellung weiter unten (nach dem SD-Mount) geladen ist - verhindert
    // einen kurzen schwarzen/falsch hellen Blitz direkt beim Boot.
    ledcSetup(Config::BACKLIGHT_PWM_CHANNEL, Config::BACKLIGHT_PWM_FREQ, Config::BACKLIGHT_PWM_RESOLUTION_BITS);
    ledcAttachPin(TFT_BL, Config::BACKLIGHT_PWM_CHANNEL);
    ledcWrite(Config::BACKLIGHT_PWM_CHANNEL, 255);

    // Diskrete 3-Kanal-RGB-LED (Alex' Auftrag, 12.10., siehe led_control.h)
    // - eigene PWM-Kanaele (1-3), unabhaengig vom Backlight (Kanal 0).
    LedControl::begin();

    TouchInput::begin();

    bool sdOk = SdStorage::init();
    if (!sdOk) {
        haltWithSdRequiredScreen(tft);
    }

    // Gespeicherte Sprache so frueh wie moeglich laden (faellt auf
    // Deutsch zurueck, falls noch keine gewaehlt wurde) - ALLE danach
    // gezeichneten Texte (Status-Screen, Menue, ISS-Radar) nutzen sofort
    // die richtige Sprache.
    I18n::init();

    // Gespeicherte Einheiten-Einstellung (Metrisch/Imperial, Alex'
    // Auftrag, 07.10.) - ebenfalls so frueh wie moeglich, vor dem ersten
    // Wetter-Abruf (net_task.cpp liest Units::current() direkt beim
    // Zusammenbauen der API-URL).
    Units::init();

    // Manuelle Waehrungs-Ueberschreibung fuer den Krypto-Ticker (Alex'
    // Auftrag, 12.10., siehe currency.h) - ebenfalls so frueh wie
    // moeglich, vor dem ersten Krypto-Abruf.
    Currency::init();

    // Eigene Kalender-Termine (Alex' Auftrag, 12.10., siehe appointments.h)
    // - ebenfalls so frueh wie moeglich, vor dem ersten Betreten des
    // Kalender-Screens.
    Appointments::init();

    // LED-Benachrichtigungs-Einstellungen (Alex' Auftrag, 12.10., siehe
    // led_notifications.h) - vor dem ersten Betreten eines LED-faehigen
    // Screens (ISS-Radar/Raketenstart) geladen haben.
    LedNotifications::init();

    WifiMgr::init();
    LocationPresets::init();

    // BUGFIX (06.10.): wurde bisher NUR innerhalb des isFirstRun-Blocks
    // aufgerufen - im Normalfall (Ersteinrichtung bereits abgeschlossen,
    // Fall d) lief loadCalibration() dadurch NIE, TouchInput blieb auf
    // den groben Standardwerten stehen. Jetzt unbedingt bei jedem Start.
    TouchInput::loadCalibration();

    // Helligkeit/Bildschirm-Timeout (settings_display_screen.cpp) laden und
    // die gespeicherte Helligkeit sofort anwenden (ersetzt die
    // provisorische volle Helligkeit von oben).
    SettingsDisplayScreen::loadSettings();
    SettingsDisplayScreen::applyBacklight();

    // 180-Grad-Display-Flip (settings_display_screen.cpp) - ebenfalls bei
    // JEDEM Start laden und anwenden, nicht nur beim Umschalten selbst.
    // Faellt mangels gespeicherter Datei auf false/Rotation 0 zurueck
    // (Standard bei einem neuen Geraet). tft.setRotation(2) UND
    // TouchInput::setRotated180() muessen beide gesetzt werden (gleiches
    // Muster wie Eiswolfs Flightradar CYD, dortige main.cpp) - ersteres
    // dreht die Bildschirmausgabe, zweiteres die Touch-Koordinaten-
    // Zuordnung passend dazu.
    bool rotated180 = TouchInput::loadRotated180();
    tft.setRotation(rotated180 ? 2 : 0);

    bool isFirstRun = !SD.exists(Config::SD_SETUP_DONE_FILE);
    Serial.printf("[Setup] isFirstRun: %d\n", isFirstRun);

    if (isFirstRun) {
        // Fest auf Englisch, bis die Sprachauswahl weiter unten lief -
        // siehe grosser Kommentar am Dateianfang (Alex' Auftrag, 06.10.).
        I18n::setLanguagePreview(I18n::LANG_EN);

        FirstRunWelcomeScreen::run(tft);

        // Reine Erkennung - liest nur, schreibt nichts (siehe dortiger
        // Kommentar). Legt bei Erfolg die WLAN-/Standortdaten direkt im
        // RAM-Zustand von WifiMgr/LocationPresets ab.
        FlightradarDetection fr = detectAndLoadFlightradarData();

        // Ablaufplan EINMAL berechnen und loggen, dann strikt Schritt
        // fuer Schritt abarbeiten (siehe buildSetupPlan()-Kommentar oben
        // fuer die Begruendung des Umbaus).
        SetupPlan plan = buildSetupPlan(fr);
        Serial.print("[Setup] Ablaufplan: ");
        for (uint8_t i = 0; i < plan.count; i++) {
            Serial.print(setupStepName(plan.steps[i]));
            if (i + 1 < plan.count) Serial.print(" -> ");
        }
        Serial.println();

        for (uint8_t i = 0; i < plan.count; i++) {
            switch (plan.steps[i]) {
                case SetupStep::Calibration:
                    CalibrationScreen::run(tft);
                    break;
                case SetupStep::WifiSetup:
                    WifiSetupScreen::run(tft);
                    break;
                case SetupStep::Language:
                    // Ab hier gilt die tatsaechlich gewaehlte Sprache
                    // statt des Englisch-Defaults von oben (nur
                    // Vorschau, siehe first_run_language_screen.cpp -
                    // noch nicht gespeichert).
                    FirstRunLanguageScreen::run(tft);
                    break;
                case SetupStep::Units:
                    // Direkt nach der Sprachauswahl (Alex' Auftrag,
                    // 07.10.) - der Titel/die Optionen nutzen bereits die
                    // gerade gewaehlte Sprache (nur Vorschau, siehe
                    // first_run_units_screen.cpp - noch nicht
                    // gespeichert).
                    FirstRunUnitsScreen::run(tft);
                    break;
                case SetupStep::Address:
                    FirstRunLocationScreen::run(tft);
                    break;
                case SetupStep::SwipeIntro:
                    // Liefert ausschliesslich Left oder Right zurueck
                    // (siehe swipe_intro_screen.cpp, Hoch/Runter loesen
                    // dort bewusst nichts aus). GENAU DIESER Ruecksprung
                    // ist der einzige Ausloeser fuer den Commit - vorher
                    // wurde nichts auf die SD-Karte geschrieben.
                    g_initialGesture = SwipeIntroScreen::run(tft);
                    break;
            }
        }
        commitFirstRunData();
    }

    // Boot-Screen (Logo + Titel) - laeuft bei JEDEM Start GENAU EINMAL,
    // hier und nicht frueher, siehe Kommentar an drawBootLogoAndTitle().
    drawBootLogoAndTitle(tft);
    drawBootStatus(tft, StringId::STATUS_CONNECTING, "");

    if (WifiMgr::networkCount() > 0) {
        WifiMgr::beginConnect();
        uint32_t waitStart = millis();
        while (WifiMgr::getState() == WifiMgr::State::Connecting && millis() - waitStart < 16000) {
            WifiMgr::update();
            MenuStars::update(tft);
            delay(100);
        }
    }

    if (WifiMgr::getState() == WifiMgr::State::Connected) {
        String ip = WifiMgr::getIP();
        Serial.printf("[WiFi] Verbunden, IP: %s\n", ip.c_str());
        drawBootStatus(tft, StringId::STATUS_CONNECTED, ip);

        // Uhrzeit per NTP synchronisieren - wird vom Raketenstart-
        // Countdown gebraucht (rocket_client.cpp/rocket_screen.cpp
        // berechnen die Restzeit lokal aus time(nullptr), keine
        // wiederholten API-Anfragen dafuer). Gleiches Muster wie
        // Flightradars AdsbClient::primeTime() - kurz (bis zu 3s)
        // blockierend warten, damit die Uhr beim ersten Betreten des
        // Raketenstart-Screens schon korrekt steht, statt dort kurz einen
        // falschen Countdown zu zeigen.
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        time_t now = time(nullptr);
        uint32_t ntpStart = millis();
        while (now < 8 * 3600 * 2 && millis() - ntpStart < 3000) {
            MenuStars::update(tft);
            delay(50);
            now = time(nullptr);
        }

        // Einmaliger Nachlade-Versuch fuer aeltere, vor Einfuehrung des
        // countryCode-Felds gespeicherte Standorte (Alex' Auftrag,
        // 12.10., siehe address_search_screen.h). Macht intern selbst
        // nichts, falls bereits ein Laendercode vorhanden ist oder kein
        // Preset existiert - kurzer, blockierender Call, gleiche
        // Kategorie wie der NTP-Sync direkt darueber.
        AddressSearchScreen::backfillCountryCodeIfMissing();

    } else {
        Serial.println("[WiFi] Nicht verbunden.");
        drawBootStatus(tft, StringId::STATUS_NOT_CONNECTED, "", TFT_RED);
    }

    // Netzwerk-Hintergrund-Task starten (ISS-Position, Raketenstart-Daten,
    // Wetter, Krypto, Erdbeben, ...) - laeuft ab jetzt dauerhaft auf Core 0,
    // siehe net_task.h. WICHTIG: IMMER starten, auch wenn die WLAN-
    // Verbindung beim Boot fehlgeschlagen ist (07.10., Bugfix) - die
    // Semaphoren fuer alle Getter (issMutex, weatherMutex, cryptoMutex,
    // ...) werden NUR hier in NetTask::begin() angelegt. Lief begin() nie,
    // blieben sie dauerhaft nullptr, und der erste Screen, der einen
    // NetTask-Getter aufruft (z.B. das Wetter-Screen direkt nach dem Boot),
    // stuerzte mit "assert failed: xQueueSemaphoreTake queue.c:1545" ab,
    // sobald er xSemaphoreTake() auf einem nullptr-Handle aufrief. Die
    // einzelnen *Client::fetch()-Aufrufe in taskFunc() pruefen den WLAN-
    // Status ohnehin schon NICHT separat - sie verlassen sich darauf, dass
    // HTTPClient bei fehlender Verbindung einfach schnell fehlschlaegt
    // (dasselbe Verhalten greift bereits heute bei einem WLAN-Abbruch
    // WAEHREND der Laufzeit). Der Task darf also bedenkenlos immer
    // laufen; er liefert bei fehlendem WLAN einfach weiterhin Status::
    // Error/Pending, bis eine spaetere Verbindung zustande kommt.
    NetTask::begin();

    // Deutlich laenger stehen lassen als zuvor (Alex' Wunsch, 05.10.) -
    // damit das Endergebnis (verbunden + IP bzw. nicht verbunden)
    // tatsaechlich lesbar ist, statt nach einer kurzen blinden Pause
    // sofort zu verschwinden. Danach geht es direkt in die App
    // (AppNavigation::run() in loop(), zeigt sofort den ISS-Radar).
    constexpr uint32_t RESULT_HOLD_MS = 2000;
    uint32_t holdStart = millis();
    while (millis() - holdStart < RESULT_HOLD_MS) {
        MenuStars::update(tft);
        delay(20);
    }
}

void loop() {
    // Laeuft dauerhaft (kehrt nie zurueck) - siehe app_navigation.h.
    // g_initialGesture: None im Normalfall, oder die Geste, mit der der
    // einmalige Wisch-Erklaer-Screen gerade verlassen wurde (siehe
    // setup()).
    AppNavigation::run(tft, g_initialGesture);
}
