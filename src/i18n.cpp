#include "i18n.h"
#include "i18n_de.h"
#include "i18n_en.h"
#include "i18n_es.h"
#include "i18n_fr.h"
#include <SD.h>
#include "config.h"

// Mehrsprachen-Dispatch nach EXAKT demselben Muster wie bei Eiswolfs
// Flightradar CYD (dortige i18n.cpp) - nur die Sprachspeicherung ist
// hier lokal (eine eigene kleine SD-Datei, siehe Config::
// SD_LANGUAGE_FILE) statt ueber ein eigenes SettingsStore-Modul, da
// dieses Projekt bewusst keinen grossen zentralen Einstellungen-Speicher
// wie dort hat (siehe CLAUDE.md "Tech-Stack").
namespace I18n {

namespace {
    // Reihenfolge MUSS zu LANG_DE/LANG_EN/LANG_ES/LANG_FR in i18n.h
    // passen - neue Sprachen immer anhaengen, nie einschieben.
    const char* const* TABLES[LANG_COUNT] = {
        I18N_DE, I18N_EN, I18N_ES, I18N_FR
    };

    const char* const LANGUAGE_NAMES[LANG_COUNT] = {
        "Deutsch", "English", "Español", "Français"
    };

    uint8_t currentLanguage = LANG_DE;
}

void init() {
    currentLanguage = LANG_DE;
    File f = SD.open(Config::SD_LANGUAGE_FILE, FILE_READ);
    if (!f) return;
    String line = f.readStringUntil('\n');
    f.close();
    int idx = line.toInt();
    if (idx >= 0 && idx < LANG_COUNT) {
        currentLanguage = (uint8_t)idx;
    }
}

uint8_t language() {
    return currentLanguage;
}

void setLanguagePreview(uint8_t index) {
    if (index >= LANG_COUNT) return;
    currentLanguage = index;
}

void saveLanguage() {
    File f = SD.open(Config::SD_LANGUAGE_FILE, FILE_WRITE);
    if (!f) return;
    f.println(currentLanguage);
    f.close();
}

void setLanguage(uint8_t index) {
    setLanguagePreview(index);
    saveLanguage();
}

const char* t(StringId id) {
    uint16_t idx = (uint16_t)id;
    if (idx >= (uint16_t)StringId::COUNT) return "?";
    return TABLES[currentLanguage][idx];
}

const char* languageName(uint8_t index) {
    if (index >= LANG_COUNT) return "?";
    return LANGUAGE_NAMES[index];
}

}
