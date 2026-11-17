# SegelMacher

Lokaler C++17-Segelmodellgenerator ohne GUI. **H1 ist ein nicht physikalisch
validiertes Ingenieurmodell.** Es ist keine CFD-/Tuchsimulation und keine
gekoppelte Grosssegel-/Fock-Rechnung.

## Bauen und testen (Windows / Visual Studio 2022)

```powershell
$env:VCPKG_ROOT = "C:\tools\vcpkg"
git clone https://github.com/microsoft/vcpkg.git $env:VCPKG_ROOT
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
cmake -S . -B build -DBUILD_TESTING=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake -S Code\Core -B build-core -DBUILD_TESTING=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

Falls CMake nicht auf PATH steht, liegt es bei Visual Studio Build Tools
unter `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
Der Core hat keine Windows-, Arrow-, Python-, GPU- oder GUI-Abhaengigkeit.
Die Windows-CLI verwendet BCrypt fuer SHA256.
nlohmann/json 3.11.3 ist MIT-lizenziert mitgeliefert. Die CLI verwendet
Apache Arrow/Parquet 21.0.0 mit Zstandard; vcpkg stellt die festgelegte
Abhaengigkeit beim ersten CMake-Lauf bereit.
Fuer Modell-Fingerprints wird ausschliesslich die SegelPhysik-Core-API
`aktualisiere_modell_fingerprints` eingebunden, kein eigener Modellhash.
Standard unter Windows ist die vorhandene Release-Library
`C:\spdeps\guibuild\segelphysik-core\Release\segelphysik_core.lib`.
Die GUI-EXE selbst ist keine aufrufbare Fingerprint-Schnittstelle.
Fehlt diese Library, wird der bekannte Core-Quellordner
`..\Segelphysik\CORE\Code` verwendet. Mit den CMake-Cachewerten
`SEGELPHYSIK_CORE_LIBRARY` (leer fuer Quellbuild) und
`SEGELPHYSIK_CORE_SOURCE_DIR` sind Library und passende Header explizit
einstellbar. Die vorgebaute Library ist fuer Release-Builds vorgesehen.
Die Core-Quellintegration verwendet dessen unveraenderte Abhaengigkeiten.
Fuer den Python-Integrationstest
pyarrow installieren:

```powershell
python -m pip install -r Code\tests\requirements.txt
```

CTest verwendet neben nativen Vertragstests Python 3 und pyarrow als
**Prozess-/Parquet-Testtreiber**, nicht als aerodynamischen Rechenkern.

## Ordnerstruktur

- `Code\CLI`, `Code\Core`, `Code\Modell`: CLI, separat baubarer Rechenkern und Segelmodellbibliothek.
- `Code\tests`, `Code\Core\tests`, `Code\Modell\tests`: Prozess-/CLI-, Core- und Bibliotheksvertragstests.
- `Code\tests\daten`: unveraenderte 420-Testreferenz mit dokumentierter SHA-256.
- `Code\third_party`, `Code\ressourcen\examples`: Drittbibliothek und Auftragsvorlage.
- `Dokumentationen`: Markdown-Dokumentation und JSON-Konventionen/Herleitungen.
- `.github`, `AGENTS.md`, `Ideen`, `Spezifikationen`, `Umsetzungsplaene` bleiben am Stamm.
- `build`, `build-core`: bestehende Build-Verzeichnisse; Scratchdaten unter `build\scratch`.

Der Produktionspfad ist `build\Release\SegelMacher.exe`.
CTest umfasst native Vertragstests sowie isolierte CLI- und Modellintegration:
`ctest --test-dir build -C Release -R "^(cli_contract|model_consumer_contract|core_contract)$" --output-on-failure`.
Der Python-Integrationstest erzeugt Modelle nur in eigens angelegten Testordnern.
Der alte `lookup`-Befehl ist im Produktionsprogramm abgeloest; sein
Regressionstestpfad steht nur bei `BUILD_TESTING` als
`SegelMacherLegacyTest` zur Verfuegung.

## Auftrag

`Code\ressourcen\examples\Auftragsdatei.json` ist die H1-Vertragsvorlage. Geometrie und
Konventionen daneben kopieren und Pfade explizit anpassen.
Bestehende Entwurfsauftraege werden nicht automatisch freigegeben.

Die archivierte Numerikmatrix fuer Optimist, 420 und Folkeboot ist
wiederverwendet; ihre Berichte enthalten keinen unabhaengigen Hash des
damaligen Quellstands. Physikalisch validiert ist das Ingenieurmodell nicht.
Fuer 420- und Folkeboot-Fock liegen Stichproben, keine vollstaendige Matrix vor.
Es verwendet `ceil(Segelhoehe / 0.25 m)` gleich hohe Ausgabestreifen;
fuer jeden Streifen wird ein lokaler Windvektor benoetigt.
`Code\ressourcen\examples\Auftrag_Segelmodell.json` zeigt den separaten
Auftrag mit expliziten geschlossenen Luftdatenbereichen. `pruefen` prueft
dessen Eingaben ohne Ausgabe; `segelmodell` fuehrt die modellbezogenen
Numerik- und Datenpruefungen aus und veroeffentlicht nur nach deren Erfolg.
Projektweite Nachweise bleiben im Modellmanifest als nicht Teil dieses
einzelnen Aufrufs ausgefuehrt gekennzeichnet. Die JSON-Antwort enthaelt unter
`numerik_vorpruefung.gradienten` fuer jeden der 14 Gradienten- und
Windsonderfaelle beide Windfelder, alle Streifen- und Gesamtvergleiche
mit Lastvektoren, Bezug und Grenze sowie den schlechtesten Vergleich.
Gegenlaeufiger Wind, Azimutnaht und lokaler vertikaler Wind werden
jeweils in beiden Richtungen auf beiden Rastern geprueft.
Sie enthaelt ausserdem diagnostische Vergleiche der 32 und 64 Moden
mit 48 Moden auf beiden Rastern; diese sind kein eigener Abnahmetest.
`numerik_vorpruefung.nullwindfolge` dokumentiert eine ausgewaehlte
Geschwindigkeitsfolge gegen null einschliesslich exakt null.
`numerik_vorpruefung.stetigkeit_diagnostisch` zeigt Gegenwind an der
Azimutnaht, lokal vertikalen Wind und lokalen Nullwind mit kleinen
Stoerungen; der dortige 0,1-%-Bezug dient nur der Diagnose.
`numerik_vorpruefung.re_min_diagnostisch` vergleicht den
Reynolds-Uebergang fuer die Quadraturpunkte beidseits mit relativen
Abstaenden von 1e-6 und berichtet die beiden Vergleiche je Punkt.
Punkte, bei denen nicht beide Vergleichswerte im Bereich 0--50 m/s
liegen, werden einzeln ausgewiesen und gesondert gezaehlt.
Fuer jedes Rastergradientenszenario dokumentiert
`beitragsbilanz_diagnostisch` die Einzelbeitragsbilanz auf beiden
Rastern; die Einzelbeitraege werden nicht zusaetzlich zu den
Streifensummen bilanziert.
Besteht diese Rastervorpruefung, enthaelt `numerik_vorpruefung.gleichwind`
zusaetzlich Grund- und Uebergangswinkelvergleiche fuer 23 Geschwindigkeiten
mit H1-Gueltigkeitsstatus, schlechtestem Vergleich und fehlgeschlagenen
Faellen. Die Vergleiche verwenden den Staudruck des tatsaechlichen
Windvektors; teilweise nicht H1-gueltige Winkel werden gesondert
gekennzeichnet. Je Geschwindigkeit berichtet `uebergang_diagnostisch`
die Anzahl und den schlechtesten Kraft-/Momentenvergleich an Naht,
Neigungs- und lokalen Abrissgrenzen. Der 0,1-%-Bezug dient dabei nur
der Diagnose und ist keine eigene Abnahmegrenze. `polpruefung` vergleicht
die Streifenlasten beider Pole komponentenweise ueber alle berechneten
Azimute; eine Abweichung beendet den Auftrag mit einem Numerikfehler.
`eingaben` nennt bei dieser Vorpruefung Pfad, Schema und
SHA-256 der eingelesenen Auftrags-, Geometrie- und Konventionsdaten.
`werkzeugversion` stammt aus der CMake-Projektversion und
`bibliotheksversionen` nennt die Version der geladenen Arrow-Bibliothek.
Fuer Windows-Release-Builds wird die Parquet-Paketversion nur angegeben,
wenn die geladene DLL denselben SHA-256 wie die beim Konfigurieren
verwendete Paketdatei hat; die Herkunft aus Paketmetadaten und Hashvergleich
ist ausgewiesen. Bei identischen Windows-Release-Paketdateien von
Parquet und Arrow 21.0.0 wird zudem die Importkette des Writers bis zu den
Zstandard-Kompressionsfunktionen geprueft und die Versionsfunktion dieser
Codec-DLL aufgerufen. Ist eine solche Zuordnung nicht moeglich, bleibt
die jeweilige Version nicht nachgewiesen.
Nach erfolgreicher Numerikvorpruefung und nachgewiesener
Bibliotheksprovenienz schreibt die CLI die Tabellen in einen eigenen
Arbeitsordner und prueft ihren bitgenauen Roundtrip. Der Arbeitsordner
wird anschliessend mit der typisierten Modellbibliothek geladen; eine
Rechenprobe vergleicht Streifen-IDs, Positionen und Lasten mit dem
Ausgangsmodell. Erst danach veroeffentlicht `segelmodell` in den modellbezogenen
Ausgabeordner. Die projektweiten Numerik-, Datenformat-, Integrations- und
Publikationsnachweise bleiben im Manifest des einzelnen Modells als nicht
im CLI-Aufruf ausgefuehrt markiert; die zusammengefuehrten Projektbelege
stehen im Umsetzungsplan. Der Austausch schuetzt vorhandene Modelldaten;
der Standard `create-only` ueberschreibt kein vorhandenes Modell.
Der Test-Build `SegelMacherAbnahme` veroeffentlicht weiterhin ausschliesslich
innerhalb eines temporaeren Verzeichnisses und kann archivierte
Vorpruefberichte uebernehmen. Das ist keine Produktfreigabe und wird im
Manifest ausdruecklich gekennzeichnet.
Nur der Test-Build kann mit `--vorpruefbericht <Datei>` und
`--vorpruefbericht-sha256 <Hash>` einen bestehenden Vorpruefbericht
uebernehmen. Er prueft dessen Hash, Eingabe-, Werkzeugversions- und
Bibliotheksidentitaet, Fallstatus und die bytegleichen neu erzeugten Tabellen. Der Bericht
wird als Eingabe im Manifest genannt; die Antwort kennzeichnet die
uebernommene Numerik. Der separat anzugebende Hash schuetzt die
Integritaet, belegt aber nicht selbst die Vertrauenswuerdigkeit des
Berichts. Die Werkzeugversion allein beweist keine identische
Quellcode- oder Programmfassung.
Die fuenf Tabellen koennen innerhalb des CPU-Limits parallel geschrieben
werden. Unter Windows laufen parallele Tabellen-Worker mit abgesenkter
CPU- und I/O-Prioritaet; der Parquet-Writer startet keine eigenen
Spalten-Worker. Die Gleichwindfaelle werden ebenfalls innerhalb des
CPU-Limits parallel nach Geschwindigkeit geprueft und in fester
Geschwindigkeitsreihenfolge berichtet; die uebrige Numerikvorpruefung
bleibt davon unabhaengig. Bei langen Einzelfaellen meldet die CLI
regelmaessig ein Lebenszeichen; hochgerechnete Restzeiten sind unsicher.
`nachweise` trennt ausgefuehrte, bestandene, fehlgeschlagene
und noch nicht ausgefuehrte Vorpruefungen und kennzeichnet die fehlende
physikalische Validierung.
Ein vorhandener Ordner `Segelmodell` wird mit
`create-only` uebersprungen und bei fehlgeschlagener Pruefung nicht
ueberschrieben. Erzeugungsauftraege verwenden eine eigene
`Segelmodell.lock`-Sperre; `pruefen` bleibt schreibfrei. Fuer die
Segelmodell-Pruefung ist noch kein GPU-Backend implementiert;
`gpu=erforderlich` wird deshalb mit Fehlercode 3 abgelehnt.
Die typisierte Bibliothek `segelmodell` und ihr Vertrag
sind im Schnittstellenregister dokumentiert; die fehlende physikalische
Validierung gilt auch fuer ihre Verwendung.

```powershell
.\build\Release\SegelMacher.exe --hilfe
.\build\Release\SegelMacher.exe pruefen --auftrag "C:\Modelle\Optimist\Auftragsdatei.json"
.\build\Release\SegelMacher.exe segelmodell --auftrag "C:\Modelle\Optimist\Auftrag_Segelmodell.json"
```

`segelmodell` schreibt ausschliesslich in den modellbezogenen Ordner
`Segelmodell`; `--output-policy create-only|overwrite` steuert den Schutz
vorhandener Ergebnisse. `--fingerprint update|skip` aktualisiert optional
ausschliesslich den Aerodynamik-Fingerprint ueber die bestehende Core-API.
Der Standard ist `skip`. `pruefen` bleibt schreibfrei und akzeptiert diese
beiden schreibenden Optionen nicht. Stdout enthaelt genau ein JSON-Objekt;
Fortschritt und Fehler stehen auf stderr.

Stderr enthaelt zeitgestempelte deutsche Phasen/Fortschritt.
Im interaktiven VT-Terminal stehen darunter zwei fest aktualisierte Balken:
Gesamt blau (abgeschlossene Pakete / 6 und aktuelles Paket), Paket gruen
(erledigte / geplante Einheiten), offener Anteil grau. Meldungen bleiben
oberhalb erhalten. Unbekannte Arbeit zeigt `0 / ?`; erfolgreicher Abschluss
zeigt erst nach Veroeffentlichung 6 / 6. Umgeleitete Ausgabe bleibt reiner
Text ohne Farben oder Cursorsteuerung. Dies gilt auch fuer den Release-Build.
Der Paketbalken zeigt `(verstrichene Dauer / geschaetzte Gesamtdauer)`
und separat `Verbleibende Zeit`, beispielsweise
`(00:03:53 / 01:34:16) | Verbleibende Zeit: 01:30:23`.
Die Gesamtschaetzung ist verstrichene Zeit plus Restzeit, nicht die
abnehmende Restzeit allein. Bei konstantem Durchsatz bleibt sie konstant;
bei geaendertem Durchsatz wird sie korrigiert. Alle drei Zeiten verwenden
immer `hh:mm:ss`, einschliesslich fuehrender Nullfelder (z.B. `00:00:12`).
Sekunden und Minuten liegen jeweils zwischen 0 und 59.
Die Restzeit wird aus dem bisherigen mittleren Durchsatz des laufenden
Rechenabschnitts geschaetzt, bei Abschnittswechsel zurueckgesetzt und ist
keine Zusage. Ohne erledigte Einheiten steht `wird ermittelt`.
Codes: 0 erfolgreich, 2 Aufruf/Eingabe, 3 Verfahren/Ressourcen,
4 Rechnung/Qualitaet, 5 Ausgabe/Sperre, 130 kooperativer Abbruch.
Ctrl+C beendet nur die eigenen Worker. Kein Unterprozess/externes Solverprogramm.

## Modell, Ergebnisse und bekannte offene Abnahmen

- [Technische Modellfestlegung mit Quellen und Grenzen](Dokumentationen/Modellfestlegung_Lookup.md)
- [Oeffentlicher Core-Vertrag](Dokumentationen/Schnittstellenregister_CPP.md)
- [Freigegebene Spezifikation](Spezifikationen/Spezifikation_Lookup_Generator.md)
- [Fortgefuehrter Plan und tatsaechliche Nachweise](Umsetzungsplaene/Entwurf_Lookup_Generator.md)

Das Segelmodell speichert fuenf Parquet-Tabellen (`streifen`,
`quadratur`, `flaechenpunkte`, `kollokation`, `kopplung`) und ein
`manifest.json`; Tabellen werden mit Zstandard komprimiert. Bereits
vorhandene alte `LookUp`-Ordner werden nicht durch den neuen Generator
veraendert.

Auf Windows kann `gpu=automatisch` gebuendelte Profilpolaren mit einer
OpenCL-FP64-GPU berechnen; `gpu=erforderlich` verlangt ein passendes Geraet.
Auf nicht unterstuetzten Systemen faellt nur der automatische Modus auf CPU
zurueck. Jeder GPU-Polarwert wird eng gegen CPU-CL/CD/CM geprueft; Backend,
Geraet und Abweichung stehen im Pruefbericht. Traglinien- und
Streifenintegration bleiben CPU; es gibt keine Speedbenchmarks.

Das 10-Grad-Neigungsgrundraster 0..90 bleibt vollstaendig enthalten.
Fuer den strengen Interpolationsnachweis wird es im nichtlinearen Bereich um
13 gezielte Stuetzstellen bis hin zu 88.75 Grad erweitert; die Ausgabeachse
umfasst 23 Neigungen. Periodische tensorprodukt-kubische Hermite-Interpolation
und unveraenderte Toleranzen werden gegen Zwischenpunkte geprueft.
Der aktuelle strenge Vollraster- und Hardware-Nachweis steht samt Grenzen im
Plan. Numerische Konvergenz ist keine physikalische Validierung; vermessene
Segel-/Neigungsreferenzen fehlen weiterhin.

## Isolierte Integration

Der Pruefbericht enthaelt unter `prozentkennzahlen` der numerischen
Vergleiche getrennte Kraft-/Momentenstatistiken: 100*RMS(Vektordifferenz)/
RMS(Referenzvektor) sowie 100*max(Vektordifferenz)/max(Referenzvektor).
Referenz ist jeweils die feinere beziehungsweise direkte Rechnung.
Die Gewichtung erfolgt gleich je Vergleich, nicht nach Wahrscheinlichkeit
eines Betriebszustands. Bei verschwindendem Nenner steht JSON `null`.
Diese dimensionslosen numerischen Kennzahlen sind keine physikalische
Genauigkeitsangabe gegen Messdaten.

```powershell
python Code\tests\integration.py --exe build\Release\SegelMacher.exe --parquet-reader build\Release\parquet_reader_tests.exe --model-schemas ..\Segelphysik\Modelle
python Code\tests\integration.py --exe build\Release\SegelMacher.exe --parquet-reader build\Release\parquet_reader_tests.exe --model-schemas ..\Segelphysik\Modelle --full --strict
python Code\tests\integration.py --exe build\Release\SegelMacher.exe --parquet-reader build\Release\parquet_reader_tests.exe --model-schemas ..\Segelphysik\Modelle --full
```

Jeder Lauf verwendet einen eigenen Ordner unter `build`, die vorhandene
420-Sicherung unter `Code\tests\daten` und eine Konventionskopie. Keine Zugriffe schreibend auf
externe Modelle. `--full` prueft 21600 Summenzeilen und alle Einzelmomente.
Der reduzierte Windows-Test benoetigt zusaetzlich die gebauten Testprogramme
`cli_tests.exe` und `model_roundtrip_probe.exe` neben der CLI; abweichende
Pfade koennen mit `--publication-test-exe` und `--model-probe` angegeben werden.
**Ohne `--strict` verwendet ausschliesslich der Testauftrag gelockerte
Toleranzen (absolut/relativ 0.5) fuer Produkt-/Prozessvertraege; dies ist
keine numerische Abnahme mit 0.0005/0.01.** Mit `--full --strict` bleibt das
10-Grad-Grundraster erhalten und die adaptiv erweiterte Ausgabe wird mit den
vom Nutzer festgelegten 0.0005/0.01-Toleranzen geprueft; die Zeilenzahl steigt
entsprechend der zusaetzlichen Neigungen.
