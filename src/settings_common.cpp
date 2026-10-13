#include "settings_common.h"
#include "i18n.h"

namespace SettingsCommon {

uint16_t accentColor(TFT_eSPI& tft) {
    (void)tft;
    return TFT_GREEN;
}

Rect backButtonRect() {
    return {6, 4, 70, 28};
}

void drawBackButton(TFT_eSPI& tft) {
    Rect r = backButtonRect();
    uint16_t accent = accentColor(tft);
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, TFT_BLACK);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 4, accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(accent, TFT_BLACK);
    tft.drawString(String("< ") + I18n::t(StringId::BACK_BTN), (int16_t)(r.x + r.w / 2), (int16_t)(r.y + r.h / 2));
    tft.setTextDatum(TL_DATUM);
}

}
