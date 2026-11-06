# SegelMacher

Lokaler C++17-Lookup-Generator ohne GUI. **H1 ist ein nicht physikalisch
validiertes Ingenieurmodell.** Es ist keine CFD-/Tuchsimulation und keine
gekoppelte Grosssegel-/Fock-Rechnung.

## Bauen und testen (Windows / Visual Studio 2022)

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
cmake -S Code\Core -B build-core -DBUILD_TESTING=ON
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

Falls CMake nicht auf PATH steht, liegt es bei Visual Studio Build Tools
unter `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.
Der Core hat keine Windows-, Python-, GPU- oder GUI-Abhaengigkeit.
Die Windows-CLI verwendet BCrypt fuer SHA256.
nlohmann/json 3.11.3 ist MIT-lizenziert mitgeliefert; kein Netz beim Build/Lauf.
CTest verwendet neben nativen Vertragstests optional Python 3 (Standardbibliothek)
als **Prozess-/CSV-Testtreiber**, nicht als aerodynamischen Rechenkern.

## Ordnerstruktur

- `Code\CLI`, `Code\Core`: CLI und separat baubarer Rechenkern.
- `Code\tests`, `Code\Core\tests`: Prozess-/CLI- und Core-Vertragstests.
- `Code\tests\daten`: unveraenderte 420-Testreferenz mit dokumentierter SHA-256.
- `Code\third_party`, `Code\ressourcen\examples`: Drittbibliothek und Auftragsvorlage.
- `Dokumentationen`: Markdown-Dokumentation und JSON-Konventionen/Herleitungen.
- `.github`, `AGENTS.md`, `Ideen`, `Spezifikationen`, `Umsetzungsplaene` bleiben am Stamm.
- `build`, `build-core`: bestehende Build-Verzeichnisse; Scratchdaten unter `build\scratch`.

Der Release-Pfad bleibt `build\Release\SegelMacher.exe`.
Nur native Vertragstests ohne Lookup-Integration:
`ctest --test-dir build -C Release -R "^(cli_contract|core_contract)$" --output-on-failure`.
Der optionale Python-Integrationstest erzeugt isolierte Lookups.

## Auftrag

`Code\ressourcen\examples\Auftragsdatei.json` ist die H1-Vertragsvorlage. Geometrie und
Konventionen daneben kopieren und Pfade explizit anpassen.
Bestehende Entwurfsauftraege werden nicht automatisch freigegeben.

```powershell
.\build\Release\SegelMacher.exe --hilfe
.\build\Release\SegelMacher.exe pruefen --auftrag "C:\Modelle\main\Auftragsdatei.json"
.\build\Release\SegelMacher.exe lookup --auftrag "C:\Modelle\main\Auftragsdatei.json" --max-prozessorzahl 2
```

Pfadauflosung relativ zum Auftragsordner; Ausgaben ausschliesslich dessen
neues `LookUp`. Vorhandene Ergebnisse und Sperren werden nicht entfernt.
Pro Auftrag exklusive Sperre und eigener Arbeits-/Diagnoseordner; erst
vollstaendig gepruefte Ergebnisse werden veroeffentlicht.
Nach Absturz kann eine eigene verwaiste `LookUp.lock` verbleiben; nicht ohne
Pruefung eines noch laufenden Auftrags entfernen.

Stdout ist genau ein JSON-Objekt, auch bei Hilfe/Fehler.
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

Vier Sehnenlastbeitraege je Streifen plus gebundene Kraft und freies
Profilmoment. `kraefte.csv` erhaelt alle Beitraege, `summen.csv` ihre Summe
samt zentraler Wirkungslinie und Restmoment. Beide niemals als zusaetzliche
Lasten gleichzeitig anwenden. `profile.csv` ist lokal zweidimensional;
Neigung und Kopplung sind im Gesamtsegelmodell.

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
python Code\tests\integration.py --exe build\Release\SegelMacher.exe
python Code\tests\integration.py --exe build\Release\SegelMacher.exe --full --strict
python Code\tests\integration.py --exe build\Release\SegelMacher.exe --full
```

Jeder Lauf verwendet einen eigenen Ordner unter `build`, die vorhandene
420-Sicherung unter `Code\tests\daten` und eine Konventionskopie. Keine Zugriffe schreibend auf
externe Modelle. `--full` prueft 21600 Summenzeilen und alle Einzelmomente.
**Ohne `--strict` verwendet ausschliesslich der Testauftrag gelockerte
Toleranzen (absolut/relativ 0.5) fuer Produkt-/Prozessvertraege; dies ist
keine numerische Abnahme mit 0.0005/0.01.** Mit `--full --strict` bleibt das
10-Grad-Grundraster erhalten und die adaptiv erweiterte Ausgabe wird mit den
vom Nutzer festgelegten 0.0005/0.01-Toleranzen geprueft; die Zeilenzahl steigt
entsprechend der zusaetzlichen Neigungen.
