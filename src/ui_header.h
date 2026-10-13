#pragma once
#include <TFT_eSPI.h>

// Einheitliche, grosse Ueberschrift fuer JEDEN Screen im Projekt (Alex'
// Auftrag, 06.10.) - EINE zentrale Komponente/Konstante statt pro Screen
// unterschiedlich gewaehlter Groessen, siehe CLAUDE.md "Pflicht:
// einheitliche grosse Ueberschrift auf jedem Screen". Der zugehoerige
// feste Kopfbereich ist Config::HEADER_HEIGHT (config.h) - ebenfalls fuer
// ALLE Screens gleich.
//
// Faellt automatisch auf Textgroesse 1 zurueck, wenn der Titel bei
// TEXT_SIZE nicht in die uebergebene maxWidth passt (z.B. "Rocket Launch"
// im Englischen waere bei Groesse 2 zentriert zu breit fuer den
// Platz neben dem "?"-Button) - siehe CLAUDE.md "Pflicht: Elemente
// muessen immer in ihren Platz passen". Der Aufrufer bestimmt maxWidth
// selbst (haengt von dessen eigenen Buttons links/rechts ab), die
// Zentrierungs-/Shrink-Logik selbst ist hier EINMAL zentral statt pro
// Screen dupliziert.
namespace UiHeader {
    constexpr uint8_t TEXT_SIZE = 2;

    // Zentriert bei (cx, cy) - fuer die Feature-Screens (ISS/Rakete/
    // OEPNV/Wetter), deren Titel schon bisher mittig standen.
    void drawCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                       const String& title, uint16_t color, uint16_t bgColor = TFT_BLACK);

    // Linksbuendig ab x, cy = VERTIKALE MITTE (nicht Baseline, ML_DATUM
    // regelt das selbst korrekt, siehe CLAUDE.md-Font-Falle) - fuer die
    // Einstellungs-Screens, deren Titel schon bisher linksbuendig standen
    // (direkt neben dem "< Zurueck"-Button).
    void drawLeft(TFT_eSPI& tft, int16_t x, int16_t cy, int16_t maxWidth,
                   const String& title, uint16_t color, uint16_t bgColor = TFT_BLACK);
}
