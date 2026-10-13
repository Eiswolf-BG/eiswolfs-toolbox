#pragma once
#include <TFT_eSPI.h>

// Gemeinsame Ausstattung fuer den gesamten Einstellungs-Bereich (Hub-
// Screen + alle 6 Untermenues, siehe settings_hub_screen.cpp) - bewusst
// EIN geteiltes Modul statt pro-Screen-Duplizierung (anders als sonst in
// diesem Projekt ueblich, siehe CLAUDE.md), weil die Akzentfarbe auf
// JEDER Einstellungs-Seite identisch sein MUSS (visuelle
// Wiedererkennbarkeit "ich bin in den Einstellungen").
//
// Bis 06.10. enthielt dieses Modul zusaetzlich die Infrastruktur fuer
// wischbare Einstellungs-Themenseiten (unterer Wisch-Streifen,
// Pfeile, Zonen-Erkennung ueber den Touch-Startpunkt) - diese Navigation
// wurde noch am selben Tag auf Alex' Wunsch durch einen zentralen
// Button-Hub mit reiner Tap-Navigation ersetzt (siehe settings_hub_screen.h),
// die Wisch-Infrastruktur deshalb komplett entfernt.
namespace SettingsCommon {
    // Seit Entfernung des Mehrfach-Theme-Systems (07.10.) liefert dies
    // schlicht das feste TFT_GREEN wie jeder andere Screen - keine
    // gesonderte Abdunklung/Entsaettigung mehr fuer den Einstellungs-
    // Bereich, da es ohnehin nur noch EIN Farbschema in der ganzen App
    // gibt.
    uint16_t accentColor(TFT_eSPI& tft);

    // Einfacher Rect-Typ, von allen Einstellungs-Screens gemeinsam
    // genutzt (Signatur-Kompatibilitaet mit backButtonRect()/
    // drawBackButton() unten).
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Einheitlicher "< Zurueck"-Button oben links auf JEDEM der 6
    // Untermenues (Alex' Auftrag, 06.10.: "ein gut sichtbarer Zurueck-
    // Button/Pfeil, z.B. oben links auf jedem Untermenue-Screen") - fuehrt
    // zurueck zum Hub-Screen. backButtonRect() wird fuer die Tap-Pruefung
    // gebraucht, drawBackButton() zeichnet ihn.
    Rect backButtonRect();
    void drawBackButton(TFT_eSPI& tft);
}
