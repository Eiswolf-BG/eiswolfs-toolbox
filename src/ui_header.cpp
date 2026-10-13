#include "ui_header.h"

namespace UiHeader {

void drawCentered(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t maxWidth,
                   const String& title, uint16_t color, uint16_t bgColor) {
    tft.setTextColor(color, bgColor);
    tft.setTextSize(TEXT_SIZE);
    if (tft.textWidth(title) > maxWidth) tft.setTextSize(1);

    tft.setTextDatum(MC_DATUM);
    tft.drawString(title, cx, cy);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
}

void drawLeft(TFT_eSPI& tft, int16_t x, int16_t cy, int16_t maxWidth,
              const String& title, uint16_t color, uint16_t bgColor) {
    tft.setTextColor(color, bgColor);
    tft.setTextSize(TEXT_SIZE);
    if (tft.textWidth(title) > maxWidth) tft.setTextSize(1);

    tft.setTextDatum(ML_DATUM);
    tft.drawString(title, x, cy);
    tft.setTextDatum(TL_DATUM);
    tft.setTextSize(1);
}

}
