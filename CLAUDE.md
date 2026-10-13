# Eiswolfs Toolbox — Projektkontext für Claude Code

Lies diese Datei ZUERST, bevor du irgendwas am Code änderst.

## Was ist das Projekt?

Ein neues, eigenständiges Projekt für dieselbe Hardware wie
[Eiswolfs Flightradar (CYD)](../CYD%20Flightradar) — ESP32 "Cheap Yellow
Display" (CYD, ESP32-2432S028), 240x320 Touch-TFT. Eine Sammlung
mehrerer unabhängiger "Wisch-Bildschirme" mit Live-Daten aus
kostenlosen, anmeldefreien APIs - daher der bewusst breitere Name
"Toolbox" statt eines themenspezifischen Namens (ursprünglich als
"Eiswolfs Satellitenradar" mit reinem Weltraum-Fokus gestartet, am
05.10. von Alex umbenannt, weil weitere, nicht zwingend
weltraumbezogene Themen dazukommen sollen, z.B. Formel 1).

Aktuell umgesetzt bzw. geplant:

- **ISS-Live-Radar** (umgesetzt): Live-Position der Internationalen
  Raumstation, abgefragt über die kostenlose
  [wheretheiss.at](https://wheretheiss.at) API (kein API-Key nötig).
  Polardiagramm mit Mitte=Zenit/Rand=Horizont (siehe
  `iss_radar_screen.cpp`/`iss_client.cpp`), per Wisch-Navigation
  erreichbar (siehe "Navigation: Wisch-Gesten statt Menü" unten).
- **Raketenstart-Countdown** (umgesetzt): zeigt den nächsten
  bevorstehenden Raketenstart (Mission, Trägerrakete, Startplatz, Status)
  mit lokal jede Sekunde neu berechnetem Countdown, abgefragt über die
  kostenlose
  [Launch Library 2 API von The Space Devs](https://thespacedevs.com/llapi)
  (siehe `rocket_client.cpp`/`rocket_screen.cpp`) - nur 15 Anfragen/Stunde
  erlaubt (deutlich enger als wheretheiss.at), daher ArduinoJson-
  Feldfilter UND nur alle 12 Minuten neu abgefragt (`Config::
  ROCKET_REFETCH_INTERVAL_MS`). Der Countdown selbst braucht eine
  korrekte Uhrzeit - `main.cpp::setup()` synchronisiert sie einmalig per
  NTP (`configTime()`, gleiches Muster wie Flightradars `AdsbClient::
  primeTime()`), direkt nach erfolgreichem WLAN-Connect.
- **OEPNV-Abfahrtstafel** (umgesetzt, 06.10.): nächste Abfahrten an der
  nächstgelegenen Haltestelle zum aktuellen Standort, abgefragt über die
  kostenlose, schlüssellose [Transitous-API](https://transitous.org)
  (`api.transitous.org`, siehe `transit_client.cpp`/`transit_screen.cpp`
  sowie Abschnitt "OEPNV-Abfahrtstafel" weiter unten für Details).
- **Wetter** (umgesetzt, 06.10.): aktuelles Wetter + stündliche
  Kurzvorhersage + Sonnenaufgang/-untergang am Standort, mit eigener
  Wochenübersicht-Unterseite, abgefragt über die kostenlose, schlüssellose
  [Open-Meteo-API](https://open-meteo.com) (`api.open-meteo.com`, siehe
  `weather_client.cpp`/`weather_screen.cpp`/`weather_icons.cpp` sowie
  Abschnitt "Wetter-Screen" weiter unten für Details).
- **Formel 1** (umgesetzt, 06.10.): nächstes Rennwochenende mit Countdown
  + Top-3-Fahrerwertung, oder - während eine Session tatsächlich läuft -
  eine Live-Rangliste mit Positionen/Abständen, abgefragt über die
  kostenlosen, schlüssellosen APIs [Jolpica-F1](https://api.jolpi.ca)
  (Ergast-Nachfolger, Kalender/Wertungen/letztes Ergebnis) und
  [OpenF1](https://openf1.org) (echte Live-Daten) (siehe
  `f1_client.cpp`/`f1_screen.cpp` sowie Abschnitt "Formel 1" weiter unten
  für Details). Letzter Feature-Screen im Wisch-Karussell.
- Weitere Themen (z.B. Bahn) als lose Idee angedacht, noch kein konkreter
  Auftrag.

**Keine weiteren weltraumbezogenen Screens mehr (Alex' ausdrücklicher
Wunsch, 06.10.):** Ein "Weltraumwetter"-Screen (Kp-Index, Sonnenwind,
Röntgenfluss-Klasse, wer im Weltall ist, NOAA + open-notify.org) wurde
am 06.10. gebaut, noch am selben Tag aber wieder VOLLSTÄNDIG entfernt
(Code, i18n-Strings, `FEATURES[]`-Eintrag) - Alex' Begründung: zu
statisch/uninteressant für einen Wohnzimmer-Screen. Das ISS-Radar
bleibt bestehen, wird aber ebenfalls nicht weiter ausgebaut. Für
künftige neue Feature-Screens bitte auf andere, nicht weltraumbezogene
Themen setzen (z.B. die oben genannte Bahn-Idee).

## Optik & Bedienkonzept: wie Eiswolfs Flightradar CYD

Alex' ausdrücklicher Wunsch: Dieses Projekt soll optisch wie Eiswolfs
Flightradar CYD aussehen — gleiches dunkles Farbschema, gleiche kleine
"?"-Infoschaltflächen bei Optionen/Toggles, gleiches Schriftbild. Die
konkreten Muster dafür (Font-Verhalten, Pflicht-Textumbruch,
Info-Button-Muster, Mehrsprachensystem — jeweils eigene Abschnitte
weiter unten) sind bewusst aus der CLAUDE.md von Eiswolfs Flightradar
CYD übernommen — bei Unklarheiten lohnt sich ein Blick dorthin, die
Muster sollen sich zwischen beiden Projekten NICHT auseinanderentwickeln.

**Beim Bedienkonzept (Navigation) ist das NICHT mehr der Fall:** Toolbox
hat seit 05.10. (Alex' ausdrücklicher Wunsch) KEIN Menü mehr - anders
als Flightradar wird ausschließlich per Wisch-Geste navigiert, siehe
"Navigation: Wisch-Gesten statt Menü" unten. Dieser eine Abschnitt ist
also bewusst NICHT 1:1 von Flightradar übernommen, alle anderen (Farben,
Font, Sprache, Info-Buttons) weiterhin schon.

## Hardware (identisch zu Eiswolfs Flightradar CYD)

- ESP32-2432S028 ("Cheap Yellow Display", CYD)
- ILI9341-Display, 240x320, ansteuert über TFT_eSPI
- XPT2046 resistiver Touch-Controller
- Kein PSRAM
- Pin-Belegung/SPI-Frequenzen in `platformio.ini` 1:1 aus dem
  Flightradar-Projekt übernommen (dort über lange Testreihen ermittelt
  und bewährt) — bei Unklarheiten dort nachsehen, nicht neu raten.

## Tech-Stack

- PlatformIO, `platform = espressif32`, `board = esp32dev`,
  `framework = arduino` — siehe `platformio.ini`.
- TFT_eSPI (Display), XPT2046_Touchscreen (Touch), ArduinoJson
  (API-Antworten). **Dual-Core-Design wie bei Flightradar** (Alex'
  ausdrückliche Vorgabe von Anfang an, siehe CLAUDE.md-Historie weiter
  unten zu diesem Punkt): `net_task.cpp` läuft als eigener FreeRTOS-Task
  auf Core 0 und übernimmt ALLE Netzwerk-Abrufe (ISS-Position,
  Raketenstart-Daten, OEPNV-Abfahrten) - Core 1 (Rendering + Touch,
  `app_navigation.cpp`) wird dadurch nie durch Netzwerk-Wartezeiten
  blockiert. Datenaustausch Core 0 → Core 1 per Mutex-geschütztem Struct +
  Versionszähler pro Datenquelle (`NetTask::getIssPosition()`/
  `getRocketLaunch()`/`getTransitData()`, `NetTask::issVersion()`/
  `rocketVersion()`/`transitVersion()`) - gleiches Grundmuster wie
  Flightradars `aircraft_table.cpp` (`lock()`/`unlock()`, Versionszähler).
  Die OEPNV-Datenquelle braucht zusätzlich den UMGEKEHRTEN Weg Core 1 →
  Core 0 (`NetTask::setTransitLocation()`) - der Screen (Core 1) teilt
  `net_task.cpp` mit, WO gesucht werden soll, weil `net_task.cpp` selbst
  NIEMALS `LocationPresets`/die SD-Karte liest (siehe nächster Punkt) -
  eigener kleiner Mutex für diese Koordinaten-Übergabe, unabhängig vom
  Datenwort selbst. **Feste Regel, aus einem früheren Flightradar-Vorfall
  gelernt (dortiges WebServer-Task-Experiment, Display-Zugriff vom
  falschen Core führte zu Rendering-Fehlern):** `net_task.cpp` fasst
  NIEMALS das Display an (kein `tft`-Aufruf, keine Touch-Auswertung) UND
  liest NIEMALS selbst `LocationPresets`/die SD-Karte (das bleibt
  Core-1-exklusiv) - gerendert UND jede Standort-Abfrage passiert
  ausschließlich auf Core 1 in den jeweiligen Screens
  (`iss_radar_screen.cpp`, `rocket_screen.cpp`, `transit_screen.cpp`), die
  nur fertige Datenkopien von `net_task.cpp` abholen bzw. ihm Koordinaten
  übergeben. `iss_client.cpp`/`rocket_client.cpp`/`transit_client.cpp`
  selbst bleiben reine, zustandslose "einmal blockierend abrufen"-
  Funktionen - nur noch `net_task.cpp` ruft sie auf, nicht mehr die
  Screens direkt.
- Bewusst KEIN zentrales SettingsStore-Modul wie bei Flightradar -
  Einstellungen (aktuell nur die Sprache) werden direkt in `i18n.cpp`
  verwaltet und als eigene, kleine SD-Datei gespeichert (siehe
  `Config::SD_LANGUAGE_FILE`).

## ⚠️ WICHTIGSTE FALLE: Der eigene Font ist grundlinien-verankert

(Übernommen aus Eiswolfs Flightradar CYD, gilt hier identisch -
`src/ui_font.h`, aktiviert via `tft.setFreeFont(&UiFont11pt)` global in
`main.cpp::setup()`.) Bei `setCursor(x, y); print(...)` ist `y` die
**Grundlinie** (Baseline), NICHT die obere Kante. Der Text wächst von
`y` aus nach OBEN (um den Ascent), nicht nach unten. Zu kleine y-Werte
lassen Text oben aus dem Bildschirm/Container herausragen oder
abschneiden. **Faustregel: y sollte bei Size 1 nie kleiner als ~14 sein,
bei Size 2 nie kleiner als ~24-26** (abhängig vom Container).
`drawString()` mit `setTextDatum(MC_DATUM)` (zentriert, oder jedem
anderen Datum) ist davon NICHT betroffen — TFT_eSPI rechnet die
Positionierung selbst korrekt aus, egal ob Baseline- oder
Top-verankert. Nur rohes `setCursor()`+`print()` ist die Gefahrenzone.

**Eingeschränkter Zeichensatz:** Auf dem Display KEINE Unicode-Pfeile
oder Emoji verwenden — stattdessen ASCII-Ersatzzeichen (z.B. "^" statt
eines Pfeilsymbols für "nach oben/Shift", wie es bei Flightradar für
die Großschreibungs-Umschalttaste gemacht wird).

## Pflicht: layoutWrapped() statt println()/print() für variablen Text

Jeder Text auf dem Display, dessen Länge nicht vorab fest bekannt ist
(also praktisch jeder Text außer ganz kurzen, garantiert passenden
Label-Konstanten), MUSS über einen `layoutWrapped()`-artigen Helfer
gezeichnet werden, der wortweise und anhand der tatsächlichen
Pixel-Breite umbricht — NIEMALS über ein ungeprüftes `println()`/
`print()` an fester Position, und NIEMALS über TFT_eSPI's eingebautes
Auto-Wrap (das bricht mitten im Wort ab). Diese Pflicht gilt auch für
Titel/Überschriften, nicht nur Fließtext. Nach dem Muster aus Eiswolfs
Flightradar CYD ist dieser Helfer bewusst PRO SCREEN dupliziert statt
geteilt (z.B. `first_run_complete_screen.cpp`, `iss_radar_screen.cpp`)
- jeder neue Screen mit variablem Text bekommt seine eigene Kopie.

**Gilt jetzt, mit dem echten 4-Sprachen-System (siehe Abschnitt
"Sprache" unten), genauso streng wie bei Flightradar:** jeder neue oder
geänderte Text MUSS nachweislich in ALLEN 4 Sprachen innerhalb der
verfügbaren Bildschirmbreite/-höhe bleiben, nicht nur in der zuletzt
getesteten (meist Deutsch). Spanisch und Französisch sind oft länger
als Deutsch/Englisch - genau dort nach dem Umbau eines Screens prüfen.

## Pflicht: jedes Layout muss auf den Bildschirm passen (sonst scrollbar)

Nicht nur Text (siehe oben) - JEDES dynamisch berechnete Layout (Tasten-
Raster, Button-Zeilen, Listen) MUSS nachweislich innerhalb der 320px
Bildschirmhöhe bleiben. Wird die Zeilen-/Elementanzahl aus variablen
Daten berechnet (z.B. eine sprachabhängige Anzahl Tastatur-Zeilen),
MUSS die Höhenberechnung (z.B. `KEY_H`) den tatsächlichen SCHLIMMSTEN
Fall über ALLE möglichen Zustände/Seiten hinweg berücksichtigen - nicht
nur den Zustand, der zuletzt geändert wurde. Passt das Layout in einem
Zustand nachweislich nicht mehr auf den Bildschirm, MUSS es scrollbar
gemacht werden (siehe `runConfirmScreen()` in `address_search_screen.cpp`
als Vorbild: `scrollY`/Hoch-Pfeil/Runter-Pfeil), statt einfach über den
unteren Rand hinauszulaufen.

**Konkreter Bug als Warnung (06.10., Alex' Fund per Foto):** Bei der
Umstellung der Adress-Tastatur (`address_search_screen.cpp`) von einer
festen 5-zeiligen Sonderzeichen-Seite auf eine sprachspezifische Seite
mit nur noch 1-3 Zeilen wurde `TOTAL_ROWS` (und damit `KEY_H`) NUR noch
anhand der neuen, kleineren Sonderzeichen-Seite berechnet - die
Buchstaben-Seite (4 feste Zeilen) wurde dabei als schlimmster Fall
übersehen, weil sie vorher nie der schlimmste Fall gewesen war. Ergebnis:
`KEY_H` wurde zu groß berechnet, die unterste Aktionszeile (Suchen/
Abbrechen) lief unten über den Bildschirmrand hinaus. Immer ALLE
Seiten/Zustände eines Screens gegeneinander prüfen, nicht nur den gerade
bearbeiteten.

## Pflicht: Textbreite in ALLEN 4 Sprachen vor JEDEM Flashen prüfen

Vor dem Abschluss JEDES Bau-Schritts, der einen neuen oder geänderten
Text einführt (Button-Beschriftung, Label, Überschrift, Zustandswort
wie "An"/"Aus") - unabhängig davon, ob der Auftrag das explizit
erwähnt: nachrechnen (nicht raten/annehmen), ob dieser Text
tatsächlich in seinen vorgesehenen Bereich passt, UNTER
BERÜCKSICHTIGUNG DES LÄNGSTEN MÖGLICHEN INHALTS:

- Der längste Text/die längste Übersetzung aller 4 Sprachen (DE/EN/
  FR/ES), nicht nur die gerade benutzte oder zuletzt getestete.
- Der längste mögliche Zustand bei wiederverwendeten Wörtern (z.B.
  ist "Aus" kurz, aber Spanisch "Desactivado"/Französisch "Désactivé"
  für dasselbe `SETTINGS_ROTATION_ON/OFF`-Wortpaar deutlich länger -
  wer diese IDs anderswo wiederverwendet, erbt automatisch auch deren
  längste Übersetzung).
- Bei variablen/potenziell beliebig langen Inhalten (Ländernamen,
  Nutzereingaben, API-Texte): der realistisch längste beobachtete oder
  denkbare Fall, nicht der kürzeste Testfall.

Passt der Text nachweislich NICHT hinein, MUSS vor dem Flashen eine
Lösung gewählt werden - kürzerer Text, kleinere Schrift, Umbruch auf
mehrere Zeilen (siehe `settings_units_screen.cpp::drawOption()` als
Vorbild: zwei Zeilen statt eine lange), oder bei variablen Inhalten der
bereits vorhandene Lauftext-Mechanismus (siehe "Pflicht: automatisches
Scrollen bei zu langem Text" unten). **Text darf nie einfach
stillschweigend abgeschnitten werden.**

**Konkreter Bug als Warnung (12.10., Alex' Fund per Foto):** Der neue
Haupt-Toggle-Button im LED-Einstellungen-Screen
(`settings_led_screen.cpp`) kombinierte Label und Zustand in EINER
Zeile ("Alle LED-Benachrichtigungen: Aus") - im Deutschen gerade noch
denkbar, aber auf Französisch/Spanisch sprengten sowohl das deutlich
längere Präfix-Label als auch die längeren Zustandswörter
("Désactivé"/"Desactivado") die Button-Breite bei Weitem, der Text
wurde links UND rechts abgeschnitten. Fix: Label und Zustand auf zwei
Zeilen aufgeteilt (gleiches Prinzip wie `settings_units_screen.cpp`),
PLUS die Präfix-Labels selbst spürbar gekürzt. Diese Prüfung gilt
ab sofort für JEDEN Button/jedes Label auf JEDEM Screen, nicht nur für
neue LED-Einstellungen.

## Farbschema: dunkel, fest Grün/Weiß/Gelb (KEIN Theme-System mehr)

Übernommen aus Flightradar (Alex' ausdrücklicher Wunsch): Schwarzer
Hintergrund (`TFT_BLACK`), Rahmen/Text/Buttons in Neongrün (`TFT_GREEN`)
statt weißem/hellem Hintergrund. Gilt für jeden Screen in diesem Projekt.

Vom 06.10. bis 07.10. gab es kurzzeitig ein wählbares Mehrfach-Theme-
System (3 Farben: Grün/Amber/Blau, eigenes `ui_theme.h/.cpp`-Modul,
"Farben"-Button im Einstellungs-Hub, zusätzlich eine eigens abgedunkelte
Variante für Einstellungs-Screens über `SettingsCommon::accentColor()`).
**Am 07.10. auf Alex' ausdrücklichen Wunsch wieder komplett entfernt** -
die Toolbox hat jetzt nur noch EIN einziges, festes Farbschema: Grün als
Akzentfarbe (`TFT_GREEN`, literal verwendet, kein `UiTheme::accentColor()`
mehr), Weiß für abgefragte/lebendige Datenwerte (siehe "Pflicht:
Label/Wert-Farbkontrast" unten) und Gelb für Countdown-Anzeigen (siehe
"Pflicht: Countdown-Anzeigen in Gelb" unten). `ui_theme.h/.cpp` und
`settings_colors_screen.cpp/.h` existieren nicht mehr. Der
Einstellungs-Hub hatte dadurch kurzzeitig nur noch 5 statt 6 Buttons -
seit demselben Tag (07.10.) sitzt an der freigewordenen Stelle
stattdessen "Einheiten" (aus dem Tools-Untermenü herausgelöst, siehe
"Globale Einheiten-Einstellung" unten), der Hub hat also weiterhin 6
Buttons (WLAN/Einheiten/Sprache/Anzeige/Tools/LED).
`SettingsCommon::accentColor()` gibt seitdem ebenfalls schlicht
`TFT_GREEN` zurück, ohne Abdunklung/Entsättigung - Einstellungs-Screens
sehen optisch identisch zu allen anderen Screens aus, da es nur noch ein
Farbschema in der ganzen App gibt, das visuell vom Rest nicht mehr
abgegrenzt werden muss.

**Falls künftig doch wieder mehrere Farbschemata gewünscht werden:**
nicht versuchen, das alte `UiTheme`-Modul aus der Git-Historie
wiederherzustellen - lieber neu und einfacher aufbauen, diesmal gleich
mit Blick auf Eiswolfs Flightradar CYDs funktionierendem Mehrfach-
Theme-System (`UiTheme::accentColor()`, dort dauerhaft an die LED
gebunden, siehe Flightradars eigene CLAUDE.md) als Vorbild, statt der
hier wieder verworfenen Zwischenlösung.

## Pflicht: Label/Wert-Farbkontrast

**Standardregel ab 07.10. (Alex' Auftrag):** Bei Datenzeilen der Form
"Label: Wert" bleibt das feste Label-Wort in der Akzentfarbe, der sich
ändernde/abgefragte Wert dahinter wird in Weiss dargestellt - "klarer
Kontrast zwischen fester Bezeichnung und Live-Wert... übersichtlicher
und macht das Ganze etwas lebendiger". Gilt ab jetzt für JEDEN
künftigen Screen mit Label/Wert-Zeilen, nicht nur die, bei denen es
ausdrücklich erwähnt wird.

**Ausdrückliche Ausnahme: der Wetter-Screen wird NICHT angefasst**
("mein Lieblingsscreen... den hast du wirklich super hinbekommen") -
bleibt bewusst bei seiner bisherigen Darstellung (Icon+Wert ohne
Text-Label, siehe "Wetter-Screen" weiter unten), diese Regel gilt dort
nicht rückwirkend.

**Bereits umgesetzt (07.10.):** `iss_radar_screen.cpp`
(`layoutWrappedLabelValue()`) und `rocket_screen.cpp` (eigene, lokal
duplizierte Kopie derselben Funktion, Projekt-Konvention) - beide
zeichnen Label und Wert jetzt farblich getrennt, OHNE die bestehende
wortweise Pixelbreiten-Umbruch-Pruefung zu verlieren (die Funktion
verkettet Label+Wert intern zu einem String, umbricht genau wie
`layoutWrapped()` bisher, ermittelt beim Zeichnen jeder resultierenden
Zeile aber, welcher Anteil noch zum Label gehört, und faerbt
entsprechend um - deckt auch den theoretischen Fall ab, dass ein
besonders langer Wert selbst umbricht). **Ausnahme davon:** Zeilen mit
einer eigenen Warnfarben-Bedeutung (Rakete: Hold-/Fehlgrund-Zeile in
Gelb/Rot) bleiben bewusst KOMPLETT in dieser einen Farbe, keine
Label/Wert-Aufteilung - ein weisser Wertanteil würde die geforderte
"deutliche Sichtbarkeit" mindern.

**OEPNV-Screen nachgezogen (07.10., Alex' ausdrücklicher Hinweis):** die
Haltestellen-Kopfzeile ("Haltestelle: Geisl.(St)") stand bis dahin noch
komplett in Akzentgrün statt Label/Wert getrennt -
`transit_screen.cpp::layoutWrappedLabelValue()` (lokal dupliziert,
gleiche Logik wie bei ISS-Radar/Rocket) behebt das.

**Noch NICHT retrofittet (Stand 07.10., nur auf ausdrücklichen Wunsch
angefasst):** der Formel-1-Screen nutzt diese Farbaufteilung noch
nicht - falls gewünscht bei Gelegenheit nachziehen.

**ISS-Radar zusätzlich (07.10.):** die Kompass-Buchstaben N/S/O/W am
Kreisrand wurden komplett entfernt (nur noch Kreis-Umriss + gepunktete
Weltkarte + Mittelpunkt) - die Kompass-Richtung steht weiterhin als
Teil der Azimut-Wertzeile ("45° N" usw.) unterhalb des Kreises, siehe
`compassAbbrevId()`.

## Pflicht: Countdown-Anzeigen in Gelb

**Standardregel ab 07.10. (Alex' Auftrag): "sämtliche Countdowns sind
gelb".** Literal `TFT_YELLOW`, NICHT das feste Akzentgrün -
gleiche Begründung wie bei den übrigen literal/status-gebundenen Farben
(AQI-Kategorien, Kp-Index, Rakete-Hold/Fail): ein Countdown-Timer trägt
eine eigene, universell erkennbare Bedeutung ("es wird knapp/es läuft"),
keine Deko-Farbe. Umgesetzt in `rocket_screen.cpp::drawCountdown()` und
`f1_screen.cpp::drawCountdownBlock()` (Label-Zeile UND die große
Zahl/Uhrzeit selbst, beide gelb). Gilt für jeden künftigen Screen mit
einer Countdown-Anzeige.

## "?"-Infoschaltflächen-Muster für Einstellungen/Toggles

Übernommen aus Flightradar: Jeder neue Ein/Aus-Schalter (Toggle-Button)
bekommt automatisch einen kleinen "?"-Info-Button direkt in derselben
Button-Zeile, der in 1-2 Sätzen erklärt, was der Schalter bewirkt —
unabhängig davon, ob der jeweilige Auftrag das explizit erwähnt.
Umgesetzt z.B. beim ISS-Radar-Screen (`iss_radar_screen.cpp`, öffnet
einen eigenen kleinen Erklär-Screen). Gleiches Vorgehen gilt hier für
jeden künftigen Toggle/Info-Button - neue StringIds für Label +
Infotext IMMER in allen 4 Sprachen ergänzen (siehe "Sprache" unten).

## Pflicht: Mindest-Tapgröße ("Finger-only", kein Stift)

**Toolbox ist, anders als Flightradar (dort Bedienung mit einem kleinen
Plastik-Stift), eine reine Finger-only-App** (Alex' Auftrag, 06.10.).
Nirgends im Projekt darf ein Stift nötig sein, um ein Element sicher zu
treffen. Zwei feste Mindestmaße, gemessen am aktuell bestehenden
System-Hub/den Einstellungs-Untermenüs (Alex: "bereits knapp an der
Grenze dessen, was mit einem Finger noch gut treffbar ist"):

- **Absolute Untergrenze für JEDE Tap-Fläche** (auch kleine Icon-Buttons
  wie "?"): **31×31px.** Das ist die Größe der "?"-Infobuttons, system-
  weit von vorher 26×26 auf 31×31 vergrößert (`+5px`, Alex' Auftrag,
  06.10.) - `iss_radar_screen.cpp`, `rocket_screen.cpp`,
  `transit_screen.cpp`, `settings_display_screen.cpp`,
  `settings_wifi_manage_screen.cpp`. Gilt rückwirkend für JEDEN
  bestehenden UND jeden künftigen Button im Projekt.
- **"Menübutton"** (ein Button, der selbst ein neues Untermenü/einen
  neuen Screen öffnet - z.B. die 6 Buttons im System-Hub wie "Farben"):
  mindestens **68×90px** (die aktuelle Größe der Hub-Buttons,
  `settings_hub_screen.cpp::BTN_W/BTN_H`). Gilt NICHT für kompakte
  Listen-Zeilen (Sprachauswahl, WLAN-Netzwerke, Abfahrten-Zeilen usw.) -
  das ist eine andere Interaktions-Kategorie (Scannen/Auswählen in einer
  Liste statt Navigieren in ein neues Menü) und folgt stattdessen der
  Untergrenze oben (31px).

**Bereits bekannte, noch NICHT behobene Unterschreitungen** (gefunden
bei der Einführung dieser Regel, 06.10. - bewusst nicht selbst
vergrößert, siehe Alex' Auftrag "bitte zurückmelden statt selbst
vergrößern"): `SettingsCommon::backButtonRect()` (70×28, Höhe 28 < 31,
betrifft JEDES Einstellungs-Untermenü), `wifi_setup_screen.cpp::
cancelBtn` (30×24) sowie die dortigen `upBtn`/`downBtn`-Scroll-Pfeile
(je 30×26, zweimal im Code). Vor einer Vergrößerung dieser Elemente
bitte erst Alex' Entscheidung einholen.

## Pflicht: automatisches Scrollen bei zu langem Text (Marquee)

Passt ein EINZEILIGES Label (z.B. ein Listen-Eintrag, ein Ziel-Text) in
seiner zugewiesenen Breite nicht, wird es automatisch horizontal
durchgescrollt (Laufschrift) statt hart am Rand abgeschnitten zu werden
(Alex' Auftrag, 06.10., nach einem konkreten Überlauf-Bug im
OEPNV-Screen). Zentrale, wiederverwendbare Komponente:
`marquee_label.h/.cpp` (`MarqueeLabel::State`/`draw()`/`reset()`) - NICHT
pro Screen neu bauen, diese eine Komponente projektweit nutzen. Technik:
TFT_eSPI-Viewport-Clipping (`setViewport()`/`resetViewport()`), Text wird
zweimal versetzt gezeichnet für einen nahtlosen Loop. Muss in JEDEM
Schleifendurchlauf (wie `MenuStars::update()`) erneut aufgerufen werden,
nicht nur einmalig, sonst steht die Animation still - siehe
`transit_screen.cpp::update()` als Vorbild (nur die betroffene Spalte
wird pro Durchlauf neu gezeichnet, nicht die ganze Zeile).

**Gilt ausdrücklich NUR für kurze, einzeilige Labels - NICHT für
längere Fließtexte/Erklärungen.** Die bleiben weiterhin ganz normal mit
`layoutWrapped()` mehrzeilig umgebrochen (siehe "Pflicht: layoutWrapped()
statt println()/print()" oben) - beide Regeln ergänzen sich, ersetzen
sich aber nicht gegenseitig: `layoutWrapped()` für Absätze,
`MarqueeLabel` für einzelne überlaufende Werte/Labels.

**Marquee-Flimmer-Fix (07.10., Alex' Fund an der neuen
Luftqualitäts-Kachel im Wetter-Screen - "die Schrift flackert"):**
`MarqueeLabel::draw()` löschte/zeichnete seinen Bereich früher bei
JEDEM Aufruf neu, auch wenn sich seit dem letzten Aufruf gar nichts
geändert hatte (Text passt bereits und braucht kein Scrollen, bzw.
während einer Scroll-Pause) - bei ca. 50 Aufrufen/Sekunde (Haupt-Loop-
Intervall) ein deutlich sichtbares Flackern. Behoben zentral in
`marquee_label.cpp`, nicht nur für den einen Fundort: `State` merkt sich
jetzt den zuletzt TATSÄCHLICH gezeichneten Text
(`lastDrawnText`/`everDrawn`) - neu gezeichnet wird nur noch, wenn sich
der Text geändert hat ODER (im Scroll-Fall) der Lauftext-Offset in
diesem Tick tatsächlich einen Schritt weitergerückt ist. Da diese Fehlerklasse
in der zentralen, projektweit geteilten Komponente lag, profitieren
automatisch ALLE Verwendungsstellen von diesem Fix (OEPNV-Abfahrten,
Formel-1-Rennname/-Sessionname/-Top-3, Luftqualitäts-Kategoriewort) -
nicht nur die eine, die den Bug sichtbar gemacht hat.

**Fix (07.10., Alex' Fund im OEPNV-Screen - "weißer Block" mitten im
Lauftext, z.B. "Steige[Block]Geisl." statt "...(Steige) Geisl...."):**
Im scrollenden Zweig von `MarqueeLabel::draw()` wurde der per
`setViewport()` geclippte Bereich NIE selbst geleert - die Funktion
verließ sich komplett darauf, dass die zwei `drawString()`-Aufrufe
(Original + eine um `textW+GAP_PX` versetzte Kopie, für den nahtlosen
Loop) alte Pixel automatisch überschreiben. Das stimmt für die
TEXT-Bereiche selbst, aber NICHT für den `GAP_PX`-Zwischenraum (24px)
zwischen den beiden Kopien: dort zeichnet bewusst KEINER der beiden
Aufrufe jemals etwas hin (das ist der Sinn des Abstands) - Reste einer
Glyphe, die beim Weiterscrollen gerade erst in diese Lücke
"hineingewandert" war, blieben deshalb als stehender heller Block
liegen, bis zufällig die nächste Kopie denselben Bereich überstreicht.
**Ein erster Fix-Versuch** (Viewport nur bei `textChanged` leeren, nach
der ersten - falschen - Theorie "Text ändert sich während des
Scrollens") behob das Problem NICHT, weil der Block auch mitten in
einem stabilen Scroll-Durchlauf ohne jeden Textwechsel auftrat - erst
eine zweite Live-Diagnose (temporäre Serial-Ausgabe des tatsächlichen
`text`-Werts beim Leeren) zeigte, dass es sich um denselben,
unveränderten Text handelte, was auf die Lücke zwischen den Kopien als
eigentliche Ursache führte. **Endgültiger Fix:** der Viewport wird jetzt
UNBEDINGT vor den beiden `drawString()`-Aufrufen geleert, nicht nur bei
`textChanged` - ohne neues Flackern, da dieser Codepfad ohnehin nur
erreicht wird, wenn bereits neu gezeichnet werden MUSS (`textChanged`
ODER `stepped`, siehe die frühe Rückkehr direkt darüber) - das
zusätzliche `fillRect()` ändert nichts an der Update-Frequenz, nur am
Ergebnis. Die eigentliche Flimmer-Vermeidung bleibt weiterhin diese
frühe Rückkehr bei reinen Pause-Frames, nicht das Leeren selbst. Live am
Gerät durch Alex bestätigt (OEPNV-Abfahrtenliste, nach vorherigem
Fehlschlag des ersten Fix-Versuchs) - da der Fehler in der zentralen
Komponente lag, profitieren automatisch auch Formel-1 (Fahrernamen/
Session-/Rennname) und die Luftqualitäts-Kategorie im Wetter-Screen.
Der Krypto-Ticker ("Weitere Coins") nutzt dieselbe Komponente ebenfalls,
war aber praktisch nie betroffen, da dieser Screen bei jedem Öffnen den
gesamten Bildschirm neu zeichnet (`fillScreen()`) und innerhalb einer
offenen Liste keinen Re-Fetch auslöst, der den Text ändern könnte.

## Pflicht: vollständige API-Daten nutzen (Feature-Screen-Design)

**Standardregel für JEDEN künftigen Feature-Screen** (ÖPNV, Formel 1,
Erdbeben-Tracker usw., Alex' Auftrag, 06.10.): Bevor ein neuer
Feature-Screen gebaut wird, prüft Karl zuerst eigenständig (z.B. per
`curl` gegen die echte API, siehe "Live-API-Verifikation" im
OEPNV-Abschnitt unten als Vorbild), welche Daten die jeweilige API
TATSÄCHLICH liefert - nicht nur die paar Felder, die für die
ursprüngliche Minimal-Idee nötig wären, sondern den gesamten
Datenumfang der Antwort. Darauf aufbauend entwirft Karl selbstständig
eine logische Screen-Struktur (Hauptscreen plus bei Bedarf beliebig
viele Unter-/Detailscreens und eigene Buttons dafür), damit der Nutzer
auf ALLE sinnvollen Daten der API zugreifen kann, nicht nur auf einen
kleinen Ausschnitt. Das soll von Anfang an mitgedacht werden, statt
erst im Nachhinein durch eine Korrektur nötig zu sein - genau wie beim
OEPNV-Screen nachträglich der Detail-Screen pro Abfahrt (Start/Ziel der
Fahrt, Verkehrsunternehmen, Gleis/Steig, Echtzeit-Status) ergänzt
werden musste, obwohl die API diese Felder von Anfang an mitgeliefert
hat. Gilt unabhängig davon, ob der jeweilige Auftrag das explizit
erwähnt.

## Pflicht: Elemente müssen immer in ihren Platz passen

Jedes UI-Element (Button, Box, Icon, Rahmen usw.) muss nach dem
Zeichnen vollständig innerhalb seines vorgesehenen Bereichs UND
innerhalb des sichtbaren Bildschirms liegen - nichts darf abgeschnitten
werden. Wird ein Element vergrößert (z.B. wegen der Mindest-Tapgröße
für Finger-Bedienung, siehe oben), MUSS der Code das umgebende Layout
mit anpassen (z.B. Kopfzeile höher machen, Abstand zu Nachbarelementen
vergrößern, Position verschieben), statt das Element einfach über
seinen Platz hinausragen zu lassen. Diese Prüfung gilt ab sofort als
Standardvorgehen bei JEDER Größen-/Positionsänderung an einem
UI-Element, auf allen Screens - unabhängig davon, ob der jeweilige
Auftrag das explizit erwähnt.

**Konkreter Bug als Warnung (06.10., Alex' Fund per Foto):** Beim
systemweiten Vergrößern der "?"-Infobuttons von 26x26 auf 31x31 (siehe
"Pflicht: Mindest-Tapgröße" oben) reichte die neue Unterkante des
Buttons (y=4+31=35) auf den drei Feature-Screens (`iss_radar_screen.cpp`,
`rocket_screen.cpp`, `transit_screen.cpp`) erstmals ÜBER die feste
`CHROME_H`-Konstante (34) hinaus, die den Beginn des per
`clearContentArea()` wiederholt geleerten Inhaltsbereichs markiert -
jeder nachfolgende Inhalts-Redraw (neue Daten, Fehlermeldung, Lauftext-
Animation) hat dadurch die unterste Pixelzeile des Buttons wieder
schwarz überschrieben. Fix: `CHROME_H` auf allen drei Screens von 34 auf
39 erhöht (plus `CONTENT_TOP` bei Rocket/Transit im gleichen Abstand von
40 auf 45 mitverschoben), damit der komplette Button sicher oberhalb der
Inhaltsbereich-Grenze liegt. Bei der Gelegenheit alle fünf Screens mit
systemweit vergrößerten "?"-Buttons gegengecheckt
(`settings_display_screen.cpp`/`settings_wifi_manage_screen.cpp` zeichnen
dagegen bei jedem Tap komplett per `fillScreen()` neu, ohne einen
späteren Teil-Clear, der den Button-Bereich überlappt - dort bestand das
Problem nicht).

## Pflicht: Bildschirm vollständig ausnutzen, keine ungenutzten Leerräume

Die App ist eine reine Wisch-/Tipp-Anwendung ohne Stift auf einem
kleinen 240x320-Display - jeder Pixel ist knapp. Bei JEDEM Screen
(bestehend wie künftig) gilt deshalb: verfügbare Höhe/Breite zuerst
ermitteln und Elemente (Überschriften, Raster, Listen, Kacheln) darin
so GROSS wie möglich darstellen, statt Platz ungenutzt/leer zu lassen.
Feste, großzügig geratene Abstände oder dauerhaft reservierte, meist
leere Bereiche sind zu vermeiden - lieber nachrechnen, wie viel Platz
nach Abzug aller Pflichtelemente (Header, Mindest-Tapflächen, siehe
"Pflicht: Mindest-Tapgröße" oben) tatsächlich übrig ist, und genau
diesen Rest sinnvoll verteilen.

Das schließt **dynamisch wachsende/schrumpfende Bereiche** ein: ein
Detailbereich, der nur bei Bedarf (z.B. nach Antippen eines Elements)
Inhalt zeigt, soll im Leerzustand nur so viel Platz beanspruchen wie
für einen schmalen Hinweistext nötig ist - der dadurch gewonnene
Platz kommt stattdessen den primären, immer sichtbaren Elementen
(z.B. einem Raster oder einer Liste) zugute, nicht ungenutzter
Leerfläche. Erst wenn tatsächlich Inhalt angezeigt wird, wächst der
Bereich nach unten (siehe `calendar_screen.cpp::row1Height()` als
Vorbild, Alex' Auftrag, 12.10.).

**Konkretes Beispiel (12.10., Alex' Fund per Foto):** Der
Kalender-Screen zeichnete Monatsüberschrift und Wochentagsraster
unnötig klein/zusammengequetscht, während der Detailbereich
unterhalb des Rasters dauerhaft einen großen festen Leerraum
reservierte, selbst wenn kein Tag ausgewählt war. Fix: Kopfbereich
(Überschrift/Wochentagszeile) bekommt nur noch den nachgerechneten
Mindestabstand, das Raster nutzt die dadurch gewonnene Höhe (bei
weiterhin eingehaltener Mindest-Zellgröße 31x31px), und der
Detailbereich ist im Leerzustand nur eine schmale Hinweiszeile statt
eines festen großen Blocks.

**Erweiterte, ausdrückliche Regel (13.10., Alex' Auftrag): die
Hauptseite jedes Screens muss mit so vielen Informationen wie
möglich bestückt sein, von oben bis unten voll, mit einem "Mehr"-
Button ganz unten erst dann, wenn tatsächlich noch weitere Einträge
existieren, die nicht mehr draufpassen.** Kein Screen darf nur zu
einem Drittel gefüllt sein, während "Mehr" dann noch viele weitere
Einträge zeigt, die eigentlich auf der Hauptseite Platz gefunden
hätten. Hintergrund (Alex' Begründung): der Zweck der App ist, dass
man sich einen Screen gezielt einstellt (z.B. Formel 1 während eines
laufenden Rennens) und dort auf einen Blick möglichst viele relevante
Zusatzinfos sieht (Streckenort, Rundenzeiten, Wertung, ...) - ein
Screen, der nur einen einzelnen (ggf. sogar schon abgelaufenen/
veralteten) Eintrag zeigt und den Rest der Fläche verschenkt, verfehlt
diesen Zweck komplett. Konkret umgesetzt am Naturereignisse-Screen
(`natural_events_screen.cpp`, 13.10.): die Hauptseite zeigte vorher nur
eine Kurz-Zusammenfassung (Gesamtzahl + Kategorie-Aufschlüsselung,
weit über 1/3 ungenutzte Fläche darunter) - jetzt zeigt sie direkt so
viele einzelne Ereignis-Zeilen (Kategorie+Datum+Lauftext-Titel), wie
rechnerisch Platz haben (`VISIBLE_ROWS`, aus den uebrigen Layout-
Konstanten hergeleitet statt Magic Number), und "Weitere Ereignisse"
erscheint erst darunter und auch nur, wenn `eventCount > VISIBLE_ROWS`
tatsächlich zutrifft. Diese Regel gilt für JEDEN Screen (bestehend wie
künftig), nicht nur für Naturereignisse - bei jedem neuen/geänderten
Screen mit einer Liste/mehreren Elementen prüfen, ob die Hauptseite
wirklich maximal viele Elemente direkt zeigt, bevor ein "Mehr"-Button
überhaupt in Betracht kommt.

**Ergänzung für Screens mit einem "Hero"-Element (13.10., Alex' Fund
beim Erdbeben-Screen):** Hat ein Screen ein einzelnes, klar
wichtigstes Element (z.B. das staerkste/juengste Beben beim Erdbeben-
Screen), darf dieses weiterhin GROESSER und optisch hervorgehoben
dargestellt werden (z.B. Magnitude in Groesse 2, farblich nach
Schweregrad) - das widerspricht der Regel oben NICHT. Direkt
UNTERHALB dieses Hero-Elements muessen aber trotzdem so viele
WEITERE Eintraege wie moeglich in kompakter Form angezeigt werden,
bis der verbleibende Platz (nach Hero-Hoehe und reserviertem "Mehr"-
Button-Bereich) ausgeschoepft ist - nicht einfach nur das eine
Hero-Element mit Leerraum darunter. Nur was WIRKLICH nicht mehr
passt, gehoert unter den "Mehr"-Button. Konkret umgesetzt am
Erdbeben-Screen (`earthquake_screen.cpp`, 13.10.): das staerkste Beben
bleibt gross/hervorgehoben oben (Magnitude-Farbskala, Ort, Zeit,
Tsunami-Warnung falls vorhanden), direkt darunter folgt eine
kompakte Liste weiterer Beben (Magnitude+Zeit/Ort, gleiches
Zeilenmuster wie die "Weitere Beben"-Liste), dynamisch so viele
Zeilen wie nach der (variablen) Hero-Hoehe noch Platz haben - "Weitere
Beben" erscheint erst darunter und nur fuer den tatsaechlichen Rest.
Gilt genauso fuer jeden anderen Screen mit einem Hero-Element.

## Pflicht: einheitliche grosse Ueberschrift auf jedem Screen

**Jeder Screen im Projekt (Feature-Screens UND Einstellungsscreens)
bekommt oben einen festen Bereich für eine große Überschrift** (Alex'
Auftrag, 06.10.), damit auf einen Blick klar ist, was der Screen zeigt.
Zwei zentrale, projektweit EINHEITLICHE Konstanten dafür (statt pro
Screen unterschiedlich gewählter Werte):

- `Config::HEADER_HEIGHT` (config.h, aktuell 44px) - der feste
  Kopfbereich (das CHROME_H-Äquivalent) JEDES Screens. Muss groß genug
  sein, um sowohl den "?"-Info-Button (31×31, y=4) als auch den
  "< Zurück"-Button (70×28, y=4) UND die vergrößerte Überschrift
  vollständig aufzunehmen - genau die Lektion aus dem oben
  dokumentierten abgeschnittenen "?"-Button-Bug, hier als feste Regel
  verankert: **bei jeder Änderung an `HEADER_HEIGHT` oder an einem der
  beiden Buttons erneut auf JEDEM Screen prüfen**, nicht nur an einer
  Stelle.
- `UiHeader::TEXT_SIZE` (`ui_header.h/.cpp`, aktuell 2) - die
  Schriftgröße der Überschrift selbst.

**`UiHeader::drawCentered()`/`drawLeft()`** sind die EINE zentrale
Zeichenfunktion dafür (bewusste Ausnahme von der sonst geltenden
"jeder Screen hält seine eigenen kleinen Helfer"-Konvention, siehe
"Pflicht: layoutWrapped()..." oben - nur hier zentralisiert, weil Alex
ausdrücklich EINE gemeinsame Konstante/Mechanik wollte). Beide fallen
automatisch auf Textgröße 1 zurück, wenn der Titel bei `TEXT_SIZE`
nicht in die vom Aufrufer übergebene `maxWidth` passt (z.B. "Rocket
Launch" im Englischen, zentriert neben dem "?"-Button zu breit) - der
Aufrufer berechnet `maxWidth` selbst aus seinem eigenen Button-Layout
(unterschiedlich je Screen: Feature-Screens haben nur einen "?"-Button
rechts, Einstellungs-Untermenüs zusätzlich einen "< Zurück"-Button
links und bei manchen noch einen "?"-Button rechts im Kopfbereich).
Die 4 Feature-Screens (ISS/Rakete/ÖPNV/Wetter) nutzen weiterhin
zentrierte Titel (`drawCentered()`, wie bisher), die 6
Einstellungs-Untermenüs + der Hub weiterhin linksbündige Titel
(`drawLeft()`, wie bisher) - bewusst NICHT das bisherige
Ausrichtungs-Styling der beiden Screen-Familien verändert, nur Größe
und Sicherheits-Mechanik vereinheitlicht (least-invasive Änderung).

**Reichweite dieser Regel (Alex' Auftrag umfasst "jeder Screen"):**
umgesetzt auf allen 11 primär über Wisch/Tap erreichbaren Screens
(4 Feature-Screens + Settings-Hub + 6 Untermenüs). Tief verschachtelte
Unterbildschirme (z.B. der Helligkeits-Regler innerhalb "Anzeige", die
Adresseingabe-Tastatur, Detail-Screens) behalten ihr bisheriges,
kleineres Titel-Styling - das war nicht Teil dieses Auftrags und hätte
den Umbau erheblich vergrößert; falls Alex das auch dort will, bitte
explizit sagen.

**Nachgebessert (06.10., Alex' Fund: "Raketenstart"-Überschrift blieb
klein):** `UiFont11pt` ist ein MONOSPACE-Font (DejaVu Sans Mono, siehe
`ui_font.h` - jedes Zeichen 7px breit bei Größe 1, also 14px bei
`UiHeader::TEXT_SIZE`=2). Bei den 4 Feature-Screens reichte eine echte
Zentrierung auf die Bildschirmmitte (x=120) wegen des "?"-Buttons
rechts nur für ~160px Breite - "Raketenstart" (12 Zeichen) braucht aber
168px und fiel knapp auf Größe 1 zurück. **Fix:** die 4 Feature-Screens
zentrieren den Titel jetzt auf die Mitte der vom "?"-Button FREIEN Zone
(`HEADER_TITLE_CX`/`HEADER_TITLE_MAX_W`, aus der Button-Position
hergeleitet, nicht mehr aus der wahren Bildschirmmitte) - das ergibt
197px Spielraum, in dem auch "Rocket Launch" (182px, Englisch) passt.
**Bekannte, strukturell NICHT behebbare Restfälle** (bewusst nicht
weiter verfolgt, da sie den zusätzlichen 70px breiten "<
Zurück"-Button bräuchten, der laut "Pflicht: Mindest-Tapgröße" oben
erst auf Alex' Entscheidung hin verändert werden soll): Bei den
Einstellungs-Untermenüs ist der Titel linksbündig und startet erst
NACH dem "< Zurück"-Button (x=82) - das reicht selbst im besten Fall
(kein weiterer Button) nur für ca. 148px (~10 Zeichen) bei Größe 2.
Betrifft `settings_tools_screen.cpp` (Spanisch "Herramientas", 12
Zeichen = 168px) und `settings_wifi_manage_screen.cpp` (ALLE 4
Sprachen, z.B. Deutsch "WLAN-Netzwerke", 14 Zeichen = 196px) - diese
Titel bleiben bei Größe 1 stehen, das ist die korrekt arbeitende
Rückfallsicherung, kein Bug. Falls Alex das auch dort groß haben will,
bräuchte es entweder einen kürzeren Titeltext, einen schmaleren
Zurück-Button, oder einen zweizeiligen Titel (noch nicht umgesetzt).

**Klarstellung (12.10., Alex' ausdrückliche Regel nach einem Fehlgriff
beim Kalender-Screen):** Diese einheitliche Größe (`UiHeader::TEXT_SIZE`)
gilt AUSSCHLIESSLICH für die eine Screen-Überschrift selbst (z.B.
"Kalender", "Wetter", "Krypto-Ticker"), die über `UiHeader::
drawCentered()`/`drawLeft()` gezeichnet wird - NICHT für sonstige, dem
Screen-spezifischen Inhalt zugehörige Unter-Beschriftungen (z.B. die
"Monat Jahr"-Zeile im Kalender-Raster, Kachel-Titel, Tabellen-
Kopfzeilen). Der Fehler an diesem Tag: die Monat/Jahr-Zeile wurde auf
Größe 2 vergrößert in der (falschen) Annahme, das sei im Sinne der
"einheitlichen Überschriftengröße" - gemeint war aber immer nur der
EINE Screen-Titel oben im festen `Config::HEADER_HEIGHT`-Kopfbereich.
Jeder künftige Screen MUSS für seinen eigenen Titel exakt
`UiHeader::TEXT_SIZE` verwenden (über `UiHeader::drawCentered()`/
`drawLeft()`, nicht eigenhändig `setTextSize()` im Kopfbereich setzen),
unabhängig davon, wie der jeweilige Screen sonst aufgebaut ist -
Unter-Beschriftungen innerhalb des Screens bleiben davon unberührt und
dürfen nach Bedarf anders dimensioniert sein.

## ISS-Radar: gepunktete Weltkarte im Kreis

Überarbeitung des Radarkreises (Alex' Auftrag, 06.10.), siehe
`iss_radar_screen.cpp`:

- **Kompass-Buchstaben N/S/O/W** stehen jetzt INNEN am Kreisrand
  (`INSET`-Abstand von 15px zum Rand) statt außerhalb - macht Platz für
  die größere Überschrift.
- **Kreis etwas kleiner**: `RADAR_RADIUS` von 75 auf 65 reduziert,
  Mittelpunkt/Textbeginn komplett aus `Config::HEADER_HEIGHT` +
  `RADAR_RADIUS` hergeleitet (keine festen Magic Numbers mehr).
- **Status-Text ("ISS aktuell unter dem Horizont") komplett aus dem
  Kreis entfernt** - steht jetzt als eigene erste Zeile "Status: ..."
  in der Datenliste unterhalb des Kreises (`ISS_RADAR_STATUS_LABEL`/
  `_VISIBLE`/`_BELOW_HORIZON`), zusätzlich zu
  Entfernung/Höhe/Geschwindigkeit/Azimut/Elevation/Sichtbarkeit (7
  Zeilen insgesamt).
- **Gepunktete Weltkarte als Kreis-Hintergrund** (`drawWorldMap()`,
  `world_map.h/.cpp`) - Kontinent-Bitmap-Daten (64×32, 256 Byte,
  Natural-Earth 1:110m) 1:1 von Eiswolfs Flightradar CYD übernommen
  (dortige `world_map.h/.cpp`, Alex' ausdrücklicher Verweis). **Die
  PROJEKTION selbst ist aber bewusst ANDERS als dort:** Flightradars
  `drawWorldMap()` quetscht die Rohdaten nur FLACH (Equirektangular-
  Raster 1:1 auf die Kreis-Bounding-Box skaliert, unabhängig vom
  Beobachterstandort) in den Kreis - das hätte NICHT zur hier bereits
  bestehenden Azimut/Elevation-Darstellung gepasst (Mitte=Zenit,
  Rand=Horizont, wie von Alex ausdrücklich gefordert: "bitte keine
  simple flache Weltkarte hineinquetschen"). Diese Version berechnet
  deshalb für jeden Landpunkt eine ECHTE Azimutalprojektion UM DEN
  BEOBACHTERSTANDORT (Großkreis-Distanz `c` und Peilung `theta` per
  Standard-Kugeltrigonometrie-Formeln), genau wie der ISS-Punkt selbst
  auf `(px,py)` abgebildet - nur dass der Radius hier aus
  `c/π * RADAR_RADIUS` kommt (c=0 am Beobachter, c=π am
  Antipodenpunkt = Kreisrand) statt aus der Elevation. Rotiert/
  rezentriert sich dadurch automatisch, wenn sich die allgemeine
  Toolbox-Position ändert (siehe "Eine allgemeine Toolbox-Position"
  oben). **Wichtige Einschränkung, bewusst in Kauf genommen:** die
  Zuordnung "Kreisrand = Antipode" ist eine dekorative/geografische
  Orientierungshilfe, KEIN physikalisch exaktes zweites
  Koordinatensystem - die tatsächliche Horizont-Entfernung eines
  Himmelsobjekts hängt von dessen Höhe ab (bei der ISS ca. 2300km bei
  410km Bahnhöhe), nicht von einer festen Erdoberflächen-Distanz wie
  dem halben Erdumfang. Farbe: festes gedämpftes Grün
  (`tft.color565(0, 102, 0)`, 40% Helligkeit von `TFT_GREEN` - seit
  Entfernung des Mehrfach-Theme-Systems am 07.10. kein
  `UiTheme::accentColorDimmed()` mehr, gleicher Dämpfungsfaktor wie
  vorher) - liest sich als dezenter Hintergrund, konkurriert nicht mit
  dem ISS-Punkt.
- **"ISS"-Beschriftung** neben dem Punkt, wenn sichtbar (Elevation >
  0°) - bewusst NICHT übersetzt (wie schon der Bildschirmtitel, der
  auch auf Spanisch/Französisch "Radar ISS" sagt statt z.B. "EEI"),
  international gebräuchliche Abkürzung.

### ISS-Radar: Flimmer-Fix (07.10.)

Alex' Fund: der komplette Bildschirm blitzte alle ~11-12 Sekunden kurz
auf. Ursache (per Codelektüre diagnostiziert, NICHT direkt gefixt, erst
nach Alex' ausdrücklicher Bestätigung des Befunds umgesetzt): bei jeder
neuen ISS-Position (`Config::ISS_REFETCH_INTERVAL_MS=12000`) wurde über
`clearContentArea()` der GESAMTE Inhaltsbereich (Kreis, Weltkarte,
Kompass-Buchstaben, ALLE 7 Datenzeilen) schwarz geleert und komplett neu
gezeichnet - inklusive der gepunkteten Weltkarte, die pro gesetztem
Rasterpunkt eine eigene Trigonometrie-Berechnung UND einen
`drawPixel()`-Aufruf macht. Wetter-/OEPNV-Screen machen strukturell
GENAU DASSELBE bei neuen Daten, fallen aber wegen ihrer viel selteneren
Abruf-Intervalle (20 Minuten/2,5 Minuten) nie als "Flackern" auf - bei
12 Sekunden Abstand schon.

**Fix - Kreis/Weltkarte/Kompass sind komplett statisch** (haengen nur
vom Beobachterstandort ab, NICHT von der ISS-Position) und werden jetzt
nur noch einmal gezeichnet:
- `drawStaticFrame()`/`ensureStaticFrame()` - zeichnen den Rahmen nur,
  wenn er noch nie stand ODER sich `frameLat`/`frameLon` (der Standort,
  fuer den zuletzt gezeichnet wurde) vom aktuellen Standort unterscheidet.
  JEDE Stelle, die den Bildschirm (teilweise) loescht, ohne den Rahmen
  selbst gleich wieder zu zeichnen (`drawChrome()`, `clearContentArea()`
  bei `drawCenteredHint()`), setzt `haveStaticFrame=false` zurueck - sonst
  wuerde `ensureStaticFrame()` faelschlich annehmen, der Rahmen stehe
  noch, obwohl der Bildschirm gerade erst geleert wurde.
- **Standortwechsel-Edge-Case** (Alex' ausdruecklicher Hinweis, "nicht
  nur den Normalfall bedenken"): ein reguaerer Wechsel (Einstellungen ->
  Tools -> "Adresse ändern") erfordert ohnehin ein Verlassen des Screens
  (Wisch zu den Einstellungen) - `enter()` liest `obsLat`/`obsLon` beim
  Rueckkehr-Wisch bereits frisch, `ensureStaticFrame()` erkennt die
  Aenderung dadurch automatisch. ZUSAETZLICH prueft `update()` bei JEDEM
  Durchlauf gegen `LocationPresets` (reiner RAM-Zugriff, keine SD-Last)
  und zeichnet den Rahmen bei einer erkannten Aenderung sofort neu, auch
  OHNE Verlassen des Screens, als Sicherheitsnetz.
- **7 Datenzeilen** bekommen eine eigene, kleine Loeschflaeche
  (`clearDataArea()`, nur der Textbereich unterhalb des Kreises, mit
  12px Ascent-Reserve fuer die erste Zeile wegen der Font-Baseline-Falle)
  statt weiterhin den kompletten Inhaltsbereich zu leeren - gleiches
  Prinzip wie beim F1-Countdown-Fix.
- **Beweglicher ISS-Punkt** (`updateIssDot()`): die ALTE Punktposition
  wird nur bei tatsaechlicher Aenderung (Sichtbarkeit ODER Pixel-Position
  anders als beim letzten Mal) gezielt "repariert" - ein kleines, per
  `tft.setViewport(vx, vy, vw, vh, false)` geklipptes Rechteck um die
  alte Position wird schwarz geleert, darin wird NUR `drawRadarFrame()`
  erneut aufgerufen (vpDatum=false haelt die Koordinaten absolut,
  TFT_eSPI verwirft automatisch alles ausserhalb des kleinen Ausschnitts) -
  stellt Kreisbogen/Kartenpunkte/Kompass-Buchstaben an dieser Stelle
  korrekt wieder her, kein schwarzes Loch, keine Punktspur. Patch bewusst
  asymmetrisch (mehr Platz rechts fuer die "ISS"-Beschriftung). Erst
  DANACH wird der neue Punkt gesetzt. Fuenf Sichtbarkeits-
  Uebergangsfaelle (unsichtbar->unsichtbar/unsichtbar->sichtbar/
  sichtbar->sichtbar unveraendert/sichtbar->sichtbar bewegt/
  sichtbar->unsichtbar) wurden einzeln von Hand durchgerechnet und
  bestaetigt korrekt.
- **Live am echten Geraet bestaetigt (07.10., per temporaerer Serial-
  Diagnose UND direkter Beobachtung durch Alex, danach wieder entfernt):**
  ueber 6 Positions-Aktualisierungszyklen (~70s, exakt alle ~12s wie
  erwartet) wurde `drawStaticFrame()` (der teure Volltransfer) NUR EINMAL
  beim Betreten aufgerufen, nie wieder - Alex bestaetigte direkt am
  Geraet "jetzt flackert nichts mehr". Die ISS war waehrend des Tests
  unter dem Horizont (kein sichtbarer Punkt) - der Bewegungs-/Erase-Pfad
  des Punkts selbst konnte dadurch nicht live beobachtet werden, wurde
  aber wie oben beschrieben per Fallunterscheidung von Hand verifiziert.

## Raketenstart-Countdown: naechste 3 Starts statt nur 1

Erweiterung (Alex' Auftrag, 06.10.), siehe `rocket_client.cpp`/
`rocket_screen.cpp`:

- **`?limit=3` statt `?limit=1`** (`Config::ROCKET_API_URL`) - live
  bestätigt: dieselbe EINE Anfrage liefert die nächsten 3 Starts
  komplett, kein zusätzlicher Request nötig, bleibt deutlich unter dem
  15-Anfragen/Stunde-Limit. `Config::ROCKET_MAX_LAUNCHES = 3`,
  `NetTask::RocketData` trägt jetzt ein Array statt eines einzelnen
  `Launch` (`NetTask::getRocketData()` ersetzt das bisherige
  `getRocketLaunch()`).
- **Zusätzliche, live verifizierte API-Felder**: `launch_service_
  provider.name` (Agentur, z.B. "SpaceX"), `rocket.configuration.
  full_name` (volle Bezeichnung, z.B. "KSLV-2 Nuri" statt nur "Nuri"),
  `mission.type`/`mission.orbit.name` (Missionsart/Zielorbit),
  `mission.description` (nur im Detail-Screen, zu lang für den
  Hauptscreen), `holdreason`/`failreason` (live bestätigt: leerer
  String "" wenn nicht zutreffend, NICHT null), `probability`/
  `weather_concerns` (live bestätigt: `null` wenn nicht zutreffend -
  bei KEINEM der nächsten 50 geprüften Starts aktuell gesetzt, das
  Schema erlaubt es aber jederzeit, z.B. kurz vor einem echten
  Start-Scrub).
- **Hauptscreen** zeigt weiterhin den nächsten Start groß mit
  Live-Countdown (unverändert) - die Lücke darüber füllen jetzt
  Agentur + die volle Raketenbezeichnung (ersetzt die bisherige kurze
  Bezeichnung in der "Rakete:"-Zeile). **Hold-/Fehlgrund sind bewusst
  DIREKT auf dem Hauptscreen** (nicht erst im Detail-Screen versteckt) -
  "kritische Status-Info", rot (Fehlschlag) bzw. gelb (Hold), Fehlschlag
  hat Vorrang falls theoretisch beide gesetzt wären. Go-Chance/Wetter-
  Hinweis stehen dagegen NUR im Detail-Screen (seltener, weniger
  zeitkritisch als ein Hold/Fail).
- **`COUNTDOWN_AREA_TOP` von 234 auf 190 verschoben**, um Platz für den
  neuen "Weitere Starts"-Button darunter zu schaffen - `clearCountdownArea()`
  löscht dafür jetzt nur noch ein festes Band (`COUNTDOWN_AREA_H`=70)
  statt bis zum Bildschirmende, damit der jede Sekunde neu gezeichnete
  Countdown den Button darunter nicht mitlöscht.
- **"Weitere Starts"-Button** (nur sichtbar, wenn `launchCount > 1`,
  sein Bereich wird aber bei jedem Redraw trotzdem erst geleert, falls
  die API zwischen zwei Abrufen auf ≤1 Start zurückfällt) öffnet eine
  Liste mit Start Nr. 2/3 (`showMoreLaunchesScreen()`) - je Zeile
  Mission + volle Raketenbezeichnung + "TT.MM. HH:MM UTC"
  (`formatDateTimeUtc()`, reine Substring-Extraktion aus dem rohen
  ISO-String, KEINE Epoch-Umrechnung nötig, da nur für die Anzeige
  gebraucht - anders als beim Live-Countdown des Hauptscreens, der
  echte Sekunden braucht). Nur bis zu 2 Zeilen, bewusst ohne eigene
  Scroll-Infrastruktur.
- **Jede Zeile öffnet einen scrollbaren Detail-Screen**
  (`showLaunchDetailScreen()`, gleiches Scroll-Muster wie beim OEPNV-/
  Wetter-Detail-Screen) - für JEDEN der bis zu 3 Starts gültig (wird
  aktuell aber nur für Start Nr. 2/3 verlinkt, der Hauptstart hat keinen
  eigenen Tap-Einstieg dafür, das war nicht Teil des Auftrags). Zeigt
  alle Felder inkl. Missionsart/Zielorbit/Hold-/Fehlgrund/Go-Chance/
  Wetter-Hinweis (jeweils nur wenn nicht leer/-1) und als letztes die
  Missionsbeschreibung.
- Live am echten Gerät verifiziert (06.10., per temporärer Serial-
  Diagnose, danach wieder entfernt): Abruf liefert korrekt 3 Starts mit
  allen erwarteten Feldern.

### Fix (07.10.): Hauptscreen blieb auf bereits gestarteter Mission stehen

Alex' Fund: Hauptscreen zeigte "Erfolgreich gestartet" mit abgelaufenem
Countdown (0 Tage, 00:00:00), obwohl "Weitere Starts" bereits künftige
Starts listete. Ursache: Launch Library 2s `/upcoming`-Endpoint hält
einen gerade gestarteten Eintrag erfahrungsgemäß noch eine Weile in der
Liste (Status wechselt auf "Launch Successful", der Eintrag fällt aber
erst verzögert heraus), während spätere, wirklich noch bevorstehende
Starts bereits auf den folgenden Positionen auftauchen - der Code
vertraute aber blind `launches[0]` als "die" Hauptmission, ohne jemals
zu prüfen, ob deren `netEpoch` noch in der Zukunft liegt.

**Zwei Ebenen behoben:**
1. `net_task.cpp::promoteNextFutureLaunch()` - direkt nach jedem
   `RocketClient::fetch()`, findet den ERSTEN Eintrag mit
   `netEpoch > jetzt` und rotiert ihn an Index 0 (bereits verstrichene
   Einträge rutschen dabei nach hinten). Fallback: sind ALLE Einträge
   verstrichen, bleibt die Reihenfolge unverändert (nie "keine Daten").
2. `rocket_screen.cpp::promoteNextFutureLaunchIfNeeded()` - zusätzliche,
   lokal duplizierte Sicherung (Projekt-Konvention) bei JEDEM
   `update()`-Durchlauf: greift, wenn der Countdown für `launches[0]`
   zwischen zwei 12-Minuten-Abrufen bereits auf 0 gefallen ist, während
   ein späterer Eintrag im selben Snapshot noch in der Zukunft liegt -
   deckt den ganz normalen Fall ab, dass JEDER Start irgendwann einmal
   "fällig" wird, ohne auf den nächsten Netzwerk-Refetch warten zu
   müssen.

**Wichtige Stack-Falle dabei entdeckt und vermieden:** ein erster
Implementierungsversuch von `promoteNextFutureLaunch()` baute die neue
Reihenfolge in einem kompletten zweiten `RocketClient::Launch`-Array auf
dem Stack zusammen (`Launch reordered[Config::ROCKET_MAX_LAUNCHES]`) -
live per Serial-Diagnose sofort als **Stack-Canary-Crash
("Guru Meditation Error... Stack canary watchpoint triggered (NetTask)")
reproduziert**: `Launch` enthält ca. 13 `String`-Felder, ein komplettes
zweites 3er-Array davon sprengt den 20-KB-Stack von `NetTask` in
Kombination mit dessen bereits hohem TLS-/JSON-Stackbedarf. Fix: beide
Funktionen rotieren stattdessen in-place mit GENAU EINEM `Launch` als
Zwischenspeicher (`Launch promoted = launches[i]; ...verschieben...`) -
**Lektion für künftigen Code in `net_task.cpp`:** niemals ein komplettes
zusätzliches Array eines String-haltigen Structs (egal welchen Typs)
als lokale Stack-Variable in einer von `NetTask::taskFunc()` aufgerufenen
Funktion anlegen, ohne das vorher gegen den 20-KB-Stack abzuschätzen -
im Zweifel in-place mit Einzel-Element-Zwischenspeicher arbeiten, wie
hier gelöst.

Live getestet (07.10., per temporärer Serial-Diagnose + simulierten
Zeitstempeln, danach wieder vollständig entfernt): (1) ein künstlich auf
"vor 2 Minuten" gesetzter `launches[0]` wurde nach `promoteNextFutureLaunch()`
korrekt durch den nächsten echten künftigen Start ersetzt, mehrfach über
35s ohne Crash reproduziert; (2) mit `launches[0]` künstlich auf "in 8s"
gesetzt (Bildschirm temporär als Boot-Einstieg erzwungen) lief der
Countdown live bis 0 herunter, worauf `update()` automatisch auf den
nächsten künftigen Start umschaltete - ohne Warten auf den nächsten
12-Minuten-Refetch.

### Fix (07.10.): Rahmenlinie lief durch den Titel in "Weitere Starts"

Alex' Fund: die obere Rahmenlinie jeder Box in `showMoreLaunchesScreen()`
lief optisch mitten durch den Mission-Titel, wie durchgestrichen. Ursache:
klassische Baseline-Falle (siehe "WICHTIGSTE FALLE" oben) - `rowTop`
wurde mit einem geratenen `y - 4`-Versatz berechnet, der von einem
top-verankerten Font ausging. Bei unserem grundlinien-verankerten Font
wächst der Text von `y` aus nach oben, der tatsächliche Glyphen-Top liegt
also deutlich höher als `y - 4`. Fix: `y - 12` statt `y - 4` - wiederverwendet
exakt die bereits an anderer Stelle im Projekt verifizierte
"12px Ascent-Reserve" (`iss_radar_screen.cpp::DATA_AREA_TOP`), keine neue
geratene Zahl. Rocket-Screen auf weitere Vorkommen desselben
Musters durchsucht (06./07.10.) - an keiner anderen Stelle in
`rocket_screen.cpp` wird ein Box-Rahmen aus einer laufenden
Text-y-Koordinate abgeleitet, alle übrigen Rahmen (Buttons,
Detail-Screen) nutzen feste `Rect`s, sind also nicht betroffen.

### Status-Übersetzungstabelle (07.10.): primär über status.id, nicht über Text

Die bisherige Status-Übersetzung lief ausschließlich über
`status.abbrev`-Textvergleiche (`if (abbrev == "Go") ...`) und deckte nur
7 der tatsächlich 9 festen Launch-Library-2-Statuswerte ab ("To Be
Confirmed"/"Payload Deployed" fehlten komplett). Alex' Auftrag (07.10.):
auf das stabilere `status.id`-Feld (laut API-Doku 9 feste numerische
Werte, 1-9) als primären Schlüssel umstellen, nach demselben
Tabellen-Prinzip wie die Session-/DNF-Übersetzungen im F1-Screen.

- **`RocketClient::Launch::statusId`** (neues `int16_t`-Feld,
  `rocket_client.h`/`.cpp`) - geparst aus `status.id` (zusätzlicher
  Filter-Eintrag), `-1` falls das Feld ausnahmsweise fehlt.
- **`rocket_screen.cpp::rocketStatusText()`** - dreistufige
  Auflösung: (1) `status.id` gegen die feste 9er-Tabelle
  (`rocketStatusStringIdById()`, `switch` 1-9), (2) falls keine ID
  vorhanden/erkannt: Text-Fallback über `status.abbrev`
  (`rocketStatusStringIdByAbbrev()`, deckt dieselben 9 Werte ab), (3)
  falls auch das nicht greift (z.B. ein künftiger, neuer API-Statuswert):
  **defensiv der rohe API-Text aus `status.name`** statt leer/Absturz.
  Ersetzt die alte `rocketStatusStringId()`-Funktion an beiden
  Einsatzstellen (Hauptscreen-Infoblock, Detail-Screen).
- **Zwei neue StringIds ergänzt** (`ROCKET_STATUS_TBC`,
  `ROCKET_STATUS_PAYLOAD_DEPLOYED`) in `i18n.h` und allen 4
  Sprachdateien. Dabei gleichzeitig die Übersetzungen der bereits
  vorhandenen 7 Status-Strings an Alex' vorgegebenen, näher am
  offiziellen API-Wortlaut orientierten Text angeglichen (z.B. DE "Go"
  von "Bestätigt" zu "Startbereit", EN "Success" von "Launched
  successfully" zu "Launch Successful", ES/FR "TBD" und "TBC" waren
  zuvor je auf dieselbe Bedeutung ("Por confirmar"/"À confirmer")
  abgebildet - jetzt sauber getrennt in "Por Determinar"/"Por Confirmar"
  bzw. "À Déterminer"/"À Confirmer").

## Navigation: Wisch-Gesten statt Menü

**Feste Design-Entscheidung (Alex' ausdrücklicher Wunsch, 05.10.): Es
gibt KEIN Menü.** Ein zuvor gebautes scrollbares Hauptmenü
(`menu_screen.cpp`) wurde noch am selben Tag wieder komplett entfernt
und durch reine Wisch-Navigation ersetzt - dieser Abschnitt ersetzt
die vorherige "Menüstruktur"/"Scrollbare Listen-Menüs"-Dokumentation.

**Konzept:**
- **Links/Rechts wischen** schaltet zwischen den Feature-Bildschirmen
  um (Karussell-Prinzip: am Ende der Reihe wieder vorne anfangen, "round
  robin" ohne Ende - vom letzten Eintrag aus weiter wischen führt zurück
  zum ersten und umgekehrt, siehe `app_navigation.cpp`). Aktuell 7
  Einträge, in dieser Reihenfolge: Wetter, ISS-Radar,
  Raketenstart-Countdown, OEPNV-Abfahrtstafel, Formel 1, Krypto-Ticker,
  Erdbeben weltweit - die Struktur ist von Anfang an auf mehrere ausgelegt, damit sich
  künftige Feature-Screens (z.B. Bahn) einfach einreihen lassen.
  **Konvention:** Wischen nach LINKS geht einen Schritt WEITER
  (nächster Eintrag), nach RECHTS einen Schritt ZURÜCK (voriger
  Eintrag) - wie bei typischen Smartphone-Homescreens.
- **Hoch ODER runter wischen** (von einem beliebigen Feature-Bildschirm
  aus) öffnet den Einstellungs-Hub `settings_hub_screen.cpp` (Titel
  "System", siehe unten).
- **Innerhalb der Einstellungen gibt es KEINE Wisch-Navigation mehr**
  (Alex' Auftrag, 06.10. - dritter und bislang letzter Anlauf bei diesem
  Bereich, siehe unten): nur noch Tippen auf die Hub-Buttons bzw. auf den
  Zurück-Button der Untermenues.
- **Vom Einstellungs-Hub aus** führt Hoch- oder Runterwischen zurück zu
  GENAU DEM Feature-Bildschirm, von dem aus man in die Einstellungen
  gewischt hat (letzten Zustand merken, siehe
  `app_navigation.cpp::featureBeforeSettings`) - dieser Teil ist über alle
  drei Anläufe hinweg unverändert geblieben.

**Umsetzung:**
- `swipe_input.h/.cpp`: erkennt Wisch-Gesten (Links/Rechts/Hoch/Runter)
  zusätzlich zu reinen Taps. Verfolgt dafür selbst den
  Berührungs-START-Punkt (`TouchInput` kennt nur den Loslass-Punkt) und
  vergleicht ihn mit dem Loslass-Punkt - erst ab `SWIPE_THRESHOLD_PX`
  (40px) in eine Achse gilt es als Wisch-Geste statt als Tap.
- `app_navigation.cpp`: die zentrale, dauerhaft laufende Schleife
  (ersetzt das frühere `menu_screen.cpp`, wird aus `main.cpp::loop()`
  aufgerufen). Hält ein `FEATURES[]`-Array mit
  `enter()`/`update()`/`handleTap()`-Funktionszeigern pro
  Feature-Screen. **Jeder neue Feature-Screen MUSS exakt diese
  Schnittstelle implementieren** (siehe `iss_radar_screen.h` als
  Vorbild) und wird dann einfach als weiterer `FEATURES[]`-Eintrag
  ergänzt - keine Änderung an der Navigationslogik selbst nötig.
  - `enter(tft)`: einmalig beim Betreten zeichnen (auch beim
    Zurückkommen von den Einstellungen) - OHNE selbst einen Netzwerk-
    Abruf auszulösen (der läuft ohnehin dauerhaft im Hintergrund auf
    Core 0, siehe `net_task.cpp`), sondern nur mit den zuletzt bekannten
    Daten (siehe `iss_radar_screen.cpp`, dortige `haveData`/`lastPos`/
    `lastLook` bleiben über enter()/leave()-Zyklen hinweg erhalten).
  - `update(tft)`: in jedem Schleifendurchlauf aufgerufen, solange
    dieser Screen aktiv ist - prüft nur, ob `net_task.cpp` seit dem
    letzten Durchlauf neue Daten geliefert hat (Versionszähler-Vergleich,
    siehe "Tech-Stack" oben), und zeichnet dann einmalig neu. Löst selbst
    NIE einen Netzwerk-Abruf aus.
  - `handleTap(tft, x, y)`: Tap-Koordinaten (kein Wisch, siehe
    `SwipeInput::Direction::None`) für interne Buttons (z.B. der
    "?"-Infobutton).
- **Einstellungs-Bereich** (Alex' Auftrag, 05.10., seitdem dreimal am
  06.10. umgebaut - siehe Git-Historie, falls die Details einer
  Zwischenstufe mal gebraucht werden: 1. eine einzelne Sprachauswahl-
  Seite, 2. drei wischbare "Themen-Seiten" Sprache/System/Theme mit
  unterem Wisch-Streifen, 3. der jetzige, dritte und finale Aufbau: ein
  zentraler Button-Hub. Grund fuer den letzten Umbau: Wisch- und Tap-Zone
  ueberschnitten sich bei Variante 2 weiterhin fuer Alex' Geschmack zu
  sehr). **Aktueller, gueltiger Aufbau:**
  - `settings_hub_screen.cpp` ("System"): EIN Screen mit 6 grossen,
    gleich grossen Buttons im 3er-Grid (WLAN/Einheiten/Sprache,
    Anzeige/Tools/LED - der frühere 6. Button "Farben" entfiel am 07.10.
    mit der kompletten Entfernung des Mehrfach-Theme-Systems, siehe
    "Farbschema" weiter oben; noch am selben Tag zog "Einheiten" aus dem
    Tools-Untermenü an diese freigewordene Stelle, siehe "Globale
    Einheiten-Einstellung" unten) - `app_navigation.cpp` spricht NUR NOCH diesen
    einen Hub direkt an (`enter()`/`update()`/`handleTap()`, kein
    `SETTINGS_SCREENS[]`-Array mehr wie in Variante 2). Ein Tap auf einen
    Hub-Button ruft das zugehoerige Untermenue als BLOCKIERENDEN
    `run(tft)`-Aufruf auf (gleiches Prinzip wie z.B.
    `WifiSetupScreen::run()` anderswo im Code) und zeichnet den Hub
    danach neu.
  - Die 6 Untermenues (`settings_wifi_manage_screen.cpp`,
    `settings_units_screen.cpp`, `settings_language_screen.cpp`,
    `settings_display_screen.cpp`, `settings_tools_screen.cpp`,
    `settings_led_screen.cpp`) sind JEWEILS ein simples `void run(TFT_eSPI&
    tft)` - KEIN `enter()`/`update()`/`handleTap()`-Trio mehr (das brauchen
    nur noch Feature-Screens und der Hub selbst). Jedes Untermenue hat
    oben links einen einheitlichen "< Zurück"-Button
    (`SettingsCommon::backButtonRect()`/`drawBackButton()`) - das ist der
    EINZIGE Weg zurück zum Hub, keine Wisch-Geste mehr. `settings_display_
    screen.cpp` buendelt dabei weiterhin Helligkeit (eigener Regler-
    Unterbildschirm), Screenflip/180°-Drehung (Direkt-Toggle) und
    Bildschirm-Timeout (eigene Zeitstufen-Auswahl) als 3 Zeilen -
    inhaltlich identisch zur fruehreren "System"-Themenseite aus Variante
    2, nur umbenannt zu "Anzeige" und ohne die dortige WLAN-Zeile (WLAN
    ist jetzt ein eigener Hub-Button mit eigenem Untermenue,
    `settings_wifi_manage_screen.cpp` - 1:1 vom Konzept aus Eiswolfs
    Flightradar CYD uebernommen, dortige `wifi_manage_screen.cpp`: bis zu
    3 Netzwerk-Slots mit "X"-Entfernen-Button, "Netzwerk hinzufuegen"
    startet denselben Scan+Passwort-Dialog wie in der Ersteinrichtung,
    `WifiSetupScreen::run()`). `settings_tools_screen.cpp` hat seit 06.10.
    nur noch die eine Funktion "Adresse ändern" (siehe "Eine allgemeine
    Toolbox-Position" weiter unten) - "Einheiten" sass dort bis 07.10.
    als zweiter Abschnitt, wurde dann aber als eigener Hub-Button
    herausgelöst (siehe oben). `settings_led_screen.cpp` ist weiterhin
    ein bewusster Platzhalter ohne echte Funktion (wird später gefüllt) -
    die CYD-Hardware hat eine ansteuerbare RGB-LED (GPIO4/16/17, siehe
    Eiswolfs Flightradar CYDs `led_alert.cpp` für das PWM-Muster), das
    LED-Untermenü ist also kein Hardware-Dead-End. Das Raster selbst
    bleibt bewusst klein (6 Buttons - Alex' Auftrag, 06.10.: KEIN 7.
    Button für die Standort-Änderung) - neue, kleinere Funktionen ziehen
    stattdessen in ein bereits bestehendes Untermenü wie "Tools" ein,
    statt das Raster zu erweitern; "Einheiten" bekam trotzdem einen
    eigenen Button, weil es sich um eine eigenständige, haeufiger
    gebrauchte Einstellung handelt, nicht um eine kleine Zusatzfunktion
    wie "Adresse ändern", UND weil dadurch ein ausgewogenes 2x3-Raster
    entsteht statt einer unausgewogenen 3+2-Aufteilung.
  - **Regel, nicht nur Beispiel:** jede einzelne Funktion/jeder Button in
    einem Untermenue (Helligkeit, Screenflip, Timeout, WLAN-Netzwerk-
    Eintrag, Sprachoption, ...) bekommt ihren/seinen eigenen
    kleinen "?"-Infobutton (gleiches Muster wie auf den Feature-Screens),
    unabhängig davon, ob der jeweilige Auftrag das explizit erwähnt.
  - **Visuelle Kennzeichnung:** Hub UND alle 6 Untermenues zeichnen in
    `SettingsCommon::accentColor(tft)` (`settings_common.h/.cpp`) - seit
    Entfernung des Mehrfach-Theme-Systems (07.10.) liefert das schlicht
    das feste `TFT_GREEN` wie jeder andere Screen, OHNE gesonderte
    Abdunklung/Entsättigung. Bis 07.10. war das bewusst eine zur
    Grau-Richtung entsättigte UND abgedunkelte Variante der damals aktiven
    Themenfarbe (eine reine Helligkeits-Abdunklung von Neongrün blieb laut
    Alex' Foto-Feedback "praktisch identisch" zur normalen Akzentfarbe) -
    diese visuelle Abgrenzung wurde zusammen mit dem Theme-System auf
    Alex' Wunsch wieder entfernt, da es jetzt nur noch EIN Farbschema in
    der ganzen App gibt und eine Sonderfarbgebung für Einstellungen
    dadurch nicht mehr nötig ist. `settings_common.h/.cpp` enthielt bis
    zum dritten Umbau (vor dem Hub) zusätzlich die gesamte Infrastruktur
    für die wischbaren Themen-Seiten aus Variante 2 (unterer Wisch-
    Streifen, Pfeile, "Hier wischen"-Hinweistext, Zonen-Erkennung über den
    Touch-Startpunkt in `SwipeInput::poll()`) - komplett entfernt, als
    Variante 2 durch den Hub ersetzt wurde.
  - **Sofort speichern:** Änderungen in den Einstellungen (Sprache,
    Helligkeit, 180°-Display-Flip, Bildschirm-Timeout, WLAN) werden
    SOFORT beim Ändern auf die SD-Karte geschrieben, NICHT erst bei einem
    späteren Commit-Zeitpunkt wie während der Ersteinrichtung (dort gilt
    bewusst das Gegenteil, siehe "Ersteinrichtung" unten - beide
    Prinzipien bewusst verschieden, je nach Kontext).
  - **Bildschirm-Timeout:** schaltet die PWM-Hintergrundbeleuchtung
    (`Config::BACKLIGHT_PWM_*`, 1:1 Flightradars `ledcSetup()`/
    `ledcAttachPin()`-Muster, aber ohne dessen Auto-Helligkeit/Nachtmodus/
    Ruhebildschirm-Komplexität) nach der eingestellten Zeit ohne
    Berührung komplett aus - die eigentliche Ueberwachung dafür lebt
    bewusst in `app_navigation.cpp::run()` selbst (nicht in
    `settings_display_screen.cpp`), weil nur die zentrale Schleife jede
    Berührung VOR ihrer Auswertung als Tap/Wisch sieht. Die Berührung,
    die den Bildschirm wieder aufweckt, wird dabei bewusst NICHT
    zusätzlich als normaler Tap/Wisch verarbeitet (`suppressNextGesture`),
    damit ein reiner Aufweck-Tipp nicht versehentlich einen darunter
    liegenden Button auslöst.
- Alle bisherigen "Zurück"-Buttons auf Feature-Screens sind entfallen
  (z.B. beim ISS-Radar) - man verlässt einen Screen nur noch per
  Wisch-Geste, nie mehr per Tap auf einen Button. Die Ersteinrichtung
  (`first_run_*.cpp`) ist davon NICHT betroffen und bleibt wie bisher
  tap-/button-basiert (eigener, einmaliger Ablauf, kein Teil des
  Wisch-Karussells).

## Eine allgemeine Toolbox-Position

**Festes Grundprinzip, gilt ab sofort für das ganze Projekt** (Alex'
Auftrag, 06.10., nachdem der Wetter-Screen zunächst einen eigenen
Adress-Button bekommen hatte): Die Adresse/Position wird einmalig bei
der Ersteinrichtung eingegeben (`first_run_location_screen.cpp`). Wer
sie später ändern möchte, macht das **ausschließlich** über den
Menüpunkt "Adresse ändern" im Einstellungs-Hub (System-Hub → "Tools" →
`settings_tools_screen.cpp`) - einzelne Feature-Screens bieten dafür
KEINE eigene Eingabemöglichkeit mehr. Diese EINE, allgemeine
Toolbox-Position (`LocationPresets`, siehe "Tech-Stack" oben) wird
automatisch von allen Feature-Screens genutzt, die eine Position
brauchen (aktuell ISS-Radar und Wetter, künftig ggf. weitere) - ändert
sich die Position hier, übernehmen diese Screens das beim nächsten
`enter()` automatisch, ohne dass dort etwas angepasst werden muss
(`iss_radar_screen.cpp`/`weather_screen.cpp::resolveAndSetLocation()`
lesen beide 1:1 denselben `LocationPresets::activeIndex()`).

**Einzige Ausnahme: der OEPNV-Screen behält seinen eigenen "Genauere
Adresse"-Button.** Der dient dort NICHT der Korrektur einer ungenauen
allgemeinen Position, sondern dem bewussten Nachschlagen einer ANDEREN
Haltestelle (z.B. unterwegs) - ein eigener, sinnvoller Anwendungsfall,
der mit der allgemeinen Position nichts zu tun hat. Solange im
OEPNV-Screen keine eigene Haltestelle gewählt wurde, geht er weiterhin
automatisch von der allgemeinen Toolbox-Position aus. Der Wetter-Screen
hatte zunächst denselben "Genauere Adresse"-Mechanismus wie der
OEPNV-Screen bekommen (eigene Override-Datei, eigener Button) - wurde
aber auf Alex' Auftrag wieder entfernt, weil beim Wetter (anders als
beim OEPNV) kein sinnvoller Anwendungsfall für eine ZWEITE, abweichende
Position besteht.

**`settings_tools_screen.cpp` ("Adresse ändern")** zeigt zuerst an, was
aktuell gespeichert ist (vom Nutzer vergebener Preset-Name, sonst die
Koordinaten, sonst - falls per WLAN-Ortung statt Adresse ermittelt -
"Ungefähre WLAN-Position"), und bietet einen Button, der denselben
`AddressSearchScreen::run()` wie die Ersteinrichtung öffnet (gleiche
Komponente, inkl. sprachabhängiger Tastatur). **Wichtige Einschränkung,
die beim Bau aufgefallen ist:** Die tatsächlich eingegebene
Adress-TEXT wird nirgends dauerhaft gespeichert - `LocationPresets`
speichert nur Koordinaten + einen optionalen, vom Nutzer selbst
vergebenen Namen (siehe `address_search_screen.cpp::runNameKeypad()`).
Die Anzeige "was aktuell gespeichert ist" zeigt deshalb den Namen bzw.
die Koordinaten, NICHT die ursprünglich eingegebene Adresse im Wortlaut
- falls Alex das wörtlich braucht, müsste zusätzlich der geokodierte
Adresstext dauerhaft gespeichert werden (aktuell nur während der
Bestätigung auf dem Bildschirm sichtbar, siehe
`address_search_screen.cpp::runConfirmScreen()`).

**Woher stammt die aktuelle Position (Adresse vs. WLAN-Ortung)?**
`LocationPresets::Source` (`Unknown`/`Address`/`Wifi`) wird als
Zusatzfeld in derselben `locations.txt` persistiert (`active=<idx>,
source=<address|wifi>` als Kopfzeile, siehe `location_presets.cpp`) -
fehlt das Feld (z.B. bei einer 1:1 kopierten Flightradar-`locations.txt`,
siehe "Ersteinrichtung: Übernahme von Flightradar-Daten" unten), fällt
es auf `Unknown` zurück. Wird von `first_run_location_screen.cpp`
(beide Pfade: Adresse UND WLAN-Fallback) UND von
`settings_tools_screen.cpp` gesetzt.

**Technische Notiz zu wiederholten Adressänderungen:** `LocationPresets`
unterstützt strukturell bis zu 3 Presets (`MAX_PRESETS`,
geerbt aus Flightradars Modul-Vorbild), Toolbox nutzt davon aber
effektiv nur EINEN Slot - es gibt keine eigene UI zum Verwalten
mehrerer Presets. `settings_tools_screen.cpp` sichert deshalb nach
einer erfolgreichen Adressänderung (`AddressSearchScreen::run()` hat
bereits ein neues Preset hinzugefügt UND aktiviert) dessen Daten,
entfernt ALLE Presets und legt nur dieses eine neu an - sonst würden
sich bei wiederholter Nutzung Presets bis zum Limit ansammeln und
`addPreset()` irgendwann stillschweigend fehlschlagen.

## Globale Einheiten-Einstellung (Metrisch/Imperial)

**Festes Grundprinzip, gilt ab sofort fuer das ganze Projekt** (Alex'
Auftrag, 07.10. - die App liegt auf einem oeffentlichen GitHub-Repo,
km/h bzw. °C bedeuten z.B. US-Nutzern nichts). `units.h`/`units.cpp`
nach EXAKT demselben Muster wie `i18n.h`/`.cpp` (eigene kleine
SD-Datei `Config::SD_UNITS_FILE`, `init()`/`current()`/
`setPreview()`/`save()`/`set()` - `setPreview()` nur RAM fuer die
Ersteinrichtung, `set()` Vorschau+Speichern in einem fuer die
Einstellungen danach).

**STANDARDREGEL (verpflichtend fuer alle kuenftigen Screens):** Jeder
neue oder geaenderte Screen, der eine physikalische Einheit anzeigt
(Temperatur, Geschwindigkeit, Niederschlag, Entfernung, ...), MUSS
`Units::current()` bzw. die zugehoerigen `Units::...Param()`/
`Units::...Suffix()`-Funktionen abfragen, statt selbst zu entscheiden
oder eine Einheit fest zu verdrahten - unabhaengig davon, ob der
jeweilige Auftrag das explizit erwaehnt. `units.h` ist bewusst EINE
zentrale, global auslesbare Instanz (kein Pro-Screen-Zustand), damit
das ohne zusaetzliche Verdrahtung moeglich ist.

- **Pflichtschritt in der Ersteinrichtung, DIREKT NACH der
  Sprachauswahl** (`SetupStep::Units`, `first_run_units_screen.cpp`,
  siehe `main.cpp::buildSetupPlan()`) - laeuft in JEDEM der 3
  Ablaufplan-Faelle (a/b/c, siehe "Ersteinrichtung" unten), genau wie
  die Sprachauswahl selbst. Nutzt bereits die zu diesem Zeitpunkt
  gewaehlte Sprache (anders als `first_run_language_screen.cpp`, das
  noch VOR jeder Sprachwahl laeuft und deshalb einen hart kodierten
  zweisprachigen Titel braucht).
- **Jederzeit aenderbar ueber den eigenstaendigen "Einheiten"-Button im
  Einstellungs-Hub** (`settings_units_screen.cpp`, gleiches Grundmuster
  wie `settings_language_screen.cpp`) - speichert sofort (`Units::set()`),
  kein separater Commit-Schritt. Bis 07.10. sass dieser Button als
  Unterabschnitt in Einstellungen → Tools (neben "Adresse ändern") - auf
  Alex' Wunsch dort entfernt und als eigener 6. Hub-Button an die Stelle
  des zuvor entfernten "Farben"-Buttons gesetzt (5 Buttons mit nur 2
  statt 3 in der unteren Reihe sahen unausgewogen aus), siehe
  `settings_hub_screen.cpp`.
- **Fix (07.10.): ISS-Radar zeigte Distanz/Hoehe/Geschwindigkeit weiterhin
  fest in km/km/km-h, unabhaengig von der eingestellten Einheit** - Alex'
  Fund. Ursache: `iss_client.cpp` liefert diese Werte aus einer eigenen
  Bahnmechanik-Berechnung (nicht von einer API mit Einheiten-Parameter
  wie Open-Meteo), `iss_radar_screen.cpp` haengte deshalb `" km"`/
  `" km/h"` fest an die Rohwerte an, statt `Units::current()` zu
  beachten. Dafuer zwei neue, bewusst wiederverwendbare Hilfsfunktionen
  in `units.h/.cpp` ergaenzt (`distanceKmToDisplay()`/
  `speedKmhToDisplay()`, beide intern derselbe km→mi-Faktor 0,621371,
  getrennt benannt fuer Lesbarkeit am Aufrufort) - fuer JEDEN kuenftigen
  Screen gedacht, der lokal (ohne Server-Unterstuetzung) rechnet, nicht
  nur fuer den ISS-Radar. `iss_radar_screen.cpp` nutzt sie jetzt
  zusammen mit `Units::distanceSuffix()`/`Units::windSpeedSuffix()`
  (Letzteres fuer die Geschwindigkeit wiederverwendet, da `"km/h"`/
  `"mph"` exakt dasselbe Format wie beim Wetter-Wind ist - kein
  eigenes `velocitySuffix()` noetig).
- **Alle Screens mit physikalischen Einheiten durchgegangen (07.10.,
  Alex' ausdruecklicher Auftrag)**: projektweite Suche nach fest
  verdrahteten Einheiten-Strings (`"km"`, `"km/h"`, `"mph"`, `"mi"`,
  `"°C"`, `"mm"` usw.) bestaetigt - ausser dem oben behobenen ISS-Radar
  nutzt JEDER Screen mit physikalischen Einheiten bereits korrekt
  `Units::`. Einzige weitere Treffer waren `crypto_screen.cpp`s
  `"sat/vB"` (Krypto-Transaktionsgebuehr, KEINE physikalische Einheit,
  bleibt bewusst literal wie "AQI"/"UV") sowie die beiden
  Einheiten-Auswahl-Screens selbst (`first_run_units_screen.cpp`/
  `settings_units_screen.cpp`, zeigen absichtlich BEIDE Systeme als
  Auswahloptionen).
- **Open-Meteo rechnet serverseitig um** (`temperature_unit=celsius/
  fahrenheit`, `wind_speed_unit=kmh/mph`, `precipitation_unit=mm/inch`,
  alle 3 Parameternamen live per curl verifiziert, 07.10.) - der Client
  rechnet selbst NICHTS um, `WeatherClient::Forecast`s Felder (`temperature`/
  `windSpeedKmh`/`precipitationMm` - Feldnamen bewusst NICHT umbenannt,
  geringinvasive Aenderung) enthalten bereits den Wert in der gewaehlten
  Einheit.
- **Einheiten-Buchstabe (C/F) NUR auf der prominenten
  Haupttemperaturanzeige**, bewusst NICHT auf den kompakten Spalten
  (stuendliche Vorhersageleiste/Wochenuebersicht) - dort waere der
  Platz zu knapp (z.B. "-12°C" = 5 Zeichen = 35px in einer nur ~36px
  breiten Spalte, praktisch ohne Reserve) und die Einheit ist durch die
  Hauptanzeige bereits einmal klar. `formatTemp()` (ohne Buchstabe,
  kompakte Stellen) und `formatTempWithUnit()` (mit Buchstabe,
  Hauptanzeige) sind deshalb zwei getrennte Funktionen in
  `weather_screen.cpp`.
- **Luftqualitaet (Alex' Auftrag): bei Metrisch `european_aqi`, bei
  Imperial `us_aqi` anzeigen** - dabei bewusst NICHT einfach die
  bereits vorhandene europaeische Farbskala auf den US-Wert angewendet
  (fachlich falsch gewesen waere, da die US-EPA-Skala andere
  Stufen-Grenzen hat: 0-50/51-100/101-150/151-200/201-300/301-500,
  inkl. einer zusaetzlichen Stufe "Unhealthy for Sensitive Groups" ohne
  europaeisches Gegenstueck) - `usAqiColor()`/
  `usAqiCategoryStringId()` in `weather_screen.cpp` sind eine komplett
  eigenstaendige, nach den offiziellen US-EPA-Grenzen aufgebaute
  Zuordnung. `displayedAqi()`/`displayedAqiColor()`/
  `displayedAqiCategoryStringId()` waehlen je nach `Units::current()`
  IMMER ein zusammenpassendes Zahl+Farbe+Kategorie-Tripel aus einer der
  beiden Skalen, nie gemischt. Der Detail-Screen zeigt weiterhin IMMER
  beide Werte nebeneinander (unveraendert, unabhaengig von der
  Einheiten-Einstellung).
- Live am echten Geraet verifiziert (07.10., per temporaerer Serial-
  Diagnose inkl. testweise erzwungenem Imperial-Pfad - ein echter Tap
  auf den Einheiten-Umschalt-Button laesst sich ohne physischen Zugriff
  aufs Geraet nicht nachstellen -, danach wieder entfernt): bei
  erzwungenem Imperial liefert der Wetter-Abruf korrekt Fahrenheit/
  mph/inch-Werte (z.B. 51,7°F statt 11°C, konsistent umgerechnet),
  die Luftqualitaets-Werte (european_aqi UND us_aqi) bleiben davon
  unberuehrt vorhanden.

## OEPNV-Abfahrtstafel

Dritter Feature-Screen (`transit_screen.cpp`, Alex' Auftrag, 06.10.) -
nächste Abfahrten an der nächstgelegenen Haltestelle. Datenquelle:
Transitous (`api.transitous.org`), schlüssellos, international nutzbar -
Endpunkte UND exakte JSON-Feldnamen live per `curl` gegen die echte API
verifiziert (nicht nur dokumentiert angenommen), siehe
`transit_client.cpp` für die Details:
- Reverse-Geocode (`GET /api/v1/reverse-geocode?place=<lat>,<lon>&type=
  STOP&numResults=5`) findet die nächste Haltestelle - Antwort ist ein
  Array am Root, `id`/`name` pro Eintrag. Läuft NUR, wenn sich die
  Position geändert hat (oder beim allerersten Mal), NICHT bei jedem
  normalen Abfahrten-Refresh.
- Abfahrten (`GET /api/v6/stoptimes?stopId=<id>&n=8`) - Antwort
  `{"stopTimes": [...]}`, pro Eintrag `place.departure` (ISO-8601 UTC,
  Echtzeit-Abfahrt), `place.track` (Gleis/Steig, NICHT bei jedem
  Eintrag vorhanden, Format je nach Betreiber unterschiedlich z.B.
  "Pos. 5" vs. "2" - live bestätigt), `routeShortName`, `headsign`,
  `tripFrom.name`/`tripTo.name` (Start/Ziel der gesamten Fahrt),
  `agencyName`, `realTime` (bool). **Wichtig:** `n` ist ein MINIMUM,
  kein Maximum (live bestätigt) - `transit_client.cpp` begrenzt die
  verarbeiteten Einträge deshalb zusätzlich explizit auf `Config::
  TRANSIT_MAX_DEPARTURES` (8), nicht nur über den Query-Parameter. Ein
  `alerts`-Feld existiert in diesem Endpunkt NICHT (mehrfach live über
  verschiedene Haltestellen/Abfahrten/Verkehrsmittel geprüft, kein
  einziges Vorkommen) - deshalb bewusst NICHT umgesetzt (weder
  Störungstext im Detail-Screen noch Warnsymbol in der Liste), statt ein
  nicht vorhandenes Feld zu erfinden. Falls das später gebraucht wird,
  müsste zuerst geklärt werden, ob/wie Transitous Störungsmeldungen
  überhaupt bereitstellt (vermutlich separater Endpunkt, falls
  überhaupt).
- **Weitere, zusätzlich genutzte Felder** (06.10., zweite Korrekturrunde
  - "alle sinnvollen API-Daten anzeigen", live bestätigt): `place.
  scheduledDeparture` (fahrplanmäßige Abfahrt, zusammen mit `place.
  departure` die Grundlage für die Verspätungsanzeige),
  `place.tz` (IANA-Zeitzone der Haltestelle, z.B. "Europe/Berlin" - für
  die Uhrzeitanzeige, siehe unten), `cancelled`/`tripCancelled` (beide
  live bestätigt vorhanden, je nach Betreiber nur eines der beiden
  gesetzt - UI behandelt beide gleich als "fällt aus"), `routeLongName`
  (volle Linienbezeichnung, oft leer, live bestätigt), `wheelchairAccessible`
  (String-Enum: "ACCESSIBLE"/"NOT_ACCESSIBLE"/"NO_INFORMATION", live nur
  "NOT_ACCESSIBLE" beobachtet), `bikesAllowed` (bool).
- Abfahrten-Refresh alle 2,5 Minuten (`Config::TRANSIT_REFETCH_INTERVAL_MS`)
  - Fahrplandaten ändern sich nicht ständig.
- **Uhrzeit statt Countdown als Hauptanzeige** (06.10., Alex' Auftrag:
  "man soll sofort sehen, wann der Bus tatsächlich kommt, ohne im Kopf
  nachzurechnen") - `transit_screen.cpp::formatClockTime()` zeigt "HH:MM"
  in der LOKALEN Zeit der Haltestelle, nicht in UTC (das Gerät selbst
  läuft komplett in UTC, `main.cpp::configTime(0,0,...)`, kein
  Zeitzonen-Offset). Die lokale Umrechnung nutzt bewusst KEINE volle
  IANA-Zeitzonendatenbank (auf dem ESP32 nicht sinnvoll umsetzbar),
  sondern eine kleine Basis-Offset-Tabelle (`TZ_BASE_OFFSETS[]`, nach
  `place.tz`) plus eine gemeinsame EU-weit einheitliche
  Sommerzeit-Berechnung (letzter Sonntag März/Oktober, 01:00 UTC - gilt
  für alle europäischen Länder identisch, nur die Standardzeit-Basis
  unterscheidet sich). Eine unbekannte/neue Zeitzone fällt auf
  Mitteleuropa (+1h) zurück. Der bisherige Countdown
  (`formatCompactTime()`/`formatRelativeSuffix()`, "NN Min"/"Xh YYm")
  bleibt als ZUSÄTZLICHE Information bestehen ("in 12 Min"), aber NUR
  dort, wo genug Platz ist (Detail-Screen, in Klammern hinter der
  Uhrzeit) - NICHT in der engen Listenzeile (siehe nächster Punkt).
- Zeitspalte in der Liste zeigt deshalb seit 06.10. NUR noch die Uhrzeit
  (immer kurz genug für `TIME_COL_W`, rechtsbündig, kein Marquee nötig für
  den Normalfall).
- **Ausfall "deutlich sichtbar" direkt in der Liste** (06.10.): eine
  ausgefallene Fahrt (`cancelled`) zeigt Linie UND Ziel-Spalte in Rot,
  der Zeilenrahmen wird ebenfalls rot statt gedimmt, UND die Zeit-Spalte
  zeigt statt der Uhrzeit den lokalisierten Ausfall-Text
  (`TRANSIT_CANCELLED_LABEL`) - über die zentrale `MarqueeLabel`-
  Komponente (wie die Ziel-Spalte), da der Text in allen 4 Sprachen
  unterschiedlich lang ist und garantiert in die feste `TIME_COL_W`
  passen muss. Der Detail-Screen zeigt denselben Text zusätzlich als
  rote Kopfzeile.
- JEDE Abfahrtszeile ist antippbar (unter Beachtung der CLAUDE.md-
  Mindest-Tapgröße, siehe "Pflicht: Mindest-Tapgröße" oben - die Anzahl
  gleichzeitig angezeigter Abfahrten wird dafür ggf. kleiner gerechnet
  als tatsächlich abgerufen, siehe `drawDepartures()`) und öffnet einen
  Detail-Screen mit den zusätzlichen, in der kompakten Liste ungenutzten
  Feldern (Start/Ziel der Fahrt, Verkehrsunternehmen, volle
  Linienbezeichnung falls vorhanden, Gleis/Steig falls vorhanden,
  Echtzeit-/Fahrplan-Status, Verspätung falls Echtzeitdaten vorliegen,
  Rollstuhl-/Fahrradmitnahme). **Der Detail-Screen ist seit 06.10.
  scrollbar** (`showDetailScreen()`, 1:1 dasselbe Hoch/Runter-Pfeil-Muster
  wie `address_search_screen.cpp::runConfirmScreen()`, Pfeile nur wenn
  tatsächlich nötig) - mit so vielen Feldern passt der Screen nicht mehr
  garantiert auf den Bildschirm, siehe CLAUDE.md "Pflicht: jedes Layout
  muss auf den Bildschirm passen".
- **"Nach:" zeigt bewusst `headsign`, NICHT `tripTo.name`** (06.10.,
  Korrektur): beide Felder können bei manchen Fahrten theoretisch
  abweichen (`headsign` ist der fahrgast-sichtbare Richtungstext,
  `tripTo.name` der GTFS-technische Fahrtendpunkt) - damit Liste (zeigt
  immer `headsign`) und Detail-Screen beim Ziel IMMER denselben Wert
  zeigen, verwendet der Detail-Screen für "Nach:" ebenfalls `headsign`.
- **Rundfahrt-Kennzeichnung** (06.10., Alex' Fund per Foto bei Linie 963
  "Geislingen (Steige)" -> "Geislingen (Steige)"): Live gegen die echte
  API verifiziert (nicht geraten) - `tripFrom.stopId`/`tripTo.name`/
  `place.name` waren für diese Fahrt tatsächlich identisch (inkl.
  gleicher Koordinaten/Gleis), also eine echte Rundfahrt (Ringlinie) im
  GTFS-Datensatz, KEIN Mapping-Fehler beim Einlesen. Erkennung:
  `tripFromName == headsign`. In diesem Fall zeigt der Detail-Screen EINE
  gemeinsame Zeile ("Rundfahrt: Geislingen (Steige)") statt zweimal
  denselben Ortsnamen unter "Von:"/"Nach:" - sonst sieht das wie ein
  Fehler aus.
- Das Ziel-Label (`headsign`) in der Liste nutzt die zentrale
  `MarqueeLabel`-Komponente (siehe "Pflicht: automatisches Scrollen bei
  zu langem Text" oben), wenn es nicht in seine Spaltenbreite passt.
- Drei GETRENNTE Fehlermeldungen (`TRANSIT_FETCH_ERROR_NETWORK`/
  `_NO_STOP`/`_NO_DEPARTURES`) statt einer generischen - der Nutzer soll
  den tatsächlichen Grund sehen.

**"Genauere Adresse"-Button:** öffnet denselben Adresseingabe-Screen wie
die Ersteinrichtung (`AddressSearchScreen::runForCoordinates()` - eine
NEUE, zweite Einstiegsfunktion neben `run()`, die NUR die bestätigten
Koordinaten zurückgibt, OHNE sie als Standort-Preset zu speichern/zu
aktivieren). Das Ergebnis landet in einer eigenen, vom allgemeinen
Toolbox-Standort komplett getrennten Datei (`Config::
SD_TRANSIT_LOCATION_FILE`, "lat,lon") - überschreibt NIEMALS
`LocationPresets`/die allgemeine Position. `transit_screen.cpp` liest bei
jedem `enter()` zuerst diese Override-Datei, fällt ohne sie auf die
allgemeine aktive Position zurück (gleiches Muster wie
`iss_radar_screen.cpp`).

## Wetter-Screen

Vierter Feature-Screen (`weather_client.cpp`/`weather_screen.cpp`/
`weather_icons.cpp`, Alex' Auftrag, 06.10.) - aktuelles Wetter,
stündliche Kurzvorhersage, Sonnenauf-/-untergang, plus eine
"Wochenübersicht"-Unterseite. Datenquelle: Open-Meteo
(`api.open-meteo.com`), schlüssellos, kostenlos für nicht-kommerzielle
Nutzung, weltweit abdeckend - live per `curl` verifiziert (nicht nur
dokumentiert angenommen):

- **EIN einziger API-Aufruf für alles** (`GET /v1/forecast?latitude=...
  &longitude=...&current=...&hourly=...&daily=...&timezone=auto
  &forecast_days=7&forecast_hours=24`) - `current`/`hourly`/`daily` in
  einer Antwort, kein zweiter Request nötig. ArduinoJson-Feldfilter
  (`DeserializationOption::Filter`) parst nur die tatsächlich benötigten
  Felder.
- **`timezone=auto` liefert alle Zeit-Strings bereits als lokale
  Wanduhrzeit** (`"YYYY-MM-DDTHH:MM"`, KEIN `"Z"`-Suffix, Sommerzeit
  automatisch berücksichtigt, live bestätigt) - GROSSER Unterschied zum
  OEPNV-Screen: hier ist KEINE eigene Zeitzonen-/Sommerzeit-Berechnung
  nötig, die Zeit-Strings werden 1:1 übernommen (nur `substring()` für
  "HH:MM" bzw. die Stunde). `weather_screen.cpp` nutzt dafür NICHT die
  Geräte-Uhrzeit (die läuft weiterhin nur in UTC, siehe OEPNV-Abschnitt),
  sondern ausschließlich `current.time` aus der API-Antwort als
  Referenzpunkt für "jetzt" in der stündlichen Vorhersage.
- **`forecast_hours` und `forecast_days` wirken UNABHÄNGIG voneinander**
  (live bestätigt) - `forecast_hours=24` begrenzt NUR das stündliche
  Array (reicht für die 3-Stunden-Leiste, hält die JSON-Antwort klein),
  `forecast_days=7` liefert trotzdem 7 vollständige Tage für die
  Wochenübersicht.
  **Bewusste Abweichung vom ursprünglichen Auftrag:** dort war
  `forecast_days=2` genannt - das würde aber keine sinnvolle
  "Wochenübersicht" ergeben (Punkt 3 des Auftrags), deshalb auf 7 Tage
  erhöht. Nicht geraten, sondern aus dem Widerspruch zwischen den beiden
  Auftragsteilen hergeleitet - falls Alex 2 Tage tatsächlich gemeint
  hatte, bitte zurückmelden.
- Automatischer Refresh alle 20 Minuten (`Config::
  WEATHER_REFETCH_INTERVAL_MS`) - Wetterdaten ändern sich nicht
  sekündlich.
- **Farbige WMO-Wettercode-Icons** (`weather_icons.cpp::draw()`) - EINE
  zentrale Zeichenfunktion für Hauptscreen, 3-Stunden-Leiste UND
  Wochenübersicht (nie unterschiedliche Darstellung an verschiedenen
  Stellen). Reine TFT_eSPI-Grundformen (Kreise/Rundrechtecke für Wolken,
  Dreiecke für Blitz, Linien für Regentropfen/Schneeflocken-Sterne),
  skaliert über einen `size`-Parameter - KEINE eingebetteten Bitmaps.
  Kategorien nach Alex' Vorgabe: 0=klar (Sonne tags/Mond nachts,
  `is_day`), 1-2=teilweise bewölkt (Sonne/Mond + kleine Wolke),
  3=bedeckt (nur Wolke, wörtlich "Sonne+Wolke BZW. Wolke" aus dem
  Auftrag), 45/48=Nebel (Wolke + horizontale Linien), 51-57=Niesel
  (wenige/kleine Tropfen), 61-67=Regen (mehr/größere Tropfen),
  71-77=Schnee (Flocken), 80-82=Regenschauer (kräftiger als Regen),
  85/86=Schneeschauer (kräftiger als Schnee), 95-99=Gewitter (dunkle
  Wolke + Blitz, lila/gelb). Die stündliche Leiste zeigt vereinfachend
  IMMER die Tag-Variante (Open-Meteo liefert `is_day` nur im
  `current`-Block, nicht pro Stunde) - die Wochenübersicht vergleicht
  stattdessen jede Stunde gegen Sonnenauf-/-untergang von `daily[0]`
  (siehe `isHourDay()`), geringfügig ungenau an den Rändern der
  ~18-Stunden-Leiste, für eine kleine Vorschau unerheblich.
- **Kompakte Zeile (Luftfeuchte/Wind/Niederschlag) UND
  Sonnenauf-/-untergang zeigen bewusst Icon + Wert OHNE Text-Label**
  (kein `%`/`km/h`/`mm`/Uhrzeit-Format braucht eine Übersetzung) - macht
  das sonst bei 4 Sprachen übliche Breiten-Risiko in diesen engen Zeilen
  von vornherein gegenstandslos, statt es nachträglich zu prüfen.
- **Windrichtung als gedrehter Pfeil statt Text** (`drawWindArrow()`,
  06.10., Alex' Auftrag) - `wind_direction_10m` ist die Richtung, AUS
  der der Wind kommt (API-Konvention), der Pfeil zeigt bewusst in die
  Richtung, in die der Wind WEHT (`fromDeg+180`) - gleiche Konvention
  wie bei den meisten Wetter-Apps (z.B. Windy). Löst das Breiten-Problem
  von Text wie "23km/h NW" komplett, ohne auf die Information zu
  verzichten.
- **UV-Index** (`uv_index_max`) steht sowohl im Hauptscreen (2x2-Raster
  aus Luftfeuchte/Wind/Niederschlag/UV, siehe unten) als auch in der
  Wochenübersicht (dritte Spalte neben Max/Min/Regenwahrscheinlichkeit,
  "UV x.x" - "UV" ist in allen 4 Sprachen dieselbe Abkürzung, braucht
  keine eigene StringId). Auf dem Hauptscreen ursprünglich NICHT
  zusätzlich in einer 3-Spalten-Zeile untergebracht (eine 4. Spalte hätte
  jede Spalte auf ~55px verengt und die Windspalte mit z.B. "23km/h"
  zuverlässig überlaufen lassen) - seit der eigene "Genauere Adresse"-
  Button entfiel (siehe "Eine allgemeine Toolbox-Position" oben), steht
  dafür genug vertikaler Platz zur Verfügung: die 4 Werte bilden jetzt
  ein 2x2-Raster (`drawMain()`) mit 2 statt 3 Spalten (~110px statt
  ~73px je Spalte) - reicht bequem auch für "23km/h" + Windrichtungs-Pfeil.
- **Windrichtung als gedrehter Pfeil statt Text** (`drawWindArrow()`,
  06.10., Alex' Auftrag) - `wind_direction_10m` ist die Richtung, AUS
  der der Wind kommt (API-Konvention), der Pfeil zeigt bewusst in die
  Richtung, in die der Wind WEHT (`fromDeg+180`) - gleiche Konvention
  wie bei den meisten Wetter-Apps (z.B. Windy). Löst das Breiten-Problem
  von Text wie "23km/h NW" komplett, ohne auf die Information zu
  verzichten.
- **Wochenübersicht** (`showWeeklyScreen()`) ist scrollbar (7 Zeilen
  passen nicht alle gleichzeitig auf den Bildschirm) - über
  `tft.setViewport()`/`resetViewport()` geklippt statt manueller
  Pro-Zeile-Sichtbarkeitsprüfung (robuster), Hoch/Runter-Pfeile nur wenn
  tatsächlich nötig, gleiches Grundprinzip wie die scrollbaren
  OEPNV-/Adress-Detail-Screens.
- **KEIN eigener Standort-Button** (06.10., Korrektur - siehe "Eine
  allgemeine Toolbox-Position" oben für die Begründung): der Wetter-
  Screen hatte zunächst, analog zum OEPNV-Screen, einen eigenen
  "Genauere Adresse"-Button samt eigener Override-Datei bekommen - wurde
  wieder entfernt. `resolveAndSetLocation()` nutzt jetzt 1:1 dasselbe
  einfache Muster wie `iss_radar_screen.cpp::enter()` (nur
  `LocationPresets::activeIndex()`, keine eigene Override-Prüfung mehr).
- Live am echten Gerät verifiziert (06.10., per temporärer Serial-
  Diagnose, danach wieder entfernt): Abruf dauert ~2,4s, liefert
  korrekt 24 stündliche + 7 tägliche Einträge mit plausiblen Werten.
- **Sonnenaufgang/-untergang: Sonnen-/Mondsichel-Symbol statt
  gleichfarbiger Pfeil-Punkte (07.10., Alex' Fund):** vorher nutzten
  beide Icons denselben Kreis+Pfeil im Akzentgrün -
  "auf einen Blick nicht unterscheidbar". Jetzt `WeatherIcons::
  drawSunIcon()`/`drawMoonIcon()` (neu als oeffentliche Huellen um die
  bereits vorhandenen, bisher nur intern genutzten `drawSun()`/
  `drawMoon()`-Formen aus dem WMO-Icon-Satz exportiert) mit FESTEN
  Farben (Gelb fuer Sonnenaufgang, `TFT_DARKGREY` fuer
  Sonnenuntergang - NICHT das Akzentgrün, literal/status-
  gebundene Farbe wie bei den AQI-Kategorien). Radius bewusst klein
  (5px statt der sonst fuer dieses Icon ueblichen Groesse) - der Platz
  bis zur bodenverankerten AQI-Kachel darunter ist knapp (nur 14px
  Gesamtabstand zwischen Sonnenzeile und Kachel), nachgerechnet statt
  geraten.

### Wetter-Screen: Luftqualität

Erweiterung des bestehenden Wetter-Screens (Alex' Auftrag, 06.10. -
AUSDRÜCKLICH kein eigener Feature-Screen, siehe `drawAqiTile()`/
`showAirQualityScreen()` in `weather_screen.cpp`). Datenquelle: Open-
Meteo Air Quality API (`air-quality-api.open-meteo.com` - ANDERE
Subdomain als das übrige Wetter, deshalb zwingend ein zweiter, eigener
HTTPS-Request), schlüssellos, kostenlos, live per curl verifiziert.

- **Läuft im selben Abruf-Zyklus wie das übrige Wetter** (Alex'
  Auftrag: "keine zusätzliche separate Abfrage-Frequenz") -
  `WeatherClient::fetch()` ruft intern zusätzlich `fetchAirQuality()`
  auf, direkt im Anschluss an den Haupt-Request. Teilausfall-tolerant:
  schlägt NUR die Luftqualität fehl, bleibt der Rest der Vorhersage
  trotzdem gültig (`airQuality.valid=false`, gleiches Prinzip wie
  `SpaceWeatherClient::fetchNoaaData` beim entfernten Weltraumwetter-
  Screen).
- **ANDERS als bei Jolpica/Formel-1**: `position`/`points`/`wins` waren
  dort JSON-Strings - hier bei Open-Meteo sind alle Felder (inkl. der
  AQI-Werte) live bestätigt native JSON-Zahlen, der normale
  ArduinoJson-"|"-Operator funktioniert also direkt ohne manuelle
  String-Konvertierung.
- **Kachel ist bodenverankert, nicht an die tatsächliche Endposition
  des darüberliegenden Inhalts gekoppelt** (`AQI_ROW_TOP = WEEK_BTN_Y -
  4 - AQI_ROW_H`) - robuster als ein Anschluss an die Sonnenauf-/
  -untergangs-Zeile, bleibt auch bei künftigen Änderungen am Inhalt
  darüber automatisch an der richtigen Stelle. Für den nötigen Platz
  wurden zwei bestehende Abstände leicht gestrafft (row2Cy+14→+8,
  stripPercentY+12→+6, je 6px) - dabei wird kein bestehendes Element
  kleiner, unleserlicher oder verdrängt (Alex' Vorgabe), reiner
  Abstands-Feinschliff.
- **Farbskala exakt nach Alex' Vorgabe**, literal/status-gebunden
  (NICHT das Akzentgrün) - gleiche Begründung wie bei Flightradars
  Höhenfarben/Kp-Index/Rakete-Hold-Fail: trägt eine eigene
  Gesundheits-/Status-Bedeutung, keine Deko-Farbe.
- **Kategoriewort läuft als Laufschrift** (`aqiCategoryMarquee`) - "ich
  will kein englisches Wort sehen"/Breiten-Pflicht gilt genauso für die
  eigene Sprache: "Extrêmement mauvaise" (FR) passt NICHT in die
  verfügbare Spaltenbreite, andere Sprachen/Kategorien schon - deshalb
  grundsätzlich über `MarqueeLabel` statt nur für den einen bekannten
  Problemfall eine Ausnahme zu bauen.
- Detail-Screen zeigt europäischen UND amerikanischen AQI, je
  Teil-Index (PM2.5/PM10/NO2/O3/SO2) den Rohmesswert mit Einheit UND
  eine laienverständliche Ein-Satz-Erklärung, zusätzlich CO/Feinstaub
  (Sand)/Ammoniak als reine Rohmesswerte (Open-Meteo liefert dafür
  keinen eigenen EAQI-Teilindex) - scrollbar, gleiches Grundmuster wie
  `rocket_screen.cpp`/`f1_screen.cpp` (erst vermessen, dann zeichnen).
- Live am echten Gerät verifiziert (06.10., per temporärer Serial-
  Diagnose, danach wieder entfernt): Abruf liefert korrekt
  `europeanAqi=42` (fällt korrekt in die "Mittelmäßig"/gelb-Kategorie)
  und alle übrigen Schadstoff-/Teilindex-Werte mit plausiblen Zahlen.
- **Layout-Korrektur (07.10., Alex' Fund per Live-Foto):** die Zahl lief
  bei automatischer `drawString()`+`ML_DATUM`-Vertikalzentrierung unten
  über den Kachelrahmen hinaus (die tatsächliche Glyphenhöhe dieses
  Fonts bei Size 2 ist offenbar größer als die für die Zentrierung
  angenommenen ~16-18px) - behoben durch denselben bewährten
  Baseline-Stil (`setCursor()`+`print()`, Baseline nahe der
  Kachel-Unterkante) wie die bereits unauffällige
  Haupttemperaturanzeige weiter oben im selben Screen. Zusätzlich fehlte
  jede Einordnung, wofür die Zahl steht ("niemand weiß, was die 42 da zu
  suchen hat") - eine "AQI"-Beschriftung (wie "UV" an anderer Stelle im
  selben Screen: ein international gebräuchliches, nicht zu
  übersetzendes Kürzel) steht jetzt direkt davor. Das gemeldete
  Flackern des Kategorieworts war ein eigener, tieferliegender Bug in
  der gemeinsam genutzten `MarqueeLabel`-Komponente selbst, siehe
  "Pflicht: automatisches Scrollen bei zu langem Text" → "Marquee-
  Flimmer-Fix".

## Formel 1

Vierter Feature-Screen (`f1_client.cpp`/`f1_screen.cpp`, Alex' Auftrag,
06.10.) - nächstes Rennwochenende mit Countdown + Top-3-Fahrerwertung,
oder - während tatsächlich eine Session läuft - eine Live-Rangliste.
Zwei Datenquellen, beide kostenlos/schlüssellos, live per curl
verifiziert:

- **Jolpica-F1** (`api.jolpi.ca`, Ergast-Nachfolger) für Kalender
  (nächstes Rennwochenende inkl. aller Sessionzeiten), Fahrer-/
  Konstrukteurswertung und Ergebnis des letzten Rennens - 4 einzelne
  Endpunkte, alle 15 Minuten gebündelt abgerufen (`Config::
  F1_SEASON_REFETCH_INTERVAL_MS`), tolerant gegenüber Teilausfällen
  (gleiches Prinzip wie `fetchNoaaData()` beim inzwischen entfernten
  Weltraumwetter-Screen).
- **OpenF1** (`api.openf1.org`) für echte Live-Daten (Fahrerliste,
  Positionen, Abstände) während einer laufenden Session.

**Wichtige, live entdeckte Besonderheit: `position`/`points`/`wins` sind
bei Jolpica JSON-STRINGS, keine Zahlen** (z.B. `"points":"320"`, live
bestätigt) - `f1_client.cpp` parst diese deshalb explizit über
`String::toInt()`/`toFloat()` statt ArduinoJsons `|`-Operator zu
vertrauen (dessen String→Zahl-Koerzierung in dieser Version nicht
sicher verifiziert war).

**OpenF1 "position"/"intervals" für eine komplette Session sind live
bestätigt teils MEHRERE MB groß** (eine 2h-Session: 21.448 `intervals`-
Einträge / 3 MB, 880 `position`-Einträge / 101 KB) - dieselbe
Fehlerklasse wie das bereits dokumentierte `xrays-6-hour.json`-Problem
beim entfernten Weltraumwetter-Screen. Ein kompletter Session-Abruf ist
deshalb an KEINER Stelle im Code vorgesehen - `f1_client.cpp` liefert
nur Zeitfenster-Ausschnitte (`date>=`-Query-Parameter), `net_task.cpp`
führt diese über mehrere Abruf-Zyklen hinweg zu einer persistenten
Pro-Fahrer-Tabelle zusammen (Positionen/Abstände bleiben erhalten, bis
ein neuerer Wert für denselben Fahrer eintrifft). Fenstergrößen live
gemessen und bewusst geknappt:
`Config::F1_LIVE_POSITION_LOOKBACK_SEC` (180s, "position" ändert sich
nur bei echten Überholmanövern, bleibt dadurch auch bei 180s winzig) und
`Config::F1_LIVE_INTERVAL_LOOKBACK_SEC` (35s statt ursprünglich 60s -
ein Live-Test direkt um das tatsächliche Sessionende herum zeigte, dass
ein 180s-Fenster dort ~111 KB/786 Einträge lieferte, zu nah an der
bekannten Problemgrenze; ein 30s-Fenster blieb bei vergleichbaren Live-
Messungen durchgehend bei ca. 14 KB). Ein einzelner besonders dichter
Moment (Start, Safety-Car-Restart, Zieldurchfahrt) kann das Fenster
trotzdem gelegentlich sprengen - wird vom ohnehin tolerant behandelten
Teilausfall abgefangen (nächster Zyklus 27s später versucht es erneut),
bewusst als bekannte Grenze dokumentiert statt stillschweigend als
"sicher" angenommen.

**OpenF1-Rate-Limit 3 Anfragen/Sekunde** (eigene Fehlermeldung der API,
live bestätigt) - zwischen mehreren OpenF1-Aufrufen im selben
Abruf-Zyklus (Fahrerliste bei neuer Session, Positionen, Abstände)
liegen jeweils 400ms Pause.

**Kein roher englischer API-Text im UI** (Alex' ausdrücklicher Auftrag
vom selben Tag, direkt im Anschluss an den Rückbau des Weltraumwetter-
Screens - "ich will kein englisches Wort sehen", siehe auch die
gespeicherte Erinnerung dazu): Sessionnamen (OpenF1 `session_name`, z.B.
"Practice 1"/"Race") und Ergebnis-Status (Ergast `status`, z.B.
"Retired"/"Accident"/"+1 Lap") werden NIE roh angezeigt, sondern über
feste Übersetzungstabellen (`sessionNameStringId()`/
`formatResultStatus()` in `f1_screen.cpp`) - anders als beim
Raketenstart-Screen (dort fällt ein unbekannter `status.abbrev`-Wert auf
den rohen `status.name`-Text zurück) gibt es hier bewusst KEINEN
Rohtext-Fallback mehr: ein nicht abgedeckter Status landet beim
übersetzten Sammelbegriff "Ausfall"/"DNF"/"Abandono"/"Non terminé".
Fahrer-/Teamnamen, Streckennamen und Ortsnamen bleiben unübersetzt
(Eigennamen, keine Prosa).

**Lokale Zeit des GERÄTS statt der jeweiligen Streckenzeitzone**
(Interpretation von Alex' Auftrag "lokale Zeitzone des Geräts, nicht die
EU-Pauschalregel" - anders als beim OEPNV-Screen gibt es hier keine von
einer API gelieferte Ziel-Zeitzone, nur die eigene Geräteposition):
`formatDeviceLocalDateTime()` in `f1_screen.cpp` nutzt dieselbe
DST-Berechnung wie der OEPNV-Screen (`isEuDst()`, 1:1 dupliziert), aber
mit einer FESTEN Mitteleuropa-Basis (+1h Winterzeit) statt einer
Zeitzonen-Tabelle, da keine Ziel-Zeitzone zum Nachschlagen vorliegt -
deckt Alex' eigenen Standort (Deutschland) korrekt ab. Falls das Gerät
dauerhaft außerhalb Mitteleuropas betrieben werden sollte, wäre eine
echte Standort→Zeitzonen-Zuordnung nötig (aktuell nicht umgesetzt) -
bitte zurückmelden, falls diese Deutung nicht gemeint war.

**Marquee auf dem Hauptscreen, `layoutWrapped()` im Detail-Screen**
(bewusste Aufteilung, nicht 1:1 Alex' Formulierung "Laufschrift für alle
Namen/Teambezeichnungen" wörtlich auf JEDE Textstelle angewandt): Der
kompakte Hauptscreen (Rennname, Ort, Top-3-Zeilen, Session-Name) nutzt
`MarqueeLabel`, weil dort feste Zeilenpositionen/-höhen gelten und ein
Zeilenumbruch das Layout sprengen würde. Der scrollbare Detail-Screen
(bis zu 26 Fahrer-/Konstrukteurs-/Ergebniszeilen) nutzt stattdessen
`layoutWrapped()` wie die übrigen Detail-Screens (Rakete/OEPNV/Wetter) -
Dutzende gleichzeitige `MarqueeLabel::State`-Instanzen für eine lange,
frei scrollende Liste wären unverhältnismäßig komplex gewesen, während
`layoutWrapped()` dieselbe Pixel-genaue Nie-abschneiden-Garantie ohne
diesen Mehraufwand liefert.

**Live-Rangliste UND Saison-Daten immer beide im Detail-Screen** (Alex'
Spezifikation listet die 4 Detail-Punkte Fahrer-/Konstrukteurswertung/
letztes Ergebnis/Sessionzeiten separat von der Live-Anforderung "Rest im
Detailscreen" - Interpretation: beide Teile gehören in DENSELBEN
scrollbaren Screen, Live-Rangliste zuerst falls gerade eine Session
läuft, danach immer die vollständige Saison-Übersicht, statt einer
Fallunterscheidung, die bei "Mehr" potenziell Inhalt weglässt).

- Live am echten Gerät verifiziert (06.10., per temporärer Serial-
  Diagnose, danach wieder entfernt): Saison-Abruf liefert korrekt
  nächstes Rennen + 23 Fahrer-/11 Konstrukteurs-Wertungen + 22
  Ergebniszeilen des letzten Rennens; Live-Pfad (testweise erzwungen, da
  die aktuelle Testsession zum Zeitpunkt der Prüfung bereits beendet
  war) liefert korrekt 22 Fahrer samt Position/Team/Teamfarbe (RGB565-
  Umrechnung stichprobenartig von Hand nachgerechnet, stimmt exakt) und
  Positions-/Intervall-Daten innerhalb des Zeitfensters.

**Flimmer-Bugfix (06.10., noch am selben Tag gefunden/behoben):**
Betroffen war AUSSCHLIESSLICH der Hauptbildschirm im Nicht-Live-Zustand
(Fall B, "Rennstart in"-Countdown) - `update()` rief dort bei JEDEM
Sekundentick ein volles `drawNextRace()` auf, das per
`clearContentArea()` den KOMPLETTEN Inhaltsbereich (Rennname, Ort,
Countdown, Top-3-Fahrerwertung) loeschte und neu zeichnete, statt nur
die sich tatsaechlich aendernde Uhrzeit. Nebenwirkung: die
Marquee-Animationen (Rennname/Ort/Top-3) wurden dabei bei jedem Tick
per `MarqueeLabel::reset()` neu gestartet und liefen dadurch faktisch
nie sichtbar durch. Fix: ein neuer `drawCountdownBlock()` zeichnet NUR
noch den Countdown selbst in einer festen, kleinen Bandflaeche
(`COUNTDOWN_AREA_TOP`/`COUNTDOWN_AREA_H`, analog zu `rocket_screen.cpp`s
laengst bewaehrtem `clearCountdownArea()`-Muster) - diese Flaeche wird
VOR jedem Neuzeichnen immer vollstaendig (volle Breite, nicht nur die
schmalere neue Ziffernbreite) mit der Hintergrundfarbe geloescht, auch
das vom Live-Foto gemeldete Problem mit stehenbleibenden Ziffernresten
loest sich dadurch vollstaendig auf. `animateNextRace()` (die
Marquee-Lauftexte) wird jetzt JEDEN Tick unabhaengig vom Countdown
aufgerufen statt nur im (vorher nie erreichten) else-Zweig.

**Live-Rundenzeiten (06.10.):** zusaetzlich zu Position/Abstand zeigt
die Live-Rangliste jetzt die letzte abgeschlossene Rundenzeit pro
Fahrer (`/v1/laps`, Feld `lap_duration`, Format "M:SS.sss" ueber
`formatLapTime()`). Gleiches Zeitfenster-Prinzip wie Position/
Intervalle (`Config::F1_LIVE_LAP_LOOKBACK_SEC=180s`) - ein kompletter
Abruf ist live bestaetigt 527 KB/1145 Eintraege gross (dieselbe
Groessenordnung wie die bereits dokumentierten Positions-/Intervall-
Probleme), ein 180s-Fenster bleibt bei ca. 26 KB. Laeuft im SELBEN
Live-Poll-Zyklus wie Position/Intervalle (zusaetzlicher dritter
OpenF1-Aufruf, ebenfalls mit 400ms Abstand wegen des 3-Anfragen/
Sekunde-Limits). WICHTIG: dieser Endpunkt filtert nach `date_start`,
NICHT nach `date` wie "position"/"intervals" - live bestaetigt, beim
Implementieren leicht zu uebersehen. Eine Runde mit `lap_duration:null`
(z.B. Boxenein-/-ausfahrt) wird uebersprungen statt einer sinnlosen
"0:00.000"-Anzeige. Live am echten Geraet verifiziert (testweise
erzwungener Live-Pfad, Zeitfenster bewusst nah an den tatsaechlich
letzten verfuegbaren Datenpunkt der Testsession gelegt statt an "jetzt"
- Letzteres ueberlappt bei einer laengst beendeten Testsession nie mit
echten Daten): korrekt 19 Rundenzeit-Eintraege im Fenster, Beispielwert
98,220s fuer Fahrzeug Nr. 3 korrekt uebernommen.

## Krypto-Ticker

Fünfter und letzter Feature-Screen (`crypto_client.cpp`/`crypto_screen.cpp`,
Alex' Auftrag, 07.10.) - Bitcoin/Ethereum-Kurse mit 24h-Änderung, dazu
zwei Bonuskacheln (Bitcoin-Mempool, Fear&Greed-Index). Drei voneinander
unabhängige, kostenlose/schlüssellose APIs, alle live per Serial-Diagnose
gegen die echten Endpunkte verifiziert (danach wieder entfernt):

- **CoinGecko** (`api.coingecko.com`) - zwei Endpunkte: `/simple/price`
  für die BTC/ETH-Hauptkacheln (liefert Preis + `usd_24h_change` als
  native JSON-Zahlen, ANDERS als bei Jolpica/Formel-1 - kein
  `String::toInt()`/`toFloat()`-Umweg nötig) und `/coins/markets`
  (Top-10 nach Marktkapitalisierung, Top-Level-Antwort ist ein JSON-
  ARRAY, kein in ein Objekt verpacktes Ergebnis wie bei Launch Library/
  Jolpica) für die "Weitere Coins"-Liste.
- **mempool.space** - zwei Endpunkte: `/api/v1/fees/recommended` (JSON,
  `fastestFee`) und `/api/blocks/tip/height`, das die Blockhöhe als
  **reinen Text** liefert (z.B. `"866543"`, KEIN JSON) - live bestätigt,
  deshalb keine `deserializeJson()` dafür, sondern direkte
  `String::toInt()`-Konvertierung des rohen Bodys.
- **alternative.me** (`/fng/?limit=1`) - Fear&Greed-Index. `value` liegt
  als JSON-STRING vor (`"34"`, nicht `34`), live bestätigt - gleiches
  Muster wie bei Jolpica/Formel-1, über `String::toInt()` geparst.
  Die 5 festen `value_classification`-Werte sind in einer eigenen
  Übersetzungstabelle (`crypto_screen.cpp::fngClassificationStringId()`)
  hinterlegt, gleiches Prinzip wie die F1-/Rocket-Status-Tabellen -
  Rückfall auf den rohen API-Text bei einem unbekannten Wert.

**Zwei getrennte Abruf-Intervalle** (Alex' Auftrag): btc/eth/markets/
mempool teilen sich EIN Intervall (`Config::CRYPTO_REFETCH_INTERVAL_MS`,
2,5 Minuten), der Fear&Greed-Index hat ein EIGENES, deutlich selteneres
Intervall (`Config::CRYPTO_FNG_REFETCH_INTERVAL_MS`, 4 Stunden - ändert
sich laut API ohnehin nur einmal täglich). Beide Datensätze leben im
selben `NetTask::CryptoData`-Struct/Mutex, aber mit zwei unabhängigen
Versionszählern (`cryptoVersion()`/`cryptoFngVersion()`).

**Teilausfall-Toleranz** (Alex' Auftrag: "jede der drei Datenquellen
soll unabhängig voneinander fehlschlagen können") - sogar eine Stufe
feiner als verlangt: `fetchMainPrices()`/`fetchMarkets()`/
`fetchMempool()` werden in `net_task.cpp` EINZELN aufgerufen und
schlagen unabhängig voneinander fehl, `cryptoVersion()` erhöht sich
bereits, wenn AUCH NUR EINE davon erfolgreich war - die jeweils anderen
Felder behalten dann ihren zuletzt bekannten Wert (gleiches Prinzip wie
`AirQuality.valid` beim Wetter-Screen). Jede der vier Kacheln
(`drawCoinTile()`/`drawMempoolTile()`/`drawFngTile()`) prüft außerdem
ihr eigenes `.valid`-Feld und zeigt bei fehlenden Daten unabhängig von
den anderen Kacheln einen eigenen "Lädt..."-Platzhalter.

**Layout:** zwei große Kacheln (Bitcoin/Ethereum, Preis groß in Weiß,
24h-Änderung literal Grün/Rot - weltweit übliche Krypto-App-Konvention,
NICHT das Akzentgrün, wie bei AQI-Kategorien/Hold-Fail eine eigene
Bedeutung). Darunter zwei kleinere Bonuskacheln nebeneinander: "Mempool"
(Blockhöhe/Gebühr, bewusst OHNE eigene übersetzte Wort-Label für die
Werte selbst - nur `#`-/`~`-Präfix + literale Einheit `sat/vB`, da ein
kombiniertes "Label: Wert" auf einer Zeile bei 106px Kachelbreite in
mind. einer Sprache überlaufen würde, nachgerechnet) und "F&G" (bewusst
als literales Kürzel statt ausgeschriebenem Label - spart den Platz für
die darunterstehende Klassifikation, die z.B. als "Codicia Extrema" (ES)
oder "Extreme Angst" (DE) bei 106px Kachelbreite nicht zuverlässig auf
eine Zeile passt - deshalb über `layoutWrapped()` mit bis zu 2 Zeilen
gezeichnet statt eine ungeprüfte Einzeiler-Annahme zu riskieren, siehe
CLAUDE.md "Pflichtprüfung: Textbreite/-höhe in ALLEN 4 Sprachen").
Fear&Greed-Farbskala Rot (Extreme Angst) bis Grün (Extreme Gier) über
5 feste, benannte `TFT_eSPI`-Konstanten (`TFT_RED`/`TFT_ORANGE`/
`TFT_YELLOW`/`TFT_GREENYELLOW`/`TFT_GREEN`) - keine geratenen Hex-Werte.

**"Weitere Coins"** (Tap auf die kombinierte Bitcoin+Ethereum-Fläche,
`mainAreaRect()`) zeigt die übrigen bis zu 8 Top-10-Coins (BTC/ETH beim
Anzeigen über `id=="bitcoin"`/`"ethereum"` ausgeschlossen) als
einzeilige Zeilen (Name per `MarqueeLabel` links, Preis/Änderung rechts)
statt der zweizeiligen Boxen wie bei "Weitere Starts" im Rocket-Screen -
8 Zeilen passen dadurch ohne eigene Scroll-Infrastruktur auf den
Bildschirm (nachgerechnet: `ROW_H=25`+`ROW_GAP=3` × 8 = 224px, bleibt
unter den ca. 230px verfügbarer Listenhöhe). Gleiches Zurück-Button-
Muster wie beim Rocket-Screen.

**Stack-Vorsicht beachtet** (siehe CLAUDE.md "Raketenstart-Countdown"
für die dort live reproduzierte Stack-Canary-Crash-Lektion): das lokale
`CryptoClient::CoinMarket markets[Config::CRYPTO_MAX_MARKETS]`-Array in
`net_task.cpp` (10 Elemente, je 3 `String`-Felder) liegt in derselben
Größenordnung wie das damals abstürzende `Launch[3]`-Array - bewusst VOR
dem Flashen dagegen abgewogen, dass `taskFunc()` bereits deutlich
größere, dauerhaft koexistierende lokale Arrays enthält (z.B. F1s
`ivUpdates[64]`), und zusätzlich live per Serial-Diagnose über 70s ohne
Absturz bestätigt.

Preis-Formatierung: eigener kleiner `formatThousands()`-Helfer (es gab
dafür noch keinen im Projekt) für Tausendertrennzeichen bei großen
USD-Beträgen (z.B. "$65,432"), `formatCoinPrice()` wechselt bei Preisen
unter 1 USD automatisch auf 4 Nachkommastellen statt Tausendertrennung
(viele Altcoins kosten Bruchteile eines Cents).

### Fix (07.10.): Rahmenlinie + Zahlen-Überlappung in den Kacheln, F&G-Label lokalisiert

Alex' Fund, direkt nach dem ersten Live-Test am Gerät - drei Probleme:

1. **Bitcoin-/Ethereum-Kachel: untere Rahmenlinie teilweise überdeckt.**
   Exakt derselbe Fehler wie zuvor bei der AQI-Zahl im Wetter-Screen:
   `drawCoinTile()` zeichnete Preis (Größe 2) und 24h-Änderung mit
   OPAKEM Hintergrund (`setTextColor(farbe, TFT_BLACK)`) nah an der
   Box-Unterkante - TFT_eSPIs automatisches Löschrechteck dafür reichte
   bis in die gerade erst gezeichnete Rahmenlinie hinein und riss ein
   Stück heraus. Fix: transparenter Text (`setTextColor(farbe)`, kein
   zweites Argument) - unproblematisch, da die Kachel bei JEDEM Aufruf
   ohnehin komplett schwarz vorgefüllt wird, kein manuelles Nach-Löschen
   nötig.
2. **F&G-Kachel: Zahl (z.B. "71") und Klassifikation ("Gier")
   überlappten unleserlich.** Dieselbe Baseline/Ascent-Fehlerklasse wie
   zuvor bei ISS-Radar/Rocket-Screen (siehe "WICHTIGSTE FALLE" oben und
   "Raketenstart-Countdown" weiter oben) - der bisherige 14px-Abstand
   zwischen den beiden Text-Baselines (Zahl bei Größe 2, Klassifikation
   bei Größe 1 direkt darunter) ließ der Klassifikationszeile keinerlei
   Reserve für ihren eigenen Ascent. Fix: Klassifikation jetzt bei
   `y+58` statt `y+50` (8px mehr Abstand) PLUS derselbe
   Transparenz-Fix wie bei Punkt 1 (reduziert zusätzlich das Risiko
   durch ein zu großzügig bemessenes Löschrechteck der Zahl).
3. **"F&G"-Kachel-Titel war fest Englisch**, unabhängig von der
   UI-Sprache - neue StringId `CRYPTO_FNG_TILE_LABEL`, sprachabhängiges
   3-Zeichen-Kürzel (DE "A&G" für Angst&Gier, EN "F&G" für Fear&Greed,
   ES "M&C" für Miedo&Codicia, FR "P&A" für Peur&Avidité) statt des
   hart codierten Strings.

**Visuelle Verifikation am Gerät NICHT von Karl durchgeführt** - Karl
hat keinen Kamera-/Bildschirmzugriff auf das physische Gerät, kann die
drei Fixes also nur anhand des Codes (gleiche, bereits anderswo
bewährte Fix-Muster) begründen, nicht tatsächlich am Display bestätigen.
Alex wurde nach dem Flashen gebeten, selbst am Gerät nachzuschauen.

### Standortabhängige Währung statt fest USD (07.10.)

Alex' Auftrag: Preise im Krypto-Ticker in der Landeswährung statt immer
in USD anzeigen, abgeleitet aus dem gespeicherten Standort.

- **`LocationPresets::countryCode()`/`setCountryCode()`** (`location_
  presets.h/.cpp`) - neues, GLOBALES Zusatzfeld (nicht pro Preset, analog
  zu `Source` - es gibt ohnehin nur "diese EINE, allgemeine Toolbox-
  Position", siehe "Eine allgemeine Toolbox-Position" weiter unten),
  2-Buchstaben-ISO-Code, persistiert als drittes optionales
  Zusatzfeld `,country=..` in derselben ersten Zeile der `locations.txt`
  wie `source=` - exakt nach demselben Rückwärtskompatibilitäts-Prinzip
  (fehlt das Feld in einer älteren/fremden Datei, bleibt es leer, kein
  Fehler). Wird NICHT automatisch gespeichert, der jeweilige Aufrufer
  committed wie gehabt selbst.
- **Zwei Quellen setzen das Feld**: `AddressSearchScreen::run()` (NICHT
  `runForCoordinates()` - das OEPNV-"Genauere Adresse"-Ergebnis darf die
  allgemeine Toolbox-Position/-Währung nicht beeinflussen, gleiche
  Begründung wie beim dortigen Nicht-Speichern der Koordinaten) liest
  Nominatims `address.country_code` (braucht dafür zusätzlich
  `&addressdetails=1` im Request, live per curl bestätigt: liefert z.B.
  `"country_code": "de"`, klein geschrieben). `first_run_location_
  screen.cpp::fetchIpLocation()` (WLAN-Fallback) liest ip-api.com's
  `countryCode`-Feld (live bestätigt: GROSS geschrieben, z.B. `"DE"` -
  `setCountryCode()` normalisiert selbst auf klein).
- **`currency.h/.cpp`** (neues, kleines eigenständiges Modul) - feste
  Länder→Währung-Tabelle (Alex' Auftrag: "nach eigenem Ermessen die
  gängigsten Länder abdecken", ca. 40 Länder: ganze Eurozone + die
  gängigsten übrigen), jede gelistete Währung live gegen CoinGeckos
  `/simple/supported_vs_currencies` geprüft (07.10.) - nur tatsächlich
  unterstützte Codes gelistet. Unbekanntes/nicht abgedecktes Land → `usd`.
- **Anzeige-Präfix bewusst IMMER der 3-Buchstaben-ISO-Code** (z.B.
  `"EUR "`), NIE ein Währungssymbol - "€" (U+20AC) liegt außerhalb des
  Zeichensatzes unseres eigenen Fonts (`ui_font.h`: `0x0020`-`0x015F`)
  und würde als Lücken-/"Tofu"-Box dargestellt, EXAKT die Fehlerklasse,
  die gerade erst im OEPNV-Screen gefunden und behoben wurde (siehe
  "Pflicht: automatisches Scrollen bei zu langem Text" oben) - bewusst
  nicht wiederholt. Der 3-Buchstaben-Code ist für JEDE unterstützte
  Währung garantiert im Zeichensatz enthalten und eindeutig (keine
  Verwechslung zwischen z.B. CAD-$/USD-$).
- **CoinGecko benennt die Preisfelder selbst nach der Währung** (z.B.
  `"eur"`/`"eur_24h_change"` statt `"usd"`/`"usd_24h_change"` - live per
  curl verifiziert) - `crypto_client.cpp::fetchMainPrices()` filtert/
  liest deshalb mit dynamischen Feldnamen statt fest "usd". Der
  `/coins/markets`-Endpunkt braucht das NICHT (Feldnamen dort fix,
  unabhängig von `vs_currency`).
- **NetTask liest NIEMALS selbst LocationPresets** (Core-0-Grundregel,
  siehe net_task.h) - `CryptoScreen::enter()` ermittelt die Währung
  einmal pro Bildschirm-Eintritt und reicht sie über das neue
  `NetTask::setCryptoCurrency()` weiter, exakt nach demselben Prinzip wie
  `setWeatherLocation()`/`setTransitLocation()`. Welche Währung ein
  konkreter Datensatz tatsächlich hatte, steht zusätzlich in
  `NetTask::CryptoData::vsCurrency` - der Screen muss dafür nicht
  nochmal selbst nachfragen, Preis und Symbol kommen garantiert aus
  demselben Abruf-Zyklus.
- `CryptoClient::MainCoin::priceUsd`/`CoinMarket::priceUsd` bewusst NICHT
  umbenannt trotz jetzt möglicher anderer Währungen - geringinvasive
  Änderung, gleiches Prinzip wie `windSpeedKmh` in `weather_client.h`
  (enthält bei Imperial tatsächlich mph).
- Live am echten Gerät verifiziert (07.10., per temporärer Serial-
  Diagnose, danach wieder entfernt): die komplette Kette Land→Währung→
  API-Aufruf→Anzeige-Präfix lief fehlerfrei durch (u.a. `de→eur`,
  `gb→gbp`, `jp→jpy`, Groß-/Kleinschreibung normalisiert, unbekanntes
  Land→`usd`-Fallback), echter Preisabruf mit dem ermittelten
  Währungscode erfolgreich, kein Absturz. Das Testgerät selbst hatte
  noch keinen gespeicherten Ländercode (Standort vor diesem Feature
  eingerichtet) - fiel dadurch korrekt auf USD zurück, wie vorgesehen.

**ENTFERNT (13.10., Alex' Auftrag):** Die komplette standortabhängige
Auto-Zuordnung oben (`Currency::vsCurrencyForCountry()`, die Länder→
Währung-Tabelle, der "Automatisch (Standort)"-Listeneintrag im
Währungs-Auswahlscreen) wurde wieder entfernt - **nicht weil die Kette
selbst fehlerhaft war** (sie funktionierte beim Testen oben einwandfrei),
sondern weil ein nachgelagerter, am 12.10. hinzugefügter Mechanismus
(manuelle Überschreibung) in der Praxis zu Verwirrung führte: schlug
EIN Abruf nach einem Währungswechsel fehl (beobachtet: CoinGecko-
Rate-Limit bei schnell hintereinander gewechselten Währungen beim
Testen), blieb die Anzeige stillschweigend auf der letzten
ERFOLGREICHEN Währung stehen, was wie "meine Wahl/Automatisch wird
ignoriert" wirkte. Alex' Entscheidung: lieber einfach und vorhersagbar
(jede*r stellt die Währung manuell ein, fester Standard `usd`) als die
zusätzliche Automatik-Komplexität beizubehalten. `currency.h/.cpp`
besteht seitdem nur noch aus `currentCurrency()`/`setCurrentCurrency()`
(ersetzt das vorherige `manualOverride()`/`setManualOverride()`) +
`displayPrefix()` + der Liste unterstützter Codes.
`LocationPresets::countryCode()` bleibt bestehen (wird weiterhin für
die länderspezifischen Feiertage im Kalender-Screen gebraucht, siehe
`holiday_client.cpp`) - nur der Krypto-Ticker nutzt ihn nicht mehr.

### Weißer Text-Rest neben der F&G-Kachel (07.10., Alex' Fund)

Analyse (kein Foto vorhanden, nur Textbeschreibung "am rechten
Bildschirmrand, neben der A&G-Kachel") - `crypto_screen.cpp` komplett
durchgegangen: nirgends wird außerhalb der vorgesehenen Kachel-Flächen
gezeichnet (alle Tile-Rects enden bei `x=230`, 10px Rand bis zum
Bildschirmrand, wie überall im Projekt üblich), UND jeder Bildschirm-
Wechsel läuft über `drawChrome()` → `tft.fillScreen(TFT_BLACK)`, ein
Rest eines VORHERIGEN Screens beim Wechsel zu Krypto ist dadurch nicht
plausibel. **Eine echte Inkonsistenz gefunden und behoben:**
`drawMempoolTile()` war die einzige der vier Kacheln, die ihren Text
noch OPAK zeichnete (`setTextColor(farbe, TFT_BLACK)`), während die
anderen drei beim vorherigen Fix-Durchgang (siehe oben) bereits auf
transparenten Text umgestellt wurden - als plausibler Mitverursacher
jetzt ebenfalls korrigiert, gleiche Begründung (die Kachel wird bei
JEDEM Aufruf ohnehin komplett schwarz vorgefüllt). **Nicht abschließend
bewiesen, da kein Foto vorlag** - falls der Rest weiterhin auftritt,
bitte ein Foto nachreichen.

## Erdbeben weltweit

Sechster und letzter Feature-Screen (`earthquake_client.cpp`/
`earthquake_screen.cpp`, Alex' Auftrag, 07.10.) - zeigt das aktuell
stärkste Erdbeben weltweit ab Magnitude 4,5 (US Geological Survey).

**Datenquelle bewusst ANDERS als ursprünglich angefragt** (Alex'
Auftrag nannte die beiden statischen USGS-Feeds `4.5_day.geojson` +
`4.5_week.geojson` als Fallback) - vor dem Bauen live geprüft
(`curl`, 07.10.): der Day-Feed ist mit 13KB/18 Einträgen unkritisch,
der Week-Fallback aber bis zu ~80KB/116 Einträge groß - dieselbe
Risikoklasse wie das bereits dokumentierte Speicherproblem bei 100km
Radius (siehe "Bekannte Probleme" unten). Per `AskUserQuestion` mit
Alex abgestimmt und stattdessen die flexible USGS-**fdsnws-Query-API**
gewählt: `Config::EARTHQUAKE_API_URL` = `.../fdsnws/event/1/query?
format=geojson&minmagnitude=4.5&orderby=magnitude&limit=20` - EINE
parametrisierte Anfrage statt zwei Endpunkten, live auf ~15KB/20
Einträge begrenzt (unabhängig vom Zeitfenster, das bei `limit=20` ohne
explizites `starttime` automatisch ca. 3-4 Wochen zurückreicht - live
geprüft), kein eigener Fallback-Request mehr nötig. **Bonus:**
`orderby=magnitude` liefert die Liste bereits serverseitig absteigend
nach Magnitude sortiert - kein eigenes Sortieren im Client nötig
(anders als ursprünglich mit den unsortierten Feeds geplant).

**Felder** (`earthquake_client.cpp`, live per `curl` gegen die echte
API geprüft): `properties.mag` (Zahl), `properties.place` (String),
`properties.time` (Unix-**Millisekunden**, nicht Sekunden - live
bestätigt z.B. `1790371383309` - als `double` geparst statt über
ArduinoJsons `|`-Operator mit `int`-Default, da das weit außerhalb des
int32-Bereichs liegt und sonst überlaufen/abschneiden würde),
`properties.tsunami` (0/1), `geometry.coordinates[2]` (Tiefe in km,
`coordinates[0]`/`[1]` sind Länge/Breite, ungenutzt).

**Hauptkachel** (`earthquake_screen.cpp::drawMainTile()`): zweistufiger
Mess-dann-Zeichnen-Ablauf (`drawMainTileContent(..., draw)`, einmal mit
`draw=false` nur zur Höhenmessung, dann die Box in exakt dieser Höhe
gezeichnet, dann nochmal mit `draw=true`) - gleiches Muster wie
`rocket_screen.cpp::showLaunchDetailScreen()`, nötig weil die Höhe von
der variabel langen Ortsbeschreibung UND davon abhängt, ob eine
Tsunami-Zeile dazukommt (CLAUDE.md "Pflicht: jedes Layout muss auf den
Bildschirm passen" - keine geratene feste Höhe). Magnitude groß und
literal/status-gebunden farblich abgestuft (Alex' exakte Vorgabe: Grün
<5.0/Gelb 5.0-5.9/Orange 6.0-6.9/Rot ab 7.0, NICHT das Akzentgrün) -
über `drawString()`+`ML_DATUM` statt rohem `setCursor()`+`print()`
(Lehre aus dem AQI-Zahlen-Fix im Wetter-Screen, siehe oben). Ort/
Tiefe/Zeit als Label(Akzentgrün)/Wert(Weiß)-Zeilen (`layoutWrapped
LabelValue()`, lokal dupliziert). Tiefe über `Units::distanceKmToDisplay()`/
`distanceSuffix()` (gleicher Fix wie beim ISS-Radar, siehe
"Raketenstart-Countdown" oben - km bei Metrisch, mi bei Imperial).
Tsunami-Warnzeile (falls `tsunami==1`) bewusst KOMPLETT in Rot ohne
Label/Wert-Aufteilung, gleiche Begründung wie Rockets Hold-/
Fehlgrund-Zeile (eigene kritische Bedeutung, "deutliche Sichtbarkeit"
nicht durch einen weißen Wertanteil mindern).

**Zeitzone:** wie beim F1-Screen (siehe dortigen Kommentar oben) hat
das Gerät keine eigene gespeicherte Zeitzone (nur lat/lon über
LocationPresets) und die USGS-Daten liefern - anders als OEPNVs
Haltestellen-`tz`-Feld - ebenfalls keine Ziel-Zeitzone. Deshalb exakt
dieselbe Lösung 1:1 aus `f1_screen.cpp` übernommen (lokal dupliziert,
Projekt-Konvention): feste Mitteleuropa-DST-Berechnung
(`formatDeviceLocalDateTime()`), keine neue Interpretation nötig, da
der Auftrag ausdrücklich "gleiches Verfahren wie bei ÖPNV/F1" verlangte.

**"Weitere Beben"** (Tap auf die Hauptkachel, deren Rect sich je nach
Ortsbeschreibungslänge ändert und deshalb als `lastMainTileRect`
nachgehalten wird) zeigt die restlichen bis zu 9 Einträge als
einzeilige Zeilen (Ort per `MarqueeLabel` links, Magnitude + Zeit
rechts) - gleiches Muster wie "Weitere Coins" im Krypto-Ticker, nicht
antippbar (kein Detail-Screen pro Beben angefragt).

Live am echten Gerät verifiziert (07.10., per temporärer Serial-
Diagnose gegen die echte API, danach wieder entfernt): 10 reale Beben
korrekt geparst (M6.6 bis M5.6, bereits absteigend sortiert), Tsunami-
Flag korrekt erkannt (ein Eintrag mit `tsunami=1`, Alaska M6.5), Tiefen
zwischen 8 und 372km plausibel, kein Absturz.

**Fix (07.10., Alex' Fund per Foto direkt nach dem ersten Live-Test):**
die Magnitude ("M 6.6", Größe 2) überschnitt sich mit der direkt
darunterstehenden "Ort:"-Zeile - dieselbe Baseline/Ascent-Fehlerklasse
wie zuvor mehrfach behoben (siehe "WICHTIGSTE FALLE" oben), diesmal beim
Übergang von einer per `ML_DATUM` zentrierten Größe-2-Zahl zu einer
rohen, baseline-verankerten Größe-1-Zeile direkt darunter - 13px Abstand
vom Zentrierungspunkt der Zahl reichten nicht für deren eigene Resthöhe
PLUS den Ascent der nächsten Zeile. Fix: derselbe, bereits am F&G-
Krypto-Kachel bewährte 22px-Abstand (`crypto_screen.cpp::drawFngTile()`)
statt der zu knappen 13px.

## Flash-Budget im Blick behalten

Übernommen aus Flightradar (dort inzwischen eng, >94% belegt): Bei
jedem neuen Feature, das eine größere statische Datentabelle braucht
(Lookup-Tabellen, Bilder/Icons, längere Textblöcke, zusätzliche
Sprachdateien), zuerst prüfen, ob eine Auslagerung nötig/sinnvoll ist,
statt den Flash-Speicher unbedacht zu füllen. Karl meldet proaktiv
Bescheid, wenn das Flash-Budget knapp wird — ohne dass Alex danach
fragen muss.

**Pflicht: VOR dem Bauen grob abschätzen, nicht erst danach überrascht
sein** (Alex' Auftrag, 06.10., nach dem spürbaren Sprung durch den
Wetter-Screen): Vor jedem größeren kommenden Feature (z.B. Bahn,
Formel 1 - nicht bei kleinen Fixes/Korrekturen) schätzt Karl grob ab,
wie viel zusätzlichen Flash-Speicher das Feature voraussichtlich
braucht (Anhaltspunkt: ein vollständiger neuer Feature-Screen mit
eigenem API-Client + Icons hat beim Wetter-Screen ~2,2 Prozentpunkte
gekostet), UND meldet das VOR dem eigentlichen Bauen zurück, falls die
Gesamtauslastung dadurch in die Nähe von ca. 95% kommen würde (bei
Flightradar gab es bei 94% bereits Probleme, siehe oben). So kann Alex
rechtzeitig entscheiden (z.B. Icons/Daten einsparen, ein älteres
Feature vorher auslagern), statt erst nach einem fertigen Build vor
einem Platzproblem zu stehen.

Aktueller Stand (07.10., `pio run`-Build-Output, NACH der Umstellung auf
das Single-App-Partitionsschema ohne OTA - siehe "Partitionsschema"
unten - UND nach dem Formel-1-Screen UND der Luftqualität im
Wetter-Screen UND der globalen Einheiten-Einstellung
(Metrisch/Imperial) UND dem Sonnen-/Mondsymbol-Fix UND dem
ISS-Radar-Flimmer-Fix UND dem Label/Wert-Farbkontrast (ISS/Rakete) UND
den gelben Countdowns (Rakete/Formel 1) UND der kompletten Entfernung
des Mehrfach-Theme-Systems UND den beiden Rocket-Screen-Fixes
(Mission-Umschaltung/Box-Rahmen-Linie) UND der AQI-Zahlen-Zentrierung
im Wetter-Screen UND der status.id-basierten Rocket-Status-
Übersetzungstabelle UND dem Krypto-Ticker-Screen UND dem ISS-Radar-
Einheiten-Fix UND dem "Einheiten"-Hub-Button UND dem Marquee-
Lücken-Fix UND dem Erdbeben-Screen UND der standortabhängigen
Krypto-Währung UND dem Erdbeben-Überschneidungs-Fix (siehe
"Raketenstart-Countdown"/"Krypto-Ticker"/"Globale Einheiten-
Einstellung"/"Pflicht: automatisches Scrollen bei zu langem Text"/
"Erdbeben weltweit" oben); der zwischenzeitlich gebaute und noch am
selben Tag wieder vollständig entfernte Weltraumwetter-Screen taucht
hier bewusst nicht mehr auf, siehe "Was ist das Projekt?" oben): RAM
23,6% (77.440/327.680 Byte), Flash 32,0% (1.298.413/4.063.232 Byte).
Der Krypto-Ticker hat ca. 0,46 Prozentpunkte Flash gekostet (drei
API-Quellen/vier Endpunkte, zwei Screens, aber keine eigenen Icons) -
spürbar weniger als vorab grob geschätzt (1,3-1,8 Prozentpunkte). Der
Erdbeben-Screen hat nochmal ca. 0,3 Prozentpunkte gekostet (ein
API-Client, zwei Screens) - Budget bleibt weiterhin deutlich entspannt.
Der Formel-1-Screen hat damit ca. 1,1 Prozentpunkte Flash gekostet
(zwei API-Clients + ein zusätzlicher Live-Merge-Zustand in
`net_task.cpp`, aber kein eigenes Icon-Set). Das Budget bleibt
weiterhin deutlich entspannt (vorher 91,8% auf dem alten Dual-OTA-
Schema); die obige Vorab-Abschätzungsregel bleibt trotzdem bei JEDEM
größeren Feature (z.B. Bahn) in Kraft.

**Größenanalyse (06.10., per temporärem `-Wl,-Map=...`-Build-Flag,
danach wieder aus `platformio.ini` entfernt - nicht geraten, siehe
Git-Historie/Chat falls die Rohdaten nochmal gebraucht werden; bezieht
sich auf den damaligen 1.310.720-Byte-OTA-Slot, VOR der
Partitionsschema-Umstellung - die relativen Anteile zwischen eigenem
Code und Bibliotheken bleiben aber unverändert aussagekräftig):** Vom
damaligen App-Partitionslimit (1.310.720 Byte)
sind ca. 235 KB eigener Code (`src/*.cpp.o`, davon `i18n.cpp.o` mit
~29 KB am größten - die 4-Sprachen-Textdatenbank - gefolgt von den
API-Client-Dateien `weather_client.cpp.o`/`transit_client.cpp.o`/
`address_search_screen.cpp.o`/`rocket_client.cpp.o`, je 18-24 KB) und
ca. 818 KB Bibliotheken/Framework. **Mit großem Abstand größter
Einzelposten ist dabei NICHT TFT_eSPI (nur ~57 KB), sondern der
WLAN-Stack:** `libmesh.a` (231 KB, ESP-Mesh-Networking) + `libnet80211.a`
(161 KB) + `libpp.a` (58 KB) + `libphy.a` (46 KB) + `libsmartconfig.a`
(41 KB) + `libWiFi.a` (33 KB) + weitere WLAN-/Bluetooth-Pakete zusammen
über 560 KB - mehr als die Hälfte des gesamten belegten Flash-Speichers,
obwohl das Projekt weder ESP-Mesh noch Smartconfig noch Bluetooth
tatsächlich nutzt. Das ist eine bekannte Eigenheit von Arduino-ESP32:
sobald `WiFi.h`/`esp_wifi_init()` eingebunden wird, werden diese
vorkompilierten `.a`-Pakete als Ganzes mitgelinkt (keine feingranulare
Dead-Code-Eliminierung innerhalb dieser Blobs) - nicht durch eigenen
Code verursacht und mit vertretbarem Aufwand nicht eigenständig
reduzierbar.

## Partitionsschema (4-MB-Flash, Single-App, KEIN OTA) - endgültig

**Endgültige Entscheidung (Alex' Auftrag, 06.10., nach einem
zwischenzeitlichen Rückbau und erneuter, ausdrücklicher Bestätigung -
siehe Git-/Chat-Historie für den kurzen Hin-und-Her-Verlauf, falls der
Kontext je gebraucht wird): Eiswolfs Toolbox nutzt **dauerhaft** ein
eigenes Single-App-Partitionsschema OHNE OTA, eingebunden über
`board_build.partitions = partitions.csv` in `platformio.ini`. Geprüft
06.10. per `gen_esp32part.py` gegen das tatsächlich gebaute, geflashte
`partitions.bin`:

```
nvs,      data, nvs,      0x9000,  20K
factory,  app,  factory,  0x10000, 3968K   <- wird per USB geflasht
coredump, data, coredump, 0x3f0000,64K
```

- **`factory` statt `ota_0`/`ota_1`**: ohne `otadata`-Partition bootet
  der Standard-ESP32-Bootloader automatisch die `factory`-Partition.
- **`coredump` (64 KB) bewusst behalten**, obwohl nicht aktiv genutzt -
  ohne diese Partition meldet das Arduino-ESP32-Framework bei JEDEM
  Boot einen Fehler ins Log (`No core dump partition found!`, live
  beobachtet) - gegenüber den gewonnenen ~2,75 MB vernachlässigbar.
- **Kein `spiffs`**: live bestätigt ungenutzt (ausschließlich die
  SD-Karte für alle gespeicherten Daten - Standorte, Einheiten,
  WLAN-Netzwerke usw., kein `SPIFFS.h`/`LittleFS.h` irgendwo im Code).
- Live am echten Gerät verifiziert (06.10., nach vollständigem
  Flash-Erase + Neuflash, wie bei jedem Partitionstabellen-Wechsel
  nötig): Boot + WLAN-Reconnect laufen einwandfrei,
  `wifi_manager.cpp` liest die Zugangsdaten bei jedem Boot explizit
  neu von der SD-Karte statt sich auf intern im NVS zwischengespeicherte
  WLAN-Daten zu verlassen - ein Flash-Erase (der genau dieses interne
  NVS löscht) hat deshalb keinen Einfluss auf die WLAN-Verbindung. Die
  SD-Karte selbst ist physisch getrennter Speicher, von einem internen
  Flash-Erase ohnehin nie betroffen.

**Ergebnis:** App-Partition jetzt 3.968 KB statt 1.280 KB (~3,1x so
viel) - aktueller Firmware-Stand (06.10.) belegt davon nur noch 29,6%
(1.202.717/4.063.232 Byte), gegenüber 91,8% vorher. Deutlich Luft für
mehrere weitere Feature-Screens (Bahn, Formel 1, ...) ohne weitere
Sparmaßnahmen - die zuvor erwogene Prüfung "Bluetooth entfernen" und
"Icons nach SPIFFS auslagern" ist dadurch erst mal nicht mehr dringend
(Alex' Auftrag, 06.10. - zurückgestellt, nicht verworfen, falls das
Budget durch künftige Features erneut eng wird).

**KEINE Umschaltfunktion zwischen Flightradar und Toolbox auf
demselben Gerät - diese Idee ist endgültig verworfen** (Alex' Auftrag,
06.10.). Der frühere Abschnitt "Geplante Verbindung zu Eiswolfs
Flightradar CYD" (ein gemeinsamer OTA-Umschalter) ist damit hinfällig
und wurde entfernt - siehe Git-Historie, falls der ursprüngliche
Wortlaut je gebraucht wird. Grund für den Rückbau-und-Wiederholungs-
Umweg: das bisherige Doppel-OTA-Schema wurde zunächst versehentlich als
mögliche Vorbereitung für genau diese (nie umgesetzte) Umschaltfunktion
missverstanden - Code-Suche (06.10.) bestätigte, dass es dafür nie
tatsächlich genutzten Code gab, nur die freie Kapazität selbst.

**Firmware-Updates für Toolbox** erfolgen bis auf Weiteres
ausschließlich per USB-Kabel über VS Code/PlatformIO. Ein eigener
Webflasher (WebSerial-basiert, nach dem Vorbild von Eiswolfs Flightradar
CYDs `index.html`) ist für die Zukunft vorgesehen, wird aber bewusst
erst zum ersten offiziellen Toolbox-Release gebaut, nicht vorher.

**Eiswolfs Flightradar CYD ist und bleibt ein komplett eigenständiges
Projekt** (eigenes Repo, eigene `platformio.ini`) mit seinem EIGENEN
Doppel-OTA-Partitionsschema und eigenem, aktiv genutztem Selbst-Update-
Mechanismus (`ota_update.cpp`, GitHub-Release-Check über `HTTPUpdate.h`/
`esp_ota`) - durch diese Toolbox-spezifische Umstellung in KEINER Weise
berührt oder verändert.

## Sprache (4 Sprachen: Deutsch/Englisch/Spanisch/Französisch)

Echtes Mehrsprachen-System nach EXAKT demselben Muster wie bei Eiswolfs
Flightradar CYD, nur mit 4 statt 8 Sprachen (Alex' Auftrag vom 05.10.,
zusammen mit der Umbenennung zu "Toolbox"):

- `src/i18n.h`: `enum class StringId` - jeder feste UI-Text hat eine ID.
- `src/i18n_de.h` (Index 0, Standard/Fallback), `i18n_en.h` (1),
  `i18n_es.h` (2), `i18n_fr.h` (3): je ein
  `static const char* const[]`-Array, in **exakt derselben Reihenfolge**
  wie das Enum. Jede Datei hat am Ende einen `static_assert`, der die
  Array-Größe gegen `StringId::COUNT` prüft - **schlägt der Build dort
  fehl, fehlt in mindestens einer Sprachdatei ein Eintrag oder es ist
  einer zu viel.** Neue StringId → in ALLEN 4 Dateien an derselben
  Position ergänzen, sonst verschiebt sich die Zuordnung.
- Eigennamen der Sprachen (`I18n::languageName()`) sind separat in
  `i18n.cpp` hinterlegt ("Deutsch", "English", "Español", "Français").
- Persistenz bewusst OHNE eigenes SettingsStore-Modul: `I18n::init()`
  lädt die gespeicherte Sprachnummer aus `Config::SD_LANGUAGE_FILE`
  (eine Zeile, numerischer Index, faellt auf Deutsch=0 zurück falls
  nicht vorhanden/ungültig), `I18n::setLanguage()` setzt UND speichert
  sofort. `I18n::init()` wird einmal ganz am Anfang von `main.cpp::
  setup()` aufgerufen (direkt nach erfolgreichem `SdStorage::init()`),
  damit ALLE danach gezeichneten Texte sofort in der richtigen Sprache
  erscheinen.
- Nominatim-Adresssuche (`address_search_screen.cpp`) nutzt für den
  `accept-language`-Parameter dieselbe aktive Sprache (`nominatimLangCode()`,
  mappt `I18n::language()` auf "de"/"en"/"es"/"fr") - analog zum
  `NOMINATIM_LANG_CODES[]`-Array im Flightradar-Original.

**Sprachauswahl im Ablauf:**
- **Ersteinrichtung** (nur beim allerersten Start, falls keine
  Flightradar-Daten übernommen wurden): Reihenfolge Willkommen →
  Touch-Kalibrierung → WLAN → **Sprache** (`first_run_language_screen.
  cpp`, kein Zurück-Button, ein Tap wählt + speichert sofort) →
  **Einheiten** (`first_run_units_screen.cpp`, Alex' Auftrag, 07.10. -
  siehe "Globale Einheiten-Einstellung" oben, Pflichtschritt direkt
  nach der Sprache, nutzt bereits deren gerade gewählte Sprache) →
  Standort → Abschluss. Reihenfolge 1:1 wie bei Flightradar übernommen
  (per Codelektüre dort verifiziert, nicht geraten) - Sprache bewusst
  NACH WLAN (WLAN-Status-Texte beim allerersten Boot erscheinen daher
  zunächst auf Deutsch, dem Standard) und VOR Standort (dessen Texte
  sollen gleich in der richtigen Sprache erscheinen). Titel dieses
  Screens ist bewusst zweisprachig hart kodiert ("Language / Sprache"),
  da die Sprache an dieser Stelle noch nicht bekannt ist - genau wie
  bei Flightradar.
- **Nachträglich ändern:** Einstellungs-Bildschirm (`settings_screen.cpp`,
  erreichbar per Hoch-/Runter-Wisch von jedem Feature-Screen aus, siehe
  "Navigation: Wisch-Gesten statt Menü" unten) - Hervorhebung der
  aktuell aktiven Sprache, Tap auf eine andere Sprache wechselt sofort.
  Kein Zurück-Button mehr (auch hier: Verlassen per erneutem
  Hoch-/Runter-Wisch).
- **Übernahme-Pfad** (Flightradar-Daten gefunden, siehe nächster
  Abschnitt): KEINE Sprachauswahl, bleibt auf Deutsch (Standard), bis
  über den Einstellungs-Bildschirm umgestellt wird - Flightradar selbst
  hat kein übertragbares Sprach-Setting in `wifi.txt`/`locations.txt`.

**Zwei bewusste Ausnahmen, die NICHT über StringId laufen** (beide wie
im Flightradar-Original begründet): der Willkommens-Screen
(`first_run_welcome_screen.cpp`, "Willkommen!"/"Start") läuft VOR der
Sprachauswahl im Ablauf und bleibt deshalb hart deutsch kodiert; die
"Keine SD-Karte gefunden"-Fehlermeldung in `main.cpp` kann vor
erfolgreichem SD-Zugriff gar nicht erst `I18n::init()` aufrufen und
bleibt ebenfalls hart deutsch kodiert. Der Produktname "Eiswolfs
Toolbox" selbst wird NIE übersetzt (eigener Produktname, kein UI-Text).

## Standard-Workflow: nur bauen und flashen

**Standing-Regel, bis ausdrücklich etwas anderes gesagt wird:** Karl
baut und flasht nach Bedarf, aber committet/taggt/pusht/erstellt
NIEMALS eigenmächtig etwas — weder lokal (git init/commit) noch auf
GitHub (Repo anlegen, Push, Release). Das gilt auch, sobald irgendwann
ein Git-Repository existiert. Jeder dieser Schritte braucht eine
ausdrückliche Aufforderung von Alex, kein implizites "ja" auf eine
andere Rückfrage reicht dafür aus.

Commits enthalten (sobald es ein Repository gibt) KEINEN
"Co-Authored-By: Claude"-Trailer — siehe `.claude/settings.json`,
`"includeCoAuthoredBy": false` (bereits für dieses Projekt angelegt).
Grund: identisch zum Flightradar-Projekt — das führte dort früher dazu,
dass Claude selbst als GitHub-Contributor im öffentlichen Repo
auftauchte.

## Eigenständige Seriell-Diagnose

Wenn zur Fehlerdiagnose ein Mitschnitt des seriellen Monitors nötig
ist, führt Karl das eigenständig mit eigenen Testinstrumenten durch:
den seriellen Monitor selbst starten, eine sinnvolle Zeit lang
mitlesen, die Ausgabe selbst auswerten, die Ursache diagnostizieren
und — falls sich ein Fix ergibt — diesen auch gleich umsetzen. Alex
soll dafür nicht selbst den Monitor beobachten oder Log-Zeilen von
Hand abtippen müssen.

## Ersteinrichtung: Übernahme von Flightradar-Daten

Feste Design-Entscheidung (Alex' Auftrag): Beim allerersten Start prüft
`main.cpp`, ob auf der SD-Karte bereits ein Flightradar-Konfigurations-
ordner mit WLAN-Zugangsdaten existiert, BEVOR die eigene Ersteinrichtung
gestartet wird:

- **Flightradar-Ordner:** `/Flightradar_cyd/` (siehe dortige
  `config.h::SD_ROOT_DIR`)
- **WLAN-Zugangsdaten:** `/Flightradar_cyd/wifi.txt` — Format: erste
  Zeile `WIFI2` (Versions-Header), danach pro gespeichertem Netzwerk
  drei Zeilen (SSID / Passwort / "1" oder "0" für versteckte SSID).
- **Standort-Presets:** `/Flightradar_cyd/locations.txt` — Format:
  erste Zeile `active=<Index>`, danach pro Preset eine Zeile
  `lat,lon,Name` (Name optional, kann leer sein).

**Eigener SD-Ordner dieses Projekts:** `/Toolbox_cyd/` (umbenannt von
`/Satellitenradar_cyd/` im Zuge der Projekt-Umbenennung am 05.10. -
noch kein echtes Gerät im Feld hatte den alten Ordnernamen, daher
unkritischer Breaking-Change).

**Gefunden** (Flightradar-`wifi.txt` existiert): Beide Dateien werden
1x nach `/Toolbox_cyd/wifi.txt` bzw. `/Toolbox_cyd/locations.txt`
kopiert (reine Byte-Kopie, kein Parsen/Neuschreiben - beide Projekte
nutzen exakt dasselbe Dateiformat). Die eigene Ersteinrichtung
(Willkommen/Kalibrierung/WLAN/Sprache/Standort/Abschluss-Screens) wird
dabei komplett übersprungen - das Gerät verbindet direkt mit den
übernommenen Daten (Sprache bleibt dabei auf Deutsch, siehe "Sprache"
oben).

**Nicht gefunden:** Die 1:1 aus Flightradar übernommenen Ersteinrich-
tungs-Screens laufen wie dort (`first_run_welcome_screen.cpp` →
`calibration_screen.cpp` falls noch keine `calibration.txt` existiert →
`wifi_setup_screen.cpp` falls noch kein Netzwerk gespeichert →
`first_run_language_screen.cpp` → `first_run_location_screen.cpp` →
`first_run_complete_screen.cpp`). Einzige inhaltliche Änderung
gegenüber dem Original: der Willkommens-Screen zeigt statt der
flugradar-thematischen Vektorgrafik (Flugzeuge/Helikopter um ein
Radar-Reticle) den Text-Schriftzug "Eiswolfs Toolbox", da es noch kein
fertiges Logo-Bild gibt.

**Wichtig, aus einem echten Bug gelernt (05.10.):** Der Touch-
Kalibrierungs-Screen wurde zunächst versuchsweise weggelassen
(`touch_input.cpp`s großzügige Standardwerte sollten reichen) - auf
dem echten zweiten Testgerät führte das aber dazu, dass Taps auf den
Bildschirmtastaturen daneben landeten (Shift reagierte nicht,
Backspace tippte Buchstaben). Seitdem läuft die Kalibrierung wie bei
Flightradar beim allerersten Setup immer mit - NICHT wieder weglassen,
auch wenn es nach einem optionalen Schritt aussieht.

Ob die Übernahme gegriffen hat, wird NICHT an der bloßen Existenz von
`/Toolbox_cyd/wifi.txt` festgemacht (die würde nach einer
eigenständigen Ersteinrichtung ja ebenfalls existieren), sondern an
einer eigenen leeren Markerdatei `/Toolbox_cyd/setup_done.txt`
(`Config::SD_SETUP_DONE_FILE`), die nach erfolgreicher Übernahme ODER
abgeschlossener eigener Ersteinrichtung geschrieben wird.

## Stand des Repos

Noch **kein Git-Repository** initialisiert, noch **kein GitHub-Repo**
angelegt. Rein lokaler Ordner. Nichts committen oder pushen, bis das
ausdrücklich gewünscht wird.

## Wer nutzt das?

Alex — wie beim Flightradar-Projekt: Anfänger bei Arduino/Embedded-
Entwicklung, arbeitet mit VS Code + PlatformIO auf einem Mac.
Erklärungen gerne etwas ausführlicher, nicht von Fachbegriffen
ausgehen, die als bekannt vorausgesetzt werden.

**Antwortsprache (Alex' ausdrücklicher Wunsch, 05.10.): IMMER
Deutsch.** Gilt für JEDE Chat-Antwort ausnahmslos - nicht nur
Erklärungen, sondern auch Zusammenfassungen, Status-Updates,
Rückfragen, Bestätigungen nach Build/Flash, kurz: alles, was Karl im
Chat schreibt. Keine englischen Abschnitte oder Sätze einstreuen,
auch nicht in sonst technischen/kurzen Statusmeldungen.
