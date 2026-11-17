# Umsetzungsplan: Lookup-Generator

Stand: 16.11.2026

Status: **Die fruehere Parquet/Zstandard-Umstellung ist umgesetzt.
Abschnitt 11 ist beauftragt und teilweise umgesetzt. Gleichwind- und
Rasterteilpruefungen fuer die drei Referenzsegel sind bestanden; die
vollstaendige Numerik- und Integrationsabnahme sowie die Abloesung des
alten Generators bleiben offen.** Aktuelle Einzelheiten und Nachweise
stehen in Abschnitt 11. Die physikalische Validierung bleibt getrennt
ausgewiesen.

## 0. Verbindlicher Umsetzungsstand Lookup-H1

Aktuelle Nutzerentscheidung (05.11.2026): absolute Beiwerttoleranz auf
0.0005 erhoehen, relative Toleranz bei 0.01 belassen. Die untenstehenden
alten Vollrasterzahlen mit 0.0001 sind historische Nachweise, keine
Bewertung des neuen Laufs. Bei der Ergaenzung der Prozentstatistiken wurde
die Aggregatinitialisierung der Qualitaetstoleranzen korrigiert; der
zwischenzeitliche Lauf mit unbeabsichtigt null initialisierten Toleranzen
ist kein gueltiger Abnahmenachweis. Prozentkennzahlen selbst sind davon
unabhaengig; neue Abnahme erfolgt mit explizit geprueften Toleranzen.

Neuer Vollrasterlauf mit 0.0005/0.01:
`build/integration_0743bfb9aff7428082d8242fba3aab60`, isolierter
420-Ordner, `LookUp/pruefbericht.json`: Status erfolgreich, Exitcode 0.
248400 Raster-/Winkelvergleiche und 49680 Providervergleiche: jeweils
null Toleranzueberschreitungen. Das 10-Grad-Grundraster bleibt erhalten,
mit zusaetzlichen Neigungen entstehen 49680 Ausgabe-Betriebspunkte.
OpenCL auf AMD gfx1103: 1308960 Polarwerte, 3926880 CPU/GPU-Komponentenvergleiche,
maximale Abweichung 4.440892098500626e-16, ohne Speedbenchmark.
Die physikalische Validierung gegen reale Segel bleibt offen.
Der komplette strenge CSV-Integrationstest des damaligen Ausgabeformats
endete ebenfalls mit Exitcode 0: CSV-Pruefungen,
Einzelkraft-/Momentensummation, zentrale Wirkungslinie,
Polidentitaet, Hashes und Schutz vor erneutem Ueberschreiben bestanden.
Die spaeteren historischen Tabellenstaende zu fehlender GPU und gescheiterter
Numerik sind durch diesen neuen Nachweis ueberholt, nicht nachtraeglich
als damals erfolgreich umzudeuten.
Integrationstest-Zeitlimit auf 1800 Sekunden erweitert: der damalige
vollstaendige CSV-Export benoetigte mehrere Minuten; 180 Sekunden brachen den Test vor
der Ergebnispruefung ab. Keine numerischen Toleranzen dadurch geaendert.

Abschliessende native Tests nach der Toleranzumstellung: Gesamt-CTest
3/3 bestanden (99.79 s), separater Core-CTest 1/1 bestanden.
Der In-flight-Abbruchtest verwendet unter Windows gezielt Ctrl+Break
fuer die eigene Prozessgruppe: CREATE_NEW_PROCESS_GROUP deaktiviert
Ctrl+C im Kindprozess. Exitcode 130, Diagnose und Sperrenbereinigung
sind damit nachgewiesen; fremde Prozesse erhalten kein Abbruchsignal.

Der Nutzer hat den bestehenden Plan zur Umsetzung freigegeben. Keine
Release-/Tag-Veroeffentlichung ist beauftragt oder erfolgt. Dieser Dateiname
bleibt als bestehende Planhauptdatei erhalten; es wird kein konkurrierender
Plan und kein eigenmaechtiger Git-Versionsstand angelegt.
Nach Nummernkreispruefung gelten die folgenden U-S-Kennungen.
Technische Umsetzung, Integration, Numerik und fachliche Abnahme sind getrennt.

| Schritt | Anforderungen | Abhaengigkeiten | Tatsaechlicher Stand und Nachweis |
|---|---|---|---|
| U-S-000001 Finalisieren/Modell festlegen | A-S-00008 bis A-S-00012, A-S-00028 | keine | Spezifikation finalisiert, IDEE-Zuordnung erhalten; `Dokumentationen/Modellfestlegung_Lookup.md`. Quellen/Formeln/Grenzen dokumentiert, keine gemessenen Segelreferenzen. |
| U-S-000002 Projektgeruest/API | A-S-00026 | U-S-000001 | C++17/CMake, CLI und eigenstaendiger statischer Core; CPPI-0001..0008; native Gesamt- und Core-CTest. |
| U-S-000003 Eingabevertrag | A-S-00001, A-S-00004, A-S-00007 | U-S-000002 | Versionen/Pfade/Status/Duplikate/Optionen/lokale Re, explizite H1-Parameter; `Code/CLI/job.cpp`, `Code/ressourcen/examples/Auftragsdatei.json`, Vertragstests. |
| U-S-000004 Geometrie/Mehrsegelflaechen vorbereiten | A-S-00005, A-S-00006, A-S-00025 | U-S-000003 | Polygon-/Schnitt-/Formkern, getrennte Segel-ID/Lasten und starre Transformation. Unit-Tests Dreieck/Viereck, Form, Transformation; keine Interferenz. |
| U-S-000005 Profil/Hybridmodell | A-S-00008 bis A-S-00012, A-S-00028 | U-S-000001, U-S-000004 | Fourier/Plattenreibung/Abriss/gekoppelte spektrale Traglinie/raeumliche Impulsschliessung implementiert. Analytische und Symmetrie-Tests vorhanden. Viskose Abriss-/Hochwinkel-/Neigungsreferenzdaten fehlen: **fachliche Abnahme offen**. |
| U-S-000006 Lastintegration | A-S-00013 bis A-S-00015 | U-S-000005 | Positionierte Einzelbeitraege und freie Eigenmomente, Wirkungslinie/Restmoment/Schwelle; native Momenten- und CSV-Summationstests. |
| U-S-000007 CPU/Abbruch | A-S-00016, A-S-00024, A-S-00029 | U-S-000003 | Ein gemeinsamer begrenzter Workerpool, keine verschachtelten Providerpools; deterministische Ergebnisse fuer Limits 1/2/4. Eigene Worker werden gejoint. In-flight-Terminalabbruch separat nachzuweisen. |
| U-S-000008 Logging/CLI | A-S-00002, A-S-00003 | U-S-000002 | Zentrale stderr-Instanz, zeilenweise terminaltaugliche Anzeige auch bei Umleitung, Zeitstempel/Phasen/Fortschritt/Lebenszeichen; ein Abschluss-JSON. Automatisierter Umleitungsnachweis, interaktive TTY-Abnahme offen. |
| U-S-000009 Numerische Pruefung | A-S-00018, A-S-00019 | U-S-000006 | Raster 1/2/4, Fourier-Referenz und Traglinienmoden 12/16, direkte Winkel-/Zellenzwischenpunkte, Naht/Pol/Stetigkeit; Fehler sperrt Publikation. Strenges Startraster muss separat bestanden werden, gelockerter Workflowtest ist kein Ersatz. |
| U-S-000010 GPU | A-S-00017 | U-S-000006, U-S-000007 | **Teilweise/offen:** Modus-/Faehigkeitsablehnung und CPU-Auswahl ohne Speedbenchmarks vorhanden. Kein implementierter GPU-Rechenbackend, keine Geraete-/Speicher-/Fehler-/CPU-GPU-Korrektheitspruefung. Nicht als fertig abgenommen. |
| U-S-000011 Export/Publikation | A-S-00020 bis A-S-00023, A-S-00030 | U-S-000009, U-S-000007 | Historischer CSV-Stand: sechs Produkte, Tabellen, Snapshots/Hashes, Sperre und atomare Ordnerumbenennung. CSV-Ausgabe ist durch die am 12.11.2026 freigegebenen A-S-00031 bis A-S-00036 ersetzt; Folgearbeiten U-S-000013 bis U-S-000016. |
| U-S-000012 Integration/Abgleich | A-S-00001 bis A-S-00030 | U-S-000008, U-S-000011 | Historische isolierte 420-Tests einschliesslich CSV-/Momenten-/Hash-/Pol-/CPU-Pruefungen. Tabellenpruefungen muessen gemaess neuem Parquet-Vertrag erneuert werden. Verbleibende fachliche, strenge numerische und GPU-Abnahmen bleiben ausdruecklich offen. |

### Nachweisbewertung

Ordnerbereinigung vom 06.11.2026 (bestehende Schritte U-S-000002,
U-S-000003 und U-S-000012; keine fachliche Vertragsaenderung):

- Code liegt unter `Code/CLI`, `Code/Core`, `Code/tests` und `Code/third_party`;
  Beispiele unter `Code/ressourcen/examples`, JSON-/Markdown-Dokumentation
  gemeinsam unter `Dokumentationen`. Die 420-Sicherung ist unveraendert als
  Testreferenz unter `Code/tests/daten/main_20261105_195154_358.json` abgelegt.
- Alle Umzuege per `Move-Item`, nichts geloescht. Root-Probedaten liegen unter
  `build/scratch`; leere ehemalige Ordner wurden dort ebenfalls aufbewahrt.
  Bestehende Build-Verzeichnisse bleiben erhalten. Die alte eigenstaendige
  Core-Konfiguration (`CMakeCache.txt`, `CMakeFiles`) liegt wegen des geaenderten
  Quellordners unter `build-core/scratch/konfiguration_vor_code_umzug`.
- Gesamtprojekt: `cmake -S . -B build -DBUILD_TESTING=ON`, Release-Build und
  native `cli_contract`/`core_contract`: **2/2 bestanden** (0.15 s).
  Release-Exe bleibt `build/Release/SegelMacher.exe`.
- Eigenstaendiger Core: `cmake -S Code\Core -B build-core -DBUILD_TESTING=ON`,
  Release-Build und Core-CTest: **1/1 bestanden** (0.12 s).
- Python-Testtreiber: Syntax, Repositorywurzel, Testreferenz und Beispielpfade
  geprueft; **kein Integrationstest und kein Lookup-Lauf ausgefuehrt**.
  Dies ersetzt keinen neuen numerischen oder physikalischen Nachweis.
- Externe 420-main/fock-Auftraege: ausschliesslich den Konventionspfad von
  `documentation` auf `Dokumentationen` geaendert; Rueckersetzung dieses
  Pfadsegments reproduziert jeweils die vorherige Datei-SHA-256.
  Relative Eingabepfade/JSON und CLI-`pruefen` read-only erfolgreich (Exitcode 0).
  Alle 19 weiteren externen Dateien (Geometrien, Lookups, Diagnosen und
  Auftragsbackup) sowie vier verschobene Dokumentations-JSONs sind per
  Vorher-/Nachher-SHA-256 unveraendert. Die Sicherungs-SHA-256 bleibt
  `E8C47FFCA62459C08AE525320794B19884A72544A2857C77606473CCEA862EB2`.
- Kein Commit, Push, Tag oder Release ausgefuehrt.

- Native Gesamt- und eigenstaendige Core-Builds: MSVC 19.44, VS 2022,
  CMake/CTest aus installiertem Visual-Studio-Verzeichnis. Letzter
  Gesamt-CTest: **3/3 bestanden**, 28.89 s; eigenstaendiger Core-CTest:
  **1/1 bestanden**, 0.13 s (05.11.2026).
- `core_contract`: analytische Duennprofil-/Reibungswerte, Camber/Twist,
  Konturfaelle, Winkelraum/Pol, Momentenidentitaet, reine Momente,
  Kraeftepaare, Transformation sowie elliptischer Fluegel gegen analytischen
  CL/induzierten CD (2/3 % numerische Referenztoleranz);
  `cli_contract`: Aufruf-/JSON-/SHA-Vertrag und kooperatives Abbruchflag
  mit Diagnose, ohne Veroeffentlichung oder zurueckbleibende eigene Sperre.
- `cli_integration`: isolierte 420-Kopien, zwei Arbeitsverzeichnisse,
  sechs Produkte, alle Einzelkraft-/Momentensummen, Polidentitaet,
  Eingabe-/Ausgabehashes, CPU-Limits 1/2/4 einschliesslich gemessener
  gleichzeitiger Workerobergrenze, Entwurf/Version/Re/Optionen/GPU/Sperre,
  monotone Fortschrittsprozente und echte konkurrierende Prozesse
  (ein Erfolg und ein Konfliktcode 5).
- Integrationsdaten/Prozesslogs werden unter `build/integration_*` erzeugt,
  nicht in externen Modellen. Ein gelockerter Vertragstest (0.5/0.5)
  ist explizit **kein** Nachweis fuer das Startraster mit 0.0001/0.01.
- Externe Originaldatei wurde vor und nach den Testarbeiten gehasht und
  stimmt mit der gesicherten SHA256 ueberein:
  `e8c47ffca62459c08ae525320794b19884a72544a2857c77606473ccea862eb2`.
- Nutzerabnahme, reale Segelpolaren/Neigungsdaten und GPU-Hardwarebelege
  liegen nicht vor. Kein Anspruch auf vollstaendige fachliche Fertigstellung.

### Strenger Vollrasterlauf: Abnahme blockiert

Nachweis: `build/integration_1793907987993917400/test_evidence.json` und
Diagnosebericht im dortigen isolierten 420-Ordner.
Alle 21600 Rasterfaelle, Faktoren 1/2/4, 12/16-Traglinienmoden und
Winkelzwischenpunkte wurden ausgefuehrt.
Bei **unveraenderten** Toleranzen 0.0001 absolut / 0.01 relativ:

| Pruefung | Ergebnis |
|---|---|
| Hoehenraster 2/4 | Keine Toleranzueberschreitung |
| Fourier 256/512 gegen analytische Integrale | Bestanden |
| Azimutzwischenpunkte | 21 Toleranzueberschreitungen |
| Neigungszwischenpunkte | 6390 Toleranzueberschreitungen |
| Winkelzellenzwischenpunkte | 6592 Toleranzueberschreitungen |
| Polidentitaet und Anschluss bei 89.999 Grad | Keine Toleranzueberschreitung |
| Traglinienprovider 12/16 | 66 Toleranzueberschreitungen |

Maximale Kraft-/Momentenvektordifferenz der Raster-/Winkelpruefungen:
0.033196811905638765, maximaler Toleranzquotient 71.95912609640122.
Maximale Providerdifferenz: 0.0014498470403071205.
**Exitcode 4, Diagnose erhalten, kein LookUp veroeffentlicht.**
Der Testtreiber bestaetigt diese korrekte Ablehnung; das ist ausdruecklich
keine bestandene numerische Abnahme.

Insbesondere das vorgegebene 10-Grad-Neigungsraster kann die nichtlinearen
raeumlichen Lasten mit der geforderten linearen Interpolation und diesen
Toleranzen nicht ausreichend darstellen. Raster, Interpolationsvertrag,
Modelluebergaenge und Provideraufloesung muessen fachlich weiterbearbeitet
werden. Keine eigenmaechtige Aenderung der freigegebenen Toleranzen oder
der geforderten 21600-Zeilen-Abnahme, kein physikalisches „Geradebiegen“.

### Vollstaendiger Produktvertragslauf (keine strenge Numerikabnahme)

Nachweis: `build/integration_1793907987993798800/test_evidence.json`,
CLI-stdout/stderr und `LookUp/pruefbericht.json` im dortigen isolierten
420-Ordner. **21600 eindeutige Summenzeilen und alle sechs Produkte**
erzeugt, exakte Spalten/Sortierung/endliche Zahlen/Profilbezug geprueft;
alle Einzelkraft-/Hebelarm-/Eigenmomentensummen stimmen innerhalb 1e-10
mit der Resultierenden ueberein. Zentrale Wirkungslinie, Restmoment,
Polidentitaet, Eingabe-/Ergebnishashes und Schutz beim zweiten Lauf bestanden.
Effektive CPU-Obergrenze 2, beobachtete gleichzeitige Workerzahl maximal 2.
Eingaben funktionieren aus zwei Arbeitsverzeichnissen.

Der **isolierte Testauftrag**, nicht die freigegebene Vorlage, verwendet
dabei 0.5 absolut / 0.5 relativ. Das belegt nur den durchgaengigen
Produkt-/Prozessvertrag und hebt die gescheiterte strenge Abnahme nicht auf.
Physikalischer Status bleibt `ingenieurmodell_nicht_validiert`.
Die Testdateien liegen unter ignoriertem `build`, keine externen Eingaben
wurden veraendert. Kein Commit, Push, Tag oder Release ausgefuehrt.

## Historische Planbeschreibung (Abnahmekriterien bleiben erhalten)

## 1. Grundlage und Umfang

Massgeblich ist die
[Spezifikation des Lookup-Generators](../Spezifikationen/Spezifikation_Lookup_Generator.md).
Offene Entscheidungen stehen in der
[Entwurfspunkteliste](../Spezifikationen/Entwurfspunkte/Lookup_Generator.md).

Geplant ist ein lokales Kommandozeilenwerkzeug ohne GUI mit wiederverwendbarem
Berechnungskern. Ein Auftrag erzeugt im segelspezifischen Unterordner `LookUp`
Profilpolaren, Gesamtsegelbeiwerte, Metadaten und Pruefnachweise.

Der erste produktive Rechenpfad verwendet das physikalisch begruendete
Hybridmodell: woelbungsabhaengige Profilrechnung, viskose Effekte/Abriss,
raeumliche Kopplung und Hochwinkelbehandlung mit positionierten Kraeften und
Profilmomenten. Geneigte Anstroemung von 0 bis 90 Grad in 10-Grad-Schritten
gehoert zum Erstumfang. Es gibt keinen vorgeschalteten synthetischen
Ersatzmodell-Meilenstein.
Konkrete Verfahren, Parameter, Gueltigkeitsbereiche und Referenzpruefungen
muessen vor Implementierung des produktiven Modells festgelegt werden.
Die Verfahrensrichtung ist entschieden, die Detailmodellierung noch offen.
Die Faehigkeitseinordnung in Abschnitt 5.1 der Spezifikation ist die
Umfangsgrundlage: Naeherungsbereiche werden umgesetzt und gekennzeichnet,
nicht als spaetere optionale Grundfunktionen behandelt.

Zunaechst entstehen unabhaengige Einzel-Lookups. Grosssegel-/Fock-Interferenz
ist ein vereinbarter spaeterer Ausbauschritt gemaess Abschnitt 5.3 der
Spezifikation. Jetzt wird nur die Erweiterbarkeit des internen Rechenkerns
vorbereitet; kein gekoppelter Solver und kein Konfigurations-Lookup werden
als Bestandteil der ersten Abnahme verlangt.

Es wird kein Release- oder Versionsstand eigenmaechtig festgelegt.
Dieser Plan ist die einzige derzeitige Planhauptdatei zum Generator.
Vor Implementierungsbeginn werden Spezifikation und Versionsschritt
freigegeben und dieser Entwurf zum versionsbezogenen Plan fortgefuehrt.
Erst dann erhalten Arbeitspakete U-S-Kennungen mit Verweisen auf freigegebene
A-S-Anforderungen. Die folgenden Paketnamen sind keine solchen Kennungen.

## 2. Schutz der Eingaben

Referenz fuer Integrations- und Ende-zu-Ende-Tests:

```text
C:\_DATEN\Code\Python\Organisierter Code\Segelphysik\Modelle\420\meshes\aerodynamik\main\main.json
```

Die [separate Sicherung/Testreferenz](../Code/tests/daten/main_20261105_195154_358.json)
ist vorhanden. SHA-256:
`E8C47FFCA62459C08AE525320794B19884A72544A2857C77606473CCEA862EB2`.

Vor Testarbeiten Original gegen Sicherung pruefen.
Bei bewusst geaendertem Original eine neue Sicherung anlegen, keine bestehende
Sicherung ersetzen. Tests verwenden isolierte Kopien von Geometrie, Auftrag
und Konventionen; sie schreiben weder in den Originalordner noch in vorhandene
Modell-Lookups. Nach Tests die Originalpruefsumme erneut pruefen.

## 3. Arbeitspakete und Abhaengigkeiten

Die folgenden urspruenglichen Paketnamen/Abhaengigkeiten bleiben zur
Nachvollziehbarkeit erhalten. Aktueller Status und finale Anforderungszuordnung
stehen in Abschnitt 0; die Spalte Grundlage verweist auf Spezifikationsabschnitte.

| Paket | Inhalt | Voraussetzung | Grundlage |
|---|---|---|---|
| Freigabe und Technik | Physikalischen Erstumfang konkretisieren, C++/CMake als akzeptierte Richtung beruecksichtigen, Distribution, konkretes CPU/GPU-Backend und Versionsschritt festlegen; Anforderungen finalisieren | keine | 1, 2, 6, 9; Entwurfspunkte |
| Physikalische Modellfestlegung | Profilverfahren, viskose Schliessung/Re/Abriss, 3D-Kopplung, Hochwinkeluebergaenge, geneigte Anstroemung/Pol, Lastverteilung und Momentenbezug mit Quellen, Grenzen und Referenzfaellen spezifizieren | Freigabe und Technik | 4.3, 5, 7, 8.2 |
| Projektgeruest | Build/Testsetup, eigenstaendiger Rechenkern und CLI; keine GUI-Abhaengigkeit | Freigabe und Technik | 3, 9 |
| Eingabevertrag | Geometrie-, Auftrags- und Konventionsschema; Statuspruefung, Pfadaufloesung, Ressourcenfelder, klare Feldfehler | Projektgeruest | 3.1, 3.2, 4 |
| Geometriekern | Konturpruefung, horizontale Schnitte, Woelbung/Twist, Flaeche/Referenzlaenge, Streifenraster und Nullsehnen; interne Segelidentitaet und starre Transformation von intrinsischer Geometrie trennen, mehrere Segelflaechen darstellbar halten; Einzelauftrag nutzt Identitaetstransformation | Eingabevertrag | 4.1, 5.3, 7 |
| Rechenmodell | Festgelegte physikalische Modellbausteine implementieren: Profilkraefte/-momente, viskose Effekte, raeumliche Kopplung, Hochwinkelmodell und Polbehandlung; Grenzen explizit pruefen | Geometriekern, Physikalische Modellfestlegung | 4.3, 5.1, 5.2 |
| Gesamtsegelintegration | Streifenkraefte mit modellgemaessen raeumlichen Ansatzpunkten, Momentensumme um Hals, dimensionslose Normierung und aequivalenter Gesamtansatzpunkt mit Restmoment/Gueltigkeit | Rechenmodell | 5, 8.2 |
| CPU-Scheduler | Globale CPU-Budgetierung einschliesslich Providerthreads; deterministische Aufgabenverteilung, Abbruch | Projektgeruest, Eingabevertrag | 6.1, 9 |
| Terminal-Logging | Zentrale Textausgabe, Teil-/Gesamtfortschritt, TTY-/Umleitungsmodus, Lebenszeichen und Fehlerabschluss | Projektgeruest | 3.3, 3.4 |
| Qualitaetspruefung | Raster 1/2/4, Zwischenazimute/zyklische Naht, Zwischenneigungen und Winkelzellen, Polstetigkeit, Toleranzen, Modellplausibilitaet | Gesamtsegelintegration | 4.3, 7 |
| GPU-Rechenpfad | Gebuendelte geeignete Profil-/Streifenrechnungen; Geraete-/Speicherpruefung, CPU-Vergleich, keine Speedpruefung | Gesamtsegelintegration, CPU-Scheduler | 6.2 |
| Ergebnisexport | Metadaten, Parquet-Tabellen mit Zstandard, Berichte, Eingabesnapshots, Pruefsummen; gesperrte Zwischenablage und sichere Veroeffentlichung | Qualitaetspruefung, CPU-Scheduler, GPU-Rechenpfad | 8, 11 |
| Durchgaengige CLI | `pruefen`/`lookup`, Exitcodes, ein Abschluss-JSON, Ressourcen und Logging mit allen Rechenphasen verbinden | Ergebnisexport, Terminal-Logging | 3, 6, 8 |
| Abnahme und Dokumentation | Isolierter 420-Main-Lauf, Fehler-/Ressourcentests, CLI-/Ergebnisbeispiele, Belege und Restpunkte | Durchgaengige CLI | 9, 10 |

CPU-Scheduler und Logging koennen nach ihren Voraussetzungen parallel zur
Geometriearbeit entstehen. GPU-Arbeit folgt auf einen geprueften CPU-Rechenpfad.
Der Exportvertrag kann vorher entworfen werden; seine Abnahme setzt jedoch
die tatsaechlichen numerischen Ergebnisse und Hardwaremetadaten voraus.

## 4. Meilensteine und Nachweise

### Eingaben und aufrufbares Geruest

Eintritt: Umfang, Technik und finale Anforderungen sind freigegeben.

- CLI ist ohne GUI baubar/startbar; Hilfe und Version funktionieren.
- `pruefen` verarbeitet den kopierten 420-Auftrag aus beliebigem Arbeitsordner.
- Fehlerhafte Versionen, Pfade, doppelte Schluessel und Optionen werden abgelehnt.
- Neue Ressourcenfelder sind dokumentiert und in einer Test-Auftragskopie
  vorhanden; der Originalauftrag wird nicht stillschweigend freigegeben.

Nachweise: Build-/Unit-Testausgabe und parsebare CLI-Ausgaben.

### Geometrie und CPU-Referenzrechnung

Eintritt: Eingabevertrag geprueft.

- Gerade Grosssegel-Unterlieke bleiben gerade.
- Synthetische Dreiecks-/Vierecksfaelle decken Spitzen und Kopfkanten ab.
- Profilrechnung liefert zu dokumentierten Referenzfaellen passende
  Auftriebs-/Momentenwerte und korrekte Winkel-/Kraftvorzeichen.
- Woelbung, Woelbungsposition und Twist wirken sich nachvollziehbar aus.
- Viskoser Widerstand, Reynolds-Einfluss und Abriss stimmen innerhalb
  der vorab festgelegten Akzeptanzgrenzen mit geeigneten Referenzdaten ueberein.
- Raeumliche Kopplung prueft Abwind und induzierten Widerstand ohne
  Doppelzaehlung; Hochwinkeluebergaenge bleiben stetig.
- Einfache analytisch pruefbare Streifenfaelle bestaetigen Normierung und Momente.
- Einzelkraefte, Kraeftepaare und reine Momente pruefen raeumliche
  Ansatzpunkte, Restmomente und die Momentenidentitaet einschliesslich
  ungueltiger Ansatzpunkte bei verschwindender Gesamtkraft.
- Modellgueltigkeitsgrenzen und fehlende Referenzabdeckung werden
  explizit ausgewiesen; kein fehlender Baustein wird still ersetzt.
- Interne Geometrietests koennen zwei identifizierbare Segelflaechen mit
  getrennten Transformationen darstellen. Identitaetstransformation bewahrt
  den Einzelpfad; synthetische Drehung/Verschiebung prueft die korrekte
  Transformation von Ansatzpunkten, Kraeften und Momenten.
  Diese Tests weisen keine aerodynamische Segelinterferenz nach.

Nachweise: Geometrie-, Modell- und Integrations-Unit-Tests.

### Ressourcen und Fortschritt

Eintritt: CPU-Referenzrechnung vorhanden.

- Limits 1/2/4 begrenzen die gemeinsame Rechenparallelitaet auch bei
  Providerthreads; CLI kann das Auftragslimit nur weiter einschraenken.
- GPU-geeignete Batches nutzen einen vorhandenen unterstuetzten GPU-Backend.
- Fehlende Hardware, Speicher-/Faehigkeitsmangel und GPU-Ausfuehrungsfehler
  werden entsprechend den GPU-Modi behandelt.
- CPU-/GPU-Ergebnisse stimmen innerhalb der Toleranzen ueberein.
- Es gibt keine Auswahlbenchmarks oder Speedpruefungen.
- Text, Teilfortschritt und monotoner Gesamtfortschritt funktionieren auch
  mit parallelen Workern und umgeleiteter Ausgabe.

Nachweise: Scheduler-/Loggingtests und simulierte GPU-Auswahltests.
Eine tatsaechliche GPU-Ausfuehrung gilt nur mit realem Geraetenachweis als
erfolgt. Ohne geeignete Hardware bleibt dieser Nachweis offen; CPU-Tests
oder Simulationen werden nicht als GPU-Nachweis ausgegeben.

### Vollstaendiger isolierter Referenzlauf

Eintritt: Numerik, Ressourcen und Export sind integriert.

- Ein bereiter Testauftrag erzeugt alle sechs vorgesehenen Dateien in `LookUp`,
  einschliesslich `kraefte.parquet` mit raeumlich positionierten Einzelbeitraegen.
- Summation der Einzelkraefte einschliesslich Hebelarm- und Eigenmomenten
  reproduziert die Gesamtsegel-Kraft und das Moment um den Hals.
- Raster- und Winkelpruefung bestehen; exportiert wird das feinste gepruefte Raster.
- Fuer das erweiterte physikalische Raster entstehen 21600 eindeutige
  Summenzeilen je Form (360 Azimute, zehn Neigungen, sechs Re-Stufen).
  Am Pol sind alle Azimutzeilen identisch; 19446 Windrichtungs-/Re-Faelle
  sind physikalisch verschieden. Ein rein horizontaler Rechenlauf ist kein
  Nachweis fuer diesen Umfang.
- Neigung ist in Tabellen-/Betriebspunktschluesseln und Metadaten enthalten.
  Zwischenneigungen und Winkelzellen bestehen die Interpolationspruefung;
  der Polfall bleibt endlich und stetig. Ungeeignete Provider werden abgelehnt.
- Profile, Varianten und Streifenbezuege sind vollstaendig nachvollziehbar.
- Kopierte Ergebnisse sind ohne Ursprungsrepository interpretierbar.
- Standardausgabe ist genau ein Abschluss-JSON, Text/Fortschritt liegt auf stderr.

Nachweise: Bericht, Tabellenpruefung, Eingabe-/Ergebnispruefsummen und
Integrationstestprotokoll. Der Referenzlauf allein ist kein umfassender
physikalischer Validierungsnachweis; dazu gehoeren die gesonderten
Modellreferenzpruefungen und deren dokumentierte Abdeckung.

### Robustheit und fachliche Abnahme

Eintritt: Referenzlauf bestanden.

- Tests fuer defekte Eingaben, nicht unterstuetzte Re-/Winkelbereiche,
  nicht erreichte Toleranzen und nicht endliche Resultate.
- Tests fuer fehlenden Speicher, Schreibfehler, konkurrierende Auftraege,
  bestehendes `LookUp`, Benutzerabbruch und Providerfehler.
- Kein stiller Ersatz, keine beschaedigten vorhandenen Ergebnisse,
  keine Veroeffentlichung scheinbar vollstaendiger Teildaten.
- Originaldatei stimmt weiterhin mit ihrer Sicherung ueberein.
- Nutzer erhaelt bekannte Grenzen und den belegten physikalischen
  Validierungsstand; ohne entsprechende Nachweise gilt
  `ingenieurmodell_nicht_validiert`.
- Einzelauftrag und Geometrien benoetigen keine neuen Mehrsegel- oder
  Einbau-Pflichtfelder. Ergebnisse weisen vernachlaessigte Segelinterferenz
  aus; die Addition einzelner Lasten wird nicht als gekoppelte Rechnung
  dokumentiert.

Nachweise: Fehlerfalltests, Schutzpruefungen und dokumentierte Nutzerabnahme.
Technische Fertigstellung, Integration und fachliche Abnahme bleiben
getrennte Statusangaben.

## 5. Dokumentation und Schnittstellen

Direkt zu pflegen sind:

- CLI-Aufruf, Status-/Exitcodes und Auftragsbeispiele;
- versionierte Eingabe-/Ergebnisformate und gemeinsame Konventionen;
- feste Bezuege, Vorzeichen, Providergrenzen und physikalischer Validierungsstand;
- CPU-Budget und dokumentierte GPU-Eignung je Funktion;
- reproduzierbarer isolierter 420-Testauftrag und dessen Nachweise.
- Trennung von Einzel-Lookups und spaeterem Konfigurationsmodus,
  einschliesslich Segelidentitaet, Transformationen und Lastzuordnung.

Sobald oeffentliche C++-Schnittstellen konkret feststehen, werden vor ihrer
Implementierung die geltende Schnittstellen-Instruction und das
Registerverfahren angewendet. Dieser Plan definiert keine exportierten
Typen oder Funktionen.

## 6. Abschluss und Abgrenzung

Vor Abschluss jede final freigegebene Anforderung einem Umsetzungsschritt
und einem Nachweis zuordnen. Offene GPU-Hardwarenachweise, fehlende
physikalische Provider und bekannte Modellgrenzen sichtbar belassen.

Der Plan beauftragt weder Commit, Push, Tag noch Release.
Keine Bootssimulation, GUI, Einbaupositionen in intrinsischen Segeldateien,
physikalische Genauigkeitsgarantie oder automatische Erweiterung um alle Ideen.

## 7. Spaeterer Ausbauschritt: gekoppelte Segelkonfiguration

Status: **vorgesehen, nicht Teil der jetzigen Umsetzung oder Abnahme**.
Die Verschiebung der Interferenzrechnung entspricht der Nutzerentscheidung;
die Architekturvorbereitung aus Abschnitt 3 bleibt im Erstumfang.

Vor dem Ausbauschritt werden Spezifikation und dann ein eigener
versionsbezogener Umsetzungsplan konkretisiert. Vorgesehene Inhalte:

- Konfigurationsauftrag mit Geometrieverweisen, gemeinsamen Bezuegen,
  Positionen/Orientierungen und Kombinationen der Segelstellungen;
- geeigneter gekoppelter Solver fuer gegenseitig veraenderte Anstroemung
  mit klaren Grenzen, insbesondere bei abgeloester Stroemung;
- erweiterte Raster-/Ressourcenplanung und versioniertes Ausgabeformat
  samt festgelegter Ablage des Konfigurations-Lookups;
- je Segel zuordenbare Kraefte, Ansatzpunkte und Eigenmomente sowie
  konsistente Summen in einem gemeinsamen Koordinatensystem;
- eigene Interferenz-Referenzfaelle und Validierungsnachweise.

Die vorhandenen Segelgeometrien werden wiederverwendet, Einzel-Lookups
bleiben gueltige Ergebnisse ihrer unabhaengigen Modelle. Gekoppelte
Ergebnisse entstehen durch neue Rechnungen, nicht durch nachtraegliches
Umetikettieren oder einen pauschalen Korrekturfaktor.

## 8. Umsetzung: Parquet-Ausgabe ohne CSV

Status: **Am 12.11.2026 durch den Nutzer zur Umsetzung beauftragt.**
Verbindlich ist Abschnitt 11 der
[Spezifikation](../Spezifikationen/Spezifikation_Lookup_Generator.md#11-kompaktes-parquet-ausgabeformat)
und die
[Entwurfspunkte](../Spezifikationen/Entwurfspunkte/Lookup_Generator.md#erweiterung-kompaktes-lookup-format-06112026).
Stand: 12.11.2026.

### 8.1 Umsetzungsschritte

| Schritt | Anforderungen | Abhaengigkeiten | Geplanter Nachweis / Status |
|---|---|---|---|
| U-S-000013 Arrow-Abhaengigkeit und C++-Prototyp | A-S-00031, A-S-00033, A-S-00034 | U-S-000011 | Apache Arrow/Parquet 21.0.0 (C++17) mit Parquet- und Zstandard-Support reproduzierbar ueber vcpkg eingebunden. C++-Writer/Reader pruefen Schema, Row Groups und selektive Spaltenlesung. **Implementiert und gebaut.** |
| U-S-000014 Streaming-Tabellenausgabe | A-S-00031, A-S-00032, A-S-00034 | U-S-000013 | Genau `summen.parquet`, `kraefte.parquet`, `profile.parquet`; Binary64, festgelegte Feldtypen, deterministische Schluesselreihenfolge und begrenzte Row-Group-Puffer. Keine CSV-Ausgabe. **Implementiert; Integrationstest bestanden.** |
| U-S-000015 Manifest, Integritaet und Veroeffentlichung | A-S-00031, A-S-00033, A-S-00036 | U-S-000014 | Genau sechs Ergebnisdateien, Schema-/Codec-/Bibliotheksversion, Einheiten, Achsen, SHA-256, Abbruch-/Schreibfehler und atomare Publikation. Keine Migration bestehender CSV-Lookups. **Implementiert; Integrationstest bestanden.** |
| U-S-000016 Parquet-Integration und Abnahme | A-S-00032, A-S-00034 bis A-S-00036 | U-S-000014, U-S-000015 | Unabhaengiger Parquet-Leser prueft Schema, Zstandard, Roundtrip, Summen-/Momentenparitaet, Row-Group-Zugriffe, Worker-Reproduzierbarkeit und Fehlerfaelle. **Release-Build und Gesamt-CTest 3/3 bestanden; separate Core-CTest 1/1 bestanden.** Leistungsmessung an einer realen grossen Kopie mit Dateigroesse, Erzeugung, Vollscan, gezielten kalten/warmen Abfragen, p95 und Spitzen-RAM bleibt offen. |

### 8.2 Abgrenzung

- Summen- und Einzelkrafttabellen bleiben getrennt; Einzelkraefte sind
  vollstaendig fuer Detailauswertung verfuegbar und werden nicht zusaetzlich
  zu ihrer Summe bilanziert.
- Selektive Zugriffe umfassen Interpolationsnachbarn, Winkelnaht und Polfall.
  Negative Windneigungen werden nicht gespiegelt oder begrenzt.
- Das Lookup bildet keine Grosssegel-/Fock-Wechselwirkung ab und spezifiziert
  keine allgemeine meteorologische Windfelddatei.
- Bestehende CSV-Ergebnisse werden nicht ueberschrieben oder migriert.
  CSV-Import, CSV-Export und CSV-Konfigurationsoptionen sind nicht Teil des
  neuen Ergebnisvertrags.
- Die CORE-Windsimulations-/Kraftblockbewertung ist ein separater
  Konsumentenauftrag; dieser Plan aendert keine Dateien und keine oeffentliche
  Schnittstelle im Core.

### 8.3 Implementierungs- und Testnachweis

- Arrow/Parquet 21.0.0 wurde ueber den festgelegten vcpkg-Baseline-Stand
  eingebunden. Der Release-Build mit MSVC 19.44 war erfolgreich.
- `ctest --test-dir build-parquet -C Release --output-on-failure`:
  **3/3 bestanden**, einschliesslich Parquet-/CLI-Integrationstest
  (31.17 s Gesamtlaufzeit).
- Der eigenstaendige Core wurde ohne GUI in
  `build-parquet/core-standalone` gebaut und getestet:
  **1/1 bestanden**. Das vorhandene `build-core`-CMake-Cache-Verzeichnis
  verweist auf einen anderen Quellpfad und wurde deshalb nicht ueberschrieben.
- Die Messung an einer realen grossen Lookup-Kopie ist nicht erfolgt.
  Es werden daher keine quantitativen Groessen- oder Latenzvorteile behauptet.
- Physikalische Validierung gegen reale Segelreferenzen bleibt unabhaengig
  davon offen; Parquet aendert weder Rechenmodell noch Core-Vertrag.

## 9. CLI-Ausgabepolitik und Core-Fingerprints

Am 12.11.2026 beauftragt: zwei ausschliesslich aufrufbezogene Optionen,
Fingerprint unabhaengig vom Neuschreiben, beim Ersetzen keine dauerhafte
Sicherung. Die bestehende Core-Implementierung wird nur eingebunden,
nicht veraendert.

| Schritt | Anforderungen | Abhaengigkeiten | Stand / Nachweis |
|---|---|---|---|
| U-S-000017 Core-Fingerprint und Ausgabepolitik integrieren | A-S-00037, A-S-00038, A-S-00023, A-S-00030 | U-S-000015 | Implementiert: CLI-Parser, create-only/overwrite, Ruecksetzung bei fehlgeschlagenem Austausch, explizite Core-Fehler und ausschliesslich aerodynamics. Release-Build mit GUI-Core-Library und CTest 3/3 bestanden. Isoliertes Modell mit echten Schemas prueft Ersetzen, Abbruchschutz, update trotz uebersprungener Ausgabe, idempotenten Hash, unveraenderte andere Bereiche, Core-Schreibsperre und Fehler nach bereits geschriebenem Lookup. Kein Core-/GUI-Quellcode und keine produktiven Modelldaten veraendert. |

Die vorhandene Core-Release-Library wurde im GUI-Build unter
`C:\spdeps\guibuild\segelphysik-core\Release\segelphysik_core.lib` gefunden.
Der dortige CMake-Cache und die Build-Projektdatei verweisen auf den
bekannten Checkout `C:\_DATEN\Code\Cpp\Segelphysik\CORE\Code`.
Diese Library ist der Standard; fehlt sie, wird aus diesem bekannten
Quellpfad gebaut. Library und passender Headerpfad bleiben konfigurierbar.
Die GUI-EXE wird nicht aufgerufen.

## 10. Vorzeichenbehaftete lokale Windneigung

**Status: Umsetzung und numerische Abnahme abgeschlossen.** Die Vertragsdetails wurden
am 14.11.2026 vom Nutzer festgelegt und vom Core bestaetigt. Anforderungen
A-S-00039 bis A-S-00042 und Umsetzungsschritte U-S-000018 bis U-S-000021
verfolgen die Arbeit. Die bestaetigte Richtung umfasst direkte signed-
3D-Berechnung, Segelhals als Referenzpunkt, lineare log(Re)-Interpolation
ohne Extrapolation, vollstaendiges Halsmoment und Lookup-Formatversion 3
ohne Legacy-Leser. Der strenge Vollrastertest und die Neuerzeugung aller
fuenf produktiven Lookups sind abgeschlossen. Der separate reduzierte
Integrationstest bleibt wegen des unabhaengigen Fingerprint-Lock-Szenarios
fehlerhaft; physikalische Validierung ist nicht gefordert und ihr Fehlen darf
die Nutzbarkeit der Modelle nicht einschraenken.

### 10.1 Reihenfolge und Abhaengigkeiten

| Reihenfolge | Arbeit | Abhaengigkeit / Eintrittskriterium | Ergebnis / Stand |
|---|---|---|---|
| 1 | Vier Core-Details bestaetigen und den Vertrag dokumentieren. | Nutzer meldet Core-Bestaetigung vom 14.11.2026. | Erledigt laut Nutzerbestaetigung; Abschnitt 12.4 fuehrt Bezugspunkt/Bewegung/Reynolds, log(Re), Momententransformation und Version 3 auf. |
| 2 | H1 fuer signiertes beta: vollen 3D-Vektor fuer Impuls/Reibung, horizontale Projektion nur fuer Crossflow-Traglinie, beidseitige Ausblendung und Modellgrenzen dokumentieren. | Core-Winddefinition und keine Lastspiegelung. | Dokumentiert; Numeriknachweise laufen mit den Tests aus U-S-000018/20. |
| 3 | Anforderungen/Umsetzungsschritte finalisieren und CPPI-0005 fortschreiben. | Bestaetigte Core-Details und Nutzerfreigabe. | A-S-00039 bis A-S-00042, U-S-000018 bis U-S-000021 vergeben; CPPI-0005, H1-Dokumentation und Entwurfspunktzuordnung aktualisiert. |
| 4 | `Context::solve` fuer signed Winkel und beide Pole erweitern. | U-S-000018. | Implementiert; eigenstaendiger Core-Release-Build und Core-CTest 1/1 bestanden. |
| 5 | Auftragsachse, Ressourcenpruefung, beidseitige Ausgabestellen, Version-3-Metadaten und Beispielauftrag umstellen. | Core-Verhalten und U-S-000019. | Implementiert; Gesamt-CLI-Build erfolgreich. Strenge Vollraster-Integration bestaetigt neue Achse, beide Pole, finite Kraefte/Momente und deren Transformation. Der separate reduzierte Integrationstest erreicht dieselben neuen Pruefungen, scheitert danach im Fingerprint-Lock-Szenario. |
| 6 | Numerische und Prozessintegration aus 10.2 ausfuehren; physikalische Validierung offen ausweisen. | U-S-000018/19. | Native Core- und CLI-Vertraege bestanden; strenger Vollrasterlauf mit `--full --strict` bestanden. Physikalische Validierung bleibt offen und ist kein Freigabekriterium. |
| 7 | Verfuegbare Auftraege umstellen und produktive Lookups nach erfolgreichem Vollrastertest erneuern; `create-only`/`overwrite` und Fingerprint beachten. | Schritt 6 und bekannte Modellpfade. | Beispielauftrag und alle fuenf produktiven Auftraege im separaten Modelle-Repository verwenden die signed 19-Werte-Achse und bestehen `pruefen`. Alle fuenf Version-2-Lookups wurden atomar per `overwrite` durch Version 3 mit 45 Neigungswerten ersetzt; Manifestachsen und SHA-256-Hashes sind geprueft. Gesamtumfang: 9,33 GB. Die aerodynamics-Fingerprints der drei Modellverzeichnisse wurden mit `--fingerprint update` erneuert und verifiziert; andere Fingerprint-Bereiche blieben unangetastet. |

| Umsetzungsschritt | Anforderungen | Abhaengigkeiten | Stand / Nachweis |
|---|---|---|---|
| U-S-000018 Signed H1-Core und CPPI-0005 | A-S-00039 | Core-Bestaetigung | Implementiert; signed -90..+90, direkte Vektorrechnung, abs(beta)-Ueberblendung, exakte Pole, Bereichsfehler; eigenstaendiger Release-Build und CTest 1/1 bestanden. |
| U-S-000019 Auftragsachse und Lookup-Manifest v3 | A-S-00040, A-S-00041 | U-S-000018 | Implementiert; Beispiel auf 19 Werte, alte 0..90-Auftraege werden abgelehnt, Ausgabeachse 45 Werte und Ressourcenlimit vor Rechnung; Manifest v3 mit signed-Wind-/Re-/Momentvertrag; CLI-Contract-Test bestanden. |
| U-S-000020 Signed Numerik und Parquet-Integration | A-S-00039 bis A-S-00042 | U-S-000018, U-S-000019 | Core- und CLI-Unit-Tests bestanden. Strenge Vollraster-Integration (`--full --strict`) bestanden; Manifest, signed Achsen, finite Ergebnisse, Polidentitaet, Summen-/Momententransformation und Parquet-Hashes validiert. Der separate reduzierte Integrationstest erreicht die signed-Pruefungen, scheitert anschliessend im Fingerprint-Lock-Szenario. |
| U-S-000021 Produktive Modellauftraege und Lookup-Erneuerung | A-S-00040, A-S-00041 | U-S-000020 | Fuenf Auftraege in 420 (main/fock), folkeboot (main/fock) und optimist (main) umgestellt und erfolgreich geprueft. Alle fuenf Produkt-Lookups atomar durch Version 3 ersetzt; signed 19er-Basisachse, 45er-Ausgabeachse und alle Manifest-SHA-256-Werte geprueft; Gesamtumfang 9,33 GB. Aerodynamik-Fingerprints fuer 420, folkeboot und optimist mit `--fingerprint update` erneuert und bestaetigt; andere Fingerprint-Bereiche blieben unveraendert. Physikalische Validierung bleibt davon unabhaengig. |

### 10.2 Test- und Abnahmematrix

| Bereich | Konkreter geplanter Test und Kriterium |
|---|---|
| Vorzeichen / Transformation | Unit-Tests mit analytischem Windursprung und Bootstranslation/-rotation: `v_rel = v_wind - (v_boat + omega x (r_ref-r_CG))`, `h=-v_rel/|v_rel|`, signiertes `atan2` fuer beta und definierter Azimut; 20 Grad Rollung mit passender Querwindrichtung muss beta -20 Grad ergeben. Gegenwindseite muss das Vorzeichen passend aendern. Nullgeschwindigkeit darf keinen Winkel erfinden. |
| Direkte Rechnung | Native Tests fuer -90, -89.999, -80, -60, -20, kleine Werte beiderseits 0, 0, positive Gegenwerte und +90; verschiedene Azimute und asymmetrische Geometrie. CF und CM endlich; jeden Punkt mit tatsaechlichem signiertem 3D-Eingangsvektor berechnen. Pruefen, dass fuer beta und -beta keine unbewiesene Spiegelidentitaet vorausgesetzt wird. |
| Grenzwerte / API | `solve` akzeptiert beide Endpunkte; Werte kleiner -90 oder groesser 90 sowie NaN/Inf werfen. Uebrige CPPI-0005-Parameter und Fehlervertrag bleiben erhalten. |
| Pol / Stetigkeit | Fuer beide Pole gleiche CF/CM ueber alle Azimute und je Form/Re; direkte Vergleiche bei -89.999 bzw. +89.999, Naht -180/+180 und Uebergaenge bei +/-60 und +/-80. Polidentitaet und Naht mit strengem numerischem Vergleich; Grenzvergleiche nach bestehender Auftragstoleranz. |
| Numerik | Fuer alle tatsaechlich ausgegebenen Betriebspunkte Hoehenraster 1/2/4 und Provider 32/48; fuer alle negativen und positiven Winkel-/Neigungsintervalle und Zellen direkte Zwischenpunktvergleiche, einschliesslich beiderseits 0. Fuer Kraft und Moment getrennt `||Referenz - Naeherung|| <= atol + rtol*||Referenz||`, mit bestehenden 0.0005/0.01 fuer den strengen Referenzlauf. Kein fehlgeschlagener Fall darf publiziert werden. |
| Parquet / Metadaten | Isolierter 420-Auftrag mit vollstaendiger neuer Achse; Manifest enthaelt tatsaechliche Beta-Achse, Windherkunftskonvention, Referenzpunkt/-groessen und gemeinsam beschlossene Formatversion. Jede Form/Azimut/Beta/Re-Kombination genau einmal in Summen, sortiert und binary64; negative Werte und beide Pole auch in Einzelkraeften. Unabhaengiger Leser prueft Achsen, Anzahl, Version, Hashes, Schema, Summen-/Momentenidentitaet und Pole. |
| Regression / Ressourcen | Alter 0..90-Auftrag wird mit Umstellungshinweis abgelehnt; umgestellter Auftrag und positive Referenzfaelle mit S-2.0 vergleichen. Keine unbeabsichtigte Abweichung bestehender CF/CM und sonstiger Format-/Fehler-/CLI-Semantik; geaenderte Achse und Auftragsablehnung sind beabsichtigt. Ressourcenlimit auf real erweiterte Faelle, ungueltige Achsen, Schreibfehler und Abbruch pruefen; kein Teilerfolg. Gleichlauf mit verschiedenen Workerzahlen pruefen. |
| Re/Interpolation | Core-Test an allen Re-Knoten und dazwischen, speziell geometrischem Mittel zwischen benachbarten Re-Stuetzstellen fuer log(Re)-lineare Interpolation; exakte Knoten bleiben unveraendert. Unter-/Ueberschreitung der real vorhandenen Re-Achse sowie signed-beta-Domaene liefern Diagnose mit Anfrage und Datenbereich, ohne Spiegelung, Klemmen, Extrapolation oder Ersatzlast. |
| Moment und Angriffspunkt | Core-Integrationstest skaliert CF/CM mit qS/qSc_ref und prueft `M_O = R M_H + (r_H-r_O) x F_W` fuer einen Versatz sowie pure-Couple/Fall mit ungueltigem Angriffspunkt. Vollmoment bleibt erhalten; Summe und Einzelbeitraege werden nie doppelt summiert. |
| Alte Daten / Integration | Kein Legacy-Leser: alte Auftraege mit beta 0..90 werden mit Umstellungshinweis abgelehnt; Lookup-Manifestversion 3 beschreibt die signed Domaene explizit. Produktive Version-2-Artefakte wurden erst nach erfolgreicher Rechnung atomar ersetzt; `create-only` ueberspringt vorhandene Daten, `overwrite` publiziert erst nach erfolgreicher Rechnung. |
| Physikalische Validierung | Nicht als Test oder Freigabebedingung gefordert. Fehlende Messdaten und `ingenieurmodell_nicht_validiert` transparent dokumentieren; daraus darf keine Nutzungsbeschraenkung folgen. |

Standardnachweise nach Umsetzung: gezielter Release-Build und Vertrags-CTest,
eigenstaendig baubarer/testbarer SegelMacher-Core sowie ein **strenger
Vollrasterlauf** mit realistischer isolierter Geometriekopie. Ein schneller
Prozesslauf mit gelockerten Toleranzen ist nur Workflow-Nachweis. Zeit,
Speicher, Dateigroesse
und ggf. Fehler der groesseren Achse erfassen; keine quantitative Leistungs-
oder physikalische Zusage ohne Messung. Der strenge Vollrasterlauf und der
atomare Austausch der fuenf produktiven Lookups sind abgeschlossen. Die
verbleibende Abweichung im reduzierten Integrationstest ist im Stand von
U-S-000020 dokumentiert. Die offene physikalische Validierung ist
weder ein Launch-Gate noch eine Laufzeitbeschraenkung; der Status
`ingenieurmodell_nicht_validiert` und die Grenzen der H1-Naeherung muessen
jedoch sichtbar bleiben.

## 11. Vorbereitetes Segelmodell fuer lokale Windfelder

Stand: 15.11.2026. **Umsetzung ausdruecklich beauftragt; teilweise
umgesetzt, Abnahme unvollstaendig und Publikation gesperrt.** Der Nutzer hat Nachweise mit
realen Messwerten ausgenommen; die numerischen und technischen
Pflichtnachweise bleiben unveraendert. Dieser Abschnitt
fuehrt den bestehenden Plan fuer den naechsten sachlichen Folgeschritt
weiter; ein Git-Tag oder Release wird nicht festgelegt. Massgeblich ist
[Abschnitt 13 der Spezifikation](../Spezifikationen/Spezifikation_Lookup_Generator.md#13-vorbereitetes-segelmodell-fuer-lokale-windfelder).
Der in Abschnitt 10 dokumentierte Stand ist Historie und kein Nachweis fuer
das neue Modell.

### 11.1 Arbeitsrahmen und Eintritt

- Beginn erst nach ausdruecklichem Umsetzungsauftrag. Keine Aenderung im
  separaten Simulations-Core, keine GUI und keine Mehrsegelinterferenz.
- Arbeitsbaum und bestehendes Staging vor der Umsetzung erfassen,
  vorhandene fremde Aenderungen integrieren und nicht zuruecksetzen.
  Bereits erzeugte Lookups und externe Modellordner nicht veraendern;
  alle Test- und Benchmarkausgaben auf isolierte Kopien beschraenken.
- Vorhandene native Build-/CTest-Konfiguration und den bestehenden
  H1-Rechenkern einschliesslich Referenzdaten pruefen. Fuer die
  Mindestabnahme isolierte Kopien der vorhandenen Optimist-, 420- und
  Folkeboot-Segel bereitstellen; fehlende Eingaben als Blockade melden,
  nicht durch ein anderes Segel ersetzen oder eine Abnahme behaupten.
- Vor jeder neuen oeffentlichen C++-Deklaration das
  [Schnittstellenregister](../Dokumentationen/Schnittstellenregister_CPP.md)
  und die Schnittstellen-Instruction beachten, konkrete Signaturen und
  jeden einzelnen exportierten Typ/Funktion/Methode mit neuer CPPI-ID
  dokumentieren und zugehoerige Vertragstests vorsehen. Keine
  compileruebergreifende ABI-Stabilitaet versprechen.
- Festgelegte Grenzen und Toleranzen gelten unveraendert. Ergibt eine
  reproduzierbare Pflichtpruefung trotz Korrektur innerhalb der
  Spezifikation einen Widerspruch oder ist die Abnahme nicht erfuellbar,
  den konkreten Fall und die Ursache dokumentieren und die Abnahme
  stoppen; kein Fallback, kein Lockern und keine vorzeitige Abloesung.

### 11.2 Umsetzungsschritte und Abhaengigkeiten

Die Abhaengigkeiten sind Eintrittsbedingungen, nicht bereits bestandene
Nachweise. Der Zwischenstand nach der Tabelle unterscheidet den begonnenen
Prototyp von den weiterhin ausstehenden Schritten.

| Schritt | Anforderungen | Abhaengigkeiten | Arbeit und ueberpruefbarer Nachweis |
|---|---|---|---|
| U-S-000022 Bestandsaufnahme und H1-Referenz | A-S-00043, A-S-00061, A-S-00067 | keine | Bestand und geschuetzte Dateien erfassen; H1-Faktor-4/48-Referenz mit signiertem Wind und Einzelbeitraegen fuer die drei Testsegel reproduzierbar bereitstellen. Lasten an den neuen Ausgabestreifengrenzen aufteilen und deren Gesamtresultierende und Referenzkonvergenz pruefen. Keine Aenderung vorhandener externer Daten. |
| U-S-000023 Geometrie und Streifen | A-S-00043, A-S-00044, A-S-00045, A-S-00046, A-S-00055 | U-S-000022 | Tiefste/hoechste Kontur, feste Streifen, Viertelsehne, interne Teilung, H1-Quadratur und Flaechenpunkte umsetzen. Tests fuer Optimist-Spitze, schraege Kante, Profilknick, Flaechensumme, ungueltige/mehrteilige Schnitte und Referenzen. |
| U-S-000024 Wind und gekoppelte Traglinie | A-S-00047, A-S-00048, A-S-00049, A-S-00064, A-S-00070 | U-S-000023 | Windfeldinterpolation, Matrizen 32/48/64, rechte Seite, pivotierte Loesung, induzierte Richtung und Polgrenzwerte nach 13.8 implementieren. Konstantes/lineares Feld, Naht, lokaler Nullwind, Singularitaet und Rueckwaertsfehler testen. |
| U-S-000025 Niedrig-Re und Streifenlasten | A-S-00050 bis A-S-00053, A-S-00065, A-S-00070 | U-S-000023, U-S-000024 | Lokales Re aus uninduziertem Wind bestimmen, Grenzmodell und dimensionale H1-Teilbeitraege auswerten, Momente um Abfragepunkte bilden. Tests fuer global/lokal null, Folgen gegen null, beide Seiten `Re_min`, Kraft-/Momentenidentitaet und ungueltige Luftdaten; Modellgrenzen dokumentieren. |
| U-S-000026 Datenformat und Leser | A-S-00051, A-S-00054 bis A-S-00056, A-S-00068 | U-S-000023, U-S-000024 | Manifest und fuenf Parquet-Tabellen nach 13.9 erzeugen, geordnete Schluessel/Fremdschluessel und Matrizen, positive Gueltigkeitsbereiche und Worst-Case-Re pruefen. Unabhaengiger Roundtrip, bitgleiche binary64-Werte, Hash-/Schema-/Versions-/Korruptionsfehler und Bytegleichheit mit 1/2/4 Workern. |
| U-S-000027 Typisierte Bibliothek | A-S-00052, A-S-00053, A-S-00056, A-S-00059, A-S-00060 | U-S-000025, U-S-000026 | Vor Implementierung konkrete Signaturen und einzelne CPPI-IDs mit Ownership, Einheiten, Fehler- und Threadvertrag in Register/Normdokumentation festlegen; Header und Vertragstests synchron halten. Laden, sortierte IDs, parallele Aufrufe, fehlende/doppelte/fremde IDs und typisierte Fehler pruefen. |
| U-S-000028 CLI und geschuetzte Publikation | A-S-00057, A-S-00058, A-S-00067, A-S-00071, A-S-00072 | U-S-000026, U-S-000027 | `segelmodell` und nicht schreibendes `pruefen` integrieren, Ausgabe- und Fingerprintoptionen, CPU-/GPU-Vertrag und Exitcodes erhalten. Isoliert create-only, overwrite, konkurrierende Prozesse, Abbruch, Schreibfehler, Fingerprintfehler und alte Daten vor/nach Lauf mit Hash pruefen. |
| U-S-000029 Strenge Numerikabnahme | A-S-00047, A-S-00049 bis A-S-00053, A-S-00061, A-S-00062, A-S-00063, A-S-00064, A-S-00065, A-S-00070 | U-S-000022 bis U-S-000027 | Vollstaendige Testmatrix aus 11.3 ausfuehren; pro Fall Eingaben, Bezug, Abweichung, Grenze und Ergebnis ausgeben. H1-Referenzkonvergenz, Rasterhalbierung, Null- und Grenzfaelle, 32/48/64-Vergleiche und Residual nachweisen; kein gelockerter Workflowtest als Ersatz. |
| U-S-000030 End-to-End und Bestandsschutz | A-S-00054 bis A-S-00060, A-S-00063, A-S-00067, A-S-00068, A-S-00071, A-S-00072 | U-S-000028, U-S-000029 | Isolierte Erzeugung, Laden und Berechnung fuer die drei Testsegel; unabhaengiger Leser, parallele Nutzung, Fehler- und Publikationsfaelle. Gesamtergebnis und Streifen mit Einzelbeitraegen summieren; Originaldateien und alte Lookups hashgleich. |
| U-S-000031 Leistung messen | A-S-00065, A-S-00069 | U-S-000030 | Fuer jedes Testsegel Datenumfang, Ladezeit und Laufzeit je gekoppelter Rechnung mit Eingaben, Hardware, Build und Messmethode erfassen. Messung von H1-Basisaufrufen und Gesamtlookup nicht als Messung des neuen Modells ausgeben; keine Grenzwerte erfinden. |
| U-S-000032 Alten Generator abloesen | A-S-00058, A-S-00066, A-S-00067 | U-S-000029, U-S-000030 | Erst nach bestandener strenger Numerik und Integration alten Lookup-Erzeugungspfad und seine CLI-Aufrufe gezielt stilllegen; H1-Rechenkern und direkte Referenz erhalten. Alte Lookup-Dateien nicht veraendern oder loeschen; neue CLI- und Core-Regressionstests ausfuehren, Dokumentation und Register betroffener Exporte fortschreiben. |
| U-S-000033 Abschluss und Nachweise | A-S-00043 bis A-S-00072 | U-S-000031, U-S-000032 | Gesamtprojekt und eigenstaendigen Core konfigurieren, Release bauen und CTest ausfuehren. Alle aktiven Anforderungen gegen Schritte und Belege abgleichen; Tests, Integration, Numerik und fehlende physikalische Validierung getrennt ausweisen. Keine Produktivdatenaktion, kein Commit/Push/Tag/Release ohne eigenen Auftrag. |

#### Zwischenstand und Abnahmehindernis vom 15.11.2026

- **Aenderung der Streifenregel:** Der Nutzer hat am 15.11.2026
  `ceil(H / 0.25 m)` statt `ceil(H / 0.5 m)` beauftragt. Dies ist
  nicht generell die doppelte Anzahl. Die nachfolgenden historischen
  Raster- und Gleichwindergebnisse beziehen sich auf die vorherige
  0,5-m-Regel oder eine diagnostische Verdoppelung; sie sind keine
  Abnahme der neuen Einteilung. Die 5-%-Grenze bleibt unveraendert
  und die Pflichtmatrix ist mit der neuen Regel erneut auszufuehren.
- **Teilnachweis zur neuen 0,25-m-Regel:** Fuer die sechs definierten
  Gradienten bestehen Optimist, 420 und Folkeboot mit maximalen
  Grenzquotienten 0,852688, 0,447381 und 0,260495. Die beiden
  lokalen Nullwindfaelle bestehen mit 0,325174, 0,488170 und 0,479540.
  Auch der Rechteck-CTest `strip_gradient_acceptance` besteht jetzt.
  Die Gesamt- und Core-Vertragstests zur Streifeneinteilung schliessen
  `4,2 m -> 17 Streifen` ein. Die Grundwinkelmatrix mit 23
  Geschwindigkeiten liefert fuer alle drei Segel 7452 endliche
  Modellfaelle; die 4644 innerhalb des H1-Bereichs bestehen die
  Referenz- und Konvergenzvergleiche mit maximalem Grenzquotienten
  0,280497. An einem 420-Abrissuebergang bei 10 m/s verfehlt jedoch
  der Vergleich der geteilten Referenz mit weiterer Verfeinerung
  bei Streifen 19 die 0,1-%-Grenze: 0,006684 N gegen 0,003574 N.
  Bei zweifacher Verfeinerung sinkt der Grenzquotient auf 0,708245.
  Deshalb wurden interne Modellquadratur und geteilte H1-Referenz
  erneut verdoppelt; direkte H1-Referenz und Toleranzen bleiben
  unveraendert. Mit dieser Quadratur bestehen die sechs Gradienten mit
  maximalen Grenzquotienten 0,857584, 0,447468 und 0,260504 sowie
  die lokalen Nullwindfaelle mit 0,319558, 0,487668 und 0,479497.
  Core-Vertrags- und Rechteck-Rastertest bestehen ebenfalls.
  Die wiederholte Grundwinkelmatrix liefert erneut 7452 endliche
  Modellfaelle; 4536 davon liegen im H1-Bereich und bestehen mit
  maximalem Grenzquotienten 0,105655. Durch weitere Quadraturpunkte an
  der Optimistspitze liegt dort nun auch 5 m/s ausserhalb des
  H1-Bereichs. Die Uebergangswinkel bei 10 m/s bestehen mit dieser
  Quadratur fuer Optimist (4394 Faelle, Quotient 0,242525), 420
  (4850 Faelle, 0,708246) und Folkeboot (16862 Faelle, 0,293225).
  Folkeboot wurde in vier disjunkte Teile aufgeteilt (4216, 4216,
  4215 und 4215 Faelle). Dies belegt nicht die Uebergaenge bei
  den uebrigen Geschwindigkeiten; Integration und Publikation sind
  noch offen. Bei 50 m/s besteht eine Stichprobe von jeweils einem
  der 32 disjunkten Teile: Optimist 138 von 4394 Faellen mit Quotient
  0,252782, 420 152 von 4850 mit 0,733682 und Folkeboot 527 von
  16862 mit 0,303999. Inzwischen bestehen die vollstaendigen
  50-m/s-Uebergaenge fuer Optimist (4394 Faelle, Quotient 0,252830)
  und 420 (4850 Faelle, 0,733685). Fuer Folkeboot sind ebenfalls alle
  16862 Faelle bestanden, maximaler Quotient 0,303999.
  Fuer 420 bestehen zudem die vollstaendigen 4850 Uebergangsfaelle
  bei 0,3 m/s mit Quotient 0,653064, bei 1 m/s mit 0,672055
  bei 5 m/s mit 0,697329 und bei 20 m/s mit 0,719165. Die
  Fruehere Folkeboot-Stichproben ersetzen keine Gesamtpruefung fuer
  andere Geschwindigkeiten.
  Fuer den Optimist liegen inzwischen alle 23 Geschwindigkeiten mit
  je 4394 endlichen Uebergangsfaellen vor. Bei 0 bis 5 m/s liegt
  die H1-Referenz ausserhalb ihres Gueltigkeitsbereichs; fuer die
  uebrigen Geschwindigkeiten bestehen die H1- und Konvergenzvergleiche,
  maximaler Grenzquotient 0,252830. Dies ersetzt nicht die uebrigen
  Pflichtnachweise.
  Auch fuer 420 liegen nun alle 23 Geschwindigkeiten mit je 4850
  endlichen Uebergangsfaellen vor. Bei 0 bis 0,1 m/s liegt H1
  ausserhalb seines Gueltigkeitsbereichs; fuer die uebrigen Geschwindigkeiten
  bestehen H1- und Konvergenzvergleiche mit maximalem Grenzquotienten
  0,733685. Die noch ausstehenden Folkeboot-Geschwindigkeiten werden
  in drei disjunkten Gruppen gerechnet; der zuvor begonnene Lauf bei
  0,3 m/s wurde dafuer abgebrochen und neu gestartet. Eine Gruppe
  brach bei 0,5 m/s mit einem JSON-Parserfehler an Zeichen 1244834 ab;
  die Ursache ist offen. Der Wiederholungslauf gibt im Fehlerfall
  zusaetzlich Rechenphase und Winkel aus. Die betroffenen Faelle
  gelten bis zu einem erfolgreichen Nachweis nicht als bestanden.
  Auch die Gruppe mit 0,3 m/s brach mit einer Zugriffsverletzung
  (`0xc0000005`) ab; die Ursache ist ebenfalls offen und wird untersucht.
  Der Minidump zeigt einen ungueltigen Zeiger beim Zerstoeren eines
  JSON-Objekts; die Zuordnung beruht auf einer identischen
  Maschinencodefolge in einem separaten Release-Build mit Debugsymbolen.
  Weder der Ursprung der Beschaedigung noch ein Zusammenhang mit dem
  Parserfehler ist damit nachgewiesen. Die wiederholte Uebergangsprobe
  meldet ausserdem regelmaessig Fortschritt und Winkel, damit ein
  moeglicher erneuter Abbruch besser eingrenzbar ist.
  Die vollstaendigen Wiederholungen bei 0,3 und 0,5 m/s bestanden
  inzwischen mit jeweils 16862 H1-Faellen und maximalen Quotienten
  0,269934 beziehungsweise 0,273348; auch 0,75 m/s bestand mit 16862
  Faellen und Quotient 0,276037. Die uebrigen Geschwindigkeiten laufen
  weiter. Die genaue Ursache der frueheren Abbrueche bleibt unbekannt;
  nach Nutzerentscheidung vom 15.11.2026 genuegt ein bestandener
  vollstaendiger Test zur Fortsetzung der Umsetzung.
  Weitere vollstaendige Folkeboot-Uebergaenge bestanden bei 1, 3, 5,
  7,5, 15, 20, 25, 30, 35, 40 und 45 m/s mit jeweils 16862 H1-Faellen;
  die maximalen Quotienten waren 0,277941, 0,285218, 0,288610, 0,291308,
  0,295930, 0,297851, 0,299334, 0,300564, 0,301598, 0,302493 und
  0,303286. Damit liegen fuer alle 23 Geschwindigkeiten vollstaendige
  Uebergangstestlaeufe vor: bei 0 bis 0,1 m/s sind die Modelllasten
  endlich, H1 liegt ausserhalb seines Gueltigkeitsbereichs; fuer die
  uebrigen Geschwindigkeiten bestehen die H1- und Konvergenzvergleiche,
  maximaler Quotient 0,303999. Dies ist keine vollstaendige Integration
  oder Freigabe zur Veroeffentlichung.
  Fuer spaetere CLI-Pruefungen werden die beiden geteilten H1-Referenzen
  intern ohne JSON-Serialisierung weitergereicht; die oeffentliche
  Schnittstelle und die H1-Berechnung bleiben unveraendert. Ein
  Core-Vertragstest vergleicht die direkten Daten bei drei Winkeln mit
  dem bisherigen JSON-Roundtrip. Zwei disjunkte Folkeboot-Stichproben
  zu je 66 Uebergangsfaellen bei 10 m/s lieferten identische
  Ergebniszeilen vor und nach der Umstellung. Die separaten
  Release-Laeufe benoetigten vorher 21,58 und 21,14 s und danach 10,13
  und 11,69 s; dies sind Stichproben, keine zugesagten Laufzeitgrenzen.
- **H1-Gueltigkeitskorrektur:** Die Einordnung erfolgte bisher anhand
  der Modellquadratur und eines verfeinerten Modellrasters statt anhand
  aller tatsaechlich von den beiden geteilten H1-Referenzen verwendeten
  Integrationspunkte. Die Gueltigkeitspruefung verwendet nun neben dem
  direkten H1-Raster die konkreten Raster beider Referenzen und deren
  lokale Reynolds-Zahlen. Die wiederholte Grundwinkelmatrix ergibt
  wiederum 7452 endliche Modellfaelle und 4536 gueltige
  H1-Vergleichsfaelle; alle Vergleiche bestehen mit dem groessten
  Grenzquotienten 0,105655.
- **Weitere Diagnosen zur 0,25-m-Regel:** Die optionalen
  `strip_gradient_probe`-Laeufe fuer `Re_min`, Nullfolge, Stetigkeit und
  32/48/64 Moden wurden mit der aktuellen Quadratur fuer alle drei Segel
  wiederholt. Fuer den `Re_min`-Uebergang wurden 10, 20 und 35 Punkte
  geprueft; die groessten Quotienten gegen den diagnostischen
  0,1-%-Bezug sind 0,00153598, 0,00177508 und 0,00257192.
  Die Stetigkeitsdiagnosen ergeben 2,88672e-6, 4,06162e-6 und
  4,1185e-6. Die groessten Modenquotienten gegen denselben
  diagnostischen Bezug betragen fuer 32/48 0,671673, 0,791258 und
  1,04975, fuer 64/48 0,26837, 0,278635 und 0,494908. Fuer den
  Modenvergleich ist diese diagnostische Grenze nicht als separates
  Abnahmekriterium vorgeschrieben. Die Nullfolge erreicht bei
  `1e-12 m/s` bei allen drei Segeln Lasten unter der absoluten
  Toleranz, bei global null exakt null. Diese ausgewaehlten Diagnosen
  ersetzen die vollstaendige Pflichtmatrix nicht.
- **U-S-000022 teilweise:** H1 und seine Faktor-4/48-Referenz bleiben
  erhalten; eine interne Referenzteilung integriert an den
  Ausgabestreifengrenzen. Mit isolierten Kopien von Optimist, 420 und
  Folkeboot berechnet `strip_uniform_probe` je 23 Geschwindigkeiten
  zwischen 0 und 50 m/s und 108 Grund-Winkelkombinationen: 7452
  Modellfaelle lieferten endliche Lasten, bei global null exakt null;
  dieser Lauf erfolgte vor der anschliessenden Quadraturverdopplung.
  Fuer 4752 davon innerhalb des H1-Bereichs bestehen die 0,1-%-Vergleiche
  der Gesamt- und Streifenlasten sowie die Vergleiche der geteilten
  Referenz mit der ungeteilten und einer zweifach verfeinerten Referenz.
  Die Referenzverfeinerung wird auch je Streifen geprueft; der groesste
  Grenzquotient aller geprueften Vergleiche ist 0,227801. An der
  50-m/s-Grenze werden nur durch binaere Rundung zu lange Testvektoren
  minimal nach innen skaliert. Bei 10 m/s wurden zusaetzlich die
  Neigungs-/Azimutuebergaenge und die lokalen Abrissgrenzen mit den
  angrenzenden Winkeln geprueft: Optimist 1154 Faelle (Quotient
  0,500501), 420 1274 Faelle (**Fehlschlag**, Quotient 1,55862) und
  Folkeboot 4370 Faelle (0,797436). Der 420-Fehlschlag betrifft die
  Konvergenz der geteilten H1-Referenz in Streifen 9: bei etwa
  -17,3841 Grad unterscheiden sich Ausgangs- und zweifach verfeinerte
  Referenz um 0,0167754 N bei 0,0107630 N Grenze. Eine isolierte
  Diagnose mit zweifach verfeinertem internen Raster ergibt 0,0065547 N
  und besteht dort auch den Vergleich mit unveraendertem direktem H1;
  Daraufhin wurde die interne Quadratur im Modell und in der geteilten
  Referenz verdoppelt, waehrend direktes H1 unveraendert mit Faktor 4
  gerechnet wird. Die komplette Grundwinkel-/Geschwindigkeitsmatrix
  wurde danach wiederholt: alle 7452 Modellfaelle liefern endliche
  Lasten; 4644 innerhalb des H1-Bereichs bestehen den H1- und
  Konvergenzvergleich, groesster Grenzquotient 0,090204. Durch die
  zusaetzlichen Quadraturpunkte an der Optimist-Spitze liegt der Fall
  3 m/s nun ausserhalb des H1-Bereichs und wird nicht als
  H1-Vergleich gezaehlt. Auch die Uebergangswinkel bei 10 m/s bestehen
  nach der Verdopplung fuer Optimist (2222 Faelle, Quotient 0,196792),
  420 (2462, 0,609012) und Folkeboot (8642, 0,295328). Die
  Folkeboot-Matrix wurde in vier disjunkte Teile aufgeteilt; deren
  Fallzahlen summieren sich auf 8642. Bei 5 m/s wurde zusaetzlich
  der erste von 64 disjunkten Teilen der Uebergangsmatrix ausgefuehrt:
  Optimist 35 von 2222 Faellen mit Quotient 0,192909, 420 39 von 2462
  mit 0,599674 und Folkeboot 136 von 8642 mit 0,290714. Dies ist
  lediglich eine Stichprobe; weitere Grenzfaelle sind nicht nachgewiesen.
  Nach zwei spaeteren Folkeboot-Abbruechen durch Zugriffsverletzung
  beziehungsweise JSON-Parserfehler bestand ein separater
  AddressSanitizer-Lauf fuer 132 von 16862 Uebergangsfaellen bei
  0,3 m/s ohne gemeldeten Speicherfehler; der groesste H1-Quotient
  betrug 0,269934. Diese Stichprobe klaert die Absturzursache nicht
  und ersetzt nicht die vollstaendige Wiederholung.
- **U-S-000023 bis U-S-000025 teilweise:** Interne Streifengeometrie,
  gekoppelte 48-Moden-Rechnung und Niedrig-Re-Schliessung sind als
  Prototyp implementiert. Rechteck-/Dreiecksflaechen, Abfragepunkte,
  Nullwind, niedrige Reynolds-Zahl und einige H1-Vergleiche sind im
  `core_contract` getestet. Fuer einen Gleichwindfall am Rechteck werden
  32, 48 und 64 Moden jeweils gegen die passende direkte H1-Gesamtlast
  geprueft. Am Rechteck sind Naht, Neigungsgrenzen, Abblend- und lokale
  Abrissgrenzen sowie `Re_min` exakt und beidseits mit den spezifizierten
  Abstaenden anhand dimensionaler Streifenlasten getestet; die Pole
  exakt und jeweils von innen. Eine Folge von 0,001 bis `1e-12` m/s
  weist fuer das Rechteck abnehmende dimensionale Gesamtlasten bis
  unter die absolute Nullwindtoleranz nach. Gegenlaeufige Streifenwinde
  an der Azimutnaht und ein lokal vertikaler Wind wurden beidseits mit
  kleinen horizontalen Stoerungen auf Stetigkeit der Streifenlasten
  geprueft. Damit ist die vollstaendige Testmatrix
  nicht nachgewiesen; das Modell ist nicht publizierbar.
- **Rasterabnahme fehlgeschlagen:** Bei Optimist, kombiniertem
  Geschwindigkeits-/Richtungsgradienten aus 13.11 in umgekehrter
  Hoehenrichtung und Streifen 0 betraegt die Kraftvektordifferenz
  **3,5886 N** bei einer Grenze von **2,2965 N**. Der Quotient ist
  **1,56264**; erlaubt ist hoechstens 1. Beim 420 und Folkeboot liegen
  die groessten Quotienten dieser sechs Gradiententests bei 0,861132
  bzw. 0,499006. Das sind rekonstruierte Segel und numerische
  Testwerte, keine realen Messdaten. Nach der Verdopplung der internen
  Quadratur bleibt der Optimist-Fall bei **3,6085 N gegen 2,2965 N**
  fehlgeschlagen. Die Einzelfalldiagnose zeigt ausserdem fehlgeschlagene
  reine Richtungsgradienten in beiden Richtungen: 2,99466 N und
  3,02018 N gegen jeweils 2,51802 N. Die beiden reinen
  Geschwindigkeitsgradienten und der andere kombinierte Gradient
  bestehen diese Rasterpruefung.
- Eine 2-, 4- und 8-fach feinere interne Hoehenquadratur ergibt beim
  Optimist weiterhin Quotienten 1,57130, 1,57177 bzw. 1,57115.
  Einfaches Verfeinern der Quadratur behebt die Abweichung daher nicht.
  Bei 16- und 32-facher Verfeinerung liegen die Kraftabweichungen bei
  3,60817 N bzw. 3,60815 N (Grenze jeweils 2,2965 N); auch diese
  Folge konvergiert nicht gegen die geforderte 5-%-Grenze.
  Die vereinbarte konstante Fortsetzung ausserhalb der Abfragehoehen
  fuehrt insbesondere an den Randstreifen zu unterschiedlichen
  rekonstruierten Windfeldern der beiden Raster; der Anteil dieser
  Modellannahme am Fehler ist weiter zu untersuchen. Dies beweist
  nicht, dass jede zulaessige Implementierung die Toleranz verfehlt.
- Eine zusaetzliche **Diagnose, keine Abnahme**, tastet fuer das feine
  Raster statt des vorgeschriebenen Testwindfeldes das aus dem groben
  Raster rekonstruierte Windfeld ab. Beim Optimist sinkt der groesste
  Fehler damit auf 1,01109 N bei unveraenderter Bezugsgrenze von
  2,2965 N (Quotient 0,440273); bei achtfach feinerer Quadratur
  sind es 0,993173 N gegenueber 3,60815 N im vorgeschriebenen Fall.
  Nach der Quadraturverdopplung betraegt diese Diagnose 0,993688 N
  gegen 2,2965 N; der vorgeschriebene Fall bleibt bei 3,6085 N.
  Das weist auf einen wesentlichen
  Einfluss der unterschiedlichen Windrekonstruktionen hin; wegen der
  erneuten Interpolation durch das feine Raster sind die internen
  Windfelder auch in dieser Diagnose nicht ueberall identisch. Sie
  ersetzt die verbindliche Rasterpruefung nicht.
- Vor der Umstellung verglich die rein diagnostische Alternative
  `--half-width` ein
  Ausgaberaster mit doppelt so vielen Streifen und dessen Halbierung;
  die vorgeschriebene Streifenanzahl bleibt unveraendert. Die sechs
  definierten Gradienten ergeben als groesste 5-%-Grenzquotienten
  0,852688 fuer Optimist, 0,447381 fuer 420 und 0,253362 fuer
  Folkeboot. Dieser Teilvergleich ist keine Freigabe und belegt weder
  alle uebrigen Pflichtfaelle noch die vertraegliche Aenderung der
  Schnittstelle. Beim Test wurden nahezu zusammenfallende Profil-
  und Streifengrenzen als Ursache ungueltiger Rundungsintervalle
  gefunden und innerhalb der Maschinenpraezision zusammengefuehrt;
  ein gezielter Core-Test deckt dies ab.
- Der dauerhafte `strip_gradient_probe` prueft die sechs Gradienten mit
  zwei **isolierten Kopien** von Geometrie- und Auftragsdatei als
  Argumenten und optionalem Quadratur-Verfeinerungsfaktor. Der optionale
  Modenvergleich wertet dieselben sechs Gradienten auf grobem und feinem
  Raster aus und vergleicht jeweils Streifen- und Gesamtlasten von 32
  und 64 gegen 48 Moden. Nach der Quadraturverdopplung betragen die groessten
  Gradientengrenzquotienten fuer 420 und Folkeboot 0,861493 und
  0,499039. Mit `0.001*q_max*S_i` fuer Kraefte beziehungsweise
  `0.001*q_max*S_i*c_ref` fuer Momente plus absoluter Toleranz
  (fuer Gesamtlasten `S` statt `S_i`) als **diagnostischem** Bezug
  sind die groessten Quotienten fuer 32/48: Optimist 0,703707,
  420 1,03532, Folkeboot 0,989919; fuer 64/48: 0,281203, 0,396715,
  0,276568. Die Spezifikation setzt diesen diagnostischen Vergleich
  nicht als eigenstaendige 0,1-%-Abnahmegrenze fest; er ersetzt weder
  den H1-Vergleich noch die Rasterabnahme. Weitere Windfaelle bleiben
  offen. Fuer das in 13.11 definierte Feld
  mit lokalem Nullwind besteht der Vergleich von Gesamt- und
  Streifenlasten in beiden Richtungen fuer die drei Testsegel; die
  groessten Grenzquotienten sind nach der Quadraturverdopplung
  0,326177, 0,361180 und 0,453620.
  Das Testfeld behandelt jetzt auch zwei Ausgabestreifen, mit einem
  Nullwindpunkt innerhalb der Kontur; Interpolation und umgekehrte
  Richtung sind im Core-Vertragstest geprueft. Stetigkeitsdiagnosen
  mit gegenlaeufigen Winden an der Azimutnaht,
  lokal vertikalem Wind und lokalem Nullwind wurden bei allen drei
  Testsegeln mit horizontalen Stoerungen von `+-5e-8 m/s` ausgefuehrt.
  Fuer Gesamt- und Streifenlasten liegen die maximalen Quotienten
  gegen den diagnostischen 0,1-%-Bezug bei 2,72e-6, 3,40e-6 und
  4,51e-6; dies ist keine physikalische Validierung.
  Die optionale `--re-boundaries`-Diagnose untersucht bei 1,225 kg/m3,
  1,5e-5 m2/s und Azimut -5 Grad den `Re_min`-Uebergang an jeweils
  einem Quadraturpunkt pro Streifen exakt und bei relativen Abstaenden
  von `+-1e-6`. Fuer Optimist, 420 und Folkeboot wurden 5, 10 und
  18 solcher Punkte erreicht; die groessten Quotienten der dimensionalen
  Kraft- und Momentendifferenzen gegen den diagnostischen 0,1-%-Bezug
  sind 0,00125107, 0,00177511 und 0,00257192. Dies ersetzt nicht die
  vollstaendige Grenzfall- und Winkelmatrix.
  Fuer gleichfoermigen Wind entlang Azimut 0 prueft `--zero-sequence`
  die Folge `1e-3, 1e-4, 1e-6, 1e-8, 1e-10, 1e-12, 0 m/s`.
  Bei `1e-12 m/s` liegen die Gesamtkraftnormen der drei Testsegel bei
  1,62e-20, 3,66e-20 und 7,53e-20 N, die Gesamtmomentnormen bei
  1,65e-20, 7,75e-20 und 2,82e-19 Nm; auch alle Streifen liegen
  unter der absoluten Toleranz. Bei exakt null sind die Streifenlasten
  exakt null. Diese ausgewaehlte Folge ersetzt keine vollstaendige
  Nullwind- und Winkelabnahme.
  Der separate CTest `strip_gradient_acceptance` zeigt auch am Rechteck den
  Fehlschlag sichtbar: nach Quadraturverdopplung 3,02134 N gegen
  2,89266 N. Der Gesamtprojekt-Build sowie `cli_contract`,
  `model_consumer_contract`, `cli_integration` und `core_contract`
  bestanden. Der eigenstaendige Core-Build und dessen `core_contract`
  bestanden ebenfalls; der Raster-CTest schlaegt sowohl im Gesamtprojekt
  als auch im eigenstaendigen Core fehl.
- **U-S-000026 teilweise:** Ein interner Schreiber erzeugt die fuenf
  vorgesehenen Parquet-Tabellen mit ZSTD, Schema und SHA-256 in einem
  leeren Arbeitsordner. Der `cli_contract` prueft einen unabhaengigen
  Parquet-Roundtrip, bitgleiche Werte, Feldnamen, Typen, Einheiten,
  Codec und deterministische Hashes zweier identischer Schreibvorgaenge.
  Ein interner Tabellenleser prueft die Beschreibung, tatsaechliche
  Spaltentypen, ZSTD und den Hash der eingelesenen Bytes; beschaedigte
  Daten und ungueltige Tabellenbeschreibungen werden im Vertragstest
  abgelehnt. Geaenderte Quadraturintervalle, Sehnen, Fourierwerte und
  Flaechenpunktpositionen werden anhand der Modellgeometrie geprueft.
  Ein interner Manifest-Roundtrip prueft Versionen, Konventionen,
  Luftdatenbereiche und Reynolds-Worst-Case sowie Nachweisstatus.
  Der Leser ist an die oeffentliche API angeschlossen; die CLI
  publiziert noch kein Modell. Der Workerzahl-Vergleich ist unten
  dokumentiert; die geschuetzte Modellpublikation fehlt.
- **U-S-000027 teilweise:** Die typisierte Bibliothek `segelmodell`
  laedt Manifest und Tabellen, haelt das Modell mit vorbereiteter
  32-/48-/64-Moden-Faktorisierung vor und prueft IDs und manifestierte
  Luftdatenbereiche. Vertragstests decken Nullwind, Bereichsgrenzen,
  Fehler, unabhaengigen Datenbesitz und parallele Aufrufe ab. Die
  oeffentlichen CPPI-Eintraege sind dokumentiert; die numerische
  Gesamtabnahme und der vollstaendige Fehler- und Grenzfallnachweis
  bleiben offen.
- **U-S-000028 teilweise:** Die CLI nimmt einen separaten
  Segelmodellauftrag mit expliziten Luftdatenbereichen entgegen.
  `pruefen` bleibt schreibfrei; `segelmodell` prueft Gradienten,
  lokale Nullwindfaelle, gegenlaeufige Winde, die Azimutnaht und lokalen
  vertikalen Wind in beiden Richtungen und verweigert bei einem
  fehlenden Pflichtnachweis die Publikation. Falls diese Rasterpruefung
  besteht, werden anschliessend die 23 Geschwindigkeiten mit 108
  Grundwinkeln und den lokalen Uebergangswinkeln je Geschwindigkeit
  gegen die gueltige direkte und geteilte H1-Referenz sowie deren
  Verfeinerung verglichen; der globale Nullwind wird auch ausserhalb
  des H1-Bereichs geprueft. Die
  JSON-Antwort enthaelt die 14 Rasterfallresultate und bei ausgefuehrter
  Gleichwindpruefung die Ergebnisse je Geschwindigkeit mit dem jeweils
  schlechtesten Kraft- oder Momentenvergleich, Eingaben, Bezug,
  Abweichung und Grenze sowie alle fehlgeschlagenen Gleichwindfaelle.
  Das Feld `nachweise` trennt ausgefuehrte, bestandene, fehlgeschlagene
  und noch nicht ausgefuehrte Vorpruefungen und kennzeichnet die fehlende
  physikalische Validierung; es stellt keine Gesamtabnahme dar.
  Fuer die Rasterfaelle werden 32 und 64 gegen 48 Moden auf grobem
  und halbiertem Raster mit einem 0,1-%-Bezug diagnostisch verglichen;
  dieser Bezug ist keine zusaetzliche Abnahmegrenze. Eine ausgewaehlte
  positive Geschwindigkeitsfolge gegen null einschliesslich exakt null
  wird anhand dimensionaler Streifen- und Gesamtlasten geprueft und
  ausgegeben. Gegenwind an der Azimutnaht, lokal vertikaler Wind und
  lokaler Nullwind werden jeweils beidseits mit kleinen horizontalen
  Stoerungen diagnostisch verglichen; der 0,1-%-Bezug ist auch hier
  keine zusaetzliche Abnahmegrenze. Die Reynoldsdiagnose erfasst nun
  alle Quadraturpunkte, nutzt identische Grenzgeschwindigkeiten gemeinsam
  und zaehlt Punkte, deren beidseitige Grenzgeschwindigkeiten nicht im
  Bereich 0--50 m/s liegen, separat. Diese Diagnose ersetzt nicht die
  vollstaendige Grenzfall- und Winkelmatrix. Fuer Optimist, 420 und
  Folkeboot wurden 364, 402 und 1403 Quadraturpunkte beidseits
  untersucht; keiner lag ausserhalb des Bereichs. Die maximalen
  Quotienten betragen 0,00157863, 0,00177508 und 0,00257192.
  Fuer das aktuelle Auftragsraster wurden die Vergleiche erneut
  durchgefuehrt und nun fuer jeden Quadraturpunkt einzeln berichtet:
  Optimist 186, 420 402 und Folkeboot 703 Punkte, jeweils mit zwei
  Vergleichen und ohne Punkte ausserhalb des Bereichs. Die maximalen
  diagnostischen Quotienten sind 0,00157888, 0,00177508 und 0,00257192.
  Ein Abgleich pruefte Punktschluessel, Anzahl, Faktoren, Quotienten und
  Uebereinstimmung mit den vorherigen zusammengefassten Ergebnissen;
  alle 2582 Bezugsgrenzen wurden separat anhand von Staudruck, Flaeche
  und Referenzsehne nachgerechnet.
  Der lokale Nachweis
  `files\re-grenzfaelle-44a7ded9\nachweis.json` hat SHA-256
  `3260B0B485B659EDCB06E98609C90F55255EB312188D4541D4B0B42B9D79577D`.
  Der 0,1-%-Bezug ist auch hier diagnostisch und keine eigenstaendige
  zusaetzliche Abnahmegrenze.
  Fuer den Bilanznachweis kann die interne Testrechnung Einzelbeitraege
  getrennt von Streifensummen ausgeben. Ein Core-Vertragstest mit
  gewoelbtem Segel und ungleichfoermigem Wind summiert Kraefte und
  verschobene Momente einschliesslich freier Profilmomente je Streifen
  und am gemeinsamen Ursprung; bei global null entstehen keine
  Einzelbeitraege. Die CLI berichtet zudem die Beitragsbilanz fuer
  beide Raster der Gradientenszenarien diagnostisch. Bei den drei
  Testsegeln ergibt die Probe jeweils fuer alle 16 Raster/Szenario-
  Kombinationen den diagnostischen Quotienten null. Dies ist
  noch keine vollstaendige End-to-End-Abnahme.
- **Zusaetzliche Windsonderfaelle:** Die Probe und die CLI verwenden
  dasselbe stetige Windfeld auf grobem und halbiertem Raster fuer
  gegenlaeufigen Wind mit lokalem Nullpunkt, die Azimutnaht und lokalen
  vertikalen Wind, jeweils in beiden Richtungen. Die sechs isolierten
  Rastervergleiche je Testsegel bestanden fuer Optimist, 420 und
  Folkeboot mit maximalen 5-%-Grenzquotienten 0,059217, 0,036884 und
  0,025902. Mit den drei separaten CLI-Auftragsrastern bestanden sie
  ebenfalls, mit maximal 0,058875, 0,036884 und 0,025942. Der
  Core-Vertragstest prueft die lokalen Grenzwerte und das gemeinsame
  lineare Feld; die vollstaendige Abnahme bleibt offen.
- **Algebraischer Traglinien-Teilnachweis:** Eine interne Diagnose
  vergleicht die tatsaechlich geloesten 48 Modellkoeffizienten mit
  `V_xy` mal den separat berechneten H1-Koeffizienten. Fuer Optimist,
  420 und Folkeboot bestanden bei 10 m/s drei Winkelpaare; zusaetzlich
  wurde fuer je vier abgeloeste Gleichwindrichtungen bestaetigt, dass
  keine Traglinie geloest wird. Der Core-Vertragstest prueft ausserdem,
  dass die Diagnose die normalen Lasten nicht veraendert. Dies sind
  ausgewaehlte Faelle und kein Ersatz fuer die gesamte Winkelmatrix.
- **CLI-Numerikvorpruefung Optimist:** Ein isolierter Aufruf bestand
  die acht Rasterfaelle und die Gleichwindvorpruefung fuer alle 23
  Geschwindigkeiten in rund 535 Sekunden. Wo H1 nicht anwendbar ist,
  wird kein bestandener H1-Vergleich behauptet. Der Aufruf endete
  erwartungsgemaess mit Exitcode 4 und ohne Publikation, da weitere
  verpflichtende Nachweise fehlen. Der CLI-Testauftrag verwendet ein
  Hoehenraster von 0,25/0,125 m und liefert je Geschwindigkeit 2258
  Uebergangswinkel; die oben dokumentierte separate Uebergangsmatrix
  verwendet 0,125/0,0625 m und ist daher nicht derselbe Rechenlauf.
  Der entsprechende 420-Aufruf mit 0,25/0,125 m bestand ebenfalls
  alle acht Rasterfaelle und die Gleichwindvorpruefung fuer 23
  Geschwindigkeiten mit je 4850 Uebergangswinkeln; sieben Geschwindigkeiten
  liegen ausserhalb des H1-Bereichs. Nach rund 3685 Sekunden endete er
  ebenfalls erwartungsgemaess mit Exitcode 4 ohne Publikation. Der
  Folkeboot-Aufruf mit 0,25/0,125 m bestand die acht Rasterfaelle und
  die Gleichwindvorpruefung fuer 23 Geschwindigkeiten mit je 8462
  Uebergangswinkeln; sieben Geschwindigkeiten liegen ausserhalb des
  H1-Bereichs. Er endete nach rund 8794 Sekunden ebenfalls mit Exitcode 4
  ohne Publikation. Alle drei Aufrufe verwendeten noch den damaligen
  Stand mit acht Rasterfaellen; die sechs spaeter hinzugefuegten
  Windsonderfaelle wurden separat nachgeprueft. Dies sind keine
  Produktabnahmen.
- **H1-Gueltigkeitsgrenze:** Die Entscheidung fuer einen Gleichwind-
  H1-Vergleich beruecksichtigt nun auch die lokalen Reynolds-Zahlen
  der Modell-Quadraturpunkte. Der Core-Vertragstest prueft eine gueltige
  untere Grenze sowie Punkte unter und ueber dem H1-Bereich. An den
  relevanten Geschwindigkeitsgrenzen und bei 50 m/s wurden fuer beide
  verwendeten Auftragsraster der drei Testsegel einzelne Faelle
  nachgeprueft: Die zuvor berichtete Einordnung in H1-gueltig und
  nicht anwendbar bleibt fuer diese Punkte unveraendert. Diese
  Einzelpruefung ist kein erneuter Vollrasterlauf.
- **Gleichwind-Fallberichte und Staudruck:** Die Gleichwindvorpruefung
  verwendet fuer jeden Winkel den tatsaechlichen Betrag des verwendeten
  Windvektors fuer Reynolds-Zahl und `q_max`; die H1-Gueltigkeit wird
  bei unterschiedlichen Rundungswerten einzeln beurteilt. Der
  Vergleich meldet beide Vektoren, Abweichung und Grenze. Die interne
  Erzeugung mehrkomponentiger 50-m/s-Testvektoren verwendet einen
  kleinen Abstand zur Obergrenze, damit Rundung die zulaessige Norm
  nicht ueberschreitet; achsparallele Faelle erreichen exakt 50 m/s.
  Die jeweils tatsaechliche Geschwindigkeit geht in die Bezuege ein.
  Die interne
  Gleichwindprobe kann mit `--matrix ... --jsonl` Grund- und
  Uebergangswinkel fallweise ausgeben; nicht H1-gueltige Faelle werden
  getrennt gekennzeichnet. Die Fallberichte enthalten ausserdem den
  tatsaechlichen Rueckwaertsfehler der geloesten Traglinie; bei
  uebersprungener Loesung steht dort kein Fehlerwert. Geprueft wurden
  einzelne gueltige und
  nicht anwendbare Faelle, die 50-m/s-Grenze sowie ein partitionierter
  Matrixlauf. Die frueheren Gesamtlaeufe verwendeten den nominellen
  Geschwindigkeitsbetrag fuer `q_max`; sie ersetzen keinen erneuten
  vollstaendigen Nachweis mit dem tatsaechlichen Staudruck.
- **Erneute Gleichwind-Einzelfallmatrix Optimist:** Fuer das Auftragsraster
  0,25/0,125 m wurden alle 23 Geschwindigkeiten mit je 2363 Grund- und
  Uebergangswinkeln erneut fallweise geprueft. 25993 H1-Vergleiche
  bestanden, 28356 Faelle lagen ausserhalb des H1-Gueltigkeitsbereichs;
  diese zaehlen nicht als bestandene H1-Vergleiche. Der maximale
  Grenzquotient der gueltigen Faelle betrug 0,701968. Die Berichte
  enthalten den tatsaechlichen Windbetrag und bei geloester Traglinie
  den Rueckwaertsfehler. Die entsprechende Matrix fuer den 420 ist
  ebenfalls durchlaufen: 79280 H1-Vergleiche bestanden, 34685 Faelle
  waren nicht anwendbar, der maximale Grenzquotient betrug 0,733685.
  Fuer das Folkeboot bestanden 137072 H1-Vergleiche, 59969 Faelle waren
  nicht anwendbar, der maximale Grenzquotient betrug 0,830844. Die
  Rueckwaertsfehler geloester Traglinien erfuellen die Grenze 1e-10.
  Fuer 10 m/s wurden zusaetzlich die Modelllasten aller 2363, 4955
  beziehungsweise 8567 Gleichwindfaelle erneut ausgegeben und ihre
  uebrigen Fallberichte mit der archivierten Matrix abgeglichen.
  Ein separater Pruefer verglich Lasten exakt an den Azimut-, Neigungs-
  und lokalen Abrissgrenzen mit den jeweils zulaessigen Seitenabstaenden
  von 1e-6 Grad. Er verwendete Streifenflaechen, Staudruck beider Faelle,
  Referenzsehne und vollstaendige Momente um den Ursprung. Fuer
  Optimist, 420 und Folkeboot wurden 1506, 3234 und 5642 Vergleiche
  ausgewertet; die maximalen Quotienten gegen einen diagnostischen
  0,1-%-Bezug sind 0,000213, 0,000239 und 0,000262.
  Der Bezug ist keine zusaetzliche Abnahmegrenze; dies ist ein
  Teilnachweis fuer 10 m/s, nicht fuer alle Geschwindigkeiten.
  Der Nachweis `files\winkelstetigkeit-10ms-a0ec91d2\nachweis.json`
  hat SHA-256 `7010213368318D29A122B9ECBF9A394A42D6626031523AE3D3874F6C47309DE4`.
  Diese Auswertung wurde anschliessend fuer alle 23 festgelegten
  Geschwindigkeiten wiederholt. Fuer Optimist, 420 und Folkeboot wurden
  34638, 74382 und 129766 Grenzvergleiche dokumentiert; die groessten
  Quotienten gegen den diagnostischen Bezug betragen 0,000218,
  0,000245 und 0,000269. Die zugrunde liegenden Modell- und
  Solverdiagnosen wurden mit den bereits vorhandenen Einzelfallberichten
  abgeglichen; H1 wurde in diesem Lauf nicht erneut verglichen.
  Der Nachweis `files\winkelstetigkeit-matrix-27dbc131\nachweis.json`
  hat SHA-256 `E29A4262A042E1CE67C208BAD11820AB2239E1BBACAF60A3BD0B5E53DE1D9B7A`.
  Auch dies ist ein numerischer Teilnachweis, keine Gesamtfreigabe.
  Die CLI meldet nun fuer jede Geschwindigkeit die Anzahl der
  Uebergangsvergleiche und den schlechtesten dimensionalen Kraft- oder
  Momentenvergleich einschliesslich beider Winkel und Staudruck.
  Der 0,1-%-Bezug bleibt diagnostisch. Fuer Optimist stimmten die
  Vergleichszahlen und Maxima aller 23 Geschwindigkeiten mit dem
  unabhaengigen Winkelbericht ueberein; Raster- und bestehende
  Gleichwindberichte blieben unveraendert. Der Nachweis
  `files\cli-winkelvergleich-cc5a9e3b\nachweis.json` hat SHA-256
  `5FD6F248E62B1B7196FB8039F638936BD3159CD3E8E9D8576F5C42A8983BD9EE`.
  Auch fuer die 420 stimmen alle 23 Vergleichszahlen und Maxima sowie
  die bisherigen Raster- und Gleichwindberichte ueberein. Nachweis:
  `files\cli-winkelvergleich-420-f610b13c\nachweis.json`, SHA-256
  `F462FA2F767CB6EDF10D3CA301C6DD3E66A6BC21633E88795FF77A0CEE474B40`.
  Der entsprechende Folkeboot-Lauf endete nach 14 von 23
  Geschwindigkeiten ohne Abschluss-JSON. Das Windows-Ereignis fuer den
  isolierten `SegelMacher.exe`-Prozess meldet `0xc0000005`, Moduloffset
  `0x305a`. Eine bytegleiche Zuordnung zu einem separaten optimierten
  Symbolbuild zeigt dort einen lesenden `std::string`-Kopiervorgang
  mit Zugriffsadresse `0x5`; eine weitere Adresse liegt beim Kopieren
  eines JSON-Map-Knotens. Weitere moegliche Stackadressen deuten auf
  das Kopieren der Lasten in der geteilten H1-Referenz hin; ohne
  vollstaendige Stackrekonstruktion ist diese Zuordnung nicht gesichert.
  Ursache und Ursprung einer moeglichen Speicherbeschaedigung sind
  damit nicht belegt. Das Protokoll liegt unter
  `files\cli-winkelvergleich-folkeboot-f610b13c\prozess.log`.
  Die isolierte Wiederholung stimmte fuer alle 23 Geschwindigkeiten
  und 129766 Uebergangsvergleiche mit dem unabhaengigen Winkelbericht
  ueberein; die groesste absolute Quotientendifferenz betrug 7e-20.
  Auch die bisherigen Raster- und Gleichwindberichte waren unveraendert.
  Die CLI endete erwartungsgemaess mit Exitcode 4 ohne Publikation,
  weil weitere Pflichtnachweise fehlen. Nachweis:
  `files\cli-winkelvergleich-folkeboot-wiederholung2\nachweis.json`,
  SHA-256 `2998FABF814555FC9A001FB3B12261157C18A228A8A6D4F952773F4B5AA858FE`.
  **Folkeboot-Grosssegel: Gleichwind-, Raster- und ergaenzende
  Winkelpruefung bestanden; Gesamt- und Produktabnahme offen.**
  Die erfolgreiche Wiederholung erklaert den Absturz nicht.
  Eine separate ASan-Stichprobe des Folkeboots bei 10 m/s durchlief
  1058 von 8462 Uebergangsfaellen einschliesslich H1-Vergleich mit
  Exitcode 0 und ohne ASan-Meldung. Das Ergebnis
  `files\asan-folkeboot-folge2.out` hat SHA-256
  `2260D1E9EADF9830C1D592325CFEAC1CFEC8B737F84AEB6CD3486AFF74C3A3C7`;
  das verwendete Programm `files\asan-core\Release\strip_uniform_probe.exe`
  hat SHA-256 `DA6A72C8DF98D244F24060019D86975CF87891B28D1EAA5790AB2DCE52D8E2B0`.
  Die serielle Teilprobe beweist weder Fehlerfreiheit noch die Ursache
  des Absturzes in der parallelen CLI.
  Nach der erneuten Pruefung aller 69 archivierten Gleichwindberichte
  bestehen fuer Optimist, 420 und Folkeboot 25993, 79280 und 137072
  H1-gueltige Vergleiche mit maximalen Grenzquotienten 0,701968,
  0,733685 und 0,830844. Die jeweils 28356, 34685 und 59969
  H1-nicht-anwendbaren Faelle sind getrennt gezaehlt. Der Pruefer
  kontrollierte unter anderem Fallmengen, Eingabe- und Programmhashes,
  Staudruck, Bezugsgrenzen und die gemeldeten Solverresiduen; es wurden
  keine neuen Gleichwindrechnungen ausgefuehrt. Das Protokoll
  `files\h1-numerik-neu-20261116.log` hat SHA-256
  `BC550F7B98B5A2F2E4F93BAEAF37638D56CB6FF0198998F22DFE777F37E8B2BF`.
  Ein weiterer Abgleich prueft die Hashes der vorhandenen Teilberichte,
  die gemeinsamen CLI-Ergebnisse von H1-, Raster- und Bilanznachweis
  sowie die Eingabeprovenienz der Residual- und Reynoldsdiagnosen. Er
  kontrolliert unter anderem die 69 H1-Dateien, die 14 Rasterfaelle je
  Segel, Polidentitaet, Nullfolgen und Rueckwaertsfehler. Er fuehrt
  keine Rechenlaeufe erneut aus und ist keine produktive Gesamtfreigabe.
  Sein Ergebnis
  `files\numerik-nachweisverknuepfung-20261116.json` hat SHA-256
  `9D26C9F9B666596D27EB6A9A14FE7C91706FABC8BA99C34F6E1CF91CDEA127B3`.
  Fuer beide Pole wurden ausserdem bei allen 23 Geschwindigkeiten und
  allen 12 vorgeschriebenen Azimuten die dimensionalen Streifen- und
  Gesamtlasten verglichen: 506 Azimutvergleiche je Segel. Bei diesen
  Faellen waren die Lastvektoren komponentengleich; die bei 10 m/s
  erzeugten Lasten stimmen mit der vorherigen Gleichwinddiagnose ueberein.
  Die CLI prueft inzwischen zusaetzlich bei jeder Geschwindigkeit die
  Polidentitaet der von ihr berechneten Streifenlasten und meldet die
  Zahl der Vergleiche unter `polpruefung`. Bei einer Abweichung bricht
  sie mit Numerikfehler und Angabe von Geschwindigkeit und Winkeln ab.
  Der gezielte `cli_contract` bestand fuer den Rechteckauftrag mit
  24 Polvergleichen je Geschwindigkeit; auch `cli_integration` bestand
  mit dem geaenderten CLI-Build. Fruehere Segelnachweise wurden dafuer
  nicht erneut ausgefuehrt.
  Einzelaufrufe bestaetigen die gleichen Modelllasten bei 10 m/s und bei
  nicht H1-anwendbarem Nullwind; ein Aufruf ohne Last-JSONL wird abgelehnt.
  Der Nachweis `files\polfaelle-matrix-0b8921c2\nachweis.json` hat
  SHA-256 `2B7A66118FF3F6EACCAC540B4EA102DE7A1066BC995B84363B6AADCA081DFB61`.
  Fuer die Polnaehe wurden aus der oben genannten Winkelstetigkeitsmatrix
  ausserdem je Segel 46 Vergleiche bei beiden Polen und innenliegendem
  Abstand `1e-6` Grad ausgewertet; die maximalen diagnostischen Quotienten
  betragen 3,71e-6 / 5,50e-6 / 8,31e-6. Auch dieser Bezug ist keine
  eigenstaendige Abnahmegrenze.
  Fuer drei Segel und fuenf ausgewaehlte Windrichtungen wurden ausserdem
  150 Einzelfaelle fuer exakt null und positive Geschwindigkeiten von
  `1e-10` bis `1e-2 m/s` berechnet. Die jeweils groessten absoluten
  Streifenkomponenten von Kraft und Moment wurden getrennt ausgewertet:
  Sie sind endlich, nehmen entlang dieser Folgen gegen null ab und sind
  bei exakt null null. Die Lastdateien und ihre Hashes wurden unabhaengig
  nachgerechnet. Dies ist ein Stichprobennachweis, keine Aussage fuer alle
  moeglichen Windrichtungen. `files\nullgrenzfolge-b76f5cf4\nachweis.json`
  hat SHA-256
  `80EFA9EA893057E810A3C1287FC5D4851F34F76D8B64BF704F2F6FBEAF29CDEC`.
  Ein Integrationstest manipuliert ausserdem sechs Parquet-Inhalte bei
  erneuerter SHA-256-Pruefsumme (Schluessel, Fremdschluessel, Sehne,
  nicht endliche Zahl, Normale und Matrixwert): Der Modell-Lader lehnt
  jeden Fall mit typisiertem Fehler ab; die unveraenderte Vorlage und
  jede vor der Manipulation mit PyArrow neu geschriebene Tabelle laden.
  Der vollstaendige `cli_integration`-Lauf bestand; sein Bericht
  `build\integration_5a06ce3b6c7b486894ba69272b5aac0b\test_evidence.json`
  hat SHA-256
  `5394F7D9F9876A0E0E2202F5A8B4A37693001A30BE7854D296E3470AE2992BEB`.
  Eine serielle Messung an Diagnosemodellen mit Hintergrundprioritaet
  ergab fuer Optimist/420/Folkeboot 154037/221351/306209 Byte,
  372/730/554 ms fuer je einen Ladevorgang und 1,04/4,53/6,86 ms
  pro Rechnung als Mittelwert aus jeweils 20 warmen Aufrufen.
  Dies sind Einzelmessungen ohne Leistungsgarantie und keine produktive
  End-to-End-Messung. `files\modell-leistungsmessung-3a1cde40\nachweis.json`
  hat SHA-256
  `8F618EED95DCDAD1439C53737D8CAA3E157487C28C3CA785461A24B5E7907176`.
  Ein separater Abgleich der hashgeprueften 69 Einzelfallberichte mit
  den erneuten CLI-Berichten bestaetigt fuer jede Geschwindigkeit die
  Fallzahl, die Zahl gueltiger H1-Vergleiche, den Status und den
  schlechtesten Vergleich. Nicht anwendbare Faelle bleiben gesondert
  gezaehlt. Der Nachweis
  `files\cli-wiederholung-929ac6814d67449187b88cc8172f1092\gleichwind-abgleich.json`
  im Sitzungsordner hat SHA-256
  `562246262901A21411F41D97567DCF2553FA277316EC0793A9AE94D9048BB8D2`.
  Der Abgleich vergleicht Zusammenfassungen; die CLI-Berichte enthalten
  weiterhin nicht jeden einzelnen Gleichwindvergleich.
  Eine zusaetzliche Pruefung der 69 Berichte kontrollierte die Neigungs-,
  Azimut- und Abrissgrenzen aus den Quadraturdaten sowie die Abstaende
  1e-6 Grad und sicherte SHA-256 aller Berichte. Das Nachweisverzeichnis
  liegt lokal im Sitzungsordner unter
  `files\h1-einzelfaelle-57c42b64c035466bb4914f40c7cc3953`;
  `winkel-und-berichtshashes.json` hat SHA-256
  `9C495439D89073795122AF7CF82EFD9EF3D3B1BE17CB929D2FCD6BFB052A8040`.
  Die vollstaendige Produktabnahme bleibt offen.
- **Erneute Optimist-CLI-Vorpruefung:** Ein isolierter Lauf mit aktuellem
  Staudruckbezug bestand alle 14 Rasterfaelle. Von den 23 geprueften
  Geschwindigkeiten bestanden elf den H1-Vergleich; bei zwoelf war H1
  nicht anwendbar. Die drei Eingabe-SHA-256 stimmten mit den kopierten
  Auftrags-, Geometrie- und Konventionsdateien ueberein. Nach rund
  414 Sekunden endete der Lauf wie vorgesehen mit Fehlercode 4 ohne
  Veroeffentlichung oder verbleibenden Sperrordner. Weitere Pflicht-
  nachweise und die Produktabnahme bleiben offen.
- **Erneute CLI-Vorpruefungen 420 und Folkeboot:** Die isolierten Laeufe
  bestanden ebenfalls alle 14 Rasterfaelle und prueften jeweils 23
  Geschwindigkeiten mit dem aktuellen Staudruckbezug. Fuer jedes Segel
  bestanden 16 H1-Vergleiche; sieben Geschwindigkeiten lagen ausserhalb
  der H1-Gueltigkeit. Die drei Eingabe-SHA-256 wurden jeweils gegen die
  kopierten Dateien geprueft. 420 und Folkeboot endeten nach 2698 bzw.
  8375 Sekunden erwartungsgemaess mit Fehlercode 4, ohne Publikation oder
  verbleibenden Sperrordner. Beim Folkeboot trat der fruehere Absturz
  nicht erneut auf; seine Ursache ist damit nicht geklaert. Dies sind
  Vorpruefungen und keine vollstaendige Produktabnahme.
- **Unabhaengige Rasterbezugspruefung:** Die 14 in jedem der drei
  CLI-Berichte ausgegebenen Rasterfaelle wurden anhand der separaten
  Streifendaten auf Fallmenge, maximalen Staudruck beider Raster,
  Referenzflaeche, 5-%-Grenze und berichteten Grenzquotienten geprueft.
  Die maximalen Quotienten betragen fuer Optimist, 420 und Folkeboot
  0,852688, 0,487668 und 0,479540. Die Berichte enthalten nicht alle
  verglichenen Lastvektoren; diese Pruefung rekonstruiert die
  Abweichungen daher nicht unabhaengig. Der lokale Nachweis
  `files\parquet-unabhaengig-d2a547006d584b7b8cab41bcff004bb1\cli-rasterbezuege.json`
  im Sitzungsordner hat SHA-256
  `DCD5011AED3A078FAC698CA81F46C89A7F686430CF373A7E73BC0BA203950B78`.
  Seitdem enthaelt jeder Rasterfall beide verwendeten Windfelder und alle
  Streifen- und Gesamtvergleiche mit groben und auf denselben Bezugspunkt
  verschobenen halbierten Lastvektoren. Ein Vertragstest rechnet fuer
  jeden Vergleich Abweichungsnorm, Grenze und Quotient sowie das Maximum
  nach. Die bereits archivierten drei Berichte enthalten diese neuen
  Felder noch nicht. In einem erneuten isolierten Optimist-Lauf wurden
  alle 308 Streifen- und Gesamtvergleiche und beide Windfelder je Fall
  anhand der berichteten Lastvektoren separat nachgerechnet; maximaler
  Quotient 0,852688. Die Lastberechnung und die zuvor ausgefuehrte
  Momentverschiebung werden dadurch nicht unabhaengig rekonstruiert.
  Der lokale Nachweis
  `files\cli-wiederholung-929ac6814d67449187b88cc8172f1092\unabhaengiger-rasterbericht-optimist.json`
  hat SHA-256
  `2B4FA495B25612094F70F4003288641E8FE2921E8B23243BD7E7F2C524CB5CF9`.
  Auch fuer den erneuten 420-Lauf wurden beide Windfelder und alle
  588 Vergleiche separat nachgerechnet; der maximale Quotient betraegt
  0,487668. Der Nachweis
  `files\cli-wiederholung-929ac6814d67449187b88cc8172f1092\unabhaengiger-rasterbericht-420.json`
  hat SHA-256
  `45F20CEBDF09C4882CC9A3B6D18B3EDD1637EE33F047F080B2E6DD2813953E40`.
  Auch der erneute Folkeboot-Lauf bestand nach 7359 Sekunden; alle
  1008 Vergleiche und beide Windfelder je Fall wurden separat
  nachgerechnet, maximaler Quotient 0,479540. Der gemeinsame Nachweis
  fuer alle drei Segel liegt unter
  `files\cli-wiederholung-929ac6814d67449187b88cc8172f1092\unabhaengiger-rasterbericht.json`
  und hat SHA-256
  `F2FD60197E4B38A43162E7D4BB67DC50A519DC5D10588167AC6834B0E1B69C5E`.
  Auch dieser Nachweis rekonstruiert weder die Lastberechnung noch die
  Momentverschiebung unabhaengig.
  Eine weitere Diagnose erzeugte fuer dieselben Eingaben getrennt die
  unverschobenen Streifenlasten und Abfragepunkte beider Raster. Ein
  separates Python-Programm rekonstruierte daraus fuer alle 14 Faelle
  je Segel die feinen Paarlasten, Momentverschiebungen und Gesamtlasten
  und verglich sie mit den CLI-Berichten: 308, 588 und 1008 Vergleiche
  stimmten komponentenweise ueberein. Die Lasten selbst stammen weiterhin
  aus derselben Modellimplementierung; dies ist keine unabhaengige
  physikalische oder solverseitige Referenzrechnung. Der Nachweis
  `files\raster-roh-5d92dcab593d47ddbe3bf824e5a719c1\momentenpruefung.json`
  hat SHA-256
  `FB8D979F5042E58CFE094103668C8E945D1A72608E29D040E7E54F5E0EE7CF4A`;
  das Diagnoseprogramm hat SHA-256
  `B6F480364705F7468E1D152CF13A119FDA390B71C7CE4BFDE340620140129B6E`.
- **Unabhaengige Beitragsbilanz:** Eine separate Diagnose gibt fuer alle
  14 Rasterfaelle die Einzelbeitraege beider Raster aus. Ein Python-
  Pruefer summierte daraus Kraefte und vollstaendige Momente um die
  Abfragepunkte und unabhaengig um den Ursprung; anschliessend verglich
  er die groben und verschobenen feinen Lasten mit den CLI-Berichten.
  Fuer Optimist, 420 und Folkeboot wurden 420, 840 und 1470 Streifen-
  summen aus 26650, 56350 und 98420 Einzelbeitraegen geprueft. Die
  groessten absoluten Komponentenabweichungen betragen rund 6,4e-14,
  7,4e-13 und 2,8e-12 in den jeweiligen SI-Einheiten. Eingabe- und
  Programmhashes sind im Nachweis enthalten. Die Einzelbeitraege selbst
  stammen aus derselben Modellimplementierung; der Nachweis ist keine
  unabhaengige physikalische oder solverseitige Referenzrechnung.
  Das Ergebnis im Sitzungsordner unter
  `files\beitragsnachweis-34cfa1d076944e42a8a3982cd02be1f8\beitragsbilanz.json`
  hat SHA-256 `2C8B8200F1CF2BFEDB661BCB4D4C4BB7EBBB3870D158753475CF445A83F0524A`.
- **Interner Austauschhelfer:** Ein isolierter CLI-Vertragstest bestand
  einschliesslich unvollstaendiger Nachweise, Fremddatei im Arbeitsordner,
  Dateibeschaedigung, `create-only`, `overwrite`, Namenskollision und
  Windows-Rollback bei gesperrtem Arbeitsordner. Der Helfer ist fuer
  den Fall vollstaendiger Nachweise an die CLI-Erzeugung angeschlossen;
  dieser Fall wird derzeit noch nicht erreicht. Er verlangt zusaetzlich
  die vier in der CLI-Vorpruefung als offen benannten Kategorien
  `vollstaendige_numerikabnahme`, `datenformat_und_integritaet`,
  `end_to_end_integration` und `geschuetzte_publikation` als bestanden.
  Der CLI-Vertragstest prueft das Fehlen jeder einzelnen Kategorie.
  Ein weiterer isolierter Vertragstest verkettet Arbeitsordnererzeugung,
  Austausch und erneutes Laden mit ausdruecklich simulierten Nachweisen
  und Testversionen; dies ist keine produktive End-to-End-Abnahme.
  Weitere Vertragstests pruefen bei simulierten Nachweisen den Abschluss
  mit Callback sowie einen Fehler nach dem Austausch: Dann werden
  Fehlercode und bereits geschriebener Modellordner ausgegeben.
  Ein isolierter Windows-Integrationstest verwendet ebenfalls ausdruecklich
  simulierte Nachweise und ein Diagnosemanifest, ruft nach dem Austausch
  aber die echte SegelPhysik-Core-Fingerprint-API auf. Er prueft die
  ausschliessliche Aktualisierung des Aerodynamikbereichs und einen
  gesperrten Fingerprint: Der Fehler meldet Exitcode 5 und den bereits
  geschriebenen Modellordner; die Modellvalidierung und der alte Lookup
  bleiben erhalten. `cli_contract` und `cli_integration` bestanden in
  204,71 beziehungsweise 133,47 Sekunden. Der Nachweis
  `build\integration_a7b3cc9f75054860a684b9ed9b40bf25\test_evidence.json`
  hat SHA-256 `5F7C31F2D67E9AE31E14D8FE200F464F5CC719E70D791F75020F0053662A5111`.
  Dies ist keine produktive End-to-End-Abnahme.
  Das Vorhandensein dieser Kennungen allein belegt jedoch weder die
  Vollstaendigkeit noch die tatsaechliche Durchfuehrung der Nachweise.
  Der Austausch erfolgt bei
  `overwrite` durch zwei Ordnerwechsel mit Rollback, nicht als atomarer
  Wechsel eines nichtleeren Verzeichnisses; bei Prozessabsturz zwischen
  den Wechseln kann manuelle Wiederherstellung erforderlich sein.
- **Interner Abbruchschutz beim Schreiben:** Der Tabellenschreiber kann
  eine Abbruchpruefung vor und waehrend der Ausgabe sowie nach dem
  Abschliessen und Hashen jeder Tabelle aufrufen. Isolierte Tests pruefen
  Abbruch vor dem Schreiben und bei bereits geoeffneter erster Datei;
  ein Manifest oder eine weitere Tabelle entstehen dabei nicht. Ein
  interner Erzeugungshelfer legt den eigenen Arbeitsordner exklusiv an,
  prueft den vollstaendigen Tabellen-/Manifest-Roundtrip und entfernt
  ihn bei Abbruch oder Fehler; Bereinigungsfehler werden gemeldet.
  Die binary64-Tabellenwerte werden beim Roundtrip nun bitgenau
  verglichen, auch bei vorzeichenbehafteter Null. Ein CLI-Vertragstest
  prueft sowohl den Erhalt negativer Null als auch die Ablehnung eines
  abweichenden Vorzeichens.
  Isolierte Tests decken Abbruch, ungueltiges Manifest, Namenskollision
  und eine unter Windows absichtlich gesperrte Bereinigung ab;
  vorhandene Ausgabe und fremder Ordner bleiben erhalten. Erzeugungs-
  und Austauschhelfer verlangen auch einen Eingabenachweis fuer die
  verwendeten Konventionen; dessen Fehlen blockiert die Publikation.
  Die produktive Publikation ueber den Helfer ist noch offen.
- **Isolierter Modell-Roundtrip:** Ein Diagnoseprogramm schreibt die
  Modelltabellen und ein als Diagnose gekennzeichnetes Manifest in
  separate Arbeitsordner, laedt das Modell erneut und vergleicht die
  typisierte Laufzeitrechnung mit der direkten internen Rechnung.
  Optimist, 420 und Folkeboot bestehen mit 10, 20 und 35 Streifen.
  Numerikabnahme und CLI-Publikation wurden dabei nicht nachgewiesen.
  Eine erweiterte isolierte Probe startete fuer jedes dieser drei Modelle
  vier Aufgaben mit unterschiedlichen Windfeldern an demselben geladenen
  Modell und verglich die Ergebnisse komponentengenau mit zuvor seriell
  berechneten Lasten. Alle drei Vergleiche bestanden; die Aufgaben
  wurden gemeinsam freigegeben. Der Nachweis
  `files\modell-parallelnutzung-6a6e5a84\nachweis.json` hat SHA-256
  `587E1B0F26745F2580FCE4E8EB609B4B06675ACE4519246E283F37CFE76866B3`.
  Die Modelle verwenden Diagnosemanifeste und belegen keine produktive
  End-to-End-Abnahme.
  Fuer alle drei Segel wurden die jeweils fuenf Tabellen zusaetzlich
  mit dem getrennt installierten PyArrow 21.0.0 gelesen und gegen die
  vor dem Schreiben erzeugten Modelldaten geprueft: Schluessel und
  endliche binary64-Werte stimmen bitgenau, die Schluessel sind
  eindeutig und sortiert, die Spalten verwenden die geforderten Typen
  und alle Row-Groups den Codec ZSTD. Die Eingabe- und Tabellenhashes
  stimmen mit dem jeweiligen Diagnosemanifest ueberein. Der lokale
  Nachweis liegt im Sitzungsordner unter
  `files\parquet-unabhaengig-d2a547006d584b7b8cab41bcff004bb1`;
  `parquet-nachweis.json` hat SHA-256
  `9EFCBEE0391478189E1BECC91B3E99A097BFCBAFD287F0F29DCCF2D3C6A8220D`.
  Diagnosemanifeste ersetzen keine produktive Provenienz oder
  End-to-End-Abnahme. Bei zwei separaten Diagnose-Prozessen waren alle
  fuenf Tabellen je Segel bytegleich; dies ist noch kein Nachweis fuer
  die produktive Erzeugung mit Workerzahlen 1, 2 und 4.
  Fuer den Diagnose-Roundtrip werden Auftrag und Geometrie inzwischen
  jeweils aus denselben eingelesenen Bytes geparst und per SHA-256
  gehasht. Der CLI-Vertragstest prueft doppelte Schluessel und die
  Unveraenderlichkeit des Snapshots nach einer Dateiaenderung; beim
  erneuten Optimist-Roundtrip stimmten beide Manifest-Hashes mit dem
  System-SHA-256 ueberein. Die CLI-Vorpruefung haelt inzwischen Auftrag,
  Geometrie und Konventionen als Snapshots fest und meldet deren Hashes
  aus den tatsaechlich eingelesenen Bytes. `pruefen` und `create-only`
  benoetigen dabei keine Hashberechnung. Die Vorpruefungsantwort meldet
  die CMake-Projektversion des Werkzeugs und die Version der tatsaechlich
  geladenen Arrow-Bibliothek ueber deren Laufzeit-API. Die
  Parquet-Paketversion wird fuer Windows-Release-Builds nur bei
  SHA-256-Gleichheit der geladenen DLL mit der konfigurierten Paketdatei
  ausgegeben; die Quelle ist als Paketmetadaten und Dateihash
  gekennzeichnet. Bei abweichendem Hash oder fehlender Paketidentitaet
  bleibt sie nicht nachgewiesen; beide Faelle prueft der CLI-Vertragstest.
  Eine eigene Parquet-Laufzeitversions-API wird nicht behauptet. Fuer
  Windows-Release-Builds mit hashgleichen Parquet-/Arrow-21.0.0-Paketen
  wird die Importkette vom Parquet-Writer ueber den Arrow-Codec zu beiden
  Zstandard-Kompressionsfunktionen auf dieselbe DLL geprueft und deren
  Versionsfunktion abgefragt. Andere Konfigurationen bleiben ausdruecklich
  nicht nachgewiesen. Ein isolierter Rechteckauftrag meldete Arrow 21.0.0,
  Parquet-Paket 21.0.0 und Zstandard 1.5.7; abweichende Pakethashes und
  fehlende Paketidentitaeten werden im CLI-Vertragstest abgelehnt. Der
  Bericht
  `files\provenienz-pruefung-24bc1995a0194c549ccac31c2152ca89\result.json`
  im Sitzungsordner hat SHA-256
  `AAE6FEF2145857501DAC1B4964BDF79737226893AE69C4BAA126DC9F2B8A466B`.
  Nach erfolgreicher Numerikvorpruefung und nachgewiesenen Versionen
  erzeugt die CLI ein temporaeres Manifest mit den echten Eingabehashes
  und Versionen und prueft alle Tabellen bitgenau. Zusaetzlich laedt sie
  das Modell ueber die typisierte Bibliothek und vergleicht bei einer
  Rechenprobe Streifen-IDs, Positionen und Lasten mit dem Ausgangsmodell.
  Der Arbeitsordner wird anschliessend entfernt. Tabellenroundtrip und
  Lade-/Rechenprobe sind Vorpruefungen, keine produktive End-to-End-Abnahme,
  Publikation oder vollstaendige Datenformatabnahme.
  Ein isolierter Rechteckauftrag bestand den Roundtrip und liess weder
  Modell noch Arbeitsordner oder Sperre zurueck; die CLI endete
  weiterhin mit Exitcode 4. Sein Ergebnis
  `files\cli-rundlauf-c4aa2b0e780c425d9757774c64168dd9\result.json`
  im Sitzungsordner hat SHA-256
  `C9528A923B5AB9017D1F1D3FDC8234B218683AF52C1FA21953D4879750BA79BE`.
  Nach der zusaetzlichen Absicherung der Windows-Importauswertung
  bestaetigte eine weitere isolierte Rechteckpruefung die Parquet- und
  Zstandard-Zuordnung sowie die typisierte Lade-/Rechenprobe. Bericht:
  `files\provenienz-neu-9a2186d7321c4e06b998776ff69672fd\result.json`,
  SHA-256
  `126730A9AF83E87C3226FB11BA00F152F1F8F4DF52F91703ACF2516C98CDB20E`.
  Die Uebernahme in ein produktives Manifest bleibt offen.
- **Tabellen-Worker:** Die fuenf unabhaengigen Parquet-Tabellen koennen
  bis zum wirksamen CPU-Limit parallel geschrieben werden; der Writer
  verwendet keine eigenen Spalten-Worker. Unter Windows haben parallele
  Tabellen-Worker Hintergrundprioritaet fuer CPU und I/O. Ein Abbruch
  beendet alle eigenen Worker vor der Bereinigung des Arbeitsordners.
  Der CLI-Vertragstest prueft tatsaechlich gestartete zwei und vier Worker,
  bytegleiche Tabellen gegen den seriellen Lauf sowie Abbruchbereinigung.
  Drei isolierte Rechteck-CLI-Vorpruefungen mit Limits 1, 2 und 4
  meldeten identische Tabellenmetadaten und SHA-256-Werte und bestanden
  die Ladeprobe; sie publizierten kein Modell. Berichte im Sitzungsordner:
  `files\worker-cli-d41a5b3219c74d798e786d4a64b6ddf7\1\result.json`,
  `2\result.json` und `4\result.json` unter demselben Ordner; ihre
  SHA-256-Werte sind `9E9405249CA0B103A7F13E18F2A2F860EC962A7E0E5A7DD9133612B637838189`,
  `9AB0A4FA75CCDB90E86A90128FC0E1810BF897C25084AE3BA7B2AC6168171B91`
  und `81AACF8042E6B2F76010C51C0B30CBD274C72B19688747D390C516420BEF3517`.
  Fuer Optimist, 420 und Folkeboot wurden ausserdem diagnostische
  Modelltabellen mit 1, 2 und 4 Workern erzeugt und dateiweise bytegenau
  verglichen; Eingabe- und Tabellenhashes sowie Programmhash stehen in
  `files\worker-modelle-113da16265114c9f933a755965f7b334\worker-nachweis.json`
  im Sitzungsordner, SHA-256
  `2168F69A154347309D3E9A40B7855FA4A124CE89A7C966BD30F4A81FE6E643C9`.
  Die Diagnosemanifeste verwenden Platzhalterversionen. Diese
  Teilpruefungen ersetzen keine produktive End-to-End-Abnahme.
- **Gleichwind-Worker:** Die 23 Geschwindigkeiten werden unabhaengig
  bis zum wirksamen CPU-Limit berechnet und anschliessend in fester
  Reihenfolge berichtet. Jeder Worker besitzt seine eigenen Solver und
  H1-Referenzen; bei Fehler oder Abbruch werden alle Worker beendet.
  Ein Vertragstest vergleicht den seriellen mit einem begrenzten
  parallelen Lauf. Drei isolierte Rechteckauftraege mit tatsaechlich
  1, 2 und 4 gleichzeitig vorhandenen Workern lieferten jeweils
  denselben vollstaendigen Gleichwindbericht wie der fruehere serielle
  Lauf. Die Gesamtzeiten unter wechselnder Fremdlast betrugen 74,0,
  35,2 und 21,6 Sekunden und sind keine zugesagten Leistungsgrenzen.
  Berichte im Sitzungsordner unter
  `files\gleichwind-worker-f4a82dfad9c14ba49af501df27e9e7ab`:
  SHA-256 der Ergebnisse fuer 1, 2 und 4 Worker
  `1A11508D21408BC9AD1514768BC81EBE073E20CF5B67BC7D9A5D436BB4747C52`,
  `8B3827C7665342340D4AB317B5A2B5084E00EA09F8249A2C375292F738DBFEC5`
  und `326965EC5D2E419CD7332F933B5946024BC1260F01D57984A18C105C34A23176`.
  Die Publikation blieb in diesen Vorpruefungen gesperrt. Eine weitere
  isolierte Optimist-Vorpruefung mit vier Workern lieferte denselben
  vollstaendigen Raster- und Gleichwindbericht wie der serielle Lauf;
  Eingabehashes, Ladeprobe, Publikationssperre und Bereinigung wurden
  geprueft. Ergebnis im Sitzungsordner unter
  `files\parallel-wiederholung-2a5124fe5ef74e62a2cbd4f146a34c8f\optimist\result.json`,
  SHA-256 `ED0244B857A0B0C5BDA2F737E56A3212E4786CE30364F3F1D1C9B0667650B0C4`.
  Auch die entsprechende 420-Wiederholung bestand; ihr Ergebnis unter
  `files\parallel-wiederholung-2a5124fe5ef74e62a2cbd4f146a34c8f\420\result.json`
  hat SHA-256 `E26652715E2F5F7FBF8F26E48B38DB673B19CC7329B6B001F106E6F44A880129`.
  Auch die entsprechende Folkeboot-Wiederholung bestand; ihr Ergebnis
  unter
  `files\parallel-wiederholung-2a5124fe5ef74e62a2cbd4f146a34c8f\folkeboot\result.json`
  hat SHA-256 `3B24926CF1D4483E76434853A8A7FA947B5AEC7D17E8EDB31174D1C0D093DBBC`.
  Die darin dokumentierten 32/48- beziehungsweise 64/48-Modenvergleiche
  der 14 Windfelder auf beiden Rastern erreichen beim umgekehrten lokalen
  Nullwind maximale Quotienten von 1,51955/0,79371 fuer Optimist,
  5,19728/5,49147 fuer 420 und 41,40290/7,26362 fuer Folkeboot.
  Diese Zahlen beziehen sich auf den diagnostischen 0,1-%-Bezug; die
  Spezifikation setzt fuer Modenvergleiche keine eigenstaendige Grenze.
  Sie sind weder ein bestandener 0,1-%-Modentest noch ein direkter
  H1-Vergleich und ersetzen nicht die verbindlichen Numeriknachweise.
  Eine separate Solverdiagnose prueft fuer dieselben 14 Windfelder beide
  Raster und 32, 48 sowie 64 Moden. Pro Segel wurden 72 geloeste Systeme
  anhand der ausgegebenen Matrix, Koeffizienten und rechten Seite separat
  nachgerechnet; 12 Diagnosen ohne geloeste Traglinie sind getrennt
  ausgewiesen. Die maximalen nachgerechneten relativen Rueckwaertsfehler
  sind 5,28e-19, 7,12e-19 und 9,92e-19 und liegen unter 1e-10.
  Der Abgleich verwendete dieselben Windfelder wie die CLI-Berichte.
  Der lokale Nachweis
  `files\gradient-residuals-407ead6b\nachweis.json` hat SHA-256
  `9278F5804092513B6CB36A491DB2CBDD1C487B91F860790F801EE3C890CC0E6F`.
  Dies ist ein algebraischer Teilnachweis, keine unabhaengige physikalische
  Validierung oder vollstaendige Numerikabnahme.
  Ein weiterer CLI-Vertragstest brach die parallele Gleichwindpruefung
  nach Beginn gezielt ab und pruefte Exitcode 130 sowie die Bereinigung.
  Fuer lange Einzelgeschwindigkeiten meldet die CLI nach 30 Sekunden
  ohne neuen Abschluss ein Lebenszeichen mit der Zahl abgeschlossener
  Geschwindigkeiten. Hochgerechnete Restzeiten bleiben wegen
  unterschiedlicher Fallkosten und Fremdlast unsicher. Bei einem
  isolierten 420-Diagnoselauf erschien das Lebenszeichen nach 30 Sekunden
  ohne Fortschritt bei 7 von 23 Geschwindigkeiten. Der eigene
  Diagnoseprozess wurde danach gezielt beendet und seine Sperre entfernt;
  dies ist kein abgeschlossener Numeriklauf. Das Protokoll im
  Sitzungsordner unter
  `files\heartbeat-pruefung-1493cb4575ca45d791256e3e834a28b8\prozess.log`
  hat SHA-256 `9B89BF0E2AB445A94CDF20E540DA47F904B93131DC30DE057229EF2A21C34F41`.
- **Vorlaeufige Leistungsmessung:** Dasselbe Diagnoseprogramm misst
  optional Dateiumfang, Laden und 20 aufeinanderfolgende typisierte
  gekoppelte Rechnungen. Auf einem AMD Ryzen 5 8600G (6 Kerne,
  12 logische Prozessoren), MSVC-Release-Build und bei gleichzeitig
  laufenden Numeriktests ergaben sich fuer Optimist, 420 und Folkeboot
  jeweils 210204, 221245 und 517162 Byte fuer sechs Dateien; das Laden
  aus bereits gelesenem Dateicache dauerte 618, 668 und 701 ms, die
  mittlere Rechnung 4,61, 2,37 und 14,42 ms. Eingaben: je Streifen
  `(-5-0,02*id; 0,5; 0,1) m/s`, Dichte 1,225 kg/m3,
  Viskositaet 1,5e-5 m2/s; eine Rechnung vorab als Warm-up.
  Diese Einzelmessungen unter Fremdlast und mit Diagnosemanifest sind
  keine Leistungsgrenzen und ersetzen nicht U-S-000031 nach vollstaendiger
  Integration.
- **Integrationstest erneut offen:** Ein erneuter Lauf des bestehenden
  `cli_integration` scheiterte beim Aktualisieren des Fingerprints mit
  `CORE-FPR-0003` beim atomaren Ersetzen einer Datei. Die unmittelbare
  Wiederholung dieser einzelnen Aktualisierung im isolierten Testmodell
  war erfolgreich; die Ursache ist damit nicht geklaert. Auch der
  vollstaendige Wiederholungslauf scheiterte mit demselben Fehler, diesmal
  bereits bei der ersten Fingerprint-Aktualisierung. In einem separaten
  Build mit unveraendert aus Quellen gebautem SegelPhysik-Core bestand der
  vollstaendige Integrationstest; dieser Einzellauf erklaert die Fehler
  der vorkompilierten Bibliothek nicht und ersetzt deren Nachpruefung nicht.
  Eine weitere Folge mit je drei wechselnden Aktualisierungen durch beide
  Builds bestand; zuvor waren sechs Aktualisierungen mit der vorkompilierten
  Bibliothek im selben isolierten Modell fehlgeschlagen. Die Ursache des
  wechselnden Verhaltens bleibt offen. Ein weiterer vollstaendiger
  `cli_integration`-Lauf mit der vorkompilierten Bibliothek bestand;
  die vorherigen Fehler werden dadurch nicht als geklaert gewertet.
  Ein spaeterer Test mit aus Quellen gebautem Core scheiterte nach dem
  Modellordneraustausch beim erwarteten erfolgreichen Fingerprintabschluss.
  Das damalige Testergebnis wurde nicht protokolliert; im isolierten
  Modell blieb eine temporaere Validierungsdatei zurueck. Der Fehlercode
  und die Ursache dieses einzelnen Fehlschlags sind nicht belegt.
  Eine anschliessende vollstaendige Wiederholung bestand; erfolgreiche
  und fehlgeschlagene Testabschluesse werden jetzt separat protokolliert.
  Ihr Nachweis
  `build\integration_38b529a452514c3b8dcd801f726d03ea\test_evidence.json`
  hat SHA-256 `5ACCC1F04FF4F032FA885E6E21B6906CC1D9B0A22FE87B48965290608B05F85C`.
  Die Wiederholung klaert den vorigen Fehlschlag nicht.
  Eine eigenstaendige I/O-Probe in einem isolierten Sitzungsordner fuehrte
  500 Dateiwechsel mit denselben Windows-Primitive und Flags durch:
  Ein `MoveFileExW` schlug mit Systemfehler 32 (Freigabekonflikt) fehl.
  Die Probe verwendet keine Core-API und beweist weder, dass dies die
  Ursache eines konkreten Core-Fehlers war, noch welcher Prozess die
  Freigabe hielt. Der Nachweis
  `files\fingerprint-io-probe-0fe7e44c\ergebnis.json` hat SHA-256
  `C6DE7AA19D95D006843006C33197827A840043CF096DC08AF139A50FCA228BEE`.
  Eine zweite eigenstaendige Probe meldete einen Freigabekonflikt bei
  1000 Wechseln. Unmittelbar danach scheiterte auch der exklusive Zugriff
  auf die temporaere Datei mit Fehler 32; der exklusive Zugriff auf die
  Zieldatei gelang. Eine kurzzeitige Sperre kann bei der Folgepruefung
  verpasst werden. Der ausloesende Prozess oder Dateisystemfilter und
  ein Zusammenhang mit den Core-Fehlern sind weiterhin nicht belegt.
  Nachweis `files\fingerprint-io-probe-dbc9122c\ergebnis.json`, SHA-256
  `7FBEF2C96B2E9971D017B8C1C94936626816A3F44365A62BBFC713508B7D4882`.
  Nicht anwendbare H1-Vergleiche sind getrennt ausgewiesen; weitere
  Nachweise bleiben als nicht ausgefuehrt gekennzeichnet. Bei Abbruch oder einem
  numerischen Loesungsfehler wird weiterhin sofort diagnostiziert.
  `create-only` schuetzt bereits vorhandene Ordner; bei fehlgeschlagener
  Neuberechnung bleibt auch bei `overwrite` der alte Ordner erhalten.
  Eine eigene `Segelmodell.lock`-Verzeichnissperre verhindert
  konkurrierende Erzeugungsauftraege; `pruefen` bleibt davon unabhaengig
  und schreibfrei. Vertragstests pruefen Sperrkonflikt und Entfernung
  der Sperre nach Ueberspringen und numerischem Fehlschlag.
  `gpu=erforderlich` wird mit Ressourcenfehler abgelehnt, da fuer diese
  Pruefung noch kein GPU-Backend implementiert ist; automatische
  CPU-Auswahl nennt den Grund. Der CLI-Vertragstest prueft die Ablehnung.
  Ein isolierter Integrationstest startete zwei echte konkurrierende
  Segelmodellprozesse mit `overwrite`: Der erste endete wegen der
  weiterhin offenen Pflichtnachweise erwartungsgemaess mit Exitcode 4,
  der zweite wegen der belegten Sperre mit Exitcode 5. Vorhandenes Modell,
  alter Lookup und Eingabedateien blieben hashgleich; Sperre und
  Arbeitsordner wurden entfernt. `cli_integration` bestand nach der
  Pruefung der vollstaendigen Ordnerinhalte in 112,90 s erneut.
  Der lokale Nachweis
  `build\integration_9d9df990bcb048a9b556839f6e396c9a\test_evidence.json`
  hat SHA-256 `E8E896041052E5A5C1039813611CD839FD275DAE5B9AF518665B0580914A83F3`.
  Der isolierte Integrationstest prueft ausserdem `create-only` fuer ein
  vorhandenes Segelmodell mit Fingerprint `skip` und `update`: Nur
  Aerodynamik wird aktualisiert; bei gesperrtem Fingerprint wird der
  Core-Fehler gemeldet. Modell-Markierung und alter Lookup bleiben
  hashgleich, Sperr- und Arbeitsordner bleiben nicht zurueck.
  Vollstaendige Erzeugung und ein echter Core-Fingerprint nach
  Publikation sind noch nicht end-to-end nachgewiesen.
- **Zusaetzliche Fock-Stichproben:** Fuer 420- und Folkeboot-Fock
  wurden Kopien der vorhandenen Geometrien und Auftraege mit dem
  jeweiligen Auftragsraster 0,125/0,0625 m isoliert geprueft.
  Der diagnostische Modell-Roundtrip und vier parallele Windrechnungen
  mit seriellem Vergleich bestanden fuer beide; die Diagnosemanifeste
  sind keine produktiven Modelle. Je neun Gleichwindfaelle zwischen
  0 und 50 m/s ergaben endliche Lasten und bei exakt null Nulllasten.
  Je drei H1-gueltige Vergleiche bestanden, die anderen sechs lagen
  ausserhalb des H1-Gueltigkeitsbereichs. Maximaler H1-Grenzquotient:
  0,065561 fuer 420 und 0,037410 fuer Folkeboot.
  Je 14 Rasterfaelle wurden anhand der ausgegebenen Lasten unabhaengig
  nachgerechnet: 420 beziehungsweise 644 Kraft- und Momentvergleiche,
  maximale 5-%-Grenzquotienten 0,578350 und 0,630284.
  Die Originaleingaben blieben hashgleich. Nachweis der Stichproben
  `files\fock-stichprobe-8a4bd98d\nachweis.json`, SHA-256
  `24DBF6199AD67AD83125395A2499538E4E5DC57F9D4D4E2C73CE93DAAA93935E`;
  unabhaengiger Rasternachweis
  `files\fock-stichprobe-8a4bd98d\raster-nachgerechnet.json`, SHA-256
  `675B960CE742FEA05898A62B0CC45D6CB10FCEEB9ABAE7936031991A4DCA8CC7`.
  Dies ist keine vollstaendige Fock- oder Produktabnahme.
  Nach diesen Stichproben bestanden im Gesamtbuild `core_contract`,
  `strip_gradient_acceptance` und `model_consumer_contract` (3/3)
  sowie im eigenstaendigen Core die beiden CTest-Tests (2/2).
- **Trennung der Nachweise:** Auf Nutzerentscheidung vom 16.11.2026
  wird die projektweite Abnahme von den einzelnen Erzeugungspruefungen
  getrennt. Der normale Generator bleibt bis zur Gesamtfreigabe gesperrt.
  Ein mit `BUILD_TESTING` gebautes `SegelMacherAbnahme` darf nach
  bestandenen Modellvorpruefungen ausschliesslich innerhalb des
  temporaeren Verzeichnisses publizieren. Das Manifest weist die
  projektweiten Pflichtnachweise weiterhin als nicht ausgefuehrt aus;
  Erzeugername und Abschluss kennzeichnen den Test-Build. Eine solche
  Publikation allein ist keine Gesamtfreigabe. `cli_contract` bestand;
  `cli_integration` bestand nach der Begrenzung auf das temporaere
  Verzeichnis. Dabei erzeugte und publizierte der Test-Build ein
  Rechteckmodell mit echten Erzeugungspruefungen und aktualisierte
  anschliessend den Fingerprint ueber die Core-API. Ein unabhaengiger
  Parquet-Leser pruefte Tabellen, Zeilenzahlen und Hashes; Eingaben und
  alter Lookup blieben unveraendert. Zugriffe ausserhalb des temporaeren
  Verzeichnisses und die alte Lookup-Erzeugung wurden abgelehnt.
  Der Nachweis `build\integration_42c83a589035475aa30c9fb497639961\test_evidence.json`
  hat SHA-256
  `4CED1542CB413672F584315C46A63F9A3052955759AB6EE48A61F9F970751F87`.
  Fuer die drei Referenzsegel wurden anschliessend die bestehenden
  CLI-Numerikberichte und deterministischen Tabellen wiederverwendet:
  Eingaben, Bericht, Programm und Tabellen wurden ueber Hashes
  abgeglichen; die drei urspruenglichen CLI-Manifeste liessen sich
  bitgenau rekonstruieren. Ein Testhelfer publizierte die Tabellen
  isoliert mit den tatsaechlich bestandenen frueheren Vorpruefungen und
  aktualisierte den Fingerprint ueber die Core-API. Ein unabhaengiger
  Parquet-Leser pruefte Schema, Zeilenzahlen, Codec und Hashes.
  Eingaben, bestehender Lookup und andere Fingerprintbereiche blieben
  unveraendert. Ein erster isolierter Versuch scheiterte an einer
  falsch gesetzten Testmodell-ID nach Publikation; nach Korrektur der
  Testdaten bestanden alle drei. Es wurden keine Numerik-Langlaeufe
  wiederholt und keine externen Modelle veraendert.
  Der Nachweis
  `files\isolierte-wiederverwendung-a65af3130a894714b07f172df92b6edf\nachweis.json`
  hat SHA-256
  `638B42BA7157AE0634FDBF506F5705A0653EC4A638485FED45B1D5722EA67557`.
  Die publizierten Modellkopien wurden geladen und ihre Lasten mit
  frisch vorbereiteten Modellen verglichen. Vier parallele Aufrufe je
  Modell stimmten mit seriellen Rechnungen ueberein. Fuer Optimist,
  420 und Folkeboot betrugen die Dateiumfaenge 154908, 222217 und
  307081 Byte, die Ladezeiten 472,1, 539,5 und 570,0 ms und die
  mittleren Rechenzeiten 1,45, 3,36 und 4,56 ms. Gemessen wurden
  jeweils 20 Rechnungen nach Aufwaermen bei warmem Dateicache auf
  Windows 11; dies sind keine Leistungsgrenzen. Nachweis
  `files\isolierte-wiederverwendung-a65af3130a894714b07f172df92b6edf\modellpruefung-identitaeten.json`,
  SHA-256
  `1537B253AEE8725D5670E569C1396ADAC043FF7246E11074095CBB17A4A7ADDB`.
  Dies ist ein zusammengesetzter Integrationsnachweis mit Testhelfer,
  kein vollstaendiger CLI-Publikationslauf je Referenzsegel. Die
  produktive Gesamtfreigabe bleibt offen.
  Danach erhielt der isolierte CLI-Testbuild eine Uebernahmeoption
  fuer einen bestehenden Vorpruefbericht und dessen vorab bekannten
  SHA-256. Er prueft Eingabehashes, Werkzeug- und Bibliotheksversionen,
  Fallstatus und die bytegleichen neu erzeugten Tabellen; das Manifest
  verweist auf den uebernommenen Bericht. Ein Hash allein authentifiziert
  den Bericht nicht; dieselbe Werkzeugversion belegt keine identische
  Programm- oder Quellcodefassung. Fehlerhafte Hashes, Eingaben,
  Werkzeugversionen oder Tabellen blockieren den Austausch.
  `cli_contract` und `cli_integration` bestanden;
  letzterer prueft diese Ablehnungen und den echten CLI-Austausch mit
  Fingerprint. Sein Nachweis
  `build\integration_39523704194e468ba72043db333a6779\test_evidence.json`
  hat SHA-256
  `A37406FA0971343C9DCFC3088BCAF5CDD72F0859CAE1FCEC97A9011EB51FE482`.
  Auch Optimist, 420 und Folkeboot wurden mit dem Testbuild und ihren
  hashgeprueften Vorpruefberichten isoliert per CLI erzeugt,
  publiziert, geladen und parallel geprueft; der Core-Fingerprint
  wurde jeweils aktualisiert und die alten Lookup-Testdateien blieben
  unveraendert. Neue Numerik-Langlaeufe wurden nicht ausgefuehrt.
  Der Nachweis
  `files\cli-wiederverwendung-7b20c8ee92a64d0899b01f0feb9de7bf\nachweis.json`
  hat SHA-256
  `D8D4B47642601F9809B7187550388BF89434CD35A2B1CF22105C692085005E64`.
  Nach allen drei erfolgreichen Pruefungen scheiterte die automatische
  Bereinigung an einem noch offenen Parquet-Leser; der eigene
  Temporaerordner wurde anschliessend entfernt. Der Leser wird im
  Skript jetzt explizit geschlossen; die Bereinigung wurde separat
  getestet. Der Gesamtlauf des Skripts endete wegen der Bereinigung
  mit Fehler, nicht mit Exitcode null. Dies bleibt ein isolierter
  Testbuild und ersetzt keine Produktfreigabe.
  Ein weiterer unabhaengiger PyArrow-Abgleich der drei publizierten
  Modellkopien bestand fuer alle fuenf Tabellen: Spaltennamen,
  Datentypen, Einheitenangaben, ZSTD, Hashes, endliche Werte,
  eindeutige Schluessel, Fremdschluessel, Streifengeometrie,
  Flaechennormalen und vollstaendige Matrizenindizes wurden geprueft.
  Nachweis
  `files\cli-wiederverwendung-7b20c8ee92a64d0899b01f0feb9de7bf\unabhaengiger-datenformatnachweis.json`,
  SHA-256
  `23E636D3091B06154D34C642976C725740B995ECC876555E165AF45B62649C0F`.
  Die aelteren Testmanifeste tragen fuer den Vorpruefbericht eine
  angenommene Schemaversion `1`, obwohl der Bericht selbst keine
  Schemaversion enthaelt. Der Testbuild kennzeichnet solche Berichte
  nun als `unversioniert`; die archivierten Testmanifeste bleiben
  unveraendert.
  Nach Ergaenzung der Werkzeugversionspruefung schlug der erste
  Integrationstest fehl, weil der Testbuildzusatz faelschlich mit der
  Version des regulaeren Vorpruefberichts verglichen wurde. Nach
  Korrektur des Vergleichs bestanden gezielte Ablehnungsproben; im
  anschliessenden Integrationstest scheiterte ein spaeterer, simulierter
  Publikationsabschluss am echten Core-Fingerprint mit `CORE-FPR-0003`.
  Der Modellordner war dabei bereits geschrieben, und genau dies wurde
  mit Exitcode 5 gemeldet. Die Ursache des sporadischen
  Dateifreigabefehlers bleibt offen. Eine Wiederholung scheiterte
  ebenfalls mit `CORE-FPR-0003`, diesmal beim Fingerprint-Update eines
  unveraendert uebersprungenen alten Lookup-Testmodells. Beide
  Integrationstestlaeufe sind daher nicht bestanden. Die zuvor im
  Test durchgefuehrte Abbruchprobe
  erreichte waehrend der Rasterpruefung Exitcode 130; vorhandenes Modell,
  alte Lookup-Testdateien und Eingaben blieben hashgleich.
  Eine erneute schreibfreie `pruefen`-Kontrolle direkt an den fuenf
  vorhandenen Segelauftraegen bestand; die drei Validierungsdateien
  blieben hashgleich. Auf einem separaten temporaeren Testmodell auf
  Laufwerk C: gelangen anschliessend zwoelf Core-Fingerprint-Updates
  nach jeweils geaendertem Testinhalt. Dies beweist weder, dass der
  sporadische Dateifehler behoben ist, noch einen Fingerprint-Update
  produktiver Modelle.
  Der danach erneut ausgefuehrte Integrationstest bestand in 193,70 s.
  Sein Nachweis
  `build\integration_4601d698642d47a4b6c8451788feac92\test_evidence.json`
  hat SHA-256
  `519FEF7783C5D49F7327F366FD5D873C2113F79F3F2B983F340570D8AE7E0B2F`.
  Die beiden vorherigen Fehlschlaege und deren ungeklaerte Ursache
  bleiben davon unberuehrt.
- **U-S-000029 bis U-S-000033 offen:** Die oben dokumentierten
  Numerik-, Integrations-, Publikations- und Leistungsergebnisse sind
  Teilnachweise, aber noch nicht als vollstaendige Abnahme gegen alle
  Pflichtkriterien zusammengefuehrt. Insbesondere sind die Erzeugungen
  isolierte Testbuild-Laeufe; die produktive Freigabe und die Abloesung
  des alten Generators sind nicht erfolgt. H1 und bestehende
  Lookup-Dateien bleiben unveraendert. Toleranzen werden nicht gelockert.

#### Nachtrag zum Abnahmestand vom 16.11.2026

Die nachfolgenden Ergebnisse aktualisieren den davor dokumentierten
Zwischenstand; historische Fehler und deren Ursachen bleiben erhalten.
Die Nachweise liegen im lokalen Sitzungsordner, nicht im Repository.

| Umsetzungsschritt | Aktueller Stand und Beleg | Verbleibende Grenze |
|---|---|---|
| U-S-000029 | **Abgeschlossen im vereinbarten Umfang:** Die archivierte Numerikmatrix fuer Optimist, 420 und Folkeboot wurde anhand hashverknuepfter Berichte wiederverwendet: 69 H1-Berichte, 42 Rasterfaelle, H1-Maximalquotienten 0,701968 / 0,733685 / 0,830844 und Raster-Maximalquotienten 0,852688 / 0,487668 / 0,479540; alle gueltigen Faelle bestanden. Nicht anwendbare H1-Faelle bleiben getrennt. Beleg `files\numerik-nachweisverknuepfung-20261116.json`, SHA-256 `9D26C9F9B666596D27EB6A9A14FE7C91706FABC8BA99C34F6E1CF91CDEA127B3`. Fuer 420- und Folkeboot-Fock genuegen gemaess Nutzerentscheidung die bestandenen Stichproben; eine Vollmatrix wird nicht behauptet. Keine Numerik-Langlaeufe wiederholt. | Die archivierten Berichte enthalten keinen unabhaengigen Hash des damaligen Quellstands. Diese Provenienzgrenze wurde vom Nutzer akzeptiert; sie bleibt sichtbar. Die Focks sind nur stichprobenartig geprueft. |
| U-S-000030 | **Abgeschlossen:** Drei isolierte CLI-Erzeugungen mit wiederverwendeten Berichten, Laden, Lastvergleich, Parallelpruefung und unabhaengiger PyArrow-Pruefung aller 15 Tabellen bestanden. Nachweise `files\cli-wiederverwendung-b19be3c1499c4d05b024e94807a28753\nachweis.json`, SHA-256 `86DC75544A2106D507FFFF3073CF0CAC91A4E74F2B202DBCDDBE10159E63AF31`, und `unabhaengiger-datenformatnachweis.json`, SHA-256 `F6CED05463D2DC821C7E01A82775407340AED498A9DDF3ABFD3D50AC0C764915`. Die Produktions-CLI erzeugte und ersetzte zusaetzlich ein Modell in einer temporaeren Testkopie; Konkurrenz und Abbruch wurden abgewiesen. | Alle realen Segelauftraege und alten Lookups blieben unveraendert. Projektweite Nachweise bleiben in einzelnen Laufzeitmanifesten als nicht in diesem Aufruf ausgefuehrt markiert; ihre zusammengefuehrten Belege stehen hier im Plan. |
| U-S-000031 | **Abgeschlossen:** Messungen an den drei isolierten CLI-Modellen: Optimist 155.151 B / 335,21 ms / 1,47 ms, 420 222.455 B / 414,42 ms / 2,05 ms, Folkeboot 307.325 B / 468,80 ms / 4,06 ms (Datenumfang / Laden / mittlere Rechnung). Warmer Cache, eine Aufwaermrechnung und 20 Rechnungen; keine Leistungsgrenzen. | Werte gelten fuer diesen isolierten Lauf und die dokumentierte Testumgebung. |
| U-S-000032 | **Abgeschlossen:** Der Produktionsbefehl `lookup` ist deaktiviert; `segelmodell` veroeffentlicht nach bestandenen modellbezogenen Pruefungen. `SegelMacherLegacyTest` erhaelt den alten Pfad nur bei `BUILD_TESTING` fuer Regressionstests. H1 bleibt als Rechenkern und Referenz erhalten. | Vorhandene alte Lookup-Ordner wurden nicht veraendert. Physikalische Validierung ist nicht erfolgt. |
| U-S-000033 | **Abgeschlossen:** Vollstaendiger Release-Build und Gesamt-CTest 5/5 bestanden; eigenstaendiger Core-Build und CTest 2/2 bestanden; `git diff --check` bestanden. Integrationsevidenz `build\integration_e0646cc9b94f4718994c0f84908ec1ca\test_evidence.json`, SHA-256 `E634877201A9BC785A44883BD9721B3BA1E94008947C7223D4C2F98C8FD94129`. Produktionsprogramm SHA-256 `2B43DF58667C91BE850BAF75DC5A4025D6AC8F2D19732FC4C7AA8DB323856DBE`. | Zwei fruehere `CORE-FPR-0003`-Fehler bleiben als vom Nutzer akzeptierte, ungeklärte Einschraenkung dokumentiert; der aktuelle Integrationstest bestand. Es erfolgte keine Produktdatenaktion. |

#### Abgleich der Anforderungen A-S-00043 bis A-S-00072

Die folgenden Belege beziehen sich auf die oben dokumentierten Builds
und isolierten Testkopien. Sie sind keine Freigabe fuer einen anderen
Programmstand; die aelteren Berichte enthalten keinen unabhaengigen
Quellcodehash. Die Nachweisdateien unter `files` liegen im lokalen
Sitzungsordner und nicht im Repository.

| Anforderungen | Vorliegender Beleg | Verbleibende Grenze |
|---|---|---|
| A-S-00043 bis A-S-00046 | Geometrie- und Core-Vertragstests; aktuelle Tabellen der drei Referenzsegel unabhaengig auf Schema, Schluessel, Referenzen, Geometrie und Flaechennormalen geprueft. | Physikalische Validierung bleibt separat offen. |
| A-S-00047 bis A-S-00053, A-S-00061 bis A-S-00064, A-S-00070 | Archivierte Numerikmatrix fuer Optimist, 420 und Folkeboot bestanden; H1- und Rasterquotienten sowie Eingabeprovenienz sind im Nachweisverzeichnis aufgefuehrt. | Fehlender unabhaengiger Quellcodehash ist akzeptierte Grenze; Focks nur mit Stichproben geprueft. |
| A-S-00054 bis A-S-00056 | Drei isolierte CLI-Modelle erzeugt, geladen, parallel berechnet und unabhaengig mit PyArrow geprueft; Produktions-CLI-End-to-End im Testordner bestanden. Lade- und Korruptionsfehler getestet. | Keine physikalische Validierung. |
| A-S-00057, A-S-00058, A-S-00067 | Konkurrenzschutz, Abbruch, geschuetzter Austausch und Erhalt alter Lookup-Testdateien geprueft; regulaerer `segelmodell`-Pfad produktiv verfuegbar. | Keine Produktivmodelle aktualisiert. |
| A-S-00059, A-S-00060 | Typisierte Bibliothek und parallele Nutzung der drei Modellkopien geprueft; produktiver CLI-Pfad baut, laedt und veroeffentlicht ein Testmodell. | Keine physikalische Validierung. |
| A-S-00065 | Fehlende physikalische Messreferenzen und der nicht validierte Modellstatus sind ausgewiesen. | Kein physikalischer Validierungsnachweis; Messdaten sind kein eigenstaendiger Nutzungsblocker. |
| A-S-00066 | H1 ist erhalten; alter `lookup`-Erzeugungspfad aus Produktions-CLI entfernt. | Legacy-Aufruf existiert nur noch im Testziel bei `BUILD_TESTING`. |
| A-S-00068 | Bytegleiche diagnostische Tabellen aus 1/2/4 Workern fuer die drei Referenzsegel sowie isolierte CLI-Ausgaben geprueft. | Keine Behauptung fuer andere Bibliotheks- oder Codecversionen. |
| A-S-00069 | Datenumfang, Ladezeit und Rechnungszeit an drei isolierten CLI-Modellen gemessen. | Keine Leistungsgrenzen. |
| A-S-00071 | CPU-Budget und Ressourcenfehler in Vertrags- und Integrationstests geprueft. | Ein simulierter GPU-Test ist kein Nachweis fuer reale GPU-Hardware. |
| A-S-00072 | Echter Core-Fingerprint nach isolierter CLI-Publikation sowie Fehlerfaelle getestet. | Sporadische fruehere `CORE-FPR-0003`-Ursache ungeklart und als Einschraenkung akzeptiert; kein produktiver Modellordner aktualisiert. |

### 11.3 Test- und Abnahmematrix

1. **Referenz:** Fuer Optimist, 420 und Folkeboot direkte H1-Rechnungen
   mit Faktor 4 und 48 Moden verwenden. Fuer jeden in 13.7 vorgegebenen
   gleichfoermigen Winkel-/Geschwindigkeitsfall mit innerhalb des H1-
   Bereichs liegenden lokalen Reynolds-Zahlen Gesamt- und
   Streifenlasten vergleichen. Das H1-Integrationsraster an den neuen
   Streifengrenzen aufteilen; pruefen, dass seine Summe die direkte
   Gesamtlast reproduziert, und Referenzkonvergenz separat dokumentieren.
   Kraft und Moment jeweils mit euklidischer Vektornorm bewerten:
   `0.001*q_max*S + 1e-9 N` beziehungsweise
   `0.001*q_max*S*c_ref + 1e-9 Nm`; je Streifen `S_i` statt `S`.
2. **Raster:** Jeden Ausgabestreifen halbieren. Dasselbe definierte
   Windfeld an den jeweiligen groben und feinen Abfragepunkten
   auswerten, feine Paare auf den groben Abfragepunkt verschieben und
   summieren. Fuer Gesamt- und Streifenlasten dieselben Normen und Bezuege
   mit `0.05` statt `0.001` verwenden; bei null Staudruck nur die
   additive Absolutgrenze verwenden.
3. **Wind-/Grenzfaelle:** Alle in 13.6/13.7 angegebenen Geschwindigkeiten
   und arithmetischen Intervallmitten, Azimute und Neigungen samt
   Uebergangspunkten sowie die dort festgelegten linearen Gradienten in
   beiden Richtungen pruefen. Hinzu kommen gegenlaeufige und die
   Azimutnaht kreuzende Winde, lokaler und globaler Nullwind, rein
   vertikaler Wind, beide Pole und die Niedrig-Re-Grenze. Nicht endliche,
   doppelte, fehlende, fremde und ausserhalb des Bereichs liegende
   Eingaben muessen mit passender Diagnose scheitern.
4. **Solver und Daten:** Rueckwaertsfehler nach 13.11, 32/48/64-
   Modenvergleiche, Schema, Schluessel, Fremdschluessel, Hashes,
   Roundtrip und Worker-Determinismus pruefen. Jeden Fehlschlag mit
   Geometrie, Wind, Luftdaten, Streifen-ID, Ergebnis und Sollgrenze
   reproduzierbar protokollieren.
5. **Status:** Numerische Pflichtkriterien, technische Integration und
   ggf. physikalische Messreferenzen getrennt bewerten. Fehlende
   Messdaten bleiben sichtbar, sind aber kein eigener Nutzungsblocker.
   Keine einzelne erfolgreiche Teilpruefung steht fuer die gesamte
   Abnahme.

### 11.4 Meilensteine und Abschluss

| Meilenstein | Eintritt | Abnahme |
|---|---|---|
| M1 Referenz und Geometrie | Umsetzungsauftrag; U-S-000022/23 | Referenz, Konturschnitte, IDs und Flaechensummen reproduzierbar geprueft. |
| M2 Gekoppelte Rechnung | M1; U-S-000024/25 | Grenz-, Fehler-, Kraft- und Momententests sowie Solverresidual bestanden; keine Aussage zur vollen Rasterabnahme. |
| M3 Modell und Schnittstelle | M2; U-S-000026 bis U-S-000028 | Versioniertes Roundtrip, CPPI-/Thread-/Fehlervertrag und geschuetzte isolierte Publikation bestanden. |
| M4 Strenge Abnahme | M3; U-S-000029/30 | Im vereinbarten Umfang bestanden: archivierte Numerikmatrix wiederverwendet, Produktionsintegration und unabhaengiger Tabellenleser bestanden; Provenienzgrenze, Fock-Stichproben und fehlende physikalische Validierung ausgewiesen. |
| M5 Ablösung und Abschluss | M4; U-S-000031 bis U-S-000033 | Bestanden: alter Generator aus Produktions-CLI entfernt, `segelmodell` freigegeben, H1 und alte/externe Dateien erhalten; Gesamtprojekt- und Core-Testpfade bestanden. |
