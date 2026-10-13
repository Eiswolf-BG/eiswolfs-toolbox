#pragma once
#include <TFT_eSPI.h>

// Wiederverwendbare "Nochmal versuchen"-Komponente fuer den Verbindungs-
// fehler-Zustand (Alex' Auftrag, 12.10.) - gleiches Prinzip wie UiHeader/
// MarqueeLabel: EINE zentrale Komponente statt pro Screen dupliziert.
// Zeigt den vom Aufrufer uebergebenen Fehlertext PLUS einen "Nochmal
// versuchen"-Button darunter. Der Aufrufer ist selbst dafuer zustaendig,
// bei einem Tap auf den zurueckgegebenen Button-Rect:
// 1. in seinen eigenen "Laedt..."-Zustand zu wechseln (gleiche Anzeige
//    wie beim ersten Betreten),
// 2. die fuer diesen Screen zustaendige NetTask::forceXRefetch()-Funktion
//    aufzurufen (erzwingt einen sofortigen Abruf in net_task.cpp UND
//    startet dessen regulaeres Poll-Intervall dabei automatisch neu, da
//    es denselben Zeitstempel wie ein regulaerer Abruf setzt - kein
//    doppelter Nachlauf-Abruf kurz danach).
namespace NetworkRetryView {
    struct Rect {
        int16_t x, y, w, h;
        bool contains(int16_t px, int16_t py) const {
            return px >= x && px < x + w && py >= y && py < y + h;
        }
    };

    // Zeichnet in den Bereich unterhalb von chromeH (loescht diesen Bereich
    // dabei komplett) - errorText darf mehrzeilig umbrechen (wortweise
    // Pixelbreite, wie ueberall sonst im Projekt). Gibt das Rect des
    // "Nochmal versuchen"-Buttons zurueck, das der Aufrufer sich merken
    // muss (gleiches Prinzip wie z.B. earthquake_screen.cpp::
    // lastMoreBtnRect), um es in seinem eigenen handleTap() abzufragen.
    Rect draw(TFT_eSPI& tft, int16_t chromeH, const String& errorText);
}
