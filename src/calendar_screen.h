#pragma once
#include <TFT_eSPI.h>

// Kalender-Screen (Alex' Auftrag, 12.10.) - Monatsraster mit Feiertagen
// (Nager.Date, siehe holiday_client.h), direkt neben dem Wetter-Screen im
// Wisch-Karussell. Gleiche enter()/update()/handleTap()-Schnittstelle wie
// jeder andere Feature-Screen (siehe app_navigation.cpp::FEATURES[]).
namespace CalendarScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
