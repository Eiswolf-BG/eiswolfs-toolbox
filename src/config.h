#pragma once
#include <Arduino.h>

// Minimale Konfiguration fuer den aktuellen Funktionsumfang (WLAN-
// Ersteinrichtung, Standort-Setup, ISS-Radar). Pin-Belegung 1:1 aus
// Eiswolfs Flightradar CYD uebernommen (gleiche Hardware, siehe
// CLAUDE.md) - Projekt-spezifische Werte (ADS-B, Watchlists etc.)
// bewusst NICHT mit herübergenommen.
namespace Config {
    // Versionsnummer dieses Releases (Alex' Auftrag, 13.10. - erste
    // oeffentliche Veroeffentlichung). Noch nirgends in der Geraete-UI
    // angezeigt (anders als bei Eiswolfs Flightradar CYD, das einen
    // eigenen OTA-/Changelog-Mechanismus hat) - dient hier zunaechst nur
    // als zentrale Referenz fuer README/Release-Tagging.
    constexpr const char* APP_VERSION = "1.0.0";

    constexpr int16_t SCREEN_WIDTH = 240;
    constexpr int16_t SCREEN_HEIGHT = 320;

    constexpr uint8_t TOUCH_CLK_PIN  = 25;
    constexpr uint8_t TOUCH_CS_PIN   = 33;
    constexpr uint8_t TOUCH_MOSI_PIN = 32;
    constexpr uint8_t TOUCH_MISO_PIN = 39;
    constexpr uint8_t TOUCH_IRQ_PIN  = 36;

    constexpr uint8_t SD_SPI_CS_PIN   = 5;
    constexpr uint8_t SD_SPI_MOSI_PIN = 23;
    constexpr uint8_t SD_SPI_MISO_PIN = 19;
    constexpr uint8_t SD_SPI_CLK_PIN  = 18;

    // Eigener SD-Ordner, getrennt vom Flightradar-Ordner (/Flightradar_cyd) -
    // siehe CLAUDE.md "Ersteinrichtung: Uebernahme von Flightradar-Daten"
    // fuer den Mechanismus, der bei Bedarf EINMALIG von dort in diesen
    // Ordner kopiert. Umbenannt von "/Satellitenradar_cyd" auf
    // "/Toolbox_cyd" im Zuge der Projekt-Umbenennung (Alex' Auftrag,
    // siehe CLAUDE.md) - noch kein echtes Geraet im Feld, daher
    // unkritischer Breaking-Change.
    constexpr const char* SD_ROOT_DIR              = "/Toolbox_cyd";
    constexpr const char* SD_WIFI_CREDENTIALS_FILE = "/Toolbox_cyd/wifi.txt";
    constexpr const char* SD_CALIBRATION_FILE      = "/Toolbox_cyd/calibration.txt";
    constexpr const char* SD_LOCATIONS_FILE        = "/Toolbox_cyd/locations.txt";
    // Manuelle Waehrungs-Ueberschreibung fuer den Krypto-Ticker (Alex'
    // Auftrag, 12.10., siehe currency.h) - leer/fehlend = automatisch
    // (Standort-basiert).
    constexpr const char* SD_CRYPTO_CURRENCY_FILE  = "/Toolbox_cyd/crypto_currency.txt";
    // Eine Zeile mit der numerischen Sprach-Indexnummer (siehe
    // I18n::init()/setLanguage()) - faellt auf Deutsch zurueck, falls die
    // Datei fehlt oder ungueltig ist.
    constexpr const char* SD_LANGUAGE_FILE         = "/Toolbox_cyd/language.txt";
    // Markerdatei, die den Abschluss der Ersteinrichtung anzeigt (siehe
    // main.cpp fuer isFirstRun-Logik) - bewusst eine eigene, leere Datei
    // statt die Existenz von wifi.txt zu pruefen, da Letzteres bei einer
    // reinen Flightradar-Uebernahme ohne eigene Ersteinrichtung ebenfalls
    // schon existieren wuerde, obwohl der Toolbox-Ordner selbst gerade
    // erst neu angelegt wird.
    constexpr const char* SD_SETUP_DONE_FILE       = "/Toolbox_cyd/setup_done.txt";
    // Eigenes, von SD_SETUP_DONE_FILE UNABHAENGIGES Flag (Alex' Auftrag,
    // 05.10.: "ähnliches Muster wie bei der Sprachauswahl") fuer den
    // einmaligen Wisch-Erklaer-Screen (swipe_intro_screen.cpp) - bewusst
    // eine eigene Datei statt denselben Marker wiederzuverwenden, falls
    // der Erklaer-Screen spaeter mal unabhaengig von der Ersteinrichtung
    // erneut anzeigbar sein soll (z.B. ueber die Einstellungen).
    constexpr const char* SD_SWIPE_INTRO_DONE_FILE = "/Toolbox_cyd/swipe_intro_done.txt";
    // Eine Zeile "0"/"1" - 180-Grad-Display-Flip (siehe
    // settings_display_screen.cpp), wirkt sofort und wird sofort
    // gespeichert (Alex' Auftrag, 06.10.: Einstellungen speichern sofort,
    // nicht erst bei einem spaeteren Commit wie in der Ersteinrichtung).
    constexpr const char* SD_DISPLAY_ROTATED_FILE  = "/Toolbox_cyd/display_rotated.txt";
    // Eine Zeile mit der Helligkeit in Prozent (10-100) bzw. dem
    // Bildschirm-Timeout in Minuten (0 = Nie) - siehe
    // settings_display_screen.cpp, gleiches Sofort-Speichern-Prinzip.
    constexpr const char* SD_BRIGHTNESS_FILE       = "/Toolbox_cyd/brightness.txt";
    constexpr const char* SD_SCREEN_TIMEOUT_FILE   = "/Toolbox_cyd/screen_timeout.txt";
    // "lat,lon" - separater, genauerer Standort NUR fuer den OEPNV-Screen
    // (siehe transit_screen.cpp::"Genauere Adresse"-Button) - UEBERSCHREIBT
    // NICHT die allgemeine Toolbox-Position (Config::SD_LOCATIONS_FILE,
    // LocationPresets), bewusst getrennte Datei (Alex' Auftrag, 06.10.).
    // Fehlt die Datei, faellt transit_screen.cpp auf die allgemeine aktive
    // Position zurueck.
    constexpr const char* SD_TRANSIT_LOCATION_FILE = "/Toolbox_cyd/transit_location.txt";
    // Eine Zeile "0"/"1" - 0=Metrisch, 1=Imperial (siehe units.h/.cpp) -
    // faellt auf Metrisch zurueck, falls die Datei fehlt oder ungueltig
    // ist. Pflichtschritt in der Ersteinrichtung (direkt nach der
    // Sprachauswahl) PLUS jederzeit aenderbar unter Einstellungen -> Tools
    // (Alex' Auftrag, 07.10.).
    constexpr const char* SD_UNITS_FILE            = "/Toolbox_cyd/units.txt";

    // LED-Benachrichtigungs-Einstellungen (led_notifications.h, Alex'
    // Auftrag, 12.10.) - Haupt-Toggle + je ein Toggle pro LED-faehigem
    // Screen. Standard bei der Ersteinrichtung: ALLES aus.
    constexpr const char* SD_LED_NOTIFICATIONS_FILE = "/Toolbox_cyd/led_notifications.txt";

    // Woher Flightradar-Daten uebernommen werden koennen (siehe CLAUDE.md) -
    // MUSS exakt zu Config::SD_ROOT_DIR/SD_WIFI_CREDENTIALS_FILE im
    // Flightradar-Projekt passen (dort config.h).
    constexpr const char* FLIGHTRADAR_ROOT_DIR              = "/Flightradar_cyd";
    constexpr const char* FLIGHTRADAR_WIFI_CREDENTIALS_FILE = "/Flightradar_cyd/wifi.txt";
    constexpr const char* FLIGHTRADAR_LOCATIONS_FILE        = "/Flightradar_cyd/locations.txt";
    // Touch-Kalibrierung ist eine Eigenschaft der physischen Hardware, nicht
    // der Firmware - auf demselben Geraet bereits per Flightradar ermittelt,
    // kann sie 1:1 uebernommen werden (Alex' Auftrag, 13.10.: die bisherige
    // Uebernahme kopierte nur WLAN/Standort, NICHT die Kalibrierung, was
    // dazu fuehrte, dass die eigene interaktive Kalibrierung lief, aber durch
    // einen vermutlichen Boot-Zeit-Phantom-Touch (siehe CalibrationScreen::
    // waitForTap()-Kommentar) kaputt/unbrauchbar wurde). Identisches
    // Dateiformat wie Config::SD_CALIBRATION_FILE (xmin,xmax,ymin,ymax),
    // verifiziert gegen touch_input.cpp im Flightradar-Projekt.
    constexpr const char* FLIGHTRADAR_CALIBRATION_FILE      = "/Flightradar_cyd/calibration.txt";

    constexpr uint8_t MAX_WIFI_NETWORKS = 3;

    constexpr const char* NOMINATIM_HOST = "nominatim.openstreetmap.org";
    constexpr const char* NOMINATIM_USER_AGENT = "EiswolfsToolboxCYD (lokales Testprojekt)";

    // Grobe automatische Standortbestimmung ueber die oeffentliche IP
    // (1:1 derselbe Dienst wie bei Eiswolfs Flightradar CYD, dortige
    // location_manager.cpp) - Alex' Auftrag, 06.10.: Alternative zur
    // Adresseingabe in der Ersteinrichtung, fuer Nutzer, die keine exakte
    // Adresse eintragen wollen (Genauigkeit laut ip-api.com-Erfahrungswert
    // ca. 20-30 km, siehe first_run_location_screen.cpp). Bewusst
    // schlichtes HTTP (kein TLS) - ip-api.com bietet HTTPS nur im
    // kostenpflichtigen Plan an, exakt wie im Flightradar-Original.
    constexpr const char* IP_GEO_HOST = "ip-api.com";
    constexpr const char* IP_GEO_PATH = "/json/?fields=status,lat,lon";

    constexpr uint32_t HTTP_TIMEOUT_MS = 6000;

    // Hintergrundbeleuchtung per PWM (Pin TFT_BL=21, siehe platformio.ini) -
    // 1:1 dasselbe ledcSetup()/ledcAttachPin()-Muster wie bei Eiswolfs
    // Flightradar CYD (dortige main.cpp), hier aber ohne dessen Auto-
    // Helligkeit/Nachtmodus/Ruhebildschirm-Komplexitaet - nur ein fester
    // Prozentwert (settings_system_screen.cpp) + ein simpler Bildschirm-
    // Timeout (Backlight komplett aus, kein gedimmter Sternenhimmel wie
    // dort). 5kHz/8-Bit ist Flightradars erprobter Wert fuer dieses Panel.
    constexpr uint8_t BACKLIGHT_PWM_CHANNEL = 0;
    constexpr uint32_t BACKLIGHT_PWM_FREQ = 5000;
    constexpr uint8_t BACKLIGHT_PWM_RESOLUTION_BITS = 8;
    constexpr uint8_t BRIGHTNESS_MIN_PERCENT = 10;
    constexpr uint8_t BRIGHTNESS_MAX_PERCENT = 100;
    constexpr uint8_t BRIGHTNESS_STEP_PERCENT = 10;
    constexpr uint8_t BRIGHTNESS_DEFAULT_PERCENT = 100;

    // Diskrete 3-Kanal-RGB-LED des CYD-Boards (led_control.h/.cpp, Alex'
    // Auftrag, 12.10.) - Pin-Belegung UND aktiv-LOW-Polung live am Geraet
    // per Testaufbau bestaetigt (settings_led_screen.cpp, 12.10.). PWM-
    // Kanaele 1-3 (Kanal 0 ist bereits das Backlight, siehe oben).
    constexpr uint8_t LED_RED_PIN = 4;
    constexpr uint8_t LED_GREEN_PIN = 16;
    constexpr uint8_t LED_BLUE_PIN = 17;
    constexpr uint8_t LED_RED_PWM_CHANNEL = 1;
    constexpr uint8_t LED_GREEN_PWM_CHANNEL = 2;
    constexpr uint8_t LED_BLUE_PWM_CHANNEL = 3;
    constexpr uint32_t LED_PWM_FREQ = 5000;
    constexpr uint8_t LED_PWM_RESOLUTION_BITS = 8;

    // Ersetzt SettingsStore::menuIdleTimeoutMs() aus Flightradar (dort
    // einstellbar) - hier vorerst ein fester Wert, da es noch keinen
    // eigenen Einstellungen-Screen gibt.
    constexpr uint32_t MENU_IDLE_TIMEOUT_MS = 60000;

    // ISS-Live-Radar (siehe iss_client.h/iss_radar_screen.h) - Rate-Limit
    // von wheretheiss.at ist 350 Anfragen/5min (live geprueft, siehe
    // CLAUDE.md), 12s Abstand liegt weit darunter.
    constexpr const char* ISS_API_URL = "https://api.wheretheiss.at/v1/satellites/25544";
    constexpr uint32_t ISS_REFETCH_INTERVAL_MS = 12000;

    // Raketenstart-Countdown (siehe rocket_client.h/rocket_screen.h) -
    // Launch Library 2 API (The Space Devs), anonymes Limit nur 15
    // Anfragen/Stunde (live recherchiert, siehe CLAUDE.md) - 12 Minuten
    // Abstand = 5 Anfragen/Stunde, deutlich darunter. "limit=3" statt "=1"
    // (Alex' Auftrag, 06.10., live bestaetigt) liefert die naechsten 3
    // Starts in DERSELBEN einen Anfrage - kein zusaetzlicher Request noetig.
    constexpr const char* ROCKET_API_URL = "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=3";
    constexpr uint32_t ROCKET_REFETCH_INTERVAL_MS = 720000;
    constexpr uint8_t ROCKET_MAX_LAUNCHES = 3;

    // OEPNV-Abfahrtstafel (siehe transit_client.h/transit_screen.h) -
    // Transitous (api.transitous.org), schluessellos, international
    // nutzbar (live recherchiert und mit echten curl-Aufrufen verifiziert,
    // siehe CLAUDE.md). Fahrplandaten aendern sich nicht staendig, 2,5
    // Minuten Abstand reicht fuer einen "naechste Abfahrten"-Screen voellig.
    constexpr const char* TRANSIT_API_HOST = "api.transitous.org";
    constexpr const char* TRANSIT_REVERSE_GEOCODE_PATH = "/api/v1/reverse-geocode";
    constexpr const char* TRANSIT_STOPTIMES_PATH = "/api/v6/stoptimes";
    // "n" ist bei dieser API ein MINIMUM, kein Maximum (live bestaetigt) -
    // TRANSIT_MAX_DEPARTURES begrenzt zusaetzlich explizit im Code, wie
    // viele der (potenziell mehr) zurueckgelieferten Eintraege tatsaechlich
    // verarbeitet/angezeigt werden - siehe transit_client.cpp.
    constexpr uint8_t TRANSIT_MAX_DEPARTURES = 8;
    constexpr uint32_t TRANSIT_REFETCH_INTERVAL_MS = 150000;

    // Wetter-Screen (siehe weather_client.h/weather_screen.h) - Open-Meteo
    // (api.open-meteo.com), schluessellos, kostenlos fuer nicht-kommerzielle
    // Nutzung, weltweit abdeckend, sehr grosszuegiges Limit (mehrere hundert
    // Anfragen/Minute laut eigener Doku) - live per curl verifiziert, siehe
    // CLAUDE.md. "timezone=auto" liefert alle Zeiten bereits als lokale
    // Wanduhrzeit-Strings (kein "Z"-Suffix) - ANDERS als beim OEPNV-Screen
    // ist hier KEINE eigene Zeitzonen-/Sommerzeit-Berechnung noetig, die
    // Zeit-Strings werden direkt uebernommen.
    constexpr const char* WEATHER_API_HOST = "api.open-meteo.com";
    constexpr const char* WEATHER_FORECAST_PATH = "/v1/forecast";
    // Wetterdaten aendern sich nicht sekuendlich - 20 Minuten Abstand reicht
    // (Alex' Auftrag, 06.10.), API-Limit ist ohnehin weit grosszuegiger.
    constexpr uint32_t WEATHER_REFETCH_INTERVAL_MS = 1200000;
    // Abweichung vom urspruenglichen Auftrag (dort "forecast_days=2"
    // genannt): die Wochenuebersicht (Punkt 3) braucht TAEGLICHE Daten fuer
    // eine ganze Woche, nicht nur 2 Tage - forecast_days=7 liefert diese 7
    // Tage. "forecast_hours" begrenzt davon UNABHAENGIG (live bestaetigt,
    // siehe CLAUDE.md) nur das stuendliche Array auf die kommenden 24h, die
    // fuer die 3-Stunden-Vorhersageleiste völlig ausreichen - haelt die
    // JSON-Antwort klein, ohne die 7 Tagesdaten zu beschneiden.
    constexpr uint8_t WEATHER_FORECAST_DAYS = 7;
    constexpr uint8_t WEATHER_FORECAST_HOURS = 24;
    constexpr uint8_t WEATHER_MAX_DAILY = 7;
    constexpr uint8_t WEATHER_MAX_HOURLY = 24;
    // Jeden 3. Eintrag aus dem stuendlichen Array, ab der aktuellen Stunde -
    // ergibt bei 24 verfuegbaren Stunden bis zu 8 Saeulen in der
    // Vorhersageleiste (Alex' Auftrag: "6-8 Zeitpunkte"), tatsaechlich
    // angezeigt werden WEATHER_STRIP_COLUMNS (siehe weather_screen.cpp).
    constexpr uint8_t WEATHER_STRIP_STEP_HOURS = 3;
    constexpr uint8_t WEATHER_STRIP_COLUMNS = 6;

    // Luftqualitaet (Alex' Auftrag, 06.10., siehe CLAUDE.md "Wetter-
    // Screen: Luftqualitaet") - ANDERE Subdomain als das uebrige Wetter
    // (air-quality-api statt api), live per curl verifiziert. Wird im
    // SELBEN Abruf-Zyklus wie der uebrige Wetter-Fetch geholt (Alex'
    // ausdruecklicher Auftrag: "keine zusaetzliche separate Abfrage-
    // Frequenz") - deshalb KEIN eigenes REFETCH_INTERVAL hier, es gilt
    // WEATHER_REFETCH_INTERVAL_MS oben.
    constexpr const char* WEATHER_AIR_QUALITY_API_HOST = "air-quality-api.open-meteo.com";
    constexpr const char* WEATHER_AIR_QUALITY_PATH = "/v1/air-quality";

    // Formel-1-Screen (siehe f1_client.h/f1_screen.h) - Jolpica-F1
    // (api.jolpi.ca, Ergast-Nachfolger, schluessellos) fuer Kalender/
    // Wertungen/letztes Ergebnis + OpenF1 (api.openf1.org, schluessellos,
    // 3 Anfragen/Sekunde Rate-Limit laut eigener Fehlermeldung, live
    // bestaetigt) fuer echte Live-Daten waehrend einer laufenden Session.
    // Beide live per curl verifiziert, siehe CLAUDE.md "Formel 1".
    constexpr const char* F1_JOLPICA_HOST = "api.jolpi.ca";
    constexpr const char* F1_JOLPICA_NEXT_PATH = "/ergast/f1/current/next.json";
    constexpr const char* F1_JOLPICA_DRIVER_STANDINGS_PATH = "/ergast/f1/current/driverstandings.json";
    constexpr const char* F1_JOLPICA_CONSTRUCTOR_STANDINGS_PATH = "/ergast/f1/current/constructorstandings.json";
    constexpr const char* F1_JOLPICA_LAST_RESULTS_PATH = "/ergast/f1/current/last/results.json";
    // Kalender/Wertungen aendern sich nicht sekuendlich - 15 Minuten
    // Abstand reicht (Alex' Auftrag, 06.10.).
    constexpr uint32_t F1_SEASON_REFETCH_INTERVAL_MS = 900000;

    constexpr const char* F1_OPENF1_HOST = "api.openf1.org";
    constexpr const char* F1_OPENF1_SESSIONS_PATH = "/v1/sessions";
    constexpr const char* F1_OPENF1_DRIVERS_PATH = "/v1/drivers";
    constexpr const char* F1_OPENF1_POSITION_PATH = "/v1/position";
    constexpr const char* F1_OPENF1_INTERVALS_PATH = "/v1/intervals";
    constexpr const char* F1_OPENF1_LAPS_PATH = "/v1/laps";
    // Ob gerade eine Session laeuft, wird regelmaessig geprueft, auch wenn
    // aktuell keine laeuft (billiger Einzelabruf) - alle 60s reicht, um den
    // Start einer neuen Session zuegig zu erkennen.
    constexpr uint32_t F1_SESSION_CHECK_INTERVAL_MS = 60000;
    // WAEHREND einer laufenden Session: Live-Rangliste alle 25-30s
    // aktualisieren (Alex' Auftrag, 06.10. - "das ist ein kostenloser
    // oeffentlicher Dienst").
    constexpr uint32_t F1_LIVE_POLL_INTERVAL_MS = 27000;
    // Zeitfenster-Breite fuer die "date>="-Positions-/Intervall-Abrufe
    // (siehe f1_client.h/.cpp-Kommentar oben) - bewusst GROSSZUEGIG
    // ueberlappend mit dem Poll-Intervall oben (nicht exakt deckungsgleich),
    // damit ein einzelner verzoegerter/verpasster Zyklus keine Luecke
    // reisst. "position" aendert sich live bestaetigt viel seltener als
    // "intervals" (nur bei echten Ueberholmanoevern) - deshalb ein
    // deutlich laengeres Rueckblickfenster dafuer, bleibt trotzdem klein
    // (live gemessen: 180s Positions-Fenster ~ wenige hundert Byte).
    constexpr uint32_t F1_LIVE_POSITION_LOOKBACK_SEC = 180;
    // Bewusst knapper als bei "position" (30-40s statt z.B. 60-90s) - live
    // am echten Geraet gemessen (06.10., siehe CLAUDE.md "Formel 1"): ein
    // Fenster von 180s direkt um das (in den Testdaten) tatsaechliche
    // Sessionende herum lieferte ~111 KB/786 Eintraege (Ziellinien-Phase
    // mit vielen gleichzeitigen Abstands-Neuberechnungen) - dieselbe
    // Groessenordnung wie das bereits dokumentierte xrays-6-hour.json-
    // Problem (Weltraumwetter-Lehre). Ein 30s-Fenster blieb bei
    // vergleichbaren Live-Messungen durchgehend bei ca. 14 KB. Ein
    // einzelner besonders dichter Moment (z.B. Start, Safety-Car-Restart,
    // Zieldurchfahrt) kann das Fenster trotzdem gelegentlich sprengen -
    // das wird vom tolerant behandelten Teilausfall abgefangen (naechster
    // Zyklus 27s spaeter versucht es erneut), kein Absturz, aber bewusst
    // als bekannte Grenze dokumentiert statt stillschweigend als "sicher"
    // angenommen.
    constexpr uint32_t F1_LIVE_INTERVAL_LOOKBACK_SEC = 35;
    // Rundenzeiten (Alex' Auftrag, 06.10.) - "/v1/laps" ist fuer eine
    // komplette Session live bestaetigt 527 KB/1145 Eintraege gross,
    // dieselbe Fehlerklasse wie "position"/"intervals" oben. Ein Fahrer
    // beendet typischerweise nur alle 1-2 Minuten eine Runde - 180s
    // Rueckblickfenster (wie bei "position") live gemessen bei 26 KB/57
    // Eintraegen, deutlich unter der bekannten Problemgrenze.
    constexpr uint32_t F1_LIVE_LAP_LOOKBACK_SEC = 180;

    // Krypto-Ticker (crypto_client.h/crypto_screen.h, Alex' Auftrag,
    // 07.10.) - drei voneinander unabhaengige, schluessellose APIs:
    // CoinGecko (Preise/Top-10-Markt), mempool.space (Bitcoin-Mempool),
    // alternative.me (Fear&Greed-Index).
    constexpr const char* CRYPTO_COINGECKO_HOST = "api.coingecko.com";
    // "vs_currencies="/"vs_currency=" bewusst NICHT mehr Teil dieser
    // statischen Pfade (Alex' Auftrag, 07.10. - standortabhaengige
    // Waehrung statt fest USD, siehe currency.h) - crypto_client.cpp
    // haengt den jeweiligen Waehrungs-Query-Parameter dynamisch an.
    constexpr const char* CRYPTO_COINGECKO_SIMPLE_PRICE_PATH =
        "/api/v3/simple/price?ids=bitcoin,ethereum&include_24hr_change=true";
    constexpr const char* CRYPTO_COINGECKO_MARKETS_PATH =
        "/api/v3/coins/markets?order=market_cap_desc&per_page=10&page=1&sparkline=false";

    constexpr const char* CRYPTO_MEMPOOL_HOST = "mempool.space";
    constexpr const char* CRYPTO_MEMPOOL_FEES_PATH = "/api/v1/fees/recommended";
    // Liefert die Blockhoehe als REINEN Text (z.B. "866543"), KEIN JSON -
    // live per curl bestaetigt (siehe crypto_client.cpp).
    constexpr const char* CRYPTO_MEMPOOL_HEIGHT_PATH = "/api/blocks/tip/height";

    constexpr const char* CRYPTO_FNG_URL = "https://api.alternative.me/fng/?limit=1";

    // Preise/Markt-Liste/Mempool teilen sich EIN Intervall (Alex' Auftrag:
    // "alle 2-3 Minuten"). Der Fear&Greed-Index aktualisiert sich laut API
    // ohnehin nur einmal taeglich - eigenes, deutlich selteneres Intervall
    // (gleiches Prinzip wie die Luftqualitaet beim Wetter-Screen, hier
    // aber mit einem GROESSEREN statt gleichem Intervall, da sich die
    // Daten serverseitig so selten aendern).
    constexpr uint32_t CRYPTO_REFETCH_INTERVAL_MS = 150000;       // 2,5 Minuten
    constexpr uint32_t CRYPTO_FNG_REFETCH_INTERVAL_MS = 14400000; // 4 Stunden

    constexpr uint8_t CRYPTO_MAX_MARKETS = 10;

    // Erdbeben-Screen (earthquake_client.h/earthquake_screen.h, Alex'
    // Auftrag, 07.10.) - USGS fdsnws-Query-API statt der urspruenglich
    // angefragten statischen day/week-Feeds: der day-Feed ist unkritisch
    // (13KB), der week-Fallback aber live bis ~80KB/116 Eintraege
    // gemessen - dieselbe Risikoklasse wie ein bereits dokumentiertes
    // Speicherproblem (siehe CLAUDE.md "Bekannte Probleme"). Die
    // fdsnws-Query-API liefert stattdessen EINE parametrisierte Anfrage
    // (minmagnitude=4.5, orderby=magnitude -Serverseitig bereits nach
    // Magnitude absteigend sortiert, kein eigenes Sortieren noetig -
    // limit=20), live auf einen Zeitraum von ca. 3-4 Wochen zurueck
    // bemessen (kein explizites starttime noetig) und auf ~15KB
    // begrenzt, praktisch nie leer. Mit Alex abgestimmt (AskUserQuestion,
    // 07.10.).
    constexpr const char* EARTHQUAKE_API_URL =
        "https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&minmagnitude=4.5&orderby=magnitude&limit=20";
    constexpr uint32_t EARTHQUAKE_REFETCH_INTERVAL_MS = 720000; // 12 Minuten
    constexpr uint8_t EARTHQUAKE_MAX_QUAKES = 10;

    // Kalender-Screen (holiday_client.h/calendar_screen.h, Alex' Auftrag,
    // 12.10.) - Nager.Date (date.nager.at), kostenlos, kein API-Key,
    // deckt ueber 200 Laender ab. Ein Abruf pro Tag reicht (Feiertage
    // aendern sich nicht laufend) - 24h-Intervall, net_task.cpp erkennt
    // zusaetzlich einen Laendercode- ODER Jahreswechsel (Geraet laeuft
    // ueber Silvester durch) und erzwingt dann sofort einen Neuabruf,
    // gleiches Prinzip wie beim manuellen Waehrungswechsel im Krypto-
    // Ticker (siehe net_task.cpp).
    constexpr const char* HOLIDAY_API_HOST = "date.nager.at";
    constexpr const char* HOLIDAY_API_PATH = "/api/v3/PublicHolidays";
    constexpr uint32_t HOLIDAY_REFETCH_INTERVAL_MS = 86400000; // 24 Stunden
    // Grosszuegig bemessen - Deutschland hat ~19 Eintraege/Jahr (inkl.
    // bundeslandspezifischer Feiertage, live per curl geprueft, 12.10.),
    // andere Laender koennen mehr haben.
    constexpr uint8_t HOLIDAY_MAX_COUNT = 40;

    // Eigene Termine im Kalender-Screen (appointments.h, Alex' Auftrag,
    // 12.10.) - lokal auf der SD-Karte gespeichert (KEIN Netzwerk-Abruf,
    // daher kein NetTask-Eintrag noetig, nur direkter Core-1-SD-Zugriff
    // wie bei units.h/currency.h). 60 Eintraege reichen grosszuegig fuer
    // ein ganzes Jahr eigener Termine.
    constexpr const char* SD_APPOINTMENTS_FILE = "/Toolbox_cyd/appointments.txt";
    constexpr uint8_t APPOINTMENT_MAX_COUNT = 60;
    // 39 Zeichen + Nullterminierung - reicht fuer "Zahnarzt"/"Geburtstag
    // Mama" und deutlich laengere Texte, bleibt aber auf der Bildschirm-
    // breite darstellbar (per Lauftext bei Bedarf sowieso umlaufend).
    constexpr uint8_t APPOINTMENT_TEXT_CAP = 40;

    // Naturereignisse-Screen (natural_events_client.h/natural_events_
    // screen.h, Alex' Auftrag, 12.10.) - NASA EONET v3, kostenlos, kein
    // API-Key. Waldbraende (category "wildfires") werden BEWUSST
    // ausgeschlossen - live per curl bestaetigt: ungefiltert sind >95%
    // aller offenen Ereignisse einzelne US-Waldbraende (laufend Hunderte
    // bis Tausende gleichzeitig), das wuerde den Screen faktisch zu einem
    // reinen US-Waldbrand-Tracker machen statt zu einem international
    // ausgewogenen Ueberblick. Mit den 4 verbleibenden Kategorien (Vulkane/
    // Stuerme inkl. Hurrikane-Taifune-Zyklone/Ueberschwemmungen/Erdrutsche)
    // + status=open live auf ~39 aktive Ereignisse / ~63KB Antwortgroesse
    // gemessen (12.10.) - spuerbar unter der bereits dokumentierten
    // ~95KB-Risikoschwelle (siehe "Bekannte Probleme" oben), aber
    // trotzdem mit demselben engen Feldfilter gegen weiteres Wachstum
    // (z.B. in einer aktiven Hurrikansaison) abgesichert.
    //
    // BUGFIX (12.10., Alex' Meldung "Keine Verbindung"): live auf dem
    // Geraet per Diagnose bestaetigt, dass die Antwort trotz HTTP 200 nach
    // nur ~18KB von 63KB abgebrochen wurde (deserializeJson() ->
    // "IncompleteInput"). Ursache per curl eingegrenzt: EONET liefert
    // OHNE Content-Length per "Transfer-Encoding: chunked", und fast die
    // gesamte Groesse (87% der Geometrie-Punkte bei nur 18% der Ereignisse,
    // live gemessen) stammt von den Bahnpunkt-Historien einzelner Stuerme
    // (z.B. ein Zyklon mit 56 Bahnpunkten) - viele kleine Chunks ueber eine
    // laengere Zeitspanne, offenbar laenger als das bisherige allgemeine
    // 6000ms-Timeout (Config::HTTP_TIMEOUT_MS) zulaesst. "limit" reduziert
    // die Groesse dabei kaum (die groessten Stuerme stehen unabhaengig vom
    // Limit meist vorn in der Ergebnisliste) - deshalb hier NICHT die
    // Kategorie-Auswahl eingeschraenkt (Stuerme bleiben wie gewuenscht
    // dabei), sondern ein eigenes, grosszuegigeres Timeout speziell fuer
    // diesen Client (siehe NATURAL_EVENTS_HTTP_TIMEOUT_MS), PLUS ein
    // moderat reduziertes "limit" als zusaetzliche, aber nicht alleinige
    // Absicherung.
    constexpr const char* NATURAL_EVENTS_API_URL =
        "https://eonet.gsfc.nasa.gov/api/v3/events?status=open&category=volcanoes,severeStorms,floods,landslides&limit=30";
    constexpr uint32_t NATURAL_EVENTS_HTTP_TIMEOUT_MS = 15000;
    constexpr uint32_t NATURAL_EVENTS_REFETCH_INTERVAL_MS = 1500000; // 25 Minuten
    constexpr uint8_t NATURAL_EVENTS_MAX_COUNT = 50;

    // Einheitlicher Kopfbereich fuer JEDEN Screen im Projekt (Alex'
    // Auftrag, 06.10. - siehe CLAUDE.md "Pflicht: einheitliche grosse
    // Ueberschrift auf jedem Screen") - EINE zentrale Hoehe statt pro
    // Screen unterschiedlich gewaehlter CHROME_H-Werte. Muss gross genug
    // sein, um sowohl den "?"-Info-Button (31x31, y=4) als auch den
    // "< Zurueck"-Button (70x28, y=4) UND die auf UiHeader::TEXT_SIZE
    // vergroesserte Ueberschrift vollstaendig aufzunehmen (siehe
    // "Pflicht: Elemente muessen immer in ihren Platz passen").
    constexpr int16_t HEADER_HEIGHT = 44;
}
