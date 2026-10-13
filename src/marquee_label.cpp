#include "marquee_label.h"

namespace MarqueeLabel {

namespace {
    constexpr uint32_t START_PAUSE_MS = 800;
    constexpr uint32_t STEP_INTERVAL_MS = 40;
    constexpr int16_t STEP_PX = 1;
    constexpr int16_t GAP_PX = 24; // Abstand zwischen Textende und Wiederholung
}

void reset(State& state) {
    state = State{};
}

void draw(TFT_eSPI& tft, State& state, int16_t x, int16_t y, int16_t maxWidth,
          int16_t lineHeight, const String& text, uint16_t color, uint16_t bgColor) {
    int16_t textW = tft.textWidth(text);
    // Alex' Fund, 07.10. (siehe CLAUDE.md "Marquee-Flimmer-Fix"): frueher
    // wurde der Bereich bei JEDEM Aufruf geloescht/neu gezeichnet, auch
    // wenn sich seit dem letzten Aufruf NICHTS geaendert hatte (Text passt
    // bereits und steht schon da, bzw. waehrend einer Scroll-Pause) - bei
    // ~50 Aufrufen/Sekunde (Haupt-Loop-Intervall) ein deutlich sichtbares
    // Flackern. textChanged erkennt auch einen reinen Textwechsel (z.B.
    // neue Daten mit anderer Laenge), nicht nur fehlendes reset().
    bool textChanged = !state.everDrawn || text != state.lastDrawnText;

    if (textW <= maxWidth) {
        // Passt normal - kein Scrollen noetig. NUR neu zeichnen, wenn sich
        // der Text seit dem letzten Aufruf tatsaechlich geaendert hat.
        if (!textChanged) return;

        tft.fillRect(x, (int16_t)(y - lineHeight / 2), maxWidth, lineHeight, bgColor);
        state.offset = 0;
        state.pauseUntilMs = 0;
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(color, bgColor);
        tft.drawString(text, x, y);
        tft.setTextDatum(TL_DATUM);
        state.lastDrawnText = text;
        state.everDrawn = true;
        return;
    }

    uint32_t now = millis();
    if (textChanged || state.pauseUntilMs == 0) {
        state.offset = 0;
        state.pauseUntilMs = now + START_PAUSE_MS;
    }

    bool stepped = false;
    if (now >= state.pauseUntilMs && now - state.lastStepMs >= STEP_INTERVAL_MS) {
        state.lastStepMs = now;
        state.offset = (int16_t)(state.offset + STEP_PX);
        stepped = true;
        int16_t cycleW = (int16_t)(textW + GAP_PX);
        if (state.offset >= cycleW) {
            state.offset = 0;
            // Kurze Pause auch am Anfang jeder Wiederholung, nicht nur
            // beim allerersten Durchlauf (Alex' Auftrag).
            state.pauseUntilMs = now + START_PAUSE_MS;
        }
    }

    // Waehrend der Pause (vor dem ersten Scroll-Schritt bzw. zwischen zwei
    // Wiederholungen) hat sich nichts veraendert - ueberspringen, statt
    // denselben Frame erneut zu loeschen/zu zeichnen.
    if (!textChanged && !stepped) return;

    // Geclippter Bereich (TFT_eSPI-Viewport) - Koordinaten danach relativ
    // zum Viewport-Ursprung (vpDatum=true, Default). Text wird ZWEIMAL
    // gezeichnet (Original + eine um textW+GAP_PX verschobene Kopie), damit
    // der Uebergang beim Zuruecksetzen auf offset=0 nahtlos wirkt.
    tft.setViewport(x, (int16_t)(y - lineHeight / 2), maxWidth, lineHeight);

    // IMMER den Viewport explizit leeren, bevor die beiden Kopien gezeichnet
    // werden (Alex' Fund, 07.10.: "weisser Block" mitten im Lauftext) -
    // dieser Zweig verliess sich zuvor komplett darauf, dass die beiden
    // drawString()-Aufrufe unten alte Pixel automatisch ueberschreiben. Das
    // gilt aber NICHT fuer den GAP_PX-Zwischenraum zwischen den beiden
    // Text-Kopien: dort zeichnet BEWUSST keiner der beiden drawString()-
    // Aufrufe jemals etwas hin (das ist der Sinn des Abstands), weshalb an
    // genau dieser Stelle Reste eines fruehen Frames (eine Glyphe, die beim
    // Weiterscrollen gerade erst in diese Luecke "hineingewandert" ist)
    // liegen bleiben, bis zufaellig die naechste Kopie denselben Bereich
    // ueberstreicht - sichtbar als stehenbleibender weisser Block. Kein
    // zusaetzliches Flackern dadurch: dieser Codepfad wird ohnehin nur
    // erreicht, wenn gerade neu gezeichnet werden MUSS (textChanged ODER
    // stepped, siehe fruehe Rueckkehr oben) - in beiden Faellen folgen
    // sofort zwei volle drawString()-Aufrufe, das fillRect() davor aendert
    // an der Update-Frequenz nichts, nur am saubereren Ergebnis. Die
    // eigentliche "Marquee-Flimmer-Fix"-Optimierung (CLAUDE.md) bleibt die
    // fruehe Rueckkehr bei reinen Pause-Frames, nicht dieses fillRect().
    tft.fillRect(0, 0, maxWidth, lineHeight, bgColor);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(color, bgColor);
    int16_t localY = (int16_t)(lineHeight / 2);
    tft.drawString(text, (int16_t)(-state.offset), localY);
    tft.drawString(text, (int16_t)(-state.offset + textW + GAP_PX), localY);
    tft.setTextDatum(TL_DATUM);
    tft.resetViewport();
    state.lastDrawnText = text;
    state.everDrawn = true;
}

}
