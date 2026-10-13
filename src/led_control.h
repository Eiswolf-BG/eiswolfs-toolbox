#pragma once
#include <Arduino.h>

// Diskrete 3-Kanal-RGB-LED des CYD-Boards (Alex' Auftrag, 12.10.) - GPIO 4/
// 16/17 (Rot/Gruen/Blau), aktiv-LOW, per PWM angesteuert (gleiches Prinzip
// wie das bestehende Backlight-PWM, siehe Config::LED_*-Konstanten in
// config.h) - live am Geraet per Testaufbau bestaetigt (ehemals
// settings_led_screen.cpp-Pin-Testlauf, mittlerweile wieder entfernt).
// KEINE adressierbare RGB-LED (kein WS2812) - jeder Kanal ist ein simpler,
// eigener PWM-Ausgang.
namespace LedControl {
    // Einmal beim Boot aufrufen (main.cpp::setup()) - richtet die 3 PWM-
    // Kanaele ein und schaltet die LED aus.
    void begin();

    // Setzt eine feste Farbe (0 = aus, 255 = volle Helligkeit je Kanal) -
    // die aktiv-LOW-Umrechnung passiert intern, der Aufrufer denkt in
    // normalen 0-255-Helligkeitswerten. Direkter Hardware-Zugriff ohne
    // Ruecksicht auf einen evtl. aktiven Blink-Zustand - der naechste
    // update()-Aufruf (laeuft alle ~20ms zentral mit) ueberschreibt eine
    // hier gesetzte Farbe wieder, falls ein Blinken aktiv ist.
    void setColor(uint8_t r, uint8_t g, uint8_t b);

    // Kurzform fuer setColor(0, 0, 0).
    void off();

    // An einen Screen gebundenes Blinken (ISS-Radar/Raketenstart) - laeuft
    // unbegrenzt, bis stopBlink() aufgerufen wird (typischerweise beim
    // Verlassen des jeweiligen Screens, siehe app_navigation.cpp). MUSS
    // regelmaessig per update() weiterlaufen (gleiches Prinzip wie
    // MenuStars::update()).
    void startBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs = 500);

    // Beendet NUR das Screen-gebundene Blinken. Ein zeitlich begrenztes
    // Blinken (siehe startTimedBlink()) laeuft davon UNBEEINFLUSST weiter -
    // ein Screen-Wechsel waehrend einer laufenden Termin-Erinnerung darf
    // diese nicht vorzeitig abschneiden (Alex' Auftrag, 12.10.: "blinkt
    // ... unabhaengig davon, welcher Screen gerade aktiv ist").
    void stopBlink();

    // Zeitlich begrenztes Blinken (Termin-Erinnerungen, Alex' Auftrag,
    // 12.10.) - laeuft fuer genau durationMs und stoppt danach VON SELBST
    // (kein manuelles stopBlink() noetig/vorgesehen), unabhaengig davon,
    // welcher Screen gerade aktiv ist oder ob dieser zwischenzeitlich
    // gewechselt wird. Hat waehrend seiner Laufzeit Vorrang vor einem
    // evtl. gleichzeitig aktiven Screen-Blinken (das Screen-Blinken selbst
    // wird dabei NICHT beendet, pausiert optisch nur so lange).
    void startTimedBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs, uint32_t durationMs);

    // Muss regelmaessig aufgerufen werden (laeuft bereits zentral in
    // app_navigation.cpp mit) - schaltet laufende Blink-Zustaende um bzw.
    // beendet ein zeitlich begrenztes Blinken nach Ablauf. Ohne jeden
    // aktiven Blink-Zustand macht der Aufruf nichts.
    void update();
}
