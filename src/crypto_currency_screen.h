#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>

// Manuelle Waehrungsauswahl fuer den Krypto-Ticker (Alex' Auftrag, 12.10.)
// - unabhaengig von/mit Vorrang vor der automatischen Standort-Zuordnung
// (siehe currency.h). Liste "Automatisch (Standort)" + alle von CoinGecko
// unterstuetzten Waehrungen, scrollbar (gleiches Hoch/Runter-Button-Muster
// wie f1_screen.cpp::showDetailScreen()). Speichert sofort bei Auswahl
// (Projekt-Konvention) und kehrt dann zum Aufrufer zurueck.
namespace CryptoCurrencyScreen {
    void run(TFT_eSPI& tft);
}
