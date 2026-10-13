#pragma once
#include <Arduino.h>

// Mehrsprachen-Text-Zugriffsschicht nach EXAKT demselben Muster wie bei
// Eiswolfs Flightradar CYD (StringId-Enum + ein Sprachdatei-Header pro
// Sprache, siehe i18n_de.h/i18n_en.h/i18n_es.h/i18n_fr.h) - hier bewusst
// mit 4 statt 8 Sprachen (Deutsch, Englisch, Spanisch, Französisch),
// siehe CLAUDE.md "Sprache". Deutsch ist Index 0 (Standard/Fallback,
// bevor eine Sprache explizit gewaehlt wurde).
enum class StringId : uint16_t {
    OK = 0,

    WIFI_SCANNING,
    WIFI_NO_NETWORKS,
    WIFI_SELECT,
    WIFI_CONNECTED_BANG,
    WIFI_CONNECTION_FAILED,
    WIFI_BACK_TO_LIST_MSG,
    WIFI_PASSWORD_LABEL,
    WIFI_SPACE,
    WIFI_CONNECT,
    WIFI_BACK_TO_LIST_BTN,
    WIFI_ALREADY_3,
    WIFI_REMOVE_ONE_FIRST,
    WIFI_CONNECTING,
    WIFI_LABEL_PREFIX,

    LOCATION_NAME_PROMPT,
    LOCATION_NAME_SKIP,

    ADDRESS_SEARCH_TITLE,
    ADDRESS_SEARCH_SEARCHING,
    ADDRESS_SEARCH_NO_RESULTS,
    ADDRESS_SEARCH_ERROR,
    ADDRESS_SEARCH_CONFIRM_TITLE,
    ADDRESS_SEARCH_USE_THIS,
    ADDRESS_SEARCH_CANCEL,
    ADDRESS_SEARCH_TRY_AGAIN,
    ADDRESS_SEARCH_MULTIPLE_TITLE,
    ADDRESS_SEARCH_HINT,

    FIRST_RUN_LOCATION_TITLE,
    FIRST_RUN_LOCATION_BODY,
    FIRST_RUN_LOCATION_SET_BTN,
    FIRST_RUN_LOCATION_SKIP_BTN,

    FIRST_RUN_COMPLETE_TITLE,
    FIRST_RUN_COMPLETE_BODY1,
    FIRST_RUN_COMPLETE_BODY2,
    FIRST_RUN_COMPLETE_TAP_TO_SKIP,

    LOCATION_NAME_HINT,

    WIFI_MANUAL_SSID,
    WIFI_ENTER_SSID_LABEL,
    WIFI_NEXT,

    CALIB_TITLE,
    CALIB_PROMPT,
    CALIB_TOP_LEFT,
    CALIB_TOP_RIGHT,
    CALIB_BOTTOM_LEFT,
    CALIB_BOTTOM_RIGHT,
    CALIB_SAVED,

    BACK_BTN,

    ISS_RADAR_TITLE,
    ISS_RADAR_LOADING,
    ISS_RADAR_FETCH_ERROR,
    ISS_RADAR_BELOW_HORIZON,
    ISS_RADAR_NO_LOCATION,
    ISS_RADAR_DISTANCE_LABEL,
    ISS_RADAR_ALTITUDE_LABEL,
    ISS_RADAR_VELOCITY_LABEL,
    ISS_RADAR_AZIMUTH_LABEL,
    ISS_RADAR_ELEVATION_LABEL,
    ISS_RADAR_VISIBILITY_LABEL,
    ISS_RADAR_VISIBILITY_DAYLIGHT,
    ISS_RADAR_VISIBILITY_ECLIPSED,
    ISS_RADAR_VISIBILITY_UNKNOWN,
    ISS_RADAR_INFO_TITLE,
    ISS_RADAR_INFO_BODY,

    COMPASS_N,
    COMPASS_NE,
    COMPASS_E,
    COMPASS_SE,
    COMPASS_S,
    COMPASS_SW,
    COMPASS_W,
    COMPASS_NW,

    // Status-Screen (main.cpp::drawStatus(), WLAN-Verbindungsaufbau beim
    // Boot) - nachtraeglich auf StringIds umgestellt (siehe CLAUDE.md,
    // Mehrsprachen-Umstellung).
    STATUS_CONNECTING,
    STATUS_CONNECTED,
    STATUS_NOT_CONNECTED,
    STATUS_IP_LABEL,

    // Einstellungs-Bereich (app_navigation.cpp::SETTINGS_SCREENS[]),
    // erreichbar per Hoch-/Runter-Wisch von jedem Feature-Screen aus
    // (siehe CLAUDE.md "Navigation: Wisch-Gesten statt Menue"). Diese
    // beiden IDs sind generisch (swipe_intro_screen.cpp-Pfeillabels,
    // Hoch/Runter-Hinweiszeile auf jeder Unterseite) - die einzelnen
    // Unterseiten-Titel/-Texte stehen weiter unten.
    SETTINGS_TITLE,
    SETTINGS_SWIPE_HINT,

    // Raketenstart-Countdown (rocket_screen.cpp/rocket_client.cpp) -
    // zweiter Feature-Screen im Wisch-Karussell, siehe CLAUDE.md
    // "Navigation: Wisch-Gesten statt Menue".
    ROCKET_TITLE,
    ROCKET_LOADING,
    ROCKET_FETCH_ERROR,
    ROCKET_MISSION_LABEL,
    ROCKET_ROCKET_LABEL,
    ROCKET_SITE_LABEL,
    ROCKET_STATUS_LABEL,
    ROCKET_COUNTDOWN_LABEL,
    ROCKET_UNIT_DAYS,
    ROCKET_INFO_TITLE,
    ROCKET_INFO_BODY,
    ROCKET_STATUS_GO,
    ROCKET_STATUS_TBD,
    ROCKET_STATUS_HOLD,
    ROCKET_STATUS_SUCCESS,
    ROCKET_STATUS_FAILURE,
    ROCKET_STATUS_IN_FLIGHT,
    ROCKET_STATUS_PARTIAL_FAILURE,
    // Ergaenzt 07.10. (Alex' Auftrag) - die Launch-Library-2-API hat
    // aktuell exakt 9 feste status.id-Werte (1-9), diese beiden fehlten
    // bisher in der Tabelle.
    ROCKET_STATUS_TBC,
    ROCKET_STATUS_PAYLOAD_DEPLOYED,

    // Einmaliger Wisch-Erklaer-Screen nach der Ersteinrichtung (siehe
    // swipe_intro_screen.cpp) - SETTINGS_TITLE ("Einstellungen") wird
    // dort fuer die Hoch-/Runter-Pfeile wiederverwendet, kein eigener
    // StringId dafuer noetig.
    SWIPE_INTRO_TEXT,
    SWIPE_INTRO_SWITCH_LABEL,
    // Nur Links/Rechts-Wischen verlaesst diesen Screen und committet die
    // Ersteinrichtung (Alex' Auftrag, 05.10.) - Hoch/Runter loest hier
    // bewusst (noch) nichts aus.
    SWIPE_INTRO_START_HINT,

    // "SD-Karte erforderlich"-Screen (main.cpp) - analog zum gleich-
    // namigen Screen bei Eiswolfs Flightradar CYD (dortige main.cpp::
    // haltWithSdRequiredScreen()), inhaltlich angepasst/uebersetzt statt
    // 1:1 kopiert.
    SD_REQUIRED_LINE1,
    SD_REQUIRED_LINE2,
    SD_REQUIRED_LINE3,
    SD_REQUIRED_HINT,
    SD_REQUIRED_TROUBLESHOOTING,

    // "?"-Erklaerung auf dem Standort-Screen der Ersteinrichtung (siehe
    // first_run_location_screen.cpp) - warum die App ueberhaupt nach
    // einer Adresse fragt.
    FIRST_RUN_LOCATION_WHY_QUESTION,
    FIRST_RUN_LOCATION_WHY_BODY,

    // Erweiterter Einstellungs-Bereich mit mehreren THEMEN-Unterseiten
    // (Alex' Auftrag, 06.10., korrigiert noch am selben Tag: Unterseiten
    // sind Themen-Seiten mit mehreren Buttons, NICHT eine Seite pro
    // Einzelfunktion - siehe app_navigation.cpp::SETTINGS_SCREENS[]:
    // Sprache/System/Theme). SETTINGS_TITLE/SETTINGS_SWIPE_HINT (oben,
    // bereits vorhanden) bleiben fuer den allgemeinen Hoch/Runter-
    // Zusammenhang (swipe_intro_screen.cpp, Hinweiszeile unten auf jeder
    // Unterseite) - hier die Titel/Texte der einzelnen Unterseiten.
    SETTINGS_LANGUAGE_TITLE,

    SETTINGS_SYSTEM_TITLE,

    // Helligkeit - oeffnet einen Regler-Unterbildschirm (settings_system_
    // screen.cpp::runBrightnessScreen()).
    SETTINGS_SYSTEM_BRIGHTNESS_LABEL,
    SETTINGS_SYSTEM_BRIGHTNESS_INFO_TITLE,
    SETTINGS_SYSTEM_BRIGHTNESS_INFO_BODY,

    // Screenflip - einfacher Direkt-Toggle (kein Unterbildschirm noetig).
    SETTINGS_ROTATION_LABEL,
    SETTINGS_ROTATION_ON,
    SETTINGS_ROTATION_OFF,
    SETTINGS_ROTATION_INFO_TITLE,
    SETTINGS_ROTATION_INFO_BODY,

    // Bildschirm-Timeout - oeffnet eine Auswahl-Unterseite mit festen
    // Zeitstufen (settings_system_screen.cpp::runTimeoutScreen()).
    SETTINGS_SYSTEM_TIMEOUT_LABEL,
    SETTINGS_SYSTEM_TIMEOUT_INFO_TITLE,
    SETTINGS_SYSTEM_TIMEOUT_INFO_BODY,
    SETTINGS_SYSTEM_TIMEOUT_NEVER,
    SETTINGS_SYSTEM_TIMEOUT_UNIT,

    // WLAN - oeffnet denselben Dialog wie in der Ersteinrichtung
    // (WifiSetupScreen::run()). SETTINGS_WIFI_CHANGE_BTN dient hier
    // zugleich als Zeilen-Label.
    SETTINGS_WIFI_CHANGE_BTN,
    SETTINGS_SYSTEM_WIFI_INFO_TITLE,
    SETTINGS_SYSTEM_WIFI_INFO_BODY,

    // Unterer Wisch-Streifen - NICHT MEHR VERWENDET seit dem Umbau auf den
    // Hub-Screen (Alex' Auftrag, 06.10., noch am selben Tag): die
    // wischbaren Themenseiten samt Streifen/Pfeilen/Zonenerkennung wurden
    // durch den Hub-Screen + Tap-only-Untermenues ersetzt (siehe unten).
    // StringId bewusst NICHT geloescht/umsortiert (Positionen muessen in
    // allen 4 Sprachdateien exakt gleich bleiben), bleibt einfach ungenutzt
    // liegen.
    SETTINGS_SWIPE_HERE_HINT,

    // Hub-Screen "System" (Alex' Auftrag, 06.10. - kompletter Neubau des
    // Einstellungs-Bereichs: EIN zentraler Button-Hub statt wischbarer
    // Themenseiten, siehe settings_hub_screen.cpp). SETTINGS_SYSTEM_TITLE
    // (oben, "System") dient als Hub-Titel, SETTINGS_LANGUAGE_TITLE
    // ("Sprache") zugleich als Button-Beschriftung fuer das Sprache-
    // Untermenue - fuer beide kein eigener neuer StringId noetig.
    SETTINGS_HUB_WIFI_BTN,
    SETTINGS_HUB_DISPLAY_BTN,
    SETTINGS_HUB_TOOLS_BTN,
    SETTINGS_HUB_LED_BTN,
    SETTINGS_HUB_HINT,

    // Platzhalter-Untermenues (Tools, LED) - noch ohne echte Funktionen.
    SETTINGS_PLACEHOLDER_NO_FUNCTIONS,

    // WLAN-Verwaltung (settings_wifi_manage_screen.cpp) - 1:1 vom Konzept
    // aus Eiswolfs Flightradar CYD uebernommen (dortige
    // wifi_manage_screen.cpp: bis zu 3 Netzwerk-Slots, je mit "X"-Entfernen-
    // Button, "Netzwerk hinzufuegen" wenn noch Platz ist), an Toolbox'
    // WifiMgr/WifiSetupScreen angepasst. SETTINGS_ROTATION_INFO_TITLE
    // ("Was macht das?") wird hier fuer den "?"-Infobutton wiederverwendet.
    SETTINGS_WIFI_MANAGE_TITLE,
    SETTINGS_WIFI_EMPTY_SLOT,
    SETTINGS_WIFI_ADD_NETWORK,
    SETTINGS_WIFI_MANAGE_INFO_BODY,

    // OEPNV-Abfahrtstafel (transit_client.h/transit_screen.h) - dritter
    // Feature-Screen im Wisch-Karussell (Alex' Auftrag, 06.10.), siehe
    // CLAUDE.md "OEPNV-Abfahrtstafel". TRANSIT_FETCH_ERROR_* sind bewusst
    // 3 getrennte StringIds (keine Verbindung/keine Haltestelle/keine
    // Abfahrten), damit der Nutzer den tatsaechlichen Grund sieht statt
    // einer generischen Fehlermeldung.
    TRANSIT_TITLE,
    TRANSIT_LOADING,
    TRANSIT_FETCH_ERROR_NETWORK,
    TRANSIT_FETCH_ERROR_NO_STOP,
    TRANSIT_FETCH_ERROR_NO_DEPARTURES,
    TRANSIT_STOP_LABEL,
    TRANSIT_DEPARTURE_NOW,
    TRANSIT_IN_PREFIX,
    TRANSIT_UNIT_MIN,
    TRANSIT_REFINE_ADDRESS_BTN,
    TRANSIT_INFO_TITLE,
    TRANSIT_INFO_BODY,

    // Kompaktes Zeitformat ab 60 Minuten (Alex' Auftrag, 06.10. -
    // Korrektur der ueberlaufenden Zeitspalte: "Xh YYm" statt "175 Min").
    // Kurz/ohne Leerzeichen davor, anders als TRANSIT_UNIT_MIN (" Min",
    // fuer den Unter-60-Minuten-Fall).
    TRANSIT_UNIT_HOUR_SHORT,
    TRANSIT_UNIT_MIN_SHORT,

    // Detail-Screen einer einzelnen Abfahrt (Alex' Auftrag, 06.10. - jede
    // Abfahrtszeile in der Liste ist jetzt antippbar). Zeigt Felder, die
    // Transitous pro Abfahrt mitliefert, in der kompakten Liste aber
    // ungenutzt blieben (siehe transit_client.h fuer die Rohdaten).
    TRANSIT_DETAIL_TITLE,
    TRANSIT_DETAIL_FROM_LABEL,
    TRANSIT_DETAIL_TO_LABEL,
    TRANSIT_DETAIL_AGENCY_LABEL,
    TRANSIT_DETAIL_TRACK_LABEL,
    TRANSIT_DETAIL_STATUS_LABEL,
    TRANSIT_DETAIL_REALTIME_YES,
    TRANSIT_DETAIL_REALTIME_NO,
    TRANSIT_DETAIL_DEPARTS_LABEL,

    // Weitere API-Felder im Detail-Screen + Rundfahrt-/Ausfall-Kennzeichnung
    // (Alex' Auftrag, 06.10. - Korrektur-Runde nach dem Foto-Fund bei Linie
    // 963, siehe CLAUDE.md "OEPNV-Abfahrtstafel").
    TRANSIT_DETAIL_LOOP_LABEL,
    TRANSIT_CANCELLED_LABEL,
    TRANSIT_DETAIL_DELAY_LABEL,
    TRANSIT_DETAIL_ON_TIME,
    TRANSIT_DETAIL_ROUTE_LONGNAME_LABEL,
    TRANSIT_DETAIL_WHEELCHAIR_LABEL,
    TRANSIT_DETAIL_WHEELCHAIR_YES,
    TRANSIT_DETAIL_WHEELCHAIR_NO,
    TRANSIT_DETAIL_WHEELCHAIR_UNKNOWN,
    TRANSIT_DETAIL_BIKES_LABEL,
    TRANSIT_DETAIL_BIKES_YES,
    TRANSIT_DETAIL_BIKES_NO,

    // Kurze Wochentags-Abkuerzungen (Sonntag=0, wie tm_wday/unsere eigene
    // Wochentagsberechnung in weather_screen.cpp) - fuer die
    // Wetter-Wochenuebersicht.
    WEEKDAY_SUN,
    WEEKDAY_MON,
    WEEKDAY_TUE,
    WEEKDAY_WED,
    WEEKDAY_THU,
    WEEKDAY_FRI,
    WEEKDAY_SAT,

    // Wetter-Screen (weather_client.h/weather_screen.h) - vierter
    // Feature-Screen im Wisch-Karussell (Alex' Auftrag, 06.10.), siehe
    // CLAUDE.md "Wetter-Screen". Nutzt NUR die allgemeine Toolbox-Position
    // (kein eigener Adress-Button mehr, siehe "Adresse aendern" unten).
    WEATHER_TITLE,
    WEATHER_LOADING,
    WEATHER_FETCH_ERROR_NETWORK,
    WEATHER_FEELS_LIKE_LABEL,
    WEATHER_FORECAST_LABEL,
    WEATHER_WEEK_OVERVIEW_BTN,
    WEATHER_WEEKLY_TITLE,
    WEATHER_INFO_TITLE,
    WEATHER_INFO_BODY,

    // "Adresse aendern" - erste echte Funktion im "Tools"-Untermenue des
    // Einstellungs-Hubs (Alex' Auftrag, 06.10.: die allgemeine
    // Toolbox-Position wird ab jetzt NUR noch hier geaendert, einzelne
    // Feature-Screens bieten dafuer keine eigene Eingabe mehr - Ausnahme
    // bleibt der OEPNV-Screen mit seinem eigenen "Genauere Adresse"-Button
    // fuer eine ANDERE Haltestelle, siehe CLAUDE.md).
    SETTINGS_TOOLS_CHANGE_ADDRESS_BTN,
    SETTINGS_TOOLS_CURRENT_LABEL,
    SETTINGS_TOOLS_NO_LOCATION,
    SETTINGS_TOOLS_WIFI_LOCATION,
    SETTINGS_TOOLS_INFO_TITLE,
    SETTINGS_TOOLS_INFO_BODY,

    // Status-Zeile in der ISS-Radar-Datenliste (Alex' Auftrag, 06.10. -
    // ersetzt den bisherigen Status-Text MITTEN IM KREIS, der jetzt
    // komplett entfernt wurde, siehe CLAUDE.md "ISS-Radar: gepunktete
    // Weltkarte"). ISS_RADAR_BELOW_HORIZON (oben) bleibt als vollstaendiger
    // Satz fuer evtl. andere Zwecke definiert, wird aber fuer DIESE Zeile
    // NICHT mehr verwendet (Label+Kurzwert statt ganzer Satz).
    ISS_RADAR_STATUS_LABEL,
    ISS_RADAR_STATUS_VISIBLE,
    ISS_RADAR_STATUS_BELOW_HORIZON,

    // Raketenstart: naechste 3 Starts statt nur 1, zusaetzliche API-Felder
    // (Alex' Auftrag, 06.10. - "limit=3" liefert das in EINER Anfrage,
    // siehe CLAUDE.md "Raketenstart-Countdown"). ROCKET_DETAIL_* werden
    // vom Detail-Screen verwendet, der fuer JEDEN der bis zu 3 Starts
    // gilt (ueber "Weitere Starts" erreichbar).
    ROCKET_AGENCY_LABEL,
    ROCKET_CRITICAL_LABEL,
    ROCKET_MORE_LAUNCHES_BTN,
    ROCKET_MORE_LAUNCHES_TITLE,
    ROCKET_DETAIL_TITLE,
    ROCKET_DETAIL_TYPE_LABEL,
    ROCKET_DETAIL_ORBIT_LABEL,
    ROCKET_DETAIL_HOLD_LABEL,
    ROCKET_DETAIL_FAIL_LABEL,
    ROCKET_DETAIL_PROBABILITY_LABEL,
    ROCKET_DETAIL_WEATHER_LABEL,
    ROCKET_DETAIL_DATE_LABEL,

    // Formel 1 (Alex' Auftrag, 06.10., siehe CLAUDE.md "Formel 1") - letzter
    // Feature-Screen im Wisch-Karussell. F1_SESSION_* werden sowohl fuer den
    // Live-Session-Namen (OpenF1 session_name) als auch fuer die
    // Sessionzeiten-Liste im Detail-Screen verwendet - FESTE Uebersetzung
    // statt den rohen englischen API-Wert durchzureichen (Alex' Auftrag,
    // 06.10.: "ich will kein englisches Wort sehen", siehe CLAUDE.md
    // "Weltraumwetter"-Lehre). F1_STATUS_* decken die haeufigsten
    // Ergast/Jolpica-"status"-Werte ab, F1_STATUS_DNF_GENERIC faengt JEDEN
    // nicht abgedeckten Wert als uebersetzter (NICHT roher englischer)
    // Sammelbegriff ab - es gibt bewusst KEINEN Rohtext-Fallback mehr bei
    // diesem Screen.
    F1_TITLE,
    F1_LOADING,
    F1_FETCH_ERROR,
    F1_MORE_BTN,
    F1_COUNTDOWN_LABEL,
    F1_TOP_DRIVERS_LABEL,
    F1_SESSION_PRACTICE1,
    F1_SESSION_PRACTICE2,
    F1_SESSION_PRACTICE3,
    F1_SESSION_QUALIFYING,
    F1_SESSION_SPRINT_QUALIFYING,
    F1_SESSION_SPRINT,
    F1_SESSION_RACE,
    F1_SESSION_GENERIC,
    F1_DETAIL_TITLE,
    F1_DETAIL_LIVE_TITLE,
    F1_DETAIL_DRIVER_STANDINGS_TITLE,
    F1_DETAIL_CONSTRUCTOR_STANDINGS_TITLE,
    F1_DETAIL_LAST_RESULT_LABEL,
    F1_DETAIL_NEXT_SESSIONS_TITLE,
    F1_POINTS_SHORT,
    F1_WINS_SHORT,
    F1_GAP_LAP_SINGULAR,
    F1_GAP_LAP_PLURAL,
    F1_STATUS_FINISHED,
    F1_STATUS_RETIRED,
    F1_STATUS_ACCIDENT,
    F1_STATUS_COLLISION,
    F1_STATUS_ENGINE,
    F1_STATUS_GEARBOX,
    F1_STATUS_TRANSMISSION,
    F1_STATUS_HYDRAULICS,
    F1_STATUS_ELECTRICAL,
    F1_STATUS_BRAKES,
    F1_STATUS_SUSPENSION,
    F1_STATUS_PUNCTURE,
    F1_STATUS_SPUN_OFF,
    F1_STATUS_DISQUALIFIED,
    F1_STATUS_NOT_CLASSIFIED,
    F1_STATUS_DID_NOT_START,
    F1_STATUS_DID_NOT_QUALIFY,
    F1_STATUS_WITHDREW,
    F1_STATUS_DNF_GENERIC,
    F1_INFO_TITLE,
    F1_INFO_BODY,

    // Luftqualitaet im Wetter-Screen (Alex' Auftrag, 06.10., siehe
    // CLAUDE.md "Wetter-Screen: Luftqualitaet") - Erweiterung des
    // bestehenden Wetter-Screens, KEIN eigener Feature-Screen.
    WEATHER_AQI_CAT_GOOD,
    WEATHER_AQI_CAT_MODERATE,
    WEATHER_AQI_CAT_MEDIUM,
    WEATHER_AQI_CAT_POOR,
    WEATHER_AQI_CAT_VERY_POOR,
    WEATHER_AQI_CAT_EXTREME,
    WEATHER_AQI_DETAIL_TITLE,
    WEATHER_AQI_EU_LABEL,
    WEATHER_AQI_US_LABEL,
    WEATHER_AQI_PM25_LABEL,
    WEATHER_AQI_PM25_EXPLAIN,
    WEATHER_AQI_PM10_LABEL,
    WEATHER_AQI_PM10_EXPLAIN,
    WEATHER_AQI_NO2_LABEL,
    WEATHER_AQI_NO2_EXPLAIN,
    WEATHER_AQI_O3_LABEL,
    WEATHER_AQI_O3_EXPLAIN,
    WEATHER_AQI_SO2_LABEL,
    WEATHER_AQI_SO2_EXPLAIN,
    WEATHER_AQI_CO_LABEL,
    WEATHER_AQI_DUST_LABEL,
    WEATHER_AQI_AMMONIA_LABEL,
    WEATHER_AQI_INDEX_LABEL,
    WEATHER_AQI_UNAVAILABLE,

    // Globale Einheiten-Einstellung (Metrisch/Imperial, Alex' Auftrag,
    // 07.10., siehe units.h/.cpp und CLAUDE.md "Pflicht: globale
    // Einheiten-Einstellung beachten"). WEATHER_AQI_US_CAT_* sind die
    // offiziellen US-EPA-AQI-Kategorien - EIGENSTAENDIG neben den
    // bereits vorhandenen europaeischen WEATHER_AQI_CAT_*-Kategorien
    // oben, NICHT wiederverwendet, da die US-Skala andere Stufen-Grenzen
    // hat (0-50/51-100/101-150/151-200/201-300/301-500) und eine davon
    // ("Unhealthy for Sensitive Groups") in der europaeischen Skala kein
    // Gegenstueck hat.
    SETTINGS_UNITS_TITLE,
    UNITS_METRIC,
    UNITS_IMPERIAL,
    WEATHER_AQI_US_CAT_GOOD,
    WEATHER_AQI_US_CAT_MODERATE,
    WEATHER_AQI_US_CAT_UNHEALTHY_SENSITIVE,
    WEATHER_AQI_US_CAT_UNHEALTHY,
    WEATHER_AQI_US_CAT_VERY_UNHEALTHY,
    WEATHER_AQI_US_CAT_HAZARDOUS,

    // Krypto-Ticker (crypto_client.h/crypto_screen.h, Alex' Auftrag,
    // 07.10.) - fuenfter Feature-Screen im Wisch-Karussell.
    CRYPTO_TITLE,
    CRYPTO_LOADING,
    CRYPTO_FETCH_ERROR,
    CRYPTO_INFO_TITLE,
    CRYPTO_INFO_BODY,
    CRYPTO_MORE_COINS_TITLE,
    // Sprachabhaengiges 3-Zeichen-Kuerzel fuer die kleine F&G-Kachel auf
    // dem Hauptscreen (Alex' Auftrag, 07.10. - "ohne Englischkenntnisse
    // verstaendlich"), z.B. DE "A&G" (Angst&Gier), siehe crypto_screen.cpp.
    CRYPTO_FNG_TILE_LABEL,
    // Fear&Greed-Klassifikation - feste 5er-Tabelle (gleiches Prinzip wie
    // bei den F1-/Rocket-Status-Tabellen), primaer ueber den rohen
    // API-Text (value_classification) abgeglichen, siehe crypto_screen.cpp.
    CRYPTO_FNG_EXTREME_FEAR,
    CRYPTO_FNG_FEAR,
    CRYPTO_FNG_NEUTRAL,
    CRYPTO_FNG_GREED,
    CRYPTO_FNG_EXTREME_GREED,
    // Manuelle Waehrungsauswahl (Alex' Auftrag, 12.10., siehe
    // crypto_currency_screen.h) - CRYPTO_CURRENCY_BTN_LABEL ist nur das
    // Wort "Waehrung" (der aktuelle Code wird im Code selbst angehaengt).
    CRYPTO_CURRENCY_BTN_LABEL,
    CRYPTO_CURRENCY_TITLE,

    // Erdbeben-Screen (earthquake_client.h/earthquake_screen.h, Alex'
    // Auftrag, 07.10.) - sechster und letzter Feature-Screen im
    // Wisch-Karussell.
    EARTHQUAKE_TITLE,
    EARTHQUAKE_LOADING,
    EARTHQUAKE_FETCH_ERROR,
    EARTHQUAKE_INFO_TITLE,
    EARTHQUAKE_INFO_BODY,
    EARTHQUAKE_MORE_BTN,
    EARTHQUAKE_MORE_TITLE,
    // Dezente Erklaerung neben der "M X.X"-Zahl auf der Hauptkachel
    // (Alex' Auftrag, 12.10.), KEIN Doppelpunkt/Leerzeichen-Suffix wie die
    // Label/Wert-Zeilen unten - steht als eigenstaendiges Wort direkt
    // neben der Zahl, siehe earthquake_screen.cpp::drawMainTileContent().
    EARTHQUAKE_MAGNITUDE_LABEL,
    EARTHQUAKE_PLACE_LABEL,
    EARTHQUAKE_DEPTH_LABEL,
    EARTHQUAKE_TIME_LABEL,
    EARTHQUAKE_TSUNAMI_WARNING,

    // Kalender-Screen (holiday_client.h/calendar_screen.h, Alex' Auftrag,
    // 12.10.) - direkt neben dem Wetter-Screen im Wisch-Karussell.
    CALENDAR_TITLE,
    CALENDAR_LOADING,
    CALENDAR_FETCH_ERROR,
    CALENDAR_INFO_TITLE,
    CALENDAR_INFO_BODY,
    // Eigene Termine (Alex' Auftrag, 12.10., zweite Runde: eigener Tages-
    // Detail-Screen statt Detailzeile) - Beschriftung des "Neuen Termin
    // erstellen"-Buttons auf diesem Screen.
    CALENDAR_ADD_APPOINTMENT_BTN,
    // Tastatur-Kopfzeile fuer die Termintext-Eingabe (lokal duplizierte
    // Tastatur, siehe calendar_screen.cpp::runAppointmentKeyboard()).
    CALENDAR_APPOINTMENT_PROMPT,
    // Wochentagskuerzel, FEST Montag-first fuer alle 4 Sprachen (ISO-8601-
    // Konvention, einheitliches Raster statt je Sprache unterschiedlicher
    // Wochenstart) - exakt 7 Eintraege, Reihenfolge Mo..So.
    CALENDAR_WEEKDAY_MON,
    CALENDAR_WEEKDAY_TUE,
    CALENDAR_WEEKDAY_WED,
    CALENDAR_WEEKDAY_THU,
    CALENDAR_WEEKDAY_FRI,
    CALENDAR_WEEKDAY_SAT,
    CALENDAR_WEEKDAY_SUN,
    // Monatsnamen fuer die Kopfzeile ("Oktober 2026") - exakt 12 Eintraege,
    // Reihenfolge Januar..Dezember.
    CALENDAR_MONTH_JAN,
    CALENDAR_MONTH_FEB,
    CALENDAR_MONTH_MAR,
    CALENDAR_MONTH_APR,
    CALENDAR_MONTH_MAY,
    CALENDAR_MONTH_JUN,
    CALENDAR_MONTH_JUL,
    CALENDAR_MONTH_AUG,
    CALENDAR_MONTH_SEP,
    CALENDAR_MONTH_OCT,
    CALENDAR_MONTH_NOV,
    CALENDAR_MONTH_DEC,

    // LED-Benachrichtigungs-Uebersicht (settings_led_screen.cpp,
    // led_notifications.h, Alex' Auftrag, 12.10.) - Haupt-Toggle + ein
    // Label/Info-Paar je LED-faehigem Screen (aktuell ISS-Radar,
    // Raketenstart). AN/AUS-Wortlaute werden bewusst wiederverwendet
    // (SETTINGS_ROTATION_ON/OFF, siehe dort - generische "Ein"/"Aus"-
    // Woerter trotz des Namens).
    SETTINGS_LED_MASTER_LABEL,
    SETTINGS_LED_MASTER_INFO_TITLE,
    SETTINGS_LED_MASTER_INFO_BODY,
    SETTINGS_LED_ISS_LABEL,
    SETTINGS_LED_ISS_INFO_TITLE,
    SETTINGS_LED_ISS_INFO_BODY,
    SETTINGS_LED_ROCKET_LABEL,
    SETTINGS_LED_ROCKET_INFO_TITLE,
    SETTINGS_LED_ROCKET_INFO_BODY,

    // Optionale Uhrzeit fuer eigene Termine (Alex' Auftrag, 12.10., erster
    // Teilschritt Richtung Termin-Erinnerung) - eigener, ueberspringbarer
    // Schritt nach der Termintext-Tastatur (calendar_screen.cpp::
    // runTimePicker()).
    CALENDAR_TIME_PROMPT,
    CALENDAR_HOUR_LABEL,
    CALENDAR_MINUTE_LABEL,
    CALENDAR_SAVE_WITHOUT_TIME,

    // Termin-Erinnerung, zweiter/letzter Teil (Alex' Auftrag, 12.10.) -
    // Auswahl-Schritt nach der Uhrzeit, siehe calendar_screen.cpp::
    // runReminderPicker(). CALENDAR_REMINDER_DAY1 ist der Basistext OHNE
    // Uhrzeit-Zusatz - der wird bei fehlender Termin-Uhrzeit im Code
    // dynamisch angehaengt ("(09:00)"), keine eigene StringId noetig.
    CALENDAR_REMINDER_PROMPT,
    CALENDAR_REMINDER_NONE,
    CALENDAR_REMINDER_MIN10,
    CALENDAR_REMINDER_MIN30,
    CALENDAR_REMINDER_HOUR1,
    CALENDAR_REMINDER_DAY1,

    // Dritter LED-faehiger Screen (led_notifications.h::ScreenId::Reminder) -
    // gleiches Label/Info-Paar-Muster wie SETTINGS_LED_ISS_*/SETTINGS_LED_
    // ROCKET_* oben.
    SETTINGS_LED_REMINDER_LABEL,
    SETTINGS_LED_REMINDER_INFO_TITLE,
    SETTINGS_LED_REMINDER_INFO_BODY,

    // Naturereignisse-Screen (natural_events_client.h/natural_events_
    // screen.h, Alex' Auftrag, 12.10.) - siebter Feature-Screen im
    // Wisch-Karussell, direkt nach dem Kalender-Screen.
    NATURAL_EVENTS_TITLE,
    NATURAL_EVENTS_LOADING,
    NATURAL_EVENTS_FETCH_ERROR,
    NATURAL_EVENTS_INFO_TITLE,
    NATURAL_EVENTS_INFO_BODY,
    NATURAL_EVENTS_MORE_BTN,
    NATURAL_EVENTS_MORE_TITLE,
    NATURAL_EVENTS_TOTAL_LABEL,
    // Kategorie-Label, Reihenfolge entspricht NaturalEventsClient::Category
    // (Volcano/Storm/Flood/Landslide) - sowohl fuer die Kurzfassung auf der
    // Hauptkachel als auch fuer das gruene Label in der "Weitere
    // Ereignisse"-Liste.
    NATURAL_EVENTS_CATEGORY_VOLCANO,
    NATURAL_EVENTS_CATEGORY_STORM,
    NATURAL_EVENTS_CATEGORY_FLOOD,
    NATURAL_EVENTS_CATEGORY_LANDSLIDE,

    // Wiederverwendbare "Nochmal versuchen"-Komponente (network_retry_
    // view.h, Alex' Auftrag, 12.10.) - EIN Button-Text fuer ALLE Screens
    // mit Netzwerk-Abruf, nicht pro Screen dupliziert.
    NETWORK_RETRY_BTN,

    // Gemeinsamer Geduld-Hinweis (Alex' Auftrag, 13.10.) - wird an JEDEN
    // "Lade .../Loading..."-Vollbild-Hinweis angehaengt (Leerzeichen-
    // getrennt, drawWrappedCentered() laeuft automatisch in weitere
    // Zeilen), damit klar ist, dass ein Abruf etwas dauern kann. NICHT bei
    // den kleinen Mini-"Lade..."-Labels innerhalb einzelner Krypto-
    // Kacheln verwendet (zu wenig Platz dort).
    LOADING_PATIENCE_HINT,

    COUNT
};

namespace I18n {
    constexpr uint8_t LANG_COUNT = 4;
    // Reihenfolge fest (siehe i18n.cpp TABLES[]) - Deutsch bewusst Index 0
    // (Standard/Fallback), danach Englisch/Spanisch/Franzoesisch. Neue
    // Sprachen IMMER anhaengen, nie mittendrin einsortieren (sonst
    // verschiebt sich eine bereits auf der SD-Karte gespeicherte
    // Sprachnummer).
    constexpr uint8_t LANG_DE = 0;
    constexpr uint8_t LANG_EN = 1;
    constexpr uint8_t LANG_ES = 2;
    constexpr uint8_t LANG_FR = 3;

    // Laedt die zuletzt gespeicherte Sprache von der SD-Karte (siehe
    // Config::SD_LANGUAGE_FILE) - faellt auf Deutsch (Index 0) zurueck,
    // falls keine Datei existiert oder sie ungueltig ist. Einmal beim
    // Boot aufrufen, bevor der erste uebersetzte Text gezeichnet wird.
    void init();

    uint8_t language();
    // Setzt die Sprache UND speichert sie sofort auf der SD-Karte - fuer
    // die Verwendung NACH der Ersteinrichtung (settings_language_screen.cpp).
    void setLanguage(uint8_t index);
    // Setzt NUR den RAM-Zustand, OHNE zu speichern (Alex' Auftrag,
    // 05.10. - "erst committen, dann speichern") - fuer die
    // Ersteinrichtung (first_run_language_screen.cpp), wo noch nichts
    // auf die SD-Karte geschrieben werden darf, die Sprache aber sofort
    // fuer die danach gezeichneten Setup-Screens aktiv sein muss.
    void setLanguagePreview(uint8_t index);
    // Schreibt die aktuelle (ggf. nur per setLanguagePreview() gesetzte)
    // Sprache auf die SD-Karte - der eigentliche "Commit"-Schritt.
    void saveLanguage();

    const char* t(StringId id);
    // Eigennamen der Sprache (z.B. "Deutsch", "English") - NICHT ueber
    // StringId uebersetzt, jede Sprache zeigt ihren eigenen Namen in der
    // eigenen Schreibweise, unabhaengig von der aktuell aktiven Sprache.
    const char* languageName(uint8_t index);
}
