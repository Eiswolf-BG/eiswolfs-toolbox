#pragma once
#include <TFT_eSPI.h>

// Zentrale Lauftext-Komponente (Alex' Auftrag, 06.10. - siehe CLAUDE.md
// "Pflicht: automatisches Scrollen bei zu langem Text"): zeigt einen
// EINZEILIGEN Text normal an, wenn er in maxWidth passt. Passt er NICHT,
// wird er automatisch horizontal durchgescrollt (Marquee) statt hart am
// Rand abgeschnitten zu werden. Gilt NUR fuer kurze, einzeilige Labels
// (z.B. ein Listen-Eintrag/Ziel-Label) - laengere Fliesstexte/Erklaerungen
// bleiben weiterhin ganz normal mit layoutWrapped() mehrzeilig
// umgebrochen, NICHT hiermit.
namespace MarqueeLabel {
    // Haelt den Scroll-Zustand EINES Labels - fuer jede Stelle, an der
    // potenziell gescrollt werden koennte (z.B. pro Listenzeile), eine
    // eigene Instanz anlegen (z.B. als Array). reset() beim Neuaufbau des
    // Inhalts aufrufen (z.B. wenn sich der angezeigte Text aendert), damit
    // wieder von vorne begonnen wird.
    struct State {
        int16_t offset = 0;
        uint32_t lastStepMs = 0;
        uint32_t pauseUntilMs = 0;
        // Zuletzt tatsaechlich gezeichneter Text (Alex' Fund, 07.10.: ohne
        // dieses Nachhalten loeschte/zeichnete draw() seinen Bereich bei
        // JEDEM Aufruf neu, auch wenn sich ueberhaupt nichts geaendert
        // hatte - bei stabilem Text (kein Scrollen noetig) bzw. waehrend
        // der Scroll-Pausen war das bei jedem der ~50 Aufrufe/Sekunde ein
        // sichtbares Flackern, siehe CLAUDE.md "Marquee-Flimmer-Fix").
        String lastDrawnText;
        bool everDrawn = false;
    };

    void reset(State& state);

    // Zeichnet den Text linksbuendig bei x, vertikal zentriert um y,
    // innerhalb von maxWidth/lineHeight - scrollt automatisch, wenn der
    // Text nicht hineinpasst. MUSS in JEDEM Schleifendurchlauf aufgerufen
    // werden (wie MenuStars::update()), nicht nur einmalig, damit die
    // Animation laeuft - zeichnet dabei auch den Hintergrund des
    // Label-Bereichs selbst (bgColor), der Aufrufer muss diesen Bereich
    // nicht separat loeschen.
    void draw(TFT_eSPI& tft, State& state, int16_t x, int16_t y, int16_t maxWidth,
              int16_t lineHeight, const String& text, uint16_t color, uint16_t bgColor);
}
