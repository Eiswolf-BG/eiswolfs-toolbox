#pragma once
#include <TFT_eSPI.h>

// Krypto-Ticker (Alex' Auftrag, 07.10.) - zeigt Bitcoin/Ethereum-Kurse
// (CoinGecko) mit 24h-Aenderung, dazu Bitcoin-Mempool-Status (mempool.space)
// und den Fear&Greed-Index der Kryptomaerkte (alternative.me). Fuenfter
// Feature-Screen im Wisch-Karussell (siehe app_navigation.h) - gleiches
// enter()/update()/handleTap()-Muster wie die uebrigen Feature-Screens.
namespace CryptoScreen {
    void enter(TFT_eSPI& tft);
    void update(TFT_eSPI& tft);
    void handleTap(TFT_eSPI& tft, int16_t x, int16_t y);
}
