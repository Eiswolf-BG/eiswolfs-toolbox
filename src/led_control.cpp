#include "led_control.h"
#include "config.h"

namespace LedControl {

namespace {
    // Aktiv-LOW: ein PWM-Duty von 0 (Pin dauerhaft LOW) bedeutet volle
    // Helligkeit, ein Duty von 255 (Pin dauerhaft HIGH) bedeutet aus -
    // deshalb hier invertiert, der Rest des Moduls denkt in normalen
    // 0-255-Helligkeitswerten (0=aus, 255=voll).
    void writeChannel(uint8_t channel, uint8_t brightness) {
        ledcWrite(channel, (uint8_t)(255 - brightness));
    }

    // An einen Screen gebundenes Blinken (ISS-Radar/Raketenstart).
    bool screenBlinking = false;
    uint8_t screenR = 0, screenG = 0, screenB = 0;
    uint32_t screenIntervalMs = 500;
    bool screenOn = false;
    uint32_t screenLastToggleMs = 0;

    // Zeitlich begrenztes Blinken (Termin-Erinnerungen, Alex' Auftrag,
    // 12.10.) - unabhaengig vom aktiven Screen, stoppt nach timedStopAtMs
    // von selbst. Hat Vorrang vor screenBlinking, solange aktiv.
    bool timedBlinking = false;
    uint8_t timedR = 0, timedG = 0, timedB = 0;
    uint32_t timedIntervalMs = 500;
    bool timedOn = false;
    uint32_t timedLastToggleMs = 0;
    uint32_t timedStopAtMs = 0;

    // Setzt die Hardware passend zum aktuellen Prioritaets-/Blink-Zustand -
    // zeitlich begrenztes Blinken gewinnt, solange es laeuft.
    void applyHardware() {
        if (timedBlinking) {
            if (timedOn) {
                writeChannel(Config::LED_RED_PWM_CHANNEL, timedR);
                writeChannel(Config::LED_GREEN_PWM_CHANNEL, timedG);
                writeChannel(Config::LED_BLUE_PWM_CHANNEL, timedB);
            } else {
                writeChannel(Config::LED_RED_PWM_CHANNEL, 0);
                writeChannel(Config::LED_GREEN_PWM_CHANNEL, 0);
                writeChannel(Config::LED_BLUE_PWM_CHANNEL, 0);
            }
        } else if (screenBlinking) {
            if (screenOn) {
                writeChannel(Config::LED_RED_PWM_CHANNEL, screenR);
                writeChannel(Config::LED_GREEN_PWM_CHANNEL, screenG);
                writeChannel(Config::LED_BLUE_PWM_CHANNEL, screenB);
            } else {
                writeChannel(Config::LED_RED_PWM_CHANNEL, 0);
                writeChannel(Config::LED_GREEN_PWM_CHANNEL, 0);
                writeChannel(Config::LED_BLUE_PWM_CHANNEL, 0);
            }
        } else {
            writeChannel(Config::LED_RED_PWM_CHANNEL, 0);
            writeChannel(Config::LED_GREEN_PWM_CHANNEL, 0);
            writeChannel(Config::LED_BLUE_PWM_CHANNEL, 0);
        }
    }
}

void begin() {
    ledcSetup(Config::LED_RED_PWM_CHANNEL, Config::LED_PWM_FREQ, Config::LED_PWM_RESOLUTION_BITS);
    ledcSetup(Config::LED_GREEN_PWM_CHANNEL, Config::LED_PWM_FREQ, Config::LED_PWM_RESOLUTION_BITS);
    ledcSetup(Config::LED_BLUE_PWM_CHANNEL, Config::LED_PWM_FREQ, Config::LED_PWM_RESOLUTION_BITS);
    ledcAttachPin(Config::LED_RED_PIN, Config::LED_RED_PWM_CHANNEL);
    ledcAttachPin(Config::LED_GREEN_PIN, Config::LED_GREEN_PWM_CHANNEL);
    ledcAttachPin(Config::LED_BLUE_PIN, Config::LED_BLUE_PWM_CHANNEL);
    off();
}

void setColor(uint8_t r, uint8_t g, uint8_t b) {
    writeChannel(Config::LED_RED_PWM_CHANNEL, r);
    writeChannel(Config::LED_GREEN_PWM_CHANNEL, g);
    writeChannel(Config::LED_BLUE_PWM_CHANNEL, b);
}

void off() {
    setColor(0, 0, 0);
}

void startBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs) {
    screenBlinking = true;
    screenR = r;
    screenG = g;
    screenB = b;
    screenIntervalMs = intervalMs;
    screenOn = true;
    screenLastToggleMs = millis();
    applyHardware();
}

void stopBlink() {
    screenBlinking = false;
    applyHardware();
}

void startTimedBlink(uint8_t r, uint8_t g, uint8_t b, uint32_t intervalMs, uint32_t durationMs) {
    timedBlinking = true;
    timedR = r;
    timedG = g;
    timedB = b;
    timedIntervalMs = intervalMs;
    timedOn = true;
    timedLastToggleMs = millis();
    timedStopAtMs = millis() + durationMs;
    applyHardware();
}

void update() {
    bool changed = false;

    if (timedBlinking) {
        if ((int32_t)(millis() - timedStopAtMs) >= 0) {
            timedBlinking = false;
            changed = true;
        } else if (millis() - timedLastToggleMs >= timedIntervalMs) {
            timedOn = !timedOn;
            timedLastToggleMs = millis();
            changed = true;
        }
    }

    if (screenBlinking && millis() - screenLastToggleMs >= screenIntervalMs) {
        screenOn = !screenOn;
        screenLastToggleMs = millis();
        changed = true;
    }

    if (changed) applyHardware();
}

}
