# Umsetzungsplan: Lookup-Generator

Stand: 12.11.2026

Status: **Parquet/Zstandard-Umstellung umgesetzt; Release-Build und CTest
3/3 sowie separater Core-Build/CTest 1/1 bestanden. Leistungsmessung an einer
realen grossen Lookup-Kopie und physikalische Validierung bleiben offen.**

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
