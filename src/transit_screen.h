#pragma once
#include <TFT_eSPI.h>

// OEPNV-Abfahrtstafel - dritter Feature-Screen im Wisch-Karussell (Alex'
// Auftrag, 06.10., siehe CLAUDE.md "OEPNV-Abfahrtstafel" und
// app_navigation.cpp::FEATURES[]). Zeigt die naechsten Abfahrten an der
// naechstgelegenen Haltestelle zum aktuellen Standort (Transitous-API,
// transit_client.h), Datenabruf laeuft im Hintergrund auf Core 0 (siehe
// net_task.h) - dieser Screen selbst fasst nur die bereits abgerufenen
// Daten an. Gleiches enter()/update()/handleTap()-Muster wie die anderen
// Feature-Screens (siehe app_navigation.h).
namespace TransitScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
