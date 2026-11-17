# SegelMacher: Offline-Generator fuer Segel-Lookups

Stand: 15.11.2026
Status: **Zur Umsetzung freigegeben am 05.11.2026 durch ausdruecklichen Nutzerauftrag.**

Abschnitt 12 wurde am 14.11.2026 nach Nutzerentscheidungen und bestaetigter
Core-Abstimmung finalisiert und zur Umsetzung freigegeben.
Abschnitt 13 wurde am 15.11.2026 finalisiert; seine Umsetzung ist noch
nicht beauftragt und seine Nachweise stehen aus.

Entscheidung vom 12.11.2026: CSV-Ausgaben und CSV-Exporte entfallen
vollstaendig. Parquet mit Zstandard ist das verbindliche Ausgabeformat fuer
die drei Ergebnistabellen. Die bisherigen CSV-Anforderungen sind durch
A-S-00031 bis A-S-00036 ersetzt. Bestehende CSV-Ergebnisse werden nicht
automatisch migriert; der Umsetzungsstand und noch ausstehende Leistungs-
messungen stehen in Abschnitt 11 und im Umsetzungsplan.

## 0. Finale Anforderungen und technische Konkretisierung

Die Freigabe lautet: „Dann setzte den Umsetzungsplan um“.
Anforderungskennungen wurden nach Pruefung der vorhandenen Nummernkreise
erst bei dieser Finalisierung vergeben. Die folgenden Anforderungen und
die [Modellfestlegung H1](../Dokumentationen/Modellfestlegung_Lookup.md)
konkretisieren die zuvor offenen Verfahren. Die nachfolgenden historischen
Formulierungen „Vorschlag“, „offen“ oder „Entwurf“ beschreiben die Herkunft;
sie sind **keine Behauptung einer technischen oder fachlichen Abnahme**.
GPU-Implementierung, physikalische Referenzabdeckung und strenge numerische
Abnahme bleiben soweit unbelegt offen; der Auftrag wird dadurch nicht
nachtraeglich eingeschraenkt.

| Anforderung | Pruefbares Ergebnis / Abschnitt |
|---|---|
| A-S-00001 | CLI lookup/pruefen, Hilfe/Version, arbeitsverzeichnisunabhaengige Unicode-Pfade (3.1/3.2). |
| A-S-00002 | Genau ein Abschluss-JSON auf stdout mit definierten Exitcodes (3.3). |
| A-S-00003 | Deutsche zeitgestempelte Phasenmeldungen, Teil-/Gesamtfortschritt, Lebenszeichen und Fehlerabschluss nur auf stderr (3.4). |
| A-S-00004 | Schemaversionen, Pflichtfelder, doppelte Schluessel, Optionen und Bereit-Status pruefen (4). |
| A-S-00005 | Geschlossene nicht selbstschneidende Kontur, horizontale Intervalle, Spitzen/Kopfkanten und Flaechenbezug korrekt (4.1). |
| A-S-00006 | Parabolische Woelbung, Position und Twist wirken sich aus; gerades Baum-Unterliek bleibt gerade (4.1/5.1). |
| A-S-00007 | Lokale Re abgeleitet und vor teurer Rechnung auf expliziten Modellbereich geprueft (5.2). |
| A-S-00008 | Woelbungsabhaengige anliegende Profilkraefte und Profilmomente, eigene Providerkonvergenz (5.1). |
| A-S-00009 | Explizit begruendete viskose/Re-/Abrissschliessung mit Grenzen und Referenzpruefungen (5.1). |
| A-S-00010 | Gekoppelte Traglinie mit Abwind/induziertem Widerstand ohne Doppelzaehlung (5.1). |
| A-S-00011 | Stetige Hochwinkel- und Rueckwaertsschliessung ohne synthetisches Ersatz-Lookup (5.1). |
| A-S-00012 | Volle 3D-Lasten fuer die signierte Neigungsachse -90..+90 alle 10 Grad, beide identische Pole und stetiger Anschluss (4.3/12). |
| A-S-00013 | Jede Einzelkraft mit realem Modellansatzpunkt, freien Eigenmomenten und exakter Momentensumme (8.2). |
| A-S-00014 | Zentrale Wirkungslinie, Restmoment, ungueltiger Ansatz bei kleiner Kraft und Momentenidentitaet (8.2). |
| A-S-00015 | Normierung q*S und q*S*c_ref sowie Hals-/Achsenbezuege dokumentiert (8.2). |
| A-S-00016 | Globales CPU-Limit aus Auftrag/CLI/Prozess, ohne vervielfachte Providerpools (6.1). |
| A-S-00017 | Fachlich geeignete GPU-Batches nutzen unterstuetzte Hardware, Modi/Fehler/numerischer Vergleich ohne Speedbenchmark (6.2). |
| A-S-00018 | Hoehenraster 1/2/4, feinste zwei vergleichen, Grenzen erhalten; Fehler sperrt Export (7). |
| A-S-00019 | Direkte Azimut-/Neigungs-/Zellenzwischenpunkte, Naht und Polstetigkeit geprueft (7). |
| A-S-00020 | Eindeutige lokale Profilpolaren samt Profil-/Streifen-/Re-Zuordnung (8.1/8.3). |
| A-S-00021 | Historisch ersetzt durch A-S-00031 bis A-S-00036; CSV-Ausgabeanforderung ist nicht mehr gueltig. |
| A-S-00022 | Selbsttragende Metadaten/Snapshots, Parameter, Grenzen, Versionen und SHA256 (8.1/8.5/8.6). |
| A-S-00023 | Exklusives Arbeitsverzeichnis, Schutz vorhandener Ergebnisse, atomare Veroeffentlichung, Fehlerdiagnose (8.7). |
| A-S-00024 | Reproduzierbare Ergebnisse unabhaengig von Workeranzahl (9). |
| A-S-00025 | Mehrere identifizierbare Segelflaechen/Transformationen intern darstellbar, keine gekoppelte Interferenz implementiert (5.3). |
| A-S-00026 | C++/CMake-CLI und eigenstaendig baubarer/testbarer Core ohne GUI (9). |
| A-S-00027 | Isolierte 420-Tests; Original/Sicherung weder geaendert noch bereinigt, Hashschutz (10). |
| A-S-00028 | Numerik und physikalische Validierung getrennt, Quellen/Annahmen/fehlende Referenzen ehrlich sichtbar (2/5/8). |
| A-S-00029 | Abbruch beendet nur eigene Worker, kein erfolgreiches Teilergebnis (3.3). |
| A-S-00030 | Fehler-/Ressourcen-/Schreib-/Konkurrenzfaelle mit passenden Codes und Nachweisen (10). |
| A-S-00031 | Genau sechs Ergebnisdateien; drei Tabellen als Parquet mit Zstandard, zwei Berichte und ein Manifest als JSON; keinerlei CSV-Erzeugung oder CSV-Export. |
| A-S-00032 | Summen-, Einzelkraft- und Profiltabellen verwenden typisierte, dokumentierte Spalten und erhalten alle bisherigen Werte und physikalischen Beitraege verlustfrei als binary64. |
| A-S-00033 | Manifest beschreibt Parquet-/Schema-/Codecversionen, Dimensionen, Einheiten, Winkel-/Momentenbezug und SHA-256 aller fuenf uebrigen Ergebnisdateien; unbekannte oder beschaedigte Formate werden sichtbar abgelehnt. |
| A-S-00034 | Daten sind deterministisch nach Schluesseln sortiert und in unabhaengig lesbaren Row Groups angeordnet; gezielte Tabellenzugriffe benoetigen keine vollstaendige Tabellendekodierung. |
| A-S-00035 | Ein Parquet-Roundtrip erhaelt alle Schluessel und endlichen binary64-Werte bitgleich; Summen, Einzelkraefte und Momente stimmen mit dem fachlichen Tabellenvertrag ueberein. |
| A-S-00036 | Gleiche Eingaben und festgelegte Bibliotheks-/Codecversion ergeben dieselben Tabellenbytes unabhaengig von der Workeranzahl; Fehler oder Abbruch veroeffentlichen kein Teilergebnis. |
| A-S-00037 | `lookup --fingerprint update\|skip` ruft ausschliesslich die bestehende Core-API fuer den Bereich aerodynamics auf; update gilt auch ohne Neuschreiben, skip ist Standard. Andere Bereiche bleiben unveraendert. |
| A-S-00038 | `lookup --output-policy overwrite\|create-only` ersetzt einen Lookup nach erfolgreicher Neuberechnung ohne dauerhafte Sicherung oder ueberspringt bestehende Ergebnisse; create-only ist Standard. Fehler vor dem Austausch erhalten den bisherigen Lookup. |
| A-S-00039 | `Context::solve` verarbeitet jeden endlichen signierten 3D-Windvektor fuer beta in [-90,+90] direkt; keine Lastspiegelung, beidseitige Crossflow-Ueberblendung und azimutinvariante exakte Pole (12.2). |
| A-S-00040 | Auftraege muessen die vollstaendige signierte 19-Werte-Achse enthalten; Ausgabeachse, Ressourcenlimit, Zwischenpunktpruefung und beide Pole werden fuer beide Vorzeichen validiert (4.3/12.3/12.5). |
| A-S-00041 | Lookup-Manifestversion 3 beschreibt die signierte Domaene und den bestaetigten Core-Vertrag; aeltere Lookup-Formatversionen werden nicht als kompatible Eingabe unterstuetzt (11.4/12.3/12.4). |
| A-S-00042 | Tests pruefen signed Winkel, Transformation, Numerik, Parquet/Manifest und Momentenvertrag; fehlende physikalische Validierung bleibt sichtbar, aber ist keine Nutzungsbedingung (12.5). |

Nachweise und offene Abnahmen stehen ausschliesslich im fortgefuehrten
[Umsetzungsplan](../Umsetzungsplaene/Entwurf_Lookup_Generator.md).

## 1. Zweck und Geltungsbereich

Das Werkzeug erzeugt aus einer Segelbeschreibung und einer Auftragsdatei
reproduzierbare aerodynamische Lookup-Daten. Es besitzt keine GUI.
Der Aufruf erfolgt auf der Kommandozeile oder als Unterprozess eines anderen
Programms. Eine gemeinsame Berechnungsbibliothek soll CLI und spaetere direkte
Programmanbindung bedienen; der konkrete H1-Vertrag steht im
[C++-Register](../Dokumentationen/Schnittstellenregister_CPP.md).

Der Nutzer hat folgende Richtung vorgegeben:

- Geometrie und Auftrag liegen gemeinsam im segelspezifischen Modellordner.
- Das Werkzeug erhaelt die Auftragsdatei und ermittelt daraus alle Eingaben.
- Ergebnisse liegen ausschliesslich im Unterordner `LookUp` dieses Ordners.
- OBJ-Dateien dienen der Visualisierung und liegen unter `meshes\gui`.
- Keine Bootsdynamik, Rumpfberechnung oder Gesamtmodellmontage im Generator.
- Erstumfang: unabhaengige Einzel-Lookups. Die Wechselwirkung zwischen
  Grosssegel und Fock wird als spaeterer Ausbauschritt vorgesehen, nicht
  bereits in diesen Ergebnissen behauptet. Der Rechenkern wird darauf
  vorbereitet (Abschnitt 5.3).
- Geneigte Anstroemung wird mit Windneigungen von 0 bis einschliesslich
  90 Grad in 10-Grad-Schritten in den physikalischen Zielumfang aufgenommen.
- Die maximale Prozessorzahl ist begrenzbar.
- GPU-Beschleunigung wird fuer fachlich als geeignet eingeschaetzte
  Rechenfunktionen verwendet, wenn geeignete Hardware vorhanden ist.
  Es werden vorerst keine Laufzeitvergleiche zur Hardwareauswahl ausgefuehrt.

Die nachfolgenden Vertragsdetails wurden mit dem Umsetzungsauftrag freigegeben
und oben finalen Anforderungen zugeordnet. Herkunftsentscheidungen stehen in der
[Entwurfspunkteliste](Entwurfspunkte/Lookup_Generator.md).

## 2. Ausgangsdaten und Abgrenzung

Die Geometriebeschreibungen liegen fuer International 420 und Nordic Folkboat
jeweils als Grosssegel und Fock sowie fuer den Optimisten als Grosssegel vor.
Es handelt sich um rekonstruierte Grundmodelle, nicht um vermessene reale Segel.
Woelbung und Verwindung sind vorlaeufige Modellannahmen.

Referenzen:

- [Gemeinsame Konventionen](../Dokumentationen/aerodynamik_konventionen.json)
- [420-Herleitung](../Dokumentationen/420_segel_herleitung.json)
- [Folkeboot-/Optimist-Herleitung](../Dokumentationen/folkeboot_optimist_segel_herleitung.json)

Die Konventionen beschreiben Geometrie, Winkel und das physikalische
Hybridmodell als Ziel. Der Erstumfang soll physikalisch begruendete Lookups
liefern, keine synthetische Ersatzmodell-Datenkette. Konkrete H1-Verfahren, Schliessungen, Parameter und Gueltigkeitsbereiche
stehen in der technischen Modellfestlegung.
Bestehende Auftragsdateien enthalten die neuen Ressourcenfelder noch nicht.

**Ein durchgerechnetes Ingenieurmodell ist nicht automatisch physikalisch
validiert.** Modellannahmen und Nachweisstand bleiben sichtbar. XFOIL ist
eine moegliche Komponente, kein festgelegter 360-Grad-Solver; CFD ist keine
beauftragte Implementierungspflicht. Referenzdaten dienen der Modellpruefung.

## 3. Dateien und Aufruf

### 3.1 Ablage

```text
<Boot>\meshes\
  gui\
    main.obj
    fock.obj
  aerodynamik\
    main\
      main.json
      Auftragsdatei.json
      LookUp\
        lookup.json
        summen.parquet
        profile.parquet
        kraefte.parquet
        pruefbericht.json
        auftrag_ausgefuehrt.json
    fock\
      fock.json
      Auftragsdatei.json
      LookUp\
        ...
```

Beim Optimisten entfaellt die Fock. Die Dateinamen der Geometrie sind nicht
fest verdrahtet; massgeblich ist `eingabe.segeldatei`.

### 3.2 Kommandozeile

```powershell
.\SegelMacher.exe lookup --auftrag "C:\Modelle\420\meshes\aerodynamik\main\Auftragsdatei.json"
.\SegelMacher.exe pruefen --auftrag "C:\Modelle\420\meshes\aerodynamik\main\Auftragsdatei.json"
.\SegelMacher.exe lookup --auftrag "C:\Modelle\420\meshes\aerodynamik\main\Auftragsdatei.json" --max-prozessorzahl 4
```

- `lookup` prueft, berechnet, validiert und exportiert.
- `pruefen` validiert Eingaben, Pfade, Modellfaehigkeiten und grundsaetzliche
  Hardwareverfuegbarkeit ohne Solverlauf und ohne Ergebnisordner.
- `--max-prozessorzahl` ist eine optionale aufrufbezogene Obergrenze.
- `lookup --fingerprint update|skip` und
  `--output-policy overwrite|create-only` steuern unabhaengig den Core-Aufruf
  und die Ausgabe; Standards sind `skip` und `create-only`.
  Die Optionen stehen ausschliesslich an der CLI und sind bei `pruefen`
  unzulaessig. `update` wird auch beim Ueberspringen vorhandener Lookups
  ausgefuehrt. Bei einer gescheiterten Berechnung wird kein Hash erneuert.
- `--hilfe` und `--version` benoetigen keinen Auftrag.
- Pfade mit Leerzeichen und Unicode werden unter Windows unterstuetzt.
- Relative Eingabepfade beziehen sich auf den Ordner der Auftragsdatei,
  unabhaengig vom Arbeitsverzeichnis des aufrufenden Programms.
- Die gemeinsame Konventionsdatei wird ueber den Auftrag referenziert, nicht
  durch einen im Programm fest eingebauten Repositorypfad gefunden.
- Es gibt keinen erforderlichen Ausgabeordner-Parameter und keine Netzwerk-
  oder Cloudabhaengigkeit zur Berechnung. Provider muessen lokal installiert sein.

**Abnahme:** Derselbe Auftrag funktioniert aus zwei verschiedenen
Arbeitsverzeichnissen und erzeugt Ergebnisse am gleichen vorgeschriebenen Ort.

### 3.3 Maschinenlesbare Rueckmeldung

Standardausgabe enthaelt genau ein abschliessendes JSON-Objekt mit
`status`, `exitcode`, `ergebnisordner`, `pruefbericht` und `meldung`.
Nicht vorhandene Pfade werden als `null` ausgegeben.
Fortschritt, Warnungen und Diagnosen gehen ausschliesslich auf Standardfehler.
Eine erfolgreiche Berechnung meldet `status: erfolgreich`; der physikalische
Validierungsstand bleibt davon getrennt in Meldung und Ergebnisdaten sichtbar.
Ohne entsprechende Nachweise gilt `ingenieurmodell_nicht_validiert`.

| Exitcode | Bedeutung |
|---|---|
| 0 | Auftrag erfolgreich bzw. Eingabepruefung erfolgreich |
| 2 | Ungueltiger Aufruf, Auftrag, Schema oder Eingabepfad |
| 3 | Verfahren/Hardware nicht unterstuetzt oder Ressourcen unzureichend |
| 4 | Berechnung oder Qualitaetspruefung fehlgeschlagen |
| 5 | Ausgabe-/Core-Schreibfehler, gesperrte Ausgabe oder Konflikt |
| 130 | Benutzerabbruch |

Ein Abbruch beendet auch gestartete Worker und Providerprozesse dieses Auftrags.
Andere Programme oder fremde Prozesse werden nicht beendet.
Bei nicht beschreibbarer Ausgabe bleiben Exitcode und Standardfehler als
Diagnose verfuegbar. Auch unerwartete interne Fehler fuehren zu einem Fehler,
nicht zu einem scheinbar erfolgreichen Teilergebnis.

### 3.4 Terminal-Logging und Fortschrittsanzeige

Das Werkzeug gibt waehrend des gesamten Auftrags auf Standardfehler
verstaendliche deutsche Textmeldungen aus. Mindestens sichtbar sind:

- Start, geladener Auftrag und verwendetes Segel;
- Beginn und Abschluss jeder Phase: Eingabepruefung, Geometrieaufbereitung,
  Profilberechnung, Gesamtsegelintegration, Qualitaetspruefung und Export;
- konkret ausgefuehrte Arbeit mit Form, Rasterstufe und Betriebspunktgruppe,
  soweit fuer die jeweilige Phase relevant;
- effektives CPU-Limit, GPU-Auswahl und deren Begruendung ohne Speedpruefung;
- Warnungen, Fehler und Abbruchgrund mit betroffenem Eingabefeld oder Rechenfall;
- Abschluss mit Status, Gesamtdauer und Ergebnis- beziehungsweise Diagnosepfad.

Jede normale Meldung besitzt Zeitstempel, Schweregrad (`INFO`, `WARNUNG`,
`FEHLER`) und Phasenbezeichnung. Fehler und Warnungen werden nicht nur durch
Farben vermittelt. Die Anzeige benoetigt keine GUI.

Die Fortschrittsanzeige enthaelt gleichzeitig:

| Anzeige | Inhalt |
|---|---|
| Teilfortschritt | Aktuelle Phase, erledigte/geplante Arbeitseinheiten und Prozentwert, sofern die Anzahl bekannt ist |
| Gesamtfortschritt | Abgeschlossene/geplante Phasen sowie ein auf dem geplanten Arbeitsumfang basierender Prozentwert, sobald der Rechenplan feststeht |
| Laufende Taetigkeit | Kurze Textbeschreibung, beispielsweise Profilberechnung oder Rastervergleich |
| Laufzeit | Bisherige Gesamtzeit; eine Restzeitschaetzung ist nicht erforderlich |

Beispiel einer Anzeige; Zahlen sind keine Leistungs- oder Laufzeitvorgabe:

```text
19:55:03 INFO [Profilberechnung] Profile fuer Rasterstufe 2 berechnen.
Gesamt: 42 % | Phasen: 2/6 | Profilberechnung: 120/300 (40 %) | Laufzeit: 00:01:12
```

Vor Festlegung des Rechenplans wird fuer unbekannte Gesamtarbeit
`Gesamt: wird ermittelt` statt eines erfundenen Prozentwerts angezeigt.
Die Gewichtung der Gesamtarbeit erfolgt anhand geplanter Arbeitseinheiten,
nicht anhand behaupteter CPU-/GPU-Geschwindigkeiten. Der Prozentwert ist eine
Arbeitsfortschrittsanzeige, keine Zusage zum Anteil der verstrichenen Laufzeit.
Nicht angeforderte Produkte werden aus dem Plan entfernt.
Rasterverfeinerungen und Winkelzwischenpruefungen sind im Plan enthalten.

Der Gesamtfortschritt ist nach Festlegung des Plans monoton und erreicht
100 Prozent erst nach erfolgreicher Ergebnisveroeffentlichung.
Bei Fehler oder Abbruch bleibt der letzte Stand erhalten und wird mit dem
Fehlerstatus abgeschlossen; es gibt keine erfolgsaehnliche 100-Prozent-Meldung.
Bei einer Phase mit unbekannter Anzahl von Arbeitseinheiten wird eine
laufende Taetigkeit ohne Prozentwert angezeigt.

Eine zentrale Ausgabeinstanz sammelt Worker-Fortschritt; parallele Prozesse
schreiben keine konkurrierenden Fortschrittsbalken. Arbeit wird nur nach
Abschluss gezaehlt, bei GPU-Arbeit nach Bestaetigung ihrer Fertigstellung.
Einzelne Worker oder aufgerufene Solver duerfen keine fremden Ausgaben in die
maschinenlesbare Standardausgabe einschleusen.

Im interaktiven VT-Terminal werden zwei uebereinanderstehende Balken
am selben Ort aktualisiert: oben blau fuer abgeschlossene Aufgabenpakete
von sechs samt aktuellem Paketnamen, unten gruen fuer erledigte/geplante
Einheiten des laufenden Rechenabschnitts. Offene Balkenteile sind grau;
unbekannte Einheiten zeigen `0 / ?`. Nach erfolgreicher Publikation
zeigt der obere Balken 6 / 6. Die Darstellung gilt auch im Release-Build.
Der untere Balken enthaelt zusaetzlich verstrichene Dauer und geschaetzte
Gesamtdauer im Format `(00:03:53 / 01:34:16)`, gefolgt von
`Verbleibende Zeit: 01:30:23`. Gesamtdauer ist verstrichene Dauer
plus geschaetzte Restdauer; sie bleibt bei konstantem Durchsatz konstant,
kann aber bei veraendertem Durchsatz korrigiert werden.
Alle drei Zeiten verwenden immer `hh:mm:ss` mit fuehrenden Nullfeldern,
beispielsweise `00:00:12`. Felder sind mindestens zweistellig; Minuten
und Sekunden sind jeweils maximal 59.
Grundlage ist der
mittlere Durchsatz erledigter Einheiten des aktuellen Rechenabschnitts.
Bei Abschnittswechsel beginnt die Schaetzung neu; ohne erledigte Einheiten
steht `wird ermittelt`. Unterschiedliche Kosten einzelner Einheiten koennen
die Schaetzung veraendern; sie ist keine Laufzeitzusage.
Textmeldungen bleiben lesbar erhalten; Warnungen und Fehler unterbrechen
die Balken sauber. Bei Umleitung in eine Datei oder Aufruf aus einem
anderen Programm werden normale zeilenweise Meldungen ohne ANSI-Steuerzeichen
oder Wagenruecklauf-Ueberschreibungen geschrieben. Fortschritt wird dabei
hoechstens einmal pro Sekunde periodisch ausgegeben, zusaetzlich bei
Phasenwechsel und Abschluss. Bei langen Rechenschritten wird mindestens alle
fuenf Sekunden der unveraenderte Fortschrittsstand mit laufender Taetigkeit
gemeldet, ohne daraus zusaetzlichen Fortschritt abzuleiten.

**Abnahme:** Am isolierten 420-Main-Testauftrag werden Textmeldungen,
Teil-/Gesamtfortschritt, Phasenwechsel und Abschluss nachgewiesen.
Tests umfassen interaktiven und umgeleiteten Ausgabemodus, parallele Worker,
unbekannten Arbeitsumfang, lange Provideraufrufe, Fehler und Benutzerabbruch.
Standardausgabe bleibt ein einzelnes parsebares Abschluss-JSON;
Fehlerlaeufe melden keinen erfolgreichen Gesamtabschluss.

## 4. Eingabevertrag

### 4.1 Geometrie und Konventionen

Die Segeldatei enthaelt Identifikation, Randpunkte/-verknuepfungen und
Profilstationen. Feste Regeln sind in der gemeinsamen Konventionsdatei
definiert und ueber `schema_version` zugeordnet.

Zu pruefen sind mindestens:

- unterstuetzte und zueinander passende Versionen;
- keine doppelten JSON-Schluessel oder unbekannten Berechnungsoptionen;
- endliche Zahlen, eindeutige IDs und gueltige Verweise;
- geschlossener, nicht selbstschneidender Rand mit positiver Flaeche/Hoehe;
- Hals im Ursprung; Ausgangskontur in der lokalen XZ-Ebene;
- horizontaler Schnitt jeweils ein zusammenhaengendes Intervall;
- aufsteigende Profilstationen mit vollstaendiger Abdeckung von eta 0 bis 1;
- gueltige Woelbung, Woelbungsposition und Verwindung;
- explizite Behandlung degenerierter Endpunkte mit Sehnenlaenge null.

Beim Optimisten muessen oberhalb der Kehle Schnittenden auf der Kopfkante
moeglich sein; bei einer Fock unterhalb des Schothorns auf dem Unterliek.
Keine Dreiecksannahme fuer alle Segel und kein Erzwingen einer nominalen
Segelflaeche. Gerade Baum-Unterlieke bleiben bei den vorliegenden
Grosssegelmodellen ungewoelbt und unverdreht.

**Abnahme:** Das 420-Grosssegel dient als Referenz fuer die notwendigen
Ende-zu-Ende-Tests. Absichtlich beschaedigte Konturen und Parameter werden
an Testkopien mit Dateipfad und Feldbezug abgelehnt. Abweichende Randformen
werden durch kleine synthetische Testgeometrien geprueft; vollstaendige
Berechnungslaeufe fuer alle fuenf Segel sind fuer diese Abnahme nicht erforderlich.

### 4.2 Auftragsinhalt

| Abschnitt | Inhalt |
|---|---|
| `eingabe` | Segeldatei und Konventionsdatei |
| `berechnung` | Produkte, Provider/Modellversion, Parameter, Behandlung abgeloester Stroemung und 3D-Effekte |
| `betriebspunkte` | Winkelraster, Referenz-Reynoldszahlen, Windneigung |
| `formvarianten` | Unveraenderte Eingabeform; spaeter explizit definierte Varianten |
| `numerik` | Integrationsverfahren und Hoehenraster |
| `qualitaetspruefung` | Rasterverfeinerung, Winkelzwischenpunkte und Toleranzen |
| `ressourcen` | Maximale Prozessorzahl und GPU-Auswahl |
| `ausgabe` | Schutz vorhandener Ergebnisse, keine freien Dateinamen/Ergebnisformate |

Der bestehende Status `entwurf_rechenparameter_nicht_freigegeben` kennzeichnet
einen Vorbereitungsauftrag. `pruefen` darf ihn lesen; `lookup` lehnt ihn ab.
Fuer einen ausfuehrbaren Auftrag wird `status: bereit` vorgeschlagen.
Ein solcher Status ist keine wissenschaftliche Validierung des Providers.
Im Beispielauftrag bezeichnet `berechnung.modell: physikalisches_hybridmodell`
die gewaehlte Richtung. Solange konkrete Verfahren und Parameter fehlen,
bleiben `modellstand: null`, `modellfestlegung.status: offen_nicht_ausfuehrbar`
und die offenen Punkte explizit erhalten. `noch_festzulegen_...` ist ein
Entwurfsmarker, kein ausfuehrbarer Verfahrensname. Vor `status: bereit` sind
diese Marker durch spezifizierte Verfahren, deren Modellversion und Parameter
zu ersetzen; `pruefen` muss die fehlende Ausfuehrbarkeit ausdruecklich melden.

Feste Bezugs- und Fehlerregeln werden nicht pro Auftrag umdefiniert.
Unbekannte Verfahren, Modellversionen und ununterstuetzte Kombinationen werden
vor Beginn der teuren Berechnung explizit abgelehnt.

### 4.3 Raeumliche Anstroemung

Das Windneigungsraster enthaelt genau
`[-90, -80, ..., 0, ..., 80, 90]` Grad als streng aufsteigendes Array
`betriebspunkte.windneigung_deg`. Fuer jeden Azimut, jede Referenz-Re-Stufe
und jede Form werden alle 19 Neigungen direkt berechnet. Die alte Achse
`[0, 10, ..., 90]` wird nicht still erweitert, sondern als veralteter Auftrag
mit Umstellungshinweis abgelehnt.

Die Windneigung beta bezeichnet die Elevation der Windherkunft gegen die
lokale XY-Ebene des Segels, nicht den Kraengungswinkel des Bootes.
Mit Azimut alpha ist der Windherkunfts-Einheitsvektor
`(cos(beta)*cos(alpha), cos(beta)*sin(alpha), sin(beta))`;
trigonometrische Funktionen verwenden Radiant. Die Stroemung zeigt entgegen
diesem Vektor. Bei beta = -90 Grad wird exakt `(0,0,-1)`, bei beta = +90 Grad exakt
`(0,0,1)` als Windherkunft verwendet.
Referenz-Re und Staudruck beziehen sich auf die gesamte relative
Windgeschwindigkeit; ein Provider muss seine lokalen Projektionen und
Geschwindigkeitsbezuege explizit dokumentieren.

SegelPhysik-Core transformiert spaeter den relativen Wind einschliesslich
Bootsbewegung und Einbaulage ins lokale Segelsystem und bestimmt daraus beide
Winkel. Die resultierenden Kraefte und Momente bleiben vollstaendige
dreidimensionale Vektoren. Ein blosses Skalieren horizontaler Kraefte mit
dem Kosinus der Neigung ersetzt kein geeignetes raeumliches Rechenmodell.
Insbesondere spannweitige Anstroemung und der Polfall benoetigen eine
ausdruecklich definierte Modellbehandlung ohne Division durch verschwindende
projizierte Geschwindigkeit.

Bei 90 Grad beschreiben alle Azimute denselben Windvektor. Das rechteckige
Ausgaberaster behaelt diese Zeilen, verlangt aber identische lokale Kraft-
und Momentenvektoren fuer gleiche Form/Re. Die Berechnung darf diese Faelle
wiederverwenden. Zwischen den Neigungen wird nichtzyklisch interpoliert;
am Pol ist keine azimutabhaengige Last zulaessig.

Negative Elevationen sind durch diesen Nutzerauftrag nicht abgedeckt.
Sie koennen bei realer Krangung/Bewegung ebenfalls auftreten; ein Konsument
darf sie weder still begrenzen noch ohne belegte Symmetrie spiegeln.
Das Raster ist daher noch keine Abdeckung aller raeumlichen Windrichtungen.

## 5. Berechnungskette und physikalische Grenzen

1. Auftrag und Eingabedateien laden und validieren.
2. Eingaben/Pruefsummen sichern, Rechenfaelle und benoetigte lokale Re bestimmen.
3. Kontur schneiden und lokale Profilparameter interpolieren.
4. Profilpolaren durch den gewaehlten Provider berechnen.
5. Bei angefordertem Gesamtsegel-Lookup Streifenkraefte und Momente integrieren.
6. Konvergenz und Winkelinterpolation pruefen.
7. Gepruefte Ergebnisse samt Herkunft und Grenzen veroeffentlichen.

### 5.1 Physikalisches Hybridmodell im Erstumfang

Die Berechnung verwendet die vorgegebene feste Segelform. Woelbung,
Woelbungsposition und Twist muessen sich in der Modellrechnung auswirken.
Die einzelnen Bausteine sind:

1. Woelbungsabhaengige Profilrechnung fuer anliegende Stroemung mit Auftrieb
   und Profilmoment; konkrete Duennprofil-, Wirbel- oder Panelmethode offen.
2. Dokumentiertes viskoses beziehungsweise empirisches Zusatzmodell fuer
   Profilwiderstand, Reynolds-Einfluss und Abriss.
3. Gekoppelte Traglinien- oder geeignete raeumliche Wirbelrechnung fuer
   Abwind, Lastverteilung und induzierten Widerstand im gueltigen Bereich.
   Induzierter Widerstand darf nicht nochmals pauschal addiert werden,
   wenn er bereits in der Kraftberechnung enthalten ist.
4. Begruendetes Hochwinkel-/Abloesemodell fuer grosse und rueckwaertige
   Anstroemung mit stetigen Uebergaengen zwischen den Modellbereichen.
5. Explizite raeumliche Behandlung der Windneigungen aus Abschnitt 4.3,
   einschliesslich spannweitiger Anstroemung und Polfall. Eine ungepruefte
   Fortsetzung der Traglinientheorie auf abgeloeste Stroemung reicht nicht.
6. Raeumlich positionierte Lastbeitraege und Eigenmomente, Integration um
   den Hals und Normierung gemaess Ergebnisvertrag. Die Viertelsehne ist
   ein Profilmomentbezug, kein vorgeschriebener physikalischer Druckpunkt.

Konkrete Formeln, Modellgrenzen, Parameterherkunft und Referenzpruefungen
sind fuer H1 in der technischen Modellfestlegung dokumentiert.
Geometrie allein bestimmt weder Abriss noch viskosen Widerstand eindeutig;
offene Schliessungen erhalten keine frei erfundenen Standardparameter.
Die Mittelpunktregel bleibt der vorgeschlagene Integrationsstart, ihre
Eignung und die Solverkonvergenz sind getrennt nachzuweisen.

Automatische Tuchverformung, Flattern, dynamischer Abriss und zeitaufgeloeste
Wirbelstroemung sind nicht enthalten. Ebenso fehlen Mast-/Rumpfeinfluss und
Segelinterferenz bei den getrennten Einzelsegelauftraegen.

Die bisher besprochene Faehigkeitseinordnung gilt fuer diesen Erstumfang:

| Faehigkeit | Umfang und Grenze |
|---|---|
| Anliegende Stroemung, Woelbung, Twist, endliche Hoehe | Physikalisch begruendet berechnen; Gueltigkeitsbereich und Referenzpruefungen dokumentieren |
| Raeumliche Einzelkraefte, Eigenmomente und Resultierende | Verbindlicher Ergebnisumfang, einschliesslich Wirkungslinie und verbleibendem Restmoment |
| Reynolds-Einfluss, Abriss, Hochwinkel und rueckwaertige Anstroemung | Mit dokumentierten Naeherungen und sichtbaren Unsicherheiten berechnen |
| Geneigte Anstroemung | 0 bis 90 Grad in 10-Grad-Schritten; raeumliche Modellierung und Polbehandlung erforderlich |
| Boeen, Wende, Halse und wechselnde Schotwinkel | Lookup-Grundlage fuer spaetere Core-Nutzung; nur quasistationaere Lasten einer vorgegebenen festen Form, keine Simulation dieser Bewegungsablaeufe im Generator |
| Druckpunkt/SDP | Kraft und Moment sowie aequivalente Wirkungslinie liefern; kein stets eindeutiger Punkt auf dem Segel zugesichert |
| Grosssegel-/Fock-Wechselwirkung | Spaeterer gekoppelter Konfigurationsmodus gemaess Abschnitt 5.3 |
| Tuchverformung, Flattern, dynamischer Abriss, zeitaufgeloeste Wirbel, Struktur-/Beschlaglasten | Nicht im Erstumfang |
| Mast, Rumpf, Deck sowie Bootsdynamik | Nicht im Erstumfang; Bootsdynamik bleibt Aufgabe des Core |

### 5.2 Providerfaehigkeiten und Gueltigkeit

Ein Provider muss seine unterstuetzten Profilgeometrien, Re-/Winkelbereiche,
Konvergenzmerkmale und 3D-Kopplung deklarieren. Lokale Reynolds-Zahlen werden
aus Referenz-Re und dem Verhaeltnis lokaler Sehne zu Referenzlaenge abgeleitet.
Der benoetigte lokale Bereich kann deutlich unterhalb des Referenzrasters liegen.

Keine stillschweigende Begrenzung auf den naechsten gueltigen Re-Wert.
Extrapolation oder ein Modellwechsel erfordern eine definierte Methode und
Qualitaetskennzeichnung. Ein nicht abgedeckter Auftrag wird abgelehnt.
XFOIL allein ist kein belastbares Verfahren fuer den vollstaendigen Winkelkreis;
ein physikalischer 360-Grad-Auftrag benoetigt eine gesondert spezifizierte
Behandlung abgeloester und rueckwaertiger Anstroemung.

3D-Effekte muessen entweder modelliert oder sichtbar als vernachlaessigt
ausgewiesen werden. Abwind und induzierter Widerstand gehoeren zum Zielumfang
und duerfen nicht als vernachlaessigt durchgehen. Ein fehlender Modellbaustein
fuehrt zu einer expliziten Ablehnung, nicht zu einem synthetischen Ersatzmodell.

### 5.3 Erweiterbarkeit fuer Grosssegel-/Fock-Wechselwirkung

**Jetzt:** Einzel-Lookups fertigstellen. Die 3D-Kopplung innerhalb eines
Segels ist Bestandteil des Erstumfangs; die Kopplung zwischen verschiedenen
Segeln noch nicht. Das Addieren unabhaengiger Einzel-Lookups berechnet
keine Segelinterferenz und darf nicht als solche bezeichnet werden.

Der interne Geometrie- und Rechenkern wird so aufgebaut, dass mehrere
identifizierbare Segelflaechen in einem gemeinsamen Rechenfall darstellbar
sind. Segelidentitaet, Geometrie, starre Transformation und Lastbeitraege
bleiben getrennt. Der aktuelle Einzelauftrag verwendet eine Segelflaeche
mit identischer Transformation; sein Schema und seine Ergebnisdateien
werden nicht vorsorglich um unbenutzte Mehrsegel-Pflichtfelder erweitert.
Eine konkrete oeffentliche Mehrsegel-API wird damit noch nicht festgelegt.

**Spaeter:** Ein eigener Konfigurationsauftrag referenziert die vorhandenen
Segelgeometrien. Er beschreibt Position und Orientierung jedes Segels in
einem gemeinsamen System sowie Raster fuer die zu untersuchenden
Segelstellungen. Einbaupositionen gehoeren weder jetzt noch spaeter in die
intrinsische Segelgeometrie. Das genaue Auftragsschema, die Ablage des
Konfigurations-Lookups und dessen Ergebnisformat werden vor diesem
Ausbauschritt versioniert spezifiziert.

Die gekoppelte Rechnung muss die gegenseitig veraenderte Anstroemung
beruecksichtigen. Neben Windazimut/-neigung und Reynolds-Zahl werden die
Kombinationen der Segelstellungen relevant; eine reine gemeinsame Drehung
der Gesamtanordnung ist davon zu unterscheiden. Relative Positionen koennen
je Konfiguration fest sein; sie muessen nicht pauschal weitere Rasterachsen
bilden. Die Rasterkombinationen koennen Datenmenge und Rechenaufwand stark
erhoehen.

Ergebnisse bleiben je Segel zuordenbare, raeumlich positionierte Kraefte
und Eigenmomente mit eindeutigem Koordinaten-/Momentenbezug. Fuer eine
Gesamtsumme werden alle Beitraege in dasselbe System und auf denselben
Momentenursprung transformiert. Ein pauschaler nachtraeglicher
Korrekturfaktor auf Einzel-Lookups ersetzt diese Rechnung nicht.

Vorhandene Geometrien und Einzel-Lookups bleiben als Einzelmodell-Ergebnisse
nutzbar; gekoppelte Ergebnisse muessen neu berechnet werden. Es wird keine
einfache oder bereits validierte Interferenzmodellierung zugesichert.
Ihre physikalischen Verfahren und Referenzpruefungen gehoeren in den
spaeteren Ausbauschritt, nicht in die jetzige Abnahme.

## 6. CPU- und GPU-Ressourcen

### 6.1 Maximale Prozessorzahl

Vorgeschlagene Erweiterung des Auftrags:

```json
"ressourcen": {
  "max_prozessorzahl": 4,
  "gpu": "automatisch"
}
```

`max_prozessorzahl` ist eine positive ganze Zahl und bezeichnet die maximale
Anzahl gleichzeitig rechnender logischer CPU-Ausfuehrungsplaetze.
Der Beispielwert 4 ist kein vorgeschriebener Standard fuer alle Rechner.

Effektive Obergrenze ist das Minimum aus Auftragswert, optionalem CLI-Limit
und fuer den Prozess verfuegbaren logischen Prozessoren. Im neuen Auftragsschema
ist der Auftragswert erforderlich; fehlende Werte werden nicht still ergaenzt.

Das Limit gilt **gemeinsam** fuer Worker, native Bibliotheksthreads,
Provider-Unterprozesse und CPU-seitige GPU-Zuarbeit. Verschachtelte Threadpools
duerfen das Budget nicht multiplizieren. Verwaltungs-/I/O-Threads duerfen
existieren, aber keine unbudgetierten Rechenarbeiten ausfuehren.
Bei Limit 1 gibt es maximal einen gleichzeitig rechnenden CPU-Worker.

**Abnahme:** Tests mit Limits 1, 2 und 4 weisen die Obergrenze einschliesslich
eines mehrthreadigen Testproviders nach. Angeforderte und effektive Werte
werden im Bericht gespeichert. Kein Eingriff in globale Systemeinstellungen.

### 6.2 GPU-Auswahl

Erlaubte Werte: `automatisch`, `aus`, `erforderlich`.

- `aus`: Ausschliesslich CPU, keine GPU-Benchmarks.
- `automatisch`: Ein geeigneter, lokal verfuegbarer GPU-Backend wird fuer
  als GPU-geeignet eingestufte Rechenfunktionen verwendet, sofern
  Funktionsunterstuetzung, numerische Praezision und Speicherbedarf passen.
- `erforderlich`: Kein geeigneter GPU-Backend oder kein ausreichender
  Geraetespeicher fuehrt zum Fehler statt zu einem unbemerkten CPU-Lauf.

Der CPU-Pfad bleibt eigenstaendig funktionsfaehig. CPU-only-Installation
benoetigt weder GPU noch GPU-Treiber. Nicht jeder Provider muss GPU unterstuetzen.

Die Auswahl basiert auf einer bei der Implementierung begruendeten
Eignungseinstufung je Rechenfunktion, nicht auf einem Laufzeitbenchmark.
Beruecksichtigt werden Parallelisierbarkeit, vorhandene GPU-Unterstuetzung,
Umfang der Daten und notwendige Datentransfers. Es werden keine
CPU-/GPU-Speedpruefungen, Kalibrierungslaeufe oder gemessenen
Beschleunigungsschwellen zur Auswahl verlangt oder ausgefuehrt.

| Rechenfunktion | Vorlaeufige Einschaetzung und Zuordnung |
|---|---|
| JSON-/Parquet-Verarbeitung, Pfade, Pruefsummen, Berichte | CPU; ueberwiegend I/O und geringe Datenparallelitaet |
| Konturvalidierung und Aufbau des Streifenrasters | CPU; bei den vorhandenen kleinen Konturen kein plausibler GPU-Vorteil |
| Einzelne Profil-/Interpolationsabfragen | CPU; Transfer- und Aufrufaufwand voraussichtlich groesser als die Rechenarbeit |
| Gebuendelte Polarberechnung ueber viele Profile, Winkel und Re-Faelle | GPU-geeignet; bei vorhandenem passendem Backend auf GPU ausfuehren |
| Gebuendelte Streifenkraefte und Momentenbildung ueber Betriebspunkte | GPU-geeignet; Daten moeglichst auf dem Geraet belassen |
| Summation weniger Streifen eines einzelnen Betriebspunktes | CPU; keine gesonderten GPU-Transfers fuer kleine Einzelreduktionen |
| Gebuendelte Raster-/Winkelpruefungen | GPU-geeignet fuer die numerischen Batches; Zusammenfassung und Bericht auf CPU |
| Externe Solver wie ein lokales XFOIL-Programm | CPU, sofern der Provider keine eigene GPU-Unterstuetzung besitzt; kein automatischer GPU-Umbau |

Der Scheduler buendelt GPU-geeignete Arbeiten, statt jeden Skalar separat
zu uebertragen. Die Einstufung wird je implementierter Funktion/Provider
dokumentiert. Ein vorhandenes GPU-Geraet allein macht CPU-Funktionen nicht
GPU-faehig. Es wird kein universeller Laufzeitgewinn zugesichert.

GPU-Implementierungen werden in numerischen Tests gegen den CPU-Referenzpfad
mit denselben absoluten/relativen Auftragstoleranzen geprueft.
Das ist eine Korrektheitspruefung, keine Speedpruefung.
Physikalische Qualitaet darf durch GPU-Nutzung nicht wechseln.
Backend, Geraet, Praezision, Eignungseinstufung je ausgefuehrter Funktion,
Speicherabschaetzung und Auswahlgrund werden protokolliert.
Normale Auftragslaufzeiten duerfen im Bericht stehen, steuern aber nicht die Auswahl.

Im Automatikmodus ist fehlende GPU-Unterstuetzung eine dokumentierte CPU-Auswahl.
Ein Fehler eines ausgewaehlten GPU-Backends oder fehlerhafte numerische
GPU-Ergebnisse fuehren dagegen zum Auftragsfehler, nicht zum stillen Weiterrechnen.

**Abnahme:** Tests fuer keine GPU, fehlende Funktionsunterstuetzung,
unzureichenden Speicher, GPU-geeignete Batches, abweichende Ergebnisse und
Ausfuehrungsfehler; Auswahltests duerfen simulierte Backends verwenden.
Es ist nachzuweisen, dass keine Speedpruefung zur Auswahl erfolgt.
Eine tatsaechliche Beschleunigung wird ohne Hardwaremessung nicht als
nachgewiesen bezeichnet.

## 7. Numerische Qualitaet

Aktuell freigegebene Starttoleranzen (Nutzerentscheidung 05.11.2026):
absolute Beiwerttoleranz 0.0005 und relative Toleranz 0.01.
Die Akzeptanzregel bleibt punktweise fuer Kraft- und Momentenvektor getrennt.
RMS-/Spitzenlast-Prozentkennzahlen sind zusaetzliche Berichtsangaben und
ersetzen diese Regel nicht. Effektive Toleranzen stehen im Pruefbericht.

Der erweiterte Beispielauftrag beschreibt 360 Azimute (-180 bis +179 Grad),
zehn Windneigungen, sechs Referenz-Re-Stufen und eine unveraenderte Form,
also 21600 Summenzeilen. Davon sind 19446 Windrichtungs-/Re-Faelle
physikalisch verschieden: `(360*9 + 1)*6`, da der Pol azimutunabhaengig ist.
Gegenueber dem horizontalen Raster verzehnfacht sich die Tabellenzahl;
Rechenzeit und Speicherbedarf wachsen nicht zwingend exakt im selben Faktor.
Dies ist ein Startraster, kein universeller Genauigkeitsstandard.

- Winkelraster ist zyklisch; kein doppelter Endpunkt bei -180/+180 Grad.
- Hoehenraster wird an Konturknicken, Profilstationen und Verfeinerungsgrenzen
  geteilt. Bei jedem Verfeinerungsfaktor bleiben diese Grenzen erhalten.
- Faktoren 1/2/4 teilen beide Hoehenschrittweiten; verglichen werden die beiden
  feinsten Raster an jedem Betriebspunkt.
- Fuer Kraft- und Momentenbeiwertvektor getrennt:
  `Norm(Differenz) <= absolute_Toleranz + relative_Toleranz*Norm(feines_Ergebnis)`.
- Winkelzwischenpunkte werden direkt berechnet und gegen die zyklische
  lineare Interpolation der Nachbarn verglichen, einschliesslich der Naht.
- Neigungszwischenpunkte (5, 15, ..., 85 Grad) werden gegen nichtzyklische
  lineare Interpolation geprueft; zusaetzlich werden Mittelpunkte der
  zweidimensionalen Winkelzellen gegen bilineare Interpolation geprueft.
  Fuer alle Vergleiche gelten die Kraft-/Momentenvektortoleranzen.
- Polzeilen muessen identisch sein; Grenzwerte gegen 90 Grad muessen stetig
  anschliessen. Nicht endliche Ergebnisse oder unbegruendetes Nullsetzen
  bei spannweitiger Anstroemung sind Fehler.
- Exportiert wird das feinste erfolgreich gepruefte Raster.
- Toleranzueberschreitung fuehrt zu einem fehlgeschlagenen Auftrag.
- Profilprovider benoetigen eigene Konvergenzpruefungen; geometrische oder
  quadraturbezogene Konvergenz ersetzt diese nicht.

Ein Prozent numerische Konvergenz ist keine Zusage eines Prozents physikalischer
Genauigkeit. Zusaetzliche Plausibilitaetspruefungen: endliche Beiwerte,
nichtnegativer Widerstand, passende Nullsehnenbehandlung, zyklische Stetigkeit,
korrekte Momentenbezugspunkte und erwartete Symmetrien des gewaehlten Modells.

## 8. Ergebnisvertrag

### 8.1 `lookup.json`

Enthaelt mindestens:

- Ergebnisformatversion, Erfolgsstatus, Segel-ID und Geometrieschemaversion;
- Provider/Modellversion, Parameter, Modellgrenzen und Qualitaetsklasse;
- Achsen, Winkeldefinition, Referenzflaeche, Referenzlaenge und Halsbezug;
- Formvarianten mit vollstaendigen Parametern, Raster und Tabellenabmessungen;
- Dateinamen, exakte Spalten, Einheiten, numerische Praezision;
- Profil-IDs mit Schnittgeometrie, Woelbung, Position und Momentenbezug;
- Zuordnung von Profilen/Streifen zur Form und zum jeweiligen Referenz-Re-Fall;
- SHA-256-Pruefsummen der Eingaben und Ergebnistabellen;
- Ausfuehrungsinformationen und Verweis auf den Pruefbericht.

Der Konsument darf weder das urspruengliche Repository noch dessen
Dokumentationspfade benoetigen. Effektive Konventionen und Eingabegeometrie
werden deshalb im ausgefuehrten Auftrag mitgesichert.

### 8.2 `summen.parquet`

Spalten in der folgenden logischen Reihenfolge:
`form_id` (UTF-8), `alpha_deg`, `windneigung_deg`, `re_ref`, `CFx`, `CFy`,
`CFz`, `CMx`, `CMy`, `CMz`, `angriff_x_m`, `angriff_y_m`, `angriff_z_m`,
`angriff_gueltig` (Boolean), `CM_rest_x`, `CM_rest_y`, `CM_rest_z` und
`qualitaet` (UTF-8). Reelle Zahlen sind IEEE-754 binary64.

Eine eindeutige Zeile je Form, Azimut, Windneigung und Referenz-Re. Komponenten liegen im
lokalen Segelsystem vor, Momente um den Hals.
Mit `q = 0.5*rho*V^2`, Referenzflaeche `S` und Referenzlaenge `c_ref` gilt:
`F = q*S*CF`, `M = q*S*c_ref*CM`.
Die passende Re-Abhaengigkeit muss zuvor ausgewaehlt/interpoliert werden.

Jeder intern berechnete Streifenkraftvektor besitzt einen dreidimensionalen
Ansatzpunkt im lokalen Segelsystem relativ zum Hals. Dessen Definition folgt
dem gewaehlten physikalischen Modell; ein zusaetzliches Profilmoment darf
nicht durch die Punktwahl verloren gehen.

Diese Einzelbeitraege werden als Pflichtprodukt in `kraefte.parquet` ausgegeben,
nicht nur intern zur Summenbildung verwendet. Mehrere Kraefte je Betriebspunkt
sind ausdruecklich zulaessig. Jede Kraft hat einen gueltigen raeumlichen
Ansatzpunkt; ohne belastbaren Ansatzpunkt darf sie nicht als positionierte
Einzelkraft exportiert werden.

Spalten in der folgenden logischen Reihenfolge:
`form_id` (UTF-8), `alpha_deg`, `windneigung_deg`, `re_ref`, `beitrag_id`
(Int64), `CFx`, `CFy`, `CFz`, `ansatz_x_m`, `ansatz_y_m`, `ansatz_z_m`,
`CM_eigen_x`, `CM_eigen_y`, `CM_eigen_z` (binary64) und `qualitaet` (UTF-8).

`beitrag_id` identifiziert den Beitrag innerhalb eines Betriebspunktes.
Alle Kraftbeiwerte verwenden dieselbe Gesamtsegel-Referenzflaeche `S`,
alle Eigenmomentbeiwerte dieselbe Laenge `c_ref`. Ansatzpunkte sind in Metern
im lokalen Segelsystem relativ zum Hals angegeben, nach Anwendung der
modellierten Verwindung. `CM_eigen` beschreibt ein freies Moment zusaetzlich
zum Hebelarmmoment der jeweiligen Kraft, nicht nochmals ihr Gesamtmoment.
Reine Momentenbeitraege koennen als Nullkraft mit explizitem Bezugspunkt und
Eigenmoment erhalten bleiben; sie sind als solche zu kennzeichnen.

Es gilt fuer jeden Betriebspunkt:
`CF_summe = Summe(CF_i)` und
`CM_summe = Summe((r_i/c_ref) kreuz CF_i + CM_eigen_i)`.
Diese Summen muessen mit `summen.parquet` innerhalb der dokumentierten numerischen
Toleranzen uebereinstimmen. Kraefte an verschiedenen Punkten duerfen fuer
eine spaetere SDP-Auswertung nicht durch eine ungewichtete Punktmittelung
zusammengefasst werden.

Der Konsument verwendet entweder die Einzelbeitraege oder die Resultierende
mit Restmoment, niemals beide gleichzeitig als zusaetzliche Lasten.
Einbaulage und Transformation ins gemeinsame Bootssystem erfolgen erst
im Gesamtmodell. Dies gilt fuer die jetzigen Einzel-Lookups; der spaetere
Konfigurationsmodus legt seine gemeinsamen Bezuege gemaess Abschnitt 5.3 fest.
Optional vereinfachte Beitraege muessen Kraft und Moment
erhalten; eine Reduktion der Streifenbeitraege ist im Erstumfang nicht vorgesehen.

Auch die resultierende Kraft erhaelt einen raeumlichen Ansatzpunkt als
Pflichtausgabe. Fuer eine reproduzierbare aequivalente Darstellung wird
der dem Hals naechste Punkt der zentralen Wirkungslinie verwendet:

`r = c_ref * (CF kreuz CM) / (CF dot CF)`

Das verbleibende Moment ist
`CM_rest = CM - (r / c_ref) kreuz CF`.
Damit bleibt die Identitaet `M_Hals = r kreuz F + M_rest` erhalten.
Der Ansatzpunkt muss nicht auf der Segelflaeche liegen und ist kein
Nachweis eines eindeutig bestimmbaren physikalischen Druckzentrums.
Er dient mit Kraft und Restmoment als Grundlage einer spaeteren
SDP-Ermittlung im Gesamtmodell. Die Integration mehrerer Segel benoetigt
zusaetzlich deren Einbaulagen und einen gemeinsamen Momentenbezug.

Bei verschwindender oder numerisch zu kleiner resultierender Kraft ist der
Punkt nicht belastbar. `angriff_gueltig` ist dann 0, die drei Punktfelder
bleiben leer und das volle Moment bleibt in `CM_rest` erhalten.
Die dimensionslose Kraftschwelle wird im Ergebnisformat eindeutig dokumentiert;
sie darf nicht stillschweigend einen Punkt im Ursprung vortaeuschen.
Bei gueltigem Punkt ist `angriff_gueltig` gleich 1.

Nach einer spaeteren Interpolation von Kraft und Moment wird der aequivalente
Ansatzpunkt erneut daraus bestimmt; unabhaengiges Interpolieren aller
Punktfelder garantiert die Momentenidentitaet nicht.

**Abnahme:** Bekannte Einzelkraefte mit vorgegebenen Punkten, Kraeftepaare,
reine Momente und nahezu verschwindende Resultierende pruefen die
Momentenidentitaet und Gueltigkeitskennzeichnung. Ein Momentenanteil parallel
zur Kraft bleibt als Restmoment erhalten.

### 8.3 `profile.parquet`

Spalten: `profil_id` (UTF-8), `alpha_deg`, `re`, `CL`, `CD`, `CM`
(binary64) und `qualitaet` (UTF-8).

Profile sind eindeutig in `lookup.json` beschrieben. Gleiche Profile duerfen
wiederverwendet werden. Winkel/Re-Stuetzstellen muessen die tatsaechlich
verwendeten lokalen Abfragen und deren Interpolation abdecken.
Identische Datensaetze duerfen keine widerspruechlichen Werte enthalten.
Diese Tabelle beschreibt lokale zweidimensionale Profilpolaren, nicht die
raeumlichen Betriebspunkte des Gesamtsegels. Deren Neigung, Projektion und
3D-Kopplung gehoeren in den Gesamtsegelprovider; sie duerfen nicht durch
blosses Duplizieren der Profilpolaren als berechnet gelten.

### 8.4 Parquet- und Zstandard-Konvention

Genau die drei Ergebnistabellen werden als Apache Parquet mit Zstandard
geschrieben. Spaltentypen folgen den Abschnitten 8.2 und 8.3; Masseinheiten,
Koordinaten-/Momentenbezug, Tabellenachsen und Schluesselsemantik werden im
Parquet-Schema und im `lookup.json`-Manifest dokumentiert. Werte sind ohne
Dezimalformatierung binary64-genau zu erhalten; nicht endliche Zahlen und
Ersatzzeilen fuer fehlgeschlagene Punkte sind unzulaessig. Zeilen sind
deterministisch nach den jeweiligen Schluesseln sortiert.

Row Groups werden unabhaengig lesbar erstellt und so dimensioniert, dass
selektive Betriebspunktzugriffe mit Interpolationsnachbarn nicht die gesamte
Tabelle dekodieren muessen. Die konkrete Zeilenzahl je Row Group und die
quantitativen Latenz-/Speicherziele werden im protokollierten Zugriffstest
festgelegt. `summen.csv`, `kraefte.csv`, `profile.csv` und andere CSV-Ableitungen
werden weder erzeugt noch angeboten.

### 8.5 `pruefbericht.json`

Faelle mit Erfolg/Fehler, maximale absolute/relative Abweichungen,
fehlgeschlagene Betriebspunkte, Providerkonvergenz, Warnungen, Annahmen,
CPU/GPU-Entscheidung, Ressourcen, Laufzeiten und Grenzen.
Der Bericht trennt numerische Pruefung von physikalischer Validierung.

### 8.6 `auftrag_ausgefuehrt.json`

Originalauftrag und effektive Einstellungen, CLI-Limit, CPU/GPU-Backend,
Tool-/Modellversion, Eingabepruefsummen sowie Snapshots der Segelgeometrie
und Konventionen. Keine ausschliessliche Abhaengigkeit von externen Pfaden.
Die effektiven CLI-Werte fuer `fingerprint` und `output_policy` werden
ebenfalls festgehalten; das nachgelagerte Core-Ergebnis steht im Abschluss-JSON.

### 8.7 Schutz vor Teil- und Konkurrenzresultaten

Ohne `--output-policy overwrite` wird vorhandenes `LookUp` erfolgreich
uebersprungen. Die explizite CLI-Option erlaubt das Ersetzen trotz des
Schutzstandards `ausgabe.vorhandene_ergebnisse=nicht_ueberschreiben` im Auftrag.
Berechnung/Export erfolgen in
einem exklusiven temporaeren Geschwisterordner; erst nach vollstaendiger
Pruefung wird dieser als `LookUp` veroeffentlicht.
Beim Ersetzen wird der alte Lookup kurz zur Seite verschoben. Scheitert
der neue Ordnerwechsel, wird der alte Stand wiederhergestellt; Fehler der
Wiederherstellung werden mit seinem verbliebenen Pfad gemeldet. Nach Erfolg
wird der alte Stand geloescht, ohne dauerhafte Sicherung. Ein Prozessabsturz
zwischen den Ordnerwechseln kann manuelle Wiederherstellung erfordern.
Zwei gleichzeitige Auftraege im gleichen Segelordner duerfen sich nicht
ueberschreiben oder vermischen. Der verlierende Aufruf meldet einen Konflikt.

`--fingerprint update` verwendet
`segelphysik::aktualisiere_modell_fingerprints` aus der echten Core-Bibliothek
mit genau dem Bereich `aerodynamics`, niemals einen nachgebildeten Hash.
Der Modellordner wird aus dem Vorfahren `modells/aerodynamics` des Auftrags
bestimmt. Core prueft die Modelldateien und referenzierten Ressourcen ueber
seine konfigurierte Schemabasis. Er aktualisiert ausschliesslich den
Aerodynamik-Eintrag in `modells_validation.json`; der gesamte Bereich, auch
andere Segel, geht in den Hash ein. Der bestehende Core-Vertrag bleibt
unveraendert.

Die Hashaktualisierung folgt erst nach Publikation und Entfernung des alten
Austauschordners oder nach dem bewussten Ueberspringen. Ein unveraenderter
Hash behaelt sein Datum. Bei Core-Fehlern meldet die CLI Code 5; bereits
geschriebene Lookups bleiben erhalten und werden im Abschluss-JSON benannt.
Lookup-Publikation und Modellvalidierung sind getrennte Transaktionen.
Erfolg meldet zusaetzlich `output_action=created|overwritten|skipped` und
bei `update` das Core-Ergebnis in `fingerprint`; ein Fehler nach
Publikation meldet `output_action=written`.

Fehlerberichte bleiben in einem eindeutig benannten Diagnoseordner neben
`LookUp` erhalten; sie sind keine gueltigen Lookup-Ergebnisse.
Temporare Solverdateien werden auf auftragseigene Dateien begrenzt und
nach Abschluss bereinigt; Diagnosebelege bleiben erhalten.

## 9. Reproduzierbarkeit und Integration

Bei gleichen Eingaben, Provider-/Toolversionen und gleichem Backend bleiben
Tabellenschluessel und numerische Ergebnisse reproduzierbar. Workeranzahl
darf die Ergebnisse nicht ausserhalb der Toleranzen veraendern.
Zwischen CPU/GPU werden numerische Toleranzen verlangt, keine pauschale
Bitgleichheit. Zeitstempel und Laufzeiten duerfen unterschiedlich sein.

Die Rechenlogik ist von Argumentparser, Prozessausgabe und Dateiexport
getrennt. Ein spaeterer direkter API-Aufruf muss dieselbe Validierung und
Berechnung verwenden koennen. Konkrete oeffentliche C++-Typen/Funktionen
werden hier nicht festgelegt; vor ihrer Einfuehrung gilt das
Schnittstellenregister-Verfahren des Projekts.

## 10. Uebergreifende Abnahmeszenarien

### Testgrundlage und Schutz der Originaldaten

Die notwendigen Integrations- und Ende-zu-Ende-Tests koennen am
420-Grosssegel erfolgen:

```text
C:\_DATEN\Code\Python\Organisierter Code\Segelphysik\Modelle\420\meshes\aerodynamik\main\main.json
```

Vor Testarbeiten wird diese Datei separat gesichert und die Kopie per SHA-256
mit dem Original verglichen. Die am 05.11.2026 erstellte
[Sicherung/Testreferenz](../Code/tests/daten/main_20261105_195154_358.json) hat die SHA-256-Pruefsumme
`E8C47FFCA62459C08AE525320794B19884A72544A2857C77606473CCEA862EB2`.

Automatisierte Tests verwenden isolierte Kopien von Geometrie, Auftrag und
Konventionen mit passend aufgeloesten Testpfaden. Fehlerfaelle duerfen nur
diese Kopien veraendern. Das Original und bestehende Modell-Lookups werden
weder geaendert noch bereinigt; die unveraenderte Originalpruefsumme ist nach
Testarbeiten zu pruefen. Sicherungen duerfen nicht durch Tests ueberschrieben
oder als temporaere Testdateien geloescht werden.

Die Sicherung dokumentiert nur den gesicherten Stand, keine bereits erfolgte
Generator-Abnahme. Fuer kuenftige bewusst geaenderte Originalstaende wird vor
Tests eine neue Sicherung angelegt.

| Szenario | Erwartung |
|---|---|
| Gueltiger bereiter 420-Auftrag | Vollstaendiger Ordner mit genau sechs Ergebnisdateien einschliesslich aller drei Parquet-Tabellen und positionierter Einzelkraefte; keine CSV-Datei |
| 420-Grosssegel und synthetische Randfaelle | Referenzlauf erfolgreich; dreieckige Endpunkte und viereckige Kopfkanten korrekt geschnitten |
| Entwurfsauftrag mit `lookup` | Klare Ablehnung, kein erfolgreiches Ergebnis |
| Falsche Pfade/Versionen/Optionen | Fehler vor Beginn teurer Rechenarbeit |
| Lokale Re ausser Providerbereich | Explizite Ablehnung oder spezifizierte gekennzeichnete Behandlung |
| Physikalisches Hybridmodell | Woelbung/Twist, viskose Effekte, raeumliche Kopplung und Hochwinkelbehandlung gegen dokumentierte Referenzfaelle geprueft; verbleibende Grenzen und Validierungsstand sichtbar |
| Zehn Windneigungen und sechs Re-Stufen | 21600 eindeutige Rasterzeilen je Form; Winkeldefinition und Neigung in Metadaten und Gesamtsegeltabellen |
| Windneigung 90 Grad | Identische lokale Kraft-/Momentenvektoren fuer alle Azimute; keine Singularitaet |
| Neigungs- und Winkelzellenzwischenpunkte | Direkte Rechnung stimmt innerhalb der Toleranzen mit Interpolation ueberein |
| Provider ohne raeumliche Anstroemung | Erweiterter Auftrag wird vor Solverstart abgelehnt; kein horizontaler Ersatz |
| Vorbereitung mehrerer Segelflaechen | Interne Geometriedarstellung erhaelt getrennte Segelidentitaeten und Transformationen; Identitaetstransformation bewahrt den Einzelpfad, synthetische Starrkoerpertransformation erhaelt Kraft-/Momentenbezuege; kein Nachweis gekoppelter Aerodynamik |
| Abgrenzung Einzel-Lookups | Keine Interferenzbehauptung in Metadaten; keine Einbaupositionen in intrinsischer Geometrie und keine Mehrsegel-Pflichtfelder im Einzelauftrag |
| Raster-/Winkelpruefung fehlgeschlagen | Kein als erfolgreich veroeffentlichtes `LookUp` |
| CPU-Limit und mehrere Providerthreads | Globales Budget eingehalten |
| GPU vorhanden/unverfuegbar und Funktion geeignet/ungeeignet | Zuordnung nach dokumentierter Einschaetzung ohne Speedpruefung; GPU-geeignete Funktionen nutzen verfuegbare unterstuetzte Hardware |
| Abbruch, Speicher-/Schreibfehler | Fehlercode und Diagnose; keine scheinbar vollstaendige Tabelle |
| Zweiter Lauf/vorhandenes Ergebnis | create-only ueberspringt; overwrite ersetzt erst nach erfolgreicher Neuberechnung |
| Ergebnisordner auf anderen Rechner kopiert | Metadaten und Tabellen ohne Ursprungsrepository interpretierbar |
| Bibliothek/CLI spaeter gemeinsam genutzt | Gleicher numerischer Kern, keine GUI-Abhaengigkeit |

Diese Datei bleibt die massgebliche, zur Umsetzung freigegebene Spezifikation.
Der vorhandene Umsetzungsplan wird fortgefuehrt; fachliche Abnahme ist separat.

## 11. Kompaktes Parquet-Ausgabeformat

Am 12.11.2026 durch den Nutzer festgelegt: **Apache Parquet mit Zstandard ist
das einzige Format der Ergebnistabellen. CSV-Erzeugung, CSV-Export und
alternative tabellarische Ausgabeformate entfallen vollstaendig.** Der
Umsetzungsplan fuehrt Prototyp, Implementierung und Abnahme nach; keine
Anforderung in diesem Abschnitt autorisiert Aenderungen am SegelPhysik-Core.

### 11.1 Zweck und Abgrenzung

SegelMacher erzeugt aerodynamische Segel-Lookups: Betriebspunkt zu Kraeften
und Momenten. Es spezifiziert keine allgemeine meteorologische
Windfelddatei. Das Ergebnisformat soll die bisherigen grossen CSV-Dateien
vermeiden und effiziente gezielte Lesezugriffe ermoeglichen, ohne
physikalische Beitraege oder binary64-Praezision zu verlieren.

Der SegelPhysik-Core prueft derzeit Lookup-Identitaet und Referenz, wertet die
Daten aber noch nicht als Windphysik aus. Ein Core-Leser wird separat
freigegeben. Bestehende CSV-Ergebnisse werden nicht automatisch konvertiert
oder ueberschrieben; neue Lookup-Auftraege erzeugen ausschliesslich das hier
festgelegte Format.

### 11.2 Ausgabeprodukte und Tabellen

Jeder Lookup-Ordner enthaelt genau sechs Ergebnisdateien:
`lookup.json`, `summen.parquet`, `kraefte.parquet`, `profile.parquet`,
`pruefbericht.json` und `auftrag_ausgefuehrt.json`. Die drei Parquet-Tabellen
werden mit Zstandard geschrieben. Es gibt keine CSV-Datei und keinen
Konfigurationsschalter fuer CSV.

Summen und Einzelkraefte bleiben getrennte Tabellen. Die kompakte
Summen-Tabelle dient dem schnellen Simulationspfad; die vollstaendige
Einzelkraft-Tabelle bleibt fuer Detaildarstellung und Auswertung erhalten.
Beide duerfen nicht gleichzeitig als unabhaengige Lasten bilanziert werden.

### 11.3 Parquet-Schema

- `summen.parquet`: `form_id` (UTF-8), `alpha_deg`, `windneigung_deg`,
  `re_ref`, `CFx`, `CFy`, `CFz`, `CMx`, `CMy`, `CMz`, `angriff_x_m`,
  `angriff_y_m`, `angriff_z_m`, `angriff_gueltig` (Boolean), `CM_rest_x`,
  `CM_rest_y`, `CM_rest_z`, `qualitaet` (UTF-8).
- `kraefte.parquet`: `form_id` (UTF-8), `alpha_deg`, `windneigung_deg`,
  `re_ref`, `beitrag_id` (Int64), `CFx`, `CFy`, `CFz`, `ansatz_x_m`,
  `ansatz_y_m`, `ansatz_z_m`, `CM_eigen_x`, `CM_eigen_y`, `CM_eigen_z`
  (binary64) und `qualitaet` (UTF-8).
- `profile.parquet`: `profil_id` (UTF-8), `alpha_deg`, `re`, `CL`, `CD`,
  `CM` (binary64) und `qualitaet` (UTF-8).
- Alle reellen Spalten sind IEEE-754 binary64. Nullwerte sind nur fuer die
  drei Resultierenden-Ansatzpunktspalten bei `angriff_gueltig = false`
  zulaessig. Es gibt keine NaN-/Infinity-Werte oder Ersatzlasten.
- Parquet-Schema und `lookup.json` beschreiben Feldtypen, Einheiten,
  Koordinaten-/Momentenbezug, Tabellenachsen, Winkelkonventionen,
  Referenzgroessen und Qualitaetssemantik. Betriebspunktidentitaet darf
  nicht ausschliesslich aus Gleitkomma-Gleichheit rekonstruiert werden.
- Summen, Einzelbeitraege, Positionen, Eigenmomente und Qualitaetsmerkmale
  bleiben vollstaendig erhalten. Keine Rundung oder Streifenreduktion.

### 11.4 Speicherung und Zugriff

Tabellen werden streamingfaehig geschrieben und in deterministischer
Schluesselreihenfolge sortiert. Parquet-Row-Groups sind unabhaengig lesbar;
Spaltenprojektion und Row-Group-Auswahl sollen gezielte Betriebspunktabfragen
einschliesslich benoetigter Interpolationsnachbarn ermoeglichen, ohne die
vollstaendige Tabelle zu dekodieren. Dateigroessen, Offsets und interne
Zaehler muessen ueber 4 GiB sicher bleiben.

Das Manifest fuehrt `formatversion` mit Wert `"3"` und die Parquet-
`schema_version` mit Wert `"1"` sowie Apache-Arrow-Version, Zstandard-Codec,
Row-Group-Anzahlen, Zeilenzahlen und SHA-256-Pruefsummen aller fuenf uebrigen
Ergebnisdateien nachvollziehbar auf (das Manifest kann nicht seine eigene
Pruefsumme enthalten). Gleiche
Eingaben und festgelegte Bibliotheks-/Codecversion muessen identische
Tabellenbytes unabhaengig von der Workeranzahl erzeugen. Unbekannte
Versionen, defekte Metadaten, abgeschnittene oder beschaedigte Daten fuehren
zu einem sichtbaren Fehler. Kein Nullkraft-Ersatz, stiller Formatwechsel
oder erfolgreiches Teilergebnis.

Die spaetere Windtransformation beruecksichtigt relativen Wind einschliesslich
Bootsbewegung und transformiert in das jeweilige Segelsystem sowie zurueck.
Der aktuelle Core prueft Lookup-Identitaet und Referenz, wertet die Daten
aber noch nicht als Windphysik aus. Negative Windneigungen werden direkt aus dem signierten 3D-Vektor berechnet
und weder gespiegelt noch begrenzt. Version 3 ist die einzige unterstuetzte
Lookup-Formatversion; es gibt keine Legacy-Leser fuer aeltere Lookups.
Einzelne Segel-Lookups enthalten keine Grosssegel-/Fock-Wechselwirkung.

### 11.5 Implementierung und Abnahme

Die C++-Implementierung verwendet Apache Arrow/Parquet 21.0.0, kompatibel
mit dem Projektstandard C++17, und den Zstandard-Codec. Die Version wird im
vcpkg-Manifest festgehalten. Der gezielte Leseprototyp prueft
Spaltenprojektion, Row-Group-Auswahl, Interpolationsnachbarn sowie kalte und
warme Zugriffe. Konkrete Schwellen fuer Dateigroesse, Zugriffslatenz und
Spitzen-RAM werden anhand einer Kopie einer realen grossen Lookup-Tabelle
festgelegt; die CSV-Ausgabe bleibt auch fuer diesen Vergleich kein
Produktausgabeformat, Zwischenformat oder Benchmark-Kandidat. Fuer die
Abnahme werden keine CSV-Dateien erzeugt.

| Nachweis | Abnahmekriterium |
|---|---|
| Ergebnisform | Genau sechs Dateien wie in 11.2; keine CSV-Datei, kein CSV-Export. |
| Verlustfreier Roundtrip | Alle Schluessel und endlichen binary64-Werte stimmen zwischen erzeugtem Parquet und unabhaengigem Parquet-Leser bitgleich ueberein. |
| Kraft-/Momentenvertrag | Summen, Einzelkraefte, Ansatzpunkte und vollstaendige Momente erfuellen die Identitaeten aus 8.2; Profilpolaren sind endlich und korrekt zugeordnet. |
| Selektiver Zugriff | Benutzte C++-Bibliothek liest nur erforderliche Spalten/Row-Groups fuer Betriebspunkte und Interpolationsnachbarn; Vollscan ist nicht Voraussetzung des Einzelzugriffs. |
| Integritaet und Fehler | Manifest-Hashes, unbekannte Versionen sowie beschaedigte/abgeschnittene Parquet-Dateien werden korrekt geprueft bzw. sichtbar abgelehnt. |
| Reproduzierbarkeit | Tabellenbytes sind bei identischen Eingaben und festen Abhaengigkeitsversionen unabhaengig von Workeranzahl identisch. |
| Ressourcenmessung | Dateigroesse, Erzeugungszeit, Vollscan, kalte/warme Zugriffe, p95-Latenz und Spitzen-RAM einer grossen Testkopie dokumentiert; quantitative Grenzen werden mit Messnachweis festgehalten. |

## 12. Vorzeichenbehaftete lokale Windneigung

**Status: am 14.11.2026 finalisiert und zur Umsetzung freigegeben.** Die
Nutzerentscheidungen und die bestaetigte Core-Abstimmung sind in dieser
Spezifikation zusammengefuehrt. Anforderungszuordnungen stehen in Abschnitt 0
und die Umsetzung samt Nachweisen im
[Umsetzungsplan](../Umsetzungsplaene/Entwurf_Lookup_Generator.md#10-vorzeichenbehaftete-lokale-windneigung).
Negative und positive Neigungen werden direkt aus dem signierten 3D-Vektor
berechnet. Alte Lookups werden nicht als Version 3 gelesen; produktive
Lookups muessen nach erfolgreicher Neuerzeugung ersetzt werden. Die
physikalische Validierung bleibt offen und ist keine Bedingung fuer die
Nutzbarkeit der Modelle.

### 12.1 Problem und Ziel

Vor dieser Umsetzung akzeptierte der S-2.0-Stand als Auftragsachse
ausschliesslich `[0,10,...,90]` und `segel::Context::solve` lehnte `beta < 0`
ab. Auch der Lookup-Inhalt deckte
nur nichtnegative Neigungen ab. Negative lokale Elevationen koennen bei
Krangung, Stampfen, Einbaulage oder relativer Bewegung auftreten; der
Weltwind bleibt dabei unabhaengig von der Bootsneigung. Bei 20 Grad Rollung
und passender seitlicher horizontaler Windherkunft ergibt sich beispielsweise
beta = -20 Grad; Windrichtung und Rollvorzeichen bestimmen das Vorzeichen.

Ziel ist ein direkt berechneter Lookup fuer die ganze Kugel:
`-90 <= beta <= +90` Grad, ohne Spiegelung bereits berechneter Lasten,
Begrenzung oder Extrapolation. Die Lasten sind weiterhin fuer ein einzelnes
Segel, nicht fuer eine gekoppelte Grosssegel-/Fock-Rechnung.

### 12.2 Winkel-, Modell- und Schnittstellenvertrag

- **H1-Eignung, was der Softwarevertrag pruefen kann:** H1 bildet den
  Stroemungseinheitsvektor als vollstaendigen vorzeichenbehafteten
  3D-Vektor `u = -h`. Der abgeloeste Anteil berechnet Flaechendruck aus
  `dot(u,n)` samt Vorzeichen und Reibung aus dem tangentialen Vektor.
  Der gebundene Anteil verwendet die horizontale Projektion fuer die
  Traglinie und wird bei Spannweitenanstroemung ausgeblendet; nahe beiden
  Polen darf keine Division durch die verschwindende Horizontalprojektion
  stattfinden. Die positive Beta-Achse darf weder fuer Lasten noch Momente
  als Symmetrie-Ersatz fuer negative Beta dienen. Fuer beide Pole, die
  beidseitige Ueberblendung und eine asymmetrische Segelgeometrie sind
  endliche, komponentenweise konsistente Kraefte und Momente nachzuweisen.
  Das ist ein Software-/Numeriknachweis, kein CFD- oder Messnachweis.
- **Modellgrenzen:** Die Crossflow-Traglinie vernachlaessigt die
  spannweitige Komponente der gebundenen Zirkulationskopplung; ihr Anteil
  wird im Hoch-Elevationsbereich ausgeblendet. Die abgeloeste
  Mittelflaechen-Impuls-/Reibungsschliessung verarbeitet den vollen Vektor,
  ist aber nicht an negativen Neigungen physikalisch validiert. Beidseitige
  Ueberblendung nach `abs(beta)` ist eine H1-Schliessungsannahme, nicht
  physikalische Symmetrie. Einzel-Lookups bleiben Einzel-Segel-Lookups;
  keine Interferenz oder gekoppelte Fock-/Grosssegelwirkung behaupten.
- Die bestehende Windherkunftskonvention bleibt erhalten:
  `h = (cos(beta)*cos(alpha), cos(beta)*sin(alpha), sin(beta))`,
  lokales X voraus, Y backbord, Z oben; Stroemungseinheitsvektor `u = -h`.
  Grad werden fuer trigonometrische Funktionen in Radiant umgerechnet.
  `beta = -90` bedeutet Herkunft exakt `(0,0,-1)`, `+90` exakt `(0,0,1)`.
  Azimut ist an beiden Polen geometrisch unbestimmt; alle ausgegebenen
  Azimutzeilen fuer denselben Pol/Form/Re haben identische CF- und CM-Vektoren.
- Der Generator berechnet jeden negativen Betriebspunkt aus dessen eigenem
  vorzeichenbehaftetem 3D-Stroemungsvektor und der wirklichen Geometrie.
  Die bisherige Ueberblendung des anliegenden Anteils bei 60 bis 80 Grad
  ist anhand von `abs(beta)` auf beide Pole auszudehnen. Impuls, Reibung,
  Lastpositionen und Momente nutzen weiterhin den vorzeichenbehafteten
  Vektor. Symmetrische Ueberblendungsgrenzen sind eine explizite
  Modellannahme, keine behauptete Lastsymmetrie.
- CPPI-0005: `Context::solve` akzeptiert endliche Elevation im geschlossenen
  Bereich `[-90,90]`; Ausserbereichswerte und
  nicht endliche Werte werfen wie bisher eine Ausnahme. Signatur, sonstige
  Eingaben, JSON-Rueckgabe, Bezugsgroessen, Ownership und Thread-Vertrag
  bleiben unveraendert. Header, Register und
  [H1-Modellfestlegung](../Dokumentationen/Modellfestlegung_Lookup.md)
  fuehren den freigegebenen Vertrag.

### 12.3 Auftrag, Ergebnis und Kompatibilitaet

Entsprechend der Nutzerentscheidung werden alle Auftraege auf die explizite, streng
aufsteigende Grundachse `[-90,-80,...,0,...,80,90]` umgestellt; der
Beispielauftrag ebenso. Fuer die neue Generatorversion ist das bisherige
`[0,10,...,90]` kein gueltiger Auftrag mehr und wird mit einer eindeutigen
Meldung zur erforderlichen Umstellung abgelehnt, nicht still erweitert.
Andere Raster, Duplikate, nicht endliche oder ausserhalb `[-90,90]`
liegende Werte werden sichtbar abgelehnt. Die bestehende
Rechenplanbegrenzung ist anhand der **tatsaechlich erweiterten Ausgabeachse**
vor der teuren Rechnung zu pruefen.

Als Anfangskandidat fuer zusaetzliche negative Stuetzstellen dienen die
negierten bisherigen positiven Verfeinerungsstellen einschliesslich -88.75.
Das ergibt zunaechst 19 Basis- und 45 Ausgabeneigungen; 45 ist keine
Akzeptanzgarantie. Nur Winkelstuetzstellen werden gespiegelt, niemals
Kraefte oder Momente. Scheitert die direkte Zwischenpunktpruefung, sind
Stuetzstellen begruendet zu ergaenzen oder die Ausgabe abzulehnen; Toleranzen
duerfen nicht still gelockert werden.

Die Parquet-Spalten, binary64-Werte und sechs Dateien bleiben unveraendert.
`lookup.json`, `auftrag_ausgefuehrt.json` und `pruefbericht.json` muessen
die tatsaechliche Basis- und Ausgabeachse, Winkelkonvention, Modellstand und
Nachweise uebereinstimmend ausweisen. Die mit Core bestaetigte
Lookup-`formatversion` ist `"3"`; die Parquet-`schema_version` bleibt `"1"`.
Manifest und Vertrag nennen explizit signed Domaene, Segelhals als
Geschwindigkeits-/Reynolds- und Momentenbezug,
`Re_ref = |v_rel| * c_ref / nu`, `q = 0.5 * rho * |v_rel|^2` und lineare
Interpolation in `log(Re)` nur zwischen vorhandenen Stuetzstellen.
Ausserhalb der manifestierten Beta- oder Re-Domaene wird mit Anfrage und
vorhandener Domaene diagnostiziert; kein Spiegeln, Klemmen oder
Extrapolieren. Aeltere Lookup-Formatversionen sind keine kompatiblen
Eingaben und erhalten keine Legacy-Implementierung.

Die vorhandenen alten Lookups werden nach Nutzerentscheidung verworfen und
fuer die umgestellten Auftraege neu erzeugt; sie werden nicht gespiegelt,
umetikettiert oder als vollstaendige neue Lookups weiterverwendet.
`create-only` allein reicht dafuer nicht: Es ueberspringt bereits vorhandene
Lookups. Die geplante Neuerzeugung verwendet `overwrite` oder einen neuen
Arbeitsort und prueft anschliessend die tatsaechliche Achse. Vor dem Austausch
bleibt der alte Stand bei Rechenfehler erhalten; Fehlversuche werden nicht
als erfolgreiche Erneuerung gemeldet. Produktive Modellauftraege und Lookups liegen im separaten Modelle-Repository.
Vorhandene Lookups werden mit der festgelegten atomaren `overwrite`-Semantik
erst nach erfolgreicher Neuberechnung ersetzt. Nach Austausch den Aerodynamik-Fingerprint auf Wunsch ausschliesslich
ueber die bestehende SegelPhysik-Core-API aktualisieren; die ganze
`modells/aerodynamics`-Struktur eines Modells ist betroffen.

### 12.4 Bestaetigter Core-Konsumentenvertrag

Die Core-Bestaetigung vom 14.11.2026 umfasst die Richtung und alle vier
folgenden Details. Die Umsetzung eines Core-Lookup-Lesers oder eines
Segelkraftpfads wird damit nicht behauptet; diese Aenderung setzt keinen
Code im separaten Core-Repository voraus.

| Vertragspunkt | Bestaetigte Regel |
|---|---|
| Lokale Geschwindigkeit und Reynolds-Zahl | Referenzpunkt ist der Segelhals/Lookup-Momentenursprung. `v_rel` wird dort als Umgebungswind minus Starrkoerpergeschwindigkeit aus Translation und `omega x (r_H-r_CG)` bestimmt. `Re_ref = |v_rel| * c_ref / nu`; `q = 0.5 * rho * |v_rel|^2` verwendet dieselbe Geschwindigkeit. Uniformes Feld am Referenzpunkt; relevante Windgradienten oder Segelverformung liegen ausserhalb dieses Einpunktmodells. |
| Re-Stuetzstellen und Interpolation | Die manifestierte Re-Achse ist massgeblich. Zwischen benachbarten Stuetzstellen wird linear in `log(Re_ref)` interpoliert. Ausserhalb der tatsaechlichen Domaene erfolgt eine Diagnose mit Anfrage und vorhandener Achse, keine Extrapolation. |
| Moment und Angriffspunkt | Lookup-Kraft und vollstaendiges CM um den Hals werden mit `q*S` bzw. `q*S*c_ref` skaliert. Transformation: `F_W = R F_H`, `M_O,W = R M_H + (r_H-r_O) x F_W`. Der resultierende Angriffspunkt/Restmoment ist keine Ersatzberechnung fuer das Gesamtmoment. Summen oder Einzelbeitraege, nie beide doppelt, gehen in die Bilanz ein. |
| Lookup-Versionierung | Neue Lookup-Manifestversion `"3"` bei unveraenderter Parquet-Schemaversion `"1"`. Manifestachsen und signierte Windkonvention sind explizit. Aeltere Lookup-Formatversionen werden nicht als Version 3 interpretiert; keine Legacy-Unterstuetzung. |

Fuer nicht null lokale Stroemung `v_rel` gilt:
Windherkunft `h = -v_rel / ||v_rel||`,
`beta = atan2(h_z, hypot(h_x,h_y))`,
`alpha = atan2(h_y,h_x)`; die Geschwindigkeit selbst bleibt fuer q und Re
erhalten. Ein Nullvektor hat keine Windrichtung und wird nicht in einen
beliebigen Tabellenwinkel umgewandelt, sondern mit einer Diagnose abgelehnt.
Am Pol sind Winkel und Interpolation azimutunabhaengig; falls
die Datenstruktur einen Azimutindex verlangt, darf ein kanonischer Index
nur fuer den Tabellenzugriff dienen und keine azimutabhaengige Physik
einfuehren.

### 12.5 Abnahme

Konkrete Tests, Messgroessen, Reihenfolge und Abbruchbedingungen stehen in
Abschnitt 10 des Umsetzungsplans. Numerische und Integrationstests sind verpflichtend;
eine physikalische Validierung anhand von Messdaten wird nicht verlangt und
bleibt als offen gekennzeichnet. Die fehlende physikalische Validierung darf
zu keiner Zeit die Nutzbarkeit der Modelle einschraenken und ist kein
Freigabe- oder Laufzeitblocker. Die bestehende Kennzeichnung als nicht
physikalisch validiertes Ingenieurmodell bleibt transparent. Fehler der
verpflichtenden Software- und Numerikpruefungen werden weiterhin ausdruecklich
gemeldet; sie werden nicht wegen der offenen Validierung unterdrueckt.
Ein Testerfolg ersetzt keine physikalische Validierung.

## 13. Vorbereitetes Segelmodell fuer lokale Windfelder

Stand: 15.11.2026.

**Status: am 15.11.2026 auf ausdruecklichen Nutzerauftrag finalisiert.**
Der zugehoerige
[Umsetzungsplan](../Umsetzungsplaene/Entwurf_Lookup_Generator.md#11-vorbereitetes-segelmodell-fuer-lokale-windfelder)
ist die einzige Arbeitsplanung fuer diesen Folgeschritt. Finalisierung
bedeutet weder Implementierungsauftrag noch bestandene Nachweise.
Die Herkunftsentscheidungen stehen in der
[Entwurfspunkteliste](Entwurfspunkte/Lookup_Generator.md#erweiterung-vorbereitetes-segelmodell-fuer-lokale-windfelder-15112026).
Die ausdruecklich entschiedene spaetere Abloesung des bisherigen
Lookup-Generators und der Schutz bereits erzeugter Dateien sind in 13.7
und 13.12 geregelt.

### 13.0 Finale Anforderungen

| Kennung | Pruefbares Ergebnis |
|---|---|
| A-S-00043 | Ein Einzel-Segelmodell aus gueltiger Kontur, Profilen, Numerik und expliziten Modellparametern erzeugen; keine Segelinterferenz behaupten (13.1/13.2). |
| A-S-00044 | Segelfestes rechtshaendiges System mit Halsursprung, X zum Bug, Y Backbord, Z oben und dokumentierten SI-Einheiten verwenden (13.3). |
| A-S-00045 | Gesamte Konturhoehe in `ceil(H/0.25 m)` gleich hohe Streifen teilen und tatsaechliche Kontur- und Profilverlaeufe integrieren (13.3). |
| A-S-00046 | Abfragepunkte bei halber Streifenhoehe und Viertelsehne eindeutig ausgeben; bei nicht eindeutig bestimmbarer Sehne mit Diagnose abbrechen (13.3). |
| A-S-00047 | Windvektoren zwischen Abfragepunkten hoehenlinear interpolieren, ausserhalb den naechsten Vektor konstant verwenden (13.7). |
| A-S-00048 | Geometrieabhaengige H1-Kopplungsmatrizen 32/48/64 und lokale windabhaengige rechte Seite nach 13.8 erzeugen und loesen; Normalbetrieb mit 48 Moden. |
| A-S-00049 | Winkelnaht und lokale horizontale Nullgeschwindigkeit nach den festgelegten stetigen Regularisierungen behandeln (13.8). |
| A-S-00050 | Niedrig-Re-Grenzmodell nach 13.8 einsetzen; bei exakt null weder `log(0)` noch Division durch null auswerten. |
| A-S-00051 | Gueltige Luftdatenbereiche explizit dokumentieren und fuer diese den lokalen Geschwindigkeitsbereich 0 bis 50 m/s abdecken; ungueltige Werte diagnostizieren (13.5/13.7/13.9). |
| A-S-00052 | Fuer jeden Streifen gekoppelte dimensionale Kraft und vollstaendiges Moment um den Abfragepunkt im Segelsystem liefern (13.4/13.8). |
| A-S-00053 | Einzelbeitraege, Streifen- und Gesamtlasten mit korrektem Momentenbezug und ohne Doppelzaehlung bilanziert nachweisen (13.4/13.10/13.11). |
| A-S-00054 | Das Modell im separaten Ordner `Segelmodell` als versioniertes JSON-Manifest und Parquet/Zstandard-Tabellen gemaess 13.9 erzeugen. |
| A-S-00055 | Tabellenfelder, eindeutige Schluessel, Fremdschluessel, Dimensionen, Einheiten, Matrixinhalte und Geometrieintegritaet nach 13.9 pruefen. |
| A-S-00056 | Unbekannte Versionen, ungueltige Schemata, korrupte Dateien und abweichende SHA-256-Pruefsummen beim Laden sichtbar ablehnen (13.7/13.9/13.10). |
| A-S-00057 | Vorhandene Modelle standardmaessig schuetzen und nur auf ausdrueckliche Option nach erfolgreicher Neuerzeugung ersetzen; Fehler/Abbruch nicht als Erfolg publizieren (13.7/13.9). |
| A-S-00058 | CLI-Befehl `segelmodell` fuer Erzeugung und separaten nicht schreibenden Pruefaufruf bereitstellen; stdout/Fehlerverhalten nach 13.13. |
| A-S-00059 | Versionierte typisierte C++-Bibliothek zum Laden, Abfragen und gekoppelten Berechnen nach 13.10 bereitstellen. |
| A-S-00060 | Unveraenderliche Modelle parallel sicher nutzen; Datenbesitz, Lebensdauer, Streifen-IDs und typisierte Fehler nach 13.7/13.10 nachweisen. |
| A-S-00061 | Direkte H1-Referenz fuer gueltige Gleichwindfaelle mit Faktor 4/48 im Gesamt- und Streifenvergleich innerhalb 0,1 % nach 13.7/13.11 einhalten. |
| A-S-00062 | Streifenhalbierung bei gleichem Testwindfeld fuer Gesamt- und passend summierte Streifenlasten innerhalb 5 % nach 13.7/13.11 einhalten. |
| A-S-00063 | Geschwindigkeiten 0 bis 50 m/s, Intervallmitten, Winkel, Gradienten, lokale Nullwinde, Naht und Pole nach 13.7/13.11 pruefen. |
| A-S-00064 | Den Residualtest und die 32/48/64-Modenvergleiche nach 13.11 dokumentieren; numerische Fehler nicht als Erfolg darstellen. |
| A-S-00065 | Physikalische Nichtvalidierung und fehlende Referenzen ausweisen; Messdaten nicht zur Laufzeit- oder Nutzungsbedingung machen (13.7). |
| A-S-00066 | Alten Gesamtsegel-Lookup-Generator bei der Umsetzung abloesen; H1 als Berechnungsgrundlage und Referenz erhalten (13.7/13.12). |
| A-S-00067 | Bereits vorhandene alte Lookup-Dateien und externe Modellordner nicht veraendern oder loeschen (13.7/13.12). |
| A-S-00068 | Parquet-Tabellen bei identischen Eingaben und festgelegten Bibliotheks-/Codecversionen unabhaengig von der Workerzahl bytegleich erzeugen (13.7). |
| A-S-00069 | Datenumfang, Ladezeit und Laufzeit pro Rechnung getrennt messen; keine unbelegten Leistungsgrenzen behaupten (13.7/13.11). |
| A-S-00070 | Naht- und Polverhalten, lokaler Nullwind, Grenzmodelluebergang sowie gueltige und fehlerhafte Eingaben reproduzierbar pruefen (13.8/13.11). |
| A-S-00071 | Ein globales CPU-Limit fuer die Erzeugung einhalten und geeignete GPU-Berechnungen nach dem bestehenden Ressourcenvertrag behandeln (13.13). |
| A-S-00072 | Optionalen Aerodynamik-Fingerprint ausschliesslich ueber die bestehende Core-API aktualisieren; Standard `skip`, Fehler nach Publikation sichtbar melden (13.13). |

### 13.1 Ziel und Abgrenzung

Der Segelmacher bereitet aus der Beschreibung **eines** Segels ein kompaktes
aerodynamisches Datenmodell fuer die spaetere Berechnung vor. Dieses
enthaelt Geometrie, Profilkennwerte und Informationen zur Kopplung der
Hoehenstreifen. Zur Laufzeit werden lokale scheinbare Windvektoren
verarbeitet und daraus **gekoppelt** Lasten des gesamten Segels berechnet.

Das ist eine Erweiterung von H1, keine Umformatierung der vorhandenen
Gesamtsegel-Lookups. Vollstaendige Kraftloesungen fuer alle Kombinationen
von Windprofilen werden nicht vorberechnet. Insbesondere duerfen fertige
Streifenlasten aus Gesamtsegel-Lookups mit unterschiedlichen gleichfoermigen
Windbedingungen nicht als Ersatz fuer eine gekoppelte Loesung gemischt
werden. Die Profilkennwerte sind in 13.8/13.9 festgelegt.

Der Segelmacher verantwortet die Modellerzeugung und das Verfahren fuer die
gekoppelte Streifenberechnung. Der separate Simulations-Core bestimmt den
lokalen scheinbaren Wind, ruft das Verfahren auf und bilanziert die
zurueckgegebenen Lasten im Bootsmodell. Dessen Implementierung,
Bootsbewegung, Windfelder und GUI sind nicht Bestandteil dieser Spezifikation.
Keine aerodynamische Interferenz zwischen mehreren Segeln wird zugesagt.

### 13.2 Eingaben und vorzubereitende Daten

Eingaben sind eine gueltige Segelgeometrie mit Kontur und Profilstationen
sowie die fuer das gewaehlte Verfahren ausdruecklich benoetigten Numerik-,
Modell- und Gueltigkeitsparameter. Herkunft, Modellannahmen und
Eingabestand sind im erzeugten Modell nachvollziehbar zu machen.

Das Datenmodell muss mindestens die fuer folgende Aufgaben benoetigten
Informationen bereitstellen:

- Segelidentitaet, Koordinaten- und Einheitenkonventionen, Bezugsgroessen
  einschliesslich Referenzflaeche und Referenzlaenge sowie Modellversion;
- geometrische Grenzen, stabile Zuordnung und Wind-Abfragepunkt jedes
  Streifens sowie die fuer Kraft und Moment benoetigte Streifengeometrie,
  Flaechenanteile, Profile, Woelbung und Verwindung;
- lokale Profilkennwerte und die zugehoerigen Modellparameter und
  Gueltigkeitsbereiche; bei Tabellen zusaetzlich Achsen, Zuordnung und
  Interpolationsregeln;
- vorberechnete geometrieabhaengige Kopplungsinformationen, etwa eine
  Matrix oder deren Faktorisierung, mit dokumentierter Bedeutung,
  Dimension und Zuordnung der Freiheitsgrade;
- die fuer die Bestimmung von Einzelkraeften und vollstaendigen Momenten
  erforderlichen Ansatzpunkte und Momentenparameter.

Die Kopplungsdaten sind keine vorab berechnete Kraftloesung und duerfen
nicht die Gueltigkeit einer fuer alle Windprofile konstanten Loesung
voraussetzen. Das festgelegte Verfahren und die vorzubereitenden
Tabellenfelder stehen in 13.8 und 13.9; das bestehende Lookup-Schema ist
nicht das neue Datenschema.

### 13.3 Segelkoordinaten und Streifen

Verwendet wird ein rechtshaendiges segelfestes Koordinatensystem mit Ursprung
im Segelhals. In neutraler Einbaulage zeigt +X zum Bug, +Y nach Backbord und
+Z nach oben. Das System dreht sich mit dem Segel. Alle im Datenmodell und
an der Laufzeitschnittstelle verwendeten Punkte und Vektoren sind diesem
System eindeutig zugeordnet; Laengen werden in Metern angegeben.

Die Hoehe reicht vom tiefsten Punkt der gesamten Segelkontur bis zur
hoechsten Segelspitze; beim Optimisten gehoert die Spitze an der Spriet dazu.
Fuer die vereinbarte Einteilung gilt bei positiver Hoehe H:

`N = ceil(H / 0.25 m)`, `Streifenhoehe = H / N`.

Diese Aenderung von zuvor 0,5 m wurde am 15.11.2026 ausdruecklich
beauftragt. Sie ersetzt die fruehere Einteilung, nicht die 5-%-Grenze.

Die N horizontalen Streifen sind gleich hoch und hoechstens 0,25 m hoch.
Beispiel: 4,2 m ergeben 17 Streifen zu je etwa 0,247 m. Jeder Streifen
wird mit der tatsaechlichen Kontur verschnitten; ein schraeges Unterliek
kann nur einen Teil eines Streifens enthalten. Spitzen werden nicht
abgeschnitten. Der Wind-Abfragepunkt liegt auf halber Streifenhoehe bei
einem Viertel der dortigen Sehne, vom vorderen Sehnenende aus gemessen.
Streifen-ID, Hoehenbereich und Abfragepunkt muessen eindeutig exportiert
werden.

Die Core-Rueckmeldung weist auf moegliche Probleme bei stark schraegem
Unterliek und dem Sehnenschnitt in Streifenmitte hin. Eine schraege Kante
allein belegt noch keinen fehlenden Schnitt: Das ist anhand der tatsaechlich
zulaessigen Konturen und der Hoehenfestlegung zu pruefen. Fehlt eine
eindeutig bestimmbare Sehne, ist die Modellerzeugung mit eindeutiger
Diagnose abzubrechen; ein stiller Ersatzpunkt ist nicht zulaessig.
Degenerierte oder mehrteilige Schnitte und ihre Pruefbarkeit sind in der
Verfahrensausarbeitung zu beruecksichtigen. Die bisherige H1-Implementierung
verlangt fuer ihren Horizontalschnitt ein zusammenhaengendes Intervall.

Das aktuelle H1-Raster fuegt Kontur- und Profilstationshoehen als Grenzen
ein und unterscheidet sich damit vom vereinbarten gleichmaessigen Raster.
Kontur- und Profilverlaeufe duerfen **innerhalb** eines festen
Ausgabestreifens fuer die Integration an Konturknicken und Profilstationen
intern unterteilt werden. Ausgabestreifen und Wind-Abfragepunkte bleiben
unveraendert. Die Umsetzung und ihr Nachweis stehen noch aus.

### 13.4 Laufzeitvertrag aus Sicht des Segelmachers

Das vorbereitete Modell und das in 13.8 festgelegte Berechnungsverfahren
nehmen fuer jeden exportierten Streifen einen scheinbaren lokalen
Stroemungsvektor in m/s entgegen, der **in Stroemungsrichtung** zeigt und
im Segelsystem angegeben ist. Hinzu kommen Luftdichte und kinematische
Viskositaet mit eindeutig dokumentierten SI-Einheiten. Der aufrufende Core
ist fuer die Beruecksichtigung von Windprofil und Bewegungen verantwortlich;
der Segelmacher berechnet daraus nicht selbst das Boots-Windfeld.

Die Ausgabe umfasst fuer jede Streifen-ID eine Kraft in N und das
vollstaendige Moment in Nm um den zugehoerigen Wind-Abfragepunkt, jeweils
im Segelsystem. Die Lasten ergeben sich aus der gekoppelten Loesung fuer
das gesamte Segel, nicht aus unabhaengigen Streifenrechnungen.

Optionale Einzelbeitraege fuer die Darstellung sind getrennt von den
Streifensummen gekennzeichnet. Bei einer Bilanz duerfen Streifensummen und
die darin enthaltenen Einzelbeitraege nicht doppelt addiert werden.
Fuer einen anderen Bezugspunkt O gilt mit Abfragepunkt P:
`M_O = M_P + (P - O) x F`, nach einer einheitlichen Koordinatentransformation.

Der neue Daten- und Laufzeitvertrag ist gesondert zu versionieren.
Datenschema und Fehlerkategorien stehen in 13.9/13.10; konkrete
Funktionssignaturen werden vor ihrer Implementierung festgelegt und
registriert. Die Core-Bestaetigung bezieht sich auf die Zielrichtung,
nicht auf bereits vereinbarte konkrete Signaturen. Der vorhandene
oeffentliche H1-Vertrag wird hier nicht veraendert.

### 13.5 Windbereich, Modellgrenzen und Fehler

Ziel ist die vollstaendige Unterstuetzung lokaler scheinbarer
Windgeschwindigkeiten von 0 bis 50 m/s. Bei ueberall exakt null Wind sind
im quasistationaeren Modell alle Kraefte und Momente null. Bei lokalem
Nullwind einzelner Streifen ist der Nullvektor als Eingabe vorzusehen; die
gesamte gekoppelte Loesung und ihr Grenzuebergang sind nachzuweisen. Ein
Nullvektor am Abfragepunkt bedeutet nicht automatisch eine verschwindende
Streifenlast: An anderen Integrationspunkten des Streifens kann nach der
Windinterpolation Wind wirken. Am einzelnen Integrationspunkt mit exakt
null lokalem Wind ergibt die Regularisierung aus 13.8 jedoch auch null
induzierte Geschwindigkeit und null lokalen Kraft- und Momentenbeitrag.
Dies ist eine Modellgrenze und kein Nachweis, dass dort real keine
Kopplungslast auftreten kann. Falls eine Berechnung nicht
moeglich ist, muss sie sichtbar diagnostiziert werden; dies ersetzt nicht
den geforderten Nachweis des Zielbereichs.

Reynolds-Zahlen sind fuer das zu spezifizierende Verfahren aus lokaler
Geschwindigkeit, passender Bezugslaenge und Viskositaet abzuleiten. Fuer
verwendete Tabellen erfolgt Interpolation ausschliesslich innerhalb der
manifestierten Gueltigkeitsbereiche, zwischen Reynolds-Stuetzstellen
linear in `log(Re)`. Ausserhalb wird diagnostiziert, nicht geklemmt oder
extrapoliert. Fuer `Re = 0` ist keine `log(Re)`-Interpolation definiert.
Wie Null und der Bereich nahe null ohne unzulaessige Extrapolation
behandelt werden, ist als Teil des Modells nachzuweisen.

Das bestehende H1 verlangt lokale Reynolds-Zahlen im expliziten Modellbereich
und kann damit nicht unveraendert die geforderte Umgebung von null
abdecken. Ein Grenzmodell ist in 13.8 festgelegt; seine
Stetigkeit, Grenzen und physikalische Eignung sind noch nachzuweisen.
Ungueltige Eingaben, fehlende oder inkonsistente Streifenvektoren,
unbekannte oder beschaedigte Modelle, numerische Fehler und nicht
konvergente Loesungen duerfen kein erfolgreiches Ergebnis vortaeuschen.

### 13.6 Erzeugung und Nachweise

Der Generator prueft Geometrie, Parameter und Modellvoraussetzungen,
bereitet die Daten vor und dokumentiert ihre Zuordnung, Einheiten,
Gueltigkeit, Version und den Nachweisstand. Ein unvollstaendiges oder
fehlerhaftes Modell darf nicht als erfolgreich veroeffentlicht werden.
Format, Integritaet, Speicherung und Veroeffentlichungsschutz sind in
13.7 und 13.9 festgelegt; ihre praktische Umsetzung und Pruefung stehen aus.

Verbindliche Abnahme nach den festgelegten Verfahren und Testfaellen:

| Nachweis | Besprochener Zielwert / Inhalt |
|---|---|
| Gleichfoermiger Wind | Reproduktion des direkten H1-Ergebnisses innerhalb 0,1 %, soweit H1 gueltig ist; Gesamt- und vergleichbare Streifenlasten nach Abschnitt 13.7. |
| Hoehenraster | Jeden Streifen halbieren; Gesamtlasten und vergleichbare Streifenlasten hoechstens 5 % Abweichung. Je zwei feine Streifen fuer den Vergleich zusammenfassen und ihre Momente zuvor auf denselben Punkt uebertragen. |
| Bezugs- und Absoluttoleranzen | Vektornorm, maximaler Staudruck und jeweilige Referenzflaeche, fuer Momente zusaetzlich `c_ref`; Prozenttoleranz plus 10^-9 N beziehungsweise 10^-9 Nm nach Abschnitt 13.7. |
| Windbereich | 0; 0,001; 0,01; 0,1; 0,5; 1; 5; 10; 20; 30; 40; 50 m/s sowie Zwischenwerte, Windgradienten und unterschiedliche Richtungen ueber die Hoehe. Mindest-Testsegel, Winkel und Zwischenwerte siehe Abschnitt 13.7. |
| Null und Fehler | Vollstaendiger Nullwind, lokaler Nullwind, Grenzuebergaenge und erwartete Diagnosen; keine stillen Ersatzwerte. |
| Kraft und Moment | Einzelbeitraege, Streifensummen und Gesamtresultierende stimmen nach Bezugspunkttransformation ueberein; keine Doppelzaehlung. |
| Datenmodell | Erzeugung, versionierter Lesezugriff, Zuordnung, Datenintegritaet und sichtbare Ablehnung ungueltiger Daten; konkrete Schema- und Formatpruefungen nach deren Festlegung. |

Fuer Optimist, 420 und Folkeboot sind die 0,1-%-H1-Vergleiche im
gueltigen H1-Bereich sowie die 5-%-Rasterhalbierung anhand archivierter,
hashverknuepfter Berichte bestanden; die vollstaendige Matrix und ihre
Grenzquotienten sind im
[Umsetzungsplan](../Umsetzungsplaene/Entwurf_Lookup_Generator.md)
dokumentiert. Die Berichte enthalten keinen unabhaengigen Hash des
damaligen Quellstands. Fuer 420- und Folkeboot-Fock liegen
Stichprobentests, keine vollstaendige Matrix vor. Gleichfoermiger Wind und
numerische Konvergenz pruefen die Uebereinstimmung bzw. Stabilitaet, sie
sind keine physikalische Validierung. Die H1-Berechnungszeit von
59,55 Mikrosekunden pro Basisaufruf aus der Opti-Messung gilt nicht als
Laufzeit dieses neuen Verfahrens. Datenumfang und Simulationslaufzeit
koennen erst nach Festlegung und Messung vergleichbar beurteilt werden.

### 13.7 Nutzerentscheidungen vom 15.11.2026

Die folgenden Nutzerentscheidungen sind Bestandteil der finalisierten
Spezifikation, aber kein Nachweis einer technischen Erfuellung und kein
Implementierungsauftrag:

- **Umfang und Bestand:** Der bisherige Gesamtsegel-Lookup und seine
  Erzeugung sollen bei einer spaeter beauftragten Umsetzung entfallen.
  H1 bleibt Ausgangspunkt fuer das erweiterte Berechnungsverfahren und
  Vergleichsreferenz. Bereits erzeugte alte Lookup-Dateien bleiben
  unangetastet, auch in externen Modellordnern. Die bisherigen
  freigegebenen Anforderungen sind in 13.12 nachvollziehbar zugeordnet
  und bleiben als Historie erhalten.
- **Kopplung:** H1 fuer unterschiedliche lokale Windvektoren erweitern.
  Das konkrete Verfahren und ein begruendetes stetiges Grenzmodell fuer
  kleine Reynolds-Zahlen sind auszuarbeiten und nachzuweisen. Bei fehlender
  Konvergenz wird ein Fehler gemeldet; ein bislang fehlender Nachweis fuer
  den geforderten Bereich wird nicht als Erfolg dargestellt.
- **Wind zwischen Abfragepunkten:** Die vom Core gelieferten Windvektoren
  werden nach der Hoehe linear interpoliert. Oberhalb des obersten und
  unterhalb des untersten Abfragepunkts wird jeweils dessen Windvektor
  konstant verwendet.
- **Ausgabe und Veroeffentlichung:** Das neue Modell liegt im eigenen
  Unterordner `Segelmodell` neben der Auftragsdatei. Es besteht aus einem
  versionierten JSON-Manifest und Parquet-Tabellen mit Zstandard und
  endlichen binary64-Werten. Manifest und Datenvertrag nennen
  Modell-/Eingabeversionen, Bibliotheks-/Codecversionen und Pruefsummen
  aller Parquet-Dateien. Unbekannte Versionen oder fehlerhafte Pruefsummen
  werden abgelehnt. Bei gleichen Eingaben und festgelegten Bibliotheks-
  versionen sind die Tabellen unabhaengig von der Workerzahl bytegleich.
  Vorhandene Modelle werden standardmaessig nicht ueberschrieben und nur
  auf ausdrueckliche Option nach erfolgreicher Neuerzeugung ersetzt.
  Der vorgesehene CLI-Befehl heisst `segelmodell`; ein separater
  Pruefaufruf schreibt keine Ergebnisse.
- **Gueltigkeit und Profilkennwerte:** Zulaessige Bereiche von Luftdichte
  und kinematischer Viskositaet werden im Erzeugungsauftrag explizit
  festgelegt und im Modell dokumentiert; Werte ausserhalb werden
  diagnostiziert. Die Abdeckung 0 bis 50 m/s ist innerhalb der
  festgelegten Bereiche nachzuweisen. Fuer tabellierte Profilkennwerte
  werden Stuetzstellen adaptiv bestimmt und gegen direkte Rechnungen
  geprueft. Das Gesamtziel von 0,1 % ist massgeblich; interne
  Teiltoleranzen werden begruendet festgelegt. Eine Winkelachse wird
  periodisch ueber die -180/180-Grad-Naht interpoliert und dort geprueft,
  ohne Lasten zu spiegeln. Die log(Re)-Regel aus 13.5 bleibt bestehen.
- **Laufzeitschnittstelle:** Eine versionierte, typisierte C++-Bibliothek
  stellt die Rechnung bereit. Ein geladenes Modell ist unveraenderlich
  und fuer parallele Aufrufe mit eigenen temporaeren Rechendaten geeignet.
  Streifen-IDs sind innerhalb eines Modells eindeutig und bei identischer
  Neuerzeugung stabil; fuer geaenderte Geometrie wird keine Stabilitaet
  zugesagt. Typisierte Fehler enthalten Ursache und Kontext; ein Fehler
  darf kein erfolgreiches Teilergebnis liefern. Eine ABI-Stabilitaet
  ueber unterschiedliche Compiler und Standardbibliotheken wird nicht
  zugesagt.
- **Abnahme und Bezugsgroessen:** Fuer den H1-Vergleich gelten direkte
  H1-Rechnungen mit Rasterfaktor 4 und 48 Traglinienmoden. Gesamtlasten
  und vergleichbare Streifenlasten werden geprueft. Die Referenz wird an
  den Ausgabestreifengrenzen aufgeteilt und die Referenzkonvergenz
  nachgewiesen. Beim Vergleich grober und halbierter Streifen wird dasselbe
  definierte Windfeld an den jeweiligen Abfragepunkten ausgewertet.
  Fuer Kraft- und Momentenabweichungen wird jeweils die euklidische Norm
  der Vektordifferenz verwendet. Als Bezuege gelten `q_max * S` fuer
  Gesamtlasten und `q_max * S_i` fuer Streifenlasten; fuer Momente jeweils
  zusaetzlich `c_ref`. `q_max` ist der maximale lokale Staudruck des
  Vergleichsfalls, `S_i` die jeweilige Streifenflaeche. Zulaessig ist
  jeweils Prozentgrenze mal Bezug plus `10^-9 N` beziehungsweise
  `10^-9 Nm`; dies gilt fuer 0,1 % im H1-Vergleich und 5 % im
  Rastervergleich.
- **Testumfang:** Mindestens die vorhandenen rekonstruierten Segel von
  Optimist, 420 und Folkeboot. Fuer gleichfoermigen Wind werden Azimute
  von -180 bis +150 Grad in 30-Grad-Schritten und Neigungen
  -90, -80, -60, -30, 0, 30, 60, 80 und 90 Grad sowie Zwischenpunkte an
  Modelluebergaengen geprueft. Zusaetzlich zu den Geschwindigkeiten aus
  13.6 ist die arithmetische Mitte jedes benachbarten Intervalls zu
  testen. Mindestfaelle fuer Gradienten sind 1 bis 10 m/s ueber die Hoehe,
  -45 bis +45 Grad bei konstantem Betrag, lokaler Nullwind sowie ein
  kombinierter Geschwindigkeits- und Richtungsgradient; Gradienten werden
  auch mit umgekehrter Richtung geprueft. Vergleiche mit H1 gelten nur,
  soweit H1 fuer den jeweiligen Fall gueltig ist.
- **Validierung und Leistung:** Fehlende physikalische Validierung mit
  Messdaten ist sichtbar zu kennzeichnen, aber fuer sich genommen kein
  Nutzungsblocker; die numerischen Pruefungen bleiben verpflichtend.
  Datenumfang, Ladezeit und Laufzeit pro Berechnung werden gemessen;
  verbindliche Leistungsgrenzen werden erst anhand dieser Ergebnisse
  festgelegt.

Die nachfolgenden technischen Festlegungen sind verbindliche
Umsetzungsgrundlage, aber keine bereits nachgewiesenen Eigenschaften.

### 13.8 Gekoppeltes Verfahren und Nullgrenzmodell

**Status: festgelegt, fachlich und numerisch nicht nachgewiesen.** Ausgangspunkt ist
die H1-Traglinie aus der [Modellfestlegung](../Dokumentationen/Modellfestlegung_Lookup.md).
Die lineare Kopplungsmatrix ist geometrieabhaengig; die rechte Seite und
die Lasten werden fuer jeden Windzustand neu berechnet. Das folgende
Verfahren ist als konkrete Erweiterung zu pruefen, nicht als validierte
Beschreibung realer Stroemungen zu verstehen.

1. Das Modell speichert fuer `N = 32, 48, 64` die H1-Matrizen
   `M[i,n] = sin(n*theta_i) * (4*H/(2*pi*c_i) + n/sin(theta_i))`,
   `theta_i = pi*(i+0.5)/N`, `i = 0..N-1`, `n = 1..N`.
   Die Hoehe ist `z_i = z_min + H*(1-cos(theta_i))/2`, `c_i` die
   dortige Sehne. Ungueltige oder entartete Kollokationsgeometrie fuehrt
   zur Diagnose. Normale Rechnung: 48 Moden; 32/64 dienen
   Vergleichsrechnungen.
2. Der lokale Stroemungsvektor wird nach 13.7 nach Hoehe interpoliert.
   Fuer jeden Kollokationspunkt gilt `V_i = hypot(u_x,u_y)` in m/s.
   Bei `V_i > 0` sei `phi_i = atan2(u_y,-u_x)` und
   `g_i = 1-smoothstep(120 Grad,180 Grad,abs(phi_i))`. Es wird
   `b_i = V_i * g_i * (phi_i + twist_i_rad - a0_i)` gebildet, mit
   Winkelumrechnung in Radiant und denselben Fourierkonventionen wie H1.
   Die Abblendung ist eine neue Annahme fuer gegenlaeufige lokale Winde:
   Sie verhindert einen Sprung der rechten Seite an der +/-180-Grad-Naht.
   Bei `V_i = 0` ist `b_i = 0`; es wird kein Winkel aus dem Nullvektor
   bestimmt. Wie in H1 entfaellt die Traglinienloesung, wenn der
   anliegende Anteil ueberall null ist.
3. Die dimensionale Gleichung `M * B = b` wird mit pivotierter
   Faktorisierung geloest; `B_n` hat die Einheit m/s. Fuer die
   modale Zirkulation und induzierte skalare Geschwindigkeit gilt
   `Gamma(z) = 2*H*sum(B_n*sin(n*theta))` in m^2/s und
   `w(z) = sum(n*B_n*sin(n*theta)/sin(theta))` in m/s.
   Die Auswertung an den Polen selbst ist gesondert grenzwertig zu
   behandeln; es darf nicht durch null geteilt werden.
4. Fuer die induzierte Richtung wird die horizontale lokale Richtung
   `n = (u_y,-u_x,0)/V_xy` verwendet. Bei unterschiedlichen Richtungen
   ist dies eine **neue Modellannahme**, keine aus H1 bewiesene
   Wirbelgeometrie. Zur stetigen Behandlung lokal verschwindender
   Horizontalgeschwindigkeit gilt mit `V_max = max_i(V_i)` ueber alle
   Abfrage- und Kollokationspunkte und `epsilon = 0.01*V_max`:
   fuer `V_xy >= epsilon` unveraendert `n`; fuer
   `0 < V_xy < epsilon` wird `n` mit `smoothstep(V_xy/epsilon)`
   multipliziert; bei `V_xy = 0` ist die Richtung null. Sind alle
   Horizontalgeschwindigkeiten null, entfaellt die Traglinienloesung.
   Die induzierte Geschwindigkeit lautet `u_ind = -w*n_reg`. Diese
   Regularisierung ist Teil der Modellfestlegung und muss auch bei
   wechselnden Windrichtungen und Grenzuebergaengen geprueft werden.
5. Lokale Wind- und induzierte Geschwindigkeiten, lokale Reynolds-Zahl und
   H1-Anteile fuer anliegende Stroemung, Abloesung, Flaechenimpuls,
   Reibung und Profilmoment ergeben die dimensionierten Teilkraefte.
   Wie bei H1 gilt fuer die Profilkennwerte
   `Re = |u| * chord / nu` mit dem interpolierten, nicht induzierten
   lokalen Stroemungsvektor; `u_ind` geht in die gebundene Kraft ein,
   nicht in diese Reynolds-Zahl.
   Die bestehende H1-Ueberblendung bei `abs(beta) = 60..80` Grad ist
   lokal anzuwenden. Die gebundene Kraft wird mit der dimensionalen
   Zirkulation und `rho * Gamma * dz * cross(u + u_ind, span)`
   berechnet; H1-Gewichtung und Vorzeichenkonvention bleiben erhalten.
   Integration und Momentenbildung nutzen die realen Ansatzpunkte.
   Es wird kein einzelner globaler Staudruck fuer das ganze Windprofil
   unterstellt.

Fuer alle oben genannten Ueberblendungen bedeutet `smoothstep(a,b,x)`
mit `t = clamp((x-a)/(b-a),0,1)` den Wert `t*t*(3-2*t)`;
`smoothstep(t)` bezeichnet dieselbe Funktion auf dem Intervall `[0,1]`.
Dieses Klemmen definiert ausschliesslich die Ueberblendungsfunktion und
ist kein Klemmen von Eingaben auf einen gueltigen Modellbereich.

Bei gleichfoermigem Wind mit nicht verschwindender
Horizontalgeschwindigkeit und `abs(phi) <= 120 Grad` gilt
`b = V_xy * b_H1` und damit `B = V_xy * A_H1`; die Regularisierung ist
dort inaktiv. Bei den derzeitigen H1-Parametergrenzen ist fuer
gleichfoermigen Wind jenseits von 120 Grad der anliegende Anteil null,
so dass dort wie in H1 keine Traglinienloesung entsteht. Beides ist bei
der Referenzpruefung ausdruecklich nachzuweisen. Diese algebraische
Voraussetzung fuer die H1-Reproduktion ersetzt nicht den Vergleich aller
ausgegebenen Kraft- und Momentenbeitraege. Insbesondere sind die
Winkelnaht, gegenlaeufige lokale Winde, die Polnaehe und lokale
Nullgeschwindigkeit auf Stetigkeit und Verfahrensgrenzen zu pruefen.

**Festlegung fuer `0 <= Re < Re_min`:** `Re_min` und alle uebrigen
H1-Parameter sind explizite Modelldaten. Bei `0 < Re < Re_min` ist
eine eigene analytische Niedrig-Re-Schliessung zu verwenden, keine
Tabellenextrapolation: mit `r = Re/Re_min` gilt
`Cf_low = Cf_H1(Re_min)/sqrt(r)` und fuer die H1-Abrissverschiebung
`delta_low = shift * (log10(Re_min/transition_re) + (r-1)/ln(10))`.
Bei `Re >= Re_min` gelten die unveraenderten H1-Formeln. Die Formeln
schliessen stetig an die jeweiligen H1-Werte an; die Abrissverschiebung
hat dort auch dieselbe erste Ableitung. Fuer den Reibungsbeitrag ist die
**dimensionale** Kombination mit dem quadratischen Staudruck auszuwerten:
Sie geht bei festem Profil und `|u| -> 0` gegen null. Bei exakt null
lokalem Wind sind die vom lokalen Wind angetriebenen Impuls- und
Reibungsbeitraege null; `log(0)` und `1/sqrt(0)` werden nicht
ausgewertet. Die gekoppelte gebundene Last wird separat gemaess der
Gesamtloesung behandelt.

Das ist eine explizite Ingenieurschliessung, keine Behauptung der
Gueltigkeit der H1-Reibungsformel im Kriechstroemungsbereich. Ob sie
physikalisch plausibel und ausreichend stetig ist und ob sie alle
vereinbarten 0--50-m/s-Faelle sowie Toleranzen erfuellt, ist offen.
Bei ungueltiger Luftdichte/Viskositaet, nicht-endlichen Eingaben,
singulaerer Matrix, nicht-endlicher Loesung oder fehlendem Nachweis darf
kein erfolgreiches Ergebnis vorgetaeuscht werden.

### 13.9 Datenmodell und Erzeugung

Ausgabe im Ordner `Segelmodell`, unabhaengig vom bisherigen `LookUp`.
Alle aufgelisteten Felder sind Pflichtfelder; `f64` bezeichnet endliches
IEEE-binary64, `i32` einen nichtnegativen 32-Bit-Integer und `text`
UTF-8. Vektoren werden als getrennte x/y/z-Spalten gespeichert. Die
folgenden Namen und Typen bilden die neue Formatversion `1`; sie ist
unabhaengig von der alten Lookup-Manifestversion `3`.

| Datei | Schluessel und Daten |
|---|---|
| `manifest.json` | Formatkennung `segelmodell`, separate Schema- und Modellversion, Segel-ID, `S_m2`, `c_ref_m`, `z_min_m`, `hoehe_m`, Achsen- und Stroemungskonvention, H1- und Grenzmodellparameter, Dichte-/Viskositaetsbereiche, Geschwindigkeitsbereich, Anzahl der Streifen, Modenzahlen und Tabellenbeschreibungen mit Zeilenzahl, Schema, Codec und SHA-256. Eingabe- und Geometriehashes, Erzeuger-/Bibliotheksversionen sowie Numerik- und Validierungsstatus sind enthalten. |
| `streifen.parquet` | `streifen_id:i32`, `z_min_m:f64`, `z_max_m:f64`, `flaeche_m2:f64`, `abfrage_x_m:f64`, `abfrage_y_m:f64`, `abfrage_z_m:f64`; eindeutig nach `streifen_id` sortiert. Die Flaeche ist die tatsaechliche ebene Konturflaeche im Streifen, nicht eine Rechtecknaeherung. |
| `quadratur.parquet` | `streifen_id:i32`, `punkt_id:i32`, `z_m:f64`, `dz_m:f64`, `chord_m:f64`, `m:f64`, `p:f64`, `twist_rad:f64`, `a0:f64`, `a1:f64`, `a2:f64`, `viertel_x_m:f64`, `viertel_y_m:f64`, `viertel_z_m:f64`; Schluessel `(streifen_id,punkt_id)`. Zeilen beschreiben interne Hoehenintegrationspunkte und ihre H1-Fourier-Profilkennwerte. |
| `flaechenpunkte.parquet` | `streifen_id:i32`, `punkt_id:i32`, `teil_id:i32`, `position_x_m:f64`, `position_y_m:f64`, `position_z_m:f64`, `normal_x:f64`, `normal_y:f64`, `normal_z:f64`, `flaeche_m2:f64`; Schluessel `(streifen_id,punkt_id,teil_id)`. Fuer jeden Hoehenpunkt werden die vier H1-Sehnenbeitraege mit Flaechenmetrik und Einheitsnormalen gespeichert. |
| `kollokation.parquet` | `moden:i32`, `index:i32`, `z_m:f64`, `chord_m:f64`, `twist_rad:f64`, `a0:f64`; Schluessel `(moden,index)`, fuer jede der drei Modenzahlen vollstaendig und in Indexreihenfolge. |
| `kopplung.parquet` | `moden:i32`, `zeile:i32`, `spalte:i32`, `wert:f64`; Schluessel `(moden,zeile,spalte)`. Fuer jede Modenzahl wird die vollstaendige dimensionslose Matrix aus 13.8 in Zeilenreihenfolge abgelegt. Pivotierung und Faktorisierung entstehen beim Laden aus dieser Matrix; sie sind keine Kraftloesung. |

`punkt_id` ist je Streifen eindeutig. Jeder Flaechenpunkt verweist auf
genau einen Quadraturpunkt; `teil_id` ist 0 bis 3. Hoehenintervalle sind
geordnet, lueckenlos und ueberlappungsfrei; Abfragepunkte liegen in der
jeweiligen Streifenmitte. Die Summe der ebenen Streifenflaechen ergibt
`S_m2` innerhalb einer explizit dokumentierten numerischen
Flaechentoleranz `1e-10 * max(1 m^2, S_m2)`. Einheitsnormalen haben
eine Betragstoleranz von `1e-10`. Reale gekruemmte Teilflaechen aus der H1-Metrik sind
davon zu unterscheiden. Fuer Matrizen sind Dimensionen und Indizes
vollstaendig zu pruefen; bei doppelten Schluesseln, fehlenden Referenzen,
nicht-endlichen Zahlen, nicht normierten Normalen, ungueltigen
Sehnen/Flaechen, fremden Versionen oder beschaedigten Dateien wird
abgebrochen.

Das Manifest ist ein UTF-8-JSON-Objekt mit folgenden Pflichtfeldern:
`formatkennung = "segelmodell"`, `formatversion = "1"`,
`modell_version = "H1-lokal-1"`, `segel_id` und die Objekte
`geometrie`, `konventionen`, `gueltigkeit`, `parameter`, `tabellen`,
`eingaben`, `erzeuger` und `nachweise`. `geometrie` enthaelt
`S_m2`, `c_ref_m`, `z_min_m`, `hoehe_m`, `streifenanzahl` und
`modenzahlen = [32,48,64]`; `konventionen` beschreibt das segelfeste
System und den Stroemungsvektor. `gueltigkeit` enthaelt
`geschwindigkeit_m_s = [0,50]`, `dichte_kg_m3` und `viskositaet_m2_s`
als jeweils aufsteigende geschlossene Bereiche mit endlichen Grenzen;
die Grenzen von Dichte und Viskositaet sind positiv. `parameter`
enthaelt alle H1-Parameter unter den Namen des Erzeugungsauftrags und
die in 13.8 festgelegten Regularisierungsparameter mit Einheiten.
`tabellen` ist ein Objekt, dessen Schluessel genau die fuenf in der
Tabelle genannten Dateinamen sind. Jeder Wert enthaelt `zeilen` als
nichtnegative Ganzzahl, `schema_version = "1"`, `codec = "ZSTD"`,
`sha256` als 64 hexadezimale Zeichen und `schema` als geordnete Liste
von Objekten mit `name`, `typ` und `einheit`; dimensionslose Felder
haben `einheit = "1"`. Die Eintraege stimmen mit den tatsaechlichen
Parquet-Spalten und der Tabelle oben ueberein. `eingaben` enthaelt fuer
jede verwendete Eingabedatei Pfad und SHA-256, `erzeuger` die konkreten
Werkzeug- und Bibliotheksversionen. `nachweise` weist ausgefuehrte,
bestandene, fehlgeschlagene und nicht ausgefuehrte Pruefungen getrennt
aus und kennzeichnet `physikalisch_validiert = false`.

Der Auftrag legt Dichte- und Viskositaetsbereiche vor der Erzeugung
explizit fest. Fuer die konkreten Quadratur- und Kollokationssehnen wird
der worst case `Re = 50 m/s * chord_m / nu_min` geprueft. Ein Auftrag,
der den expliziten oberen Modellbereich ueberschreitet, wird vor der
Publikation diagnostiziert statt den 0--50-m/s-Bereich still einzuengen.

Das Modell verwendet die analytischen H1-Profilkennwerte
aus `quadratur.parquet` und benoetigt **keine** Winkel-/Re-Polartabelle.
Sollte eine Tabelle spaeter fuer eine andere Schliessung erforderlich sein,
ist deren Schema und adaptiver Fehlernachweis vor Verwendung gesondert
festzulegen; die vereinbarte periodische Winkel- und log(Re)-Interpolation
aus 13.7 gilt dann. Aus dem Fehlen einer Tabelle folgt keine Berechtigung
zur Extrapolation von Tabellenwerten.

Erzeugung: Eingaben pruefen, Kontur in gleich hohe Ausgabestreifen schneiden,
Quadratur an Kontur- und Profilgrenzen intern unterteilen, Profilkennwerte
und Matrizen erzeugen, Schema/Referenzen/Hashes und die geltenden
Erzeugungspruefungen validieren. Erst nach Erfolg wird das Ergebnis aus
einem eigenen Arbeitsordner veroeffentlicht; Abbruch oder Fehler ergeben
kein erfolgreiches Modell. `create-only` ueberschreibt nichts, ein
ausdruecklicher Austausch bewahrt das alte Modell bis zum erfolgreichen
Austausch. Konkrete CLI-Flags und Plattformdetails folgen dem bestehenden
Fehler- und Schutzvertrag, soweit auf dieses neue Produkt anwendbar.

### 13.10 C++-Laufzeitschnittstelle

Die folgenden Bezeichnungen beschreiben fachliche Operationen, keine
konkreten C++-Exporte oder ABI-Signaturen:

- `load_model(path)` prueft Schema, Version, Referenzen, Pruefsummen,
  gleichmaessige Streifen nach 13.3 und Matrixloesbarkeit und liefert
  einen unveraenderlichen Modellbesitzer. Aeltere Versuchsdaten mit der
  ersetzten 0,5-m-Einteilung werden nicht als aktuelles Modell geladen.
  Dateizugriffe finden beim Laden statt, nicht bei der einzelnen Rechnung.
- `strip_points(model)` liefert fuer die Modelllebensdauer gueltige
  Streifen-IDs und Abfragepunkte in m im segelfesten System. Fuer ein
  neu erzeugtes Modell gilt die Einteilung `ceil(H / 0.25 m)` aus 13.3.
- `solve(model, winds, rho, nu)` verarbeitet genau einen endlichen
  Stroemungsvektor in m/s pro vorhandener Streifen-ID, Dichte in kg/m^3
  und kinematische Viskositaet in m^2/s. Doppelte, fehlende oder unbekannte
  IDs und Werte ausserhalb des manifestierten Bereichs sind Fehler.
  Reihenfolge der Eingaben ist nicht massgeblich; die Ausgabe ist nach ID
  geordnet.
- Pro Streifen liefert die Rechnung einen Kraftvektor in N und den
  vollstaendigen Momentenvektor in Nm um den zugehoerigen Abfragepunkt.
  Ergebnisdaten gehoeren dem Aufrufer. Eine getrennte optionale Ausgabe
  von Einzelbeitraegen enthaelt ID, Kraft, Ansatzpunkt und freies Moment;
  Streifensummen und Einzelbeitraege duerfen nicht doppelt bilanziert werden.
- Fehler sind typisiert: ungueltige Eingabe/ID, ungueltiges oder
  inkompatibles Modell, Integritaets-/Lesefehler, ausserhalb des
  Gueltigkeitsbereichs und numerischer Loesungsfehler. Der Fehler traegt
  Ursache und Kontext, insbesondere betroffene ID und Wert/Bereich soweit
  vorhanden. Im Fehlerfall gibt es kein erfolgreiches Teilergebnis.
  Ein unveraenderliches Modell darf parallel genutzt werden; jede Rechnung
  besitzt ihre temporaeren Daten. Der Core ist fuer die Transformation
  und Bilanzierung um den Bootsmomentenbezug verantwortlich.

Die konkreten C++-Signaturen, Typnamen und die Ownership-Darstellung sind
vor ihrer Implementierung anhand dieses Vertrags festzulegen und mit
eigenen CPPI-Kennungen in Register, Header und Vertragstests konsistent zu
fuehren. Die Core-Rueckmeldung vom 15.11.2026 bestaetigt die Zielrichtung,
nicht bereits konkrete Signaturen. Keine ABI-Zusage ueber unterschiedliche
Compiler und Standardbibliotheken.

### 13.11 Praezisierung der Nachweisfaelle

- H1-Vergleich: fuer jeden in 13.7 bezeichneten Winkel- und
  Geschwindigkeitsfall, dessen **alle** fuer H1 benoetigten lokalen
  Reynolds-Zahlen in seinem Bereich liegen, werden direkte H1- und neue
  Rechnung verglichen. Vorzeichen, Profile und Ausgabestreifen muessen
  identisch sein; die H1-Referenz wird an den Ausgabestreifengrenzen
  nachintegriert und deren Konvergenz belegt. Ausserhalb des H1-Bereichs
  wird kein H1-Vergleich behauptet; diese Faelle erhalten gesonderte
  Grenzmodelltests.
- Rastervergleich: an allen Abfragepunkten beider Raster denselben
  definierten kontinuierlichen Testwind auswerten. Je zwei feine
  Streifenlasten mit vollstaendiger Momentenverschiebung auf den
  Abfragepunkt des groben Streifens zusammenfassen. `q_max` ist das
  Maximum von `0.5*rho*|u|^2` ueber alle Abfragepunkte **beider** Raster.
  Die in 13.7 festgelegte Vektornorm und additive absolute Toleranz gelten
  auch bei `q_max = 0`; es wird keine Division durch null verwendet.
- Testgradienten aus 13.7 sind linear ueber die Hoehe zu definieren;
  Richtungswerte beziehen sich bei beta 0 auf die H1-Windherkunft. Die
  umgekehrten Faelle vertauschen obere und untere Endwerte. Lokaler
  Nullwind wird an mindestens einem, nicht an allen Abfragepunkten
  vorgegeben. Gegenlaeufige und den Azimut-Schnitt kreuzende Winde,
  lokale horizontale Nullgeschwindigkeit bei vertikalem Wind sowie beide
  Pole und beide Ueberblendungsgrenzen sind zusaetzlich zu testen.
- Fuer die reproduzierbare Mindestmatrix `rho = 1.225 kg/m^3` und
  `nu = 1.5e-5 m^2/s` verwenden; der Testauftrag muss beide Werte in
  seinen expliziten Bereichen enthalten. Die Gradienten reichen ueber
  die gesamte Konturhoehe: Betrag 1 nach 10 m/s bei Azimut 0 Grad;
  Herkunftsazimut -45 nach +45 Grad bei Betrag 10 m/s; kombiniert beide
  Verlaeufe. Die jeweils umgekehrten Faelle vertauschen die Endwerte.
  Fuer lokalen Nullwind ein stetiges, stueckweise lineares Feld durch
  die groben Abfragepunkte definieren: an einem inneren Punkt null, an
  den anderen jeweils 5 m/s bei Azimut 0 und beta 0; beide Raster
  werten dieses selbe Feld aus. Bei nur einem Streifen einen Nullpunkt
  auf einem der beiden Konturenden waehlen.
- Winkeluebergaenge bei +/-60 und +/-80 Grad Neigung, +/-120 Grad
  `phi`, der +/-180-Grad-Naht sowie an den jeweils berechneten H1-
  Abrissgrenzen exakt und beidseits mit `1e-6` Grad Abstand pruefen.
  Die Grenze `Re_min` zusaetzlich bei relativen Abstaenden `1e-6`
  untersuchen. Modellbedingte Abweichungen sind anhand der festgelegten
  Kraft-/Momentenbezuege und nicht bloss anhand einzelner Winkelwerte
  zu dokumentieren.
- Numerische Loesung: relativer Rueckwaertsfehler
  `||M*B-b||_inf / (||M||_inf*||B||_inf + ||b||_inf) <= 1e-10`;
  bei verschwindendem Nenner muss der Residualvektor exakt null sein.
  32/48/64-Modenvergleiche und die konkreten Integrationsraster werden
  dokumentiert; das verbindliche Kraft-/Momenten-Ziel bleibt 0,1 %
  gegen H1 und 5 % fuer den Streifen-Rastervergleich.
- Grenzmodell: Uebergaenge an `Re_min` von beiden Seiten, Folgen von
  positiven Geschwindigkeiten gegen null, exakt null, die H1-Gueltigkeits-
  grenze sowie die vereinbarten 0--50-m/s-Testwerte und Intervallmitten
  pruefen. Bei Reibung werden dimensionale Kraefte und Momente und nicht
  der bei null singulaere dimensionslose Reibungsbeiwert verglichen.
- Testbericht trennt bestandene numerische Pruefungen, fehlgeschlagene
  Faelle mit Eingaben/Abweichung, noch nicht ausgefuehrte Tests und
  fehlende physikalische Messreferenzen. Scheitert ein Pflichtnachweis,
  bleibt die Abnahme offen; Toleranzen und Modellbereich werden nicht
  stillschweigend eingeschraenkt.

### 13.12 Abloesung des bisherigen Lookup-Generators

Die Abloesung des bisherigen Gesamtsegel-Lookup-Generators geht auf die
ausdrueckliche Nutzerentscheidung vom 15.11.2026 zurueck; sie ist nicht
Bestandteil der urspruenglichen Core-Abstimmung. Der neue Generator
ersetzt bei Umsetzung den alten Produktausgabepfad und dessen CLI-
Erzeugung. Bestehende alte Lookup-Dateien bleiben unveraendert. H1,
einschliesslich direkter Referenzrechnungen, bleibt fuer die neue
Berechnung und den Vergleich erhalten. Uebergaenge und alte Kennungen
werden nicht geloescht oder umnummeriert.

| Bisherige Anforderungen | Entscheidung fuer diesen Folgeschritt |
|---|---|
| A-S-00001 bis A-S-00004 | Alter Lookup-Aufruf und Auftragshuellen werden durch A-S-00043 und A-S-00058 ersetzt; explizite Fehler, stdout/stderr-Trennung und Pruefbarkeit bleiben nach 13.13 erhalten. |
| A-S-00005 bis A-S-00007 | Geometrie und Profile bleiben durch A-S-00043 bis A-S-00046 erfasst; die alte lokale-Re-Ablehnung wird fuer Niedrig-Re durch A-S-00050/A-S-00051 ersetzt. |
| A-S-00008 bis A-S-00015 | Die H1-Grundlagen bleiben Referenz; fuer neue gekoppelte Lasten und deren Momentenvertrag gelten A-S-00048 bis A-S-00053. |
| A-S-00016/A-S-00017 | Ressourcenvertrag bleibt durch A-S-00071 erhalten. |
| A-S-00018/A-S-00019 | Bisherige Lookup-Raster- und Winkelzwischenpunktpruefung wird durch A-S-00061 bis A-S-00064 und A-S-00070 ersetzt. |
| A-S-00020 bis A-S-00024 | Alte Tabellen-, Manifest- und Publikationsform werden durch A-S-00054 bis A-S-00058 und A-S-00068 ersetzt; A-S-00021 bleibt historisch ersetzt. |
| A-S-00025 bis A-S-00030 | Einzel-Segel-Abgrenzung, unabhaengiger Core, Fehler-/Abbruchschutz und Nachweise bleiben durch A-S-00043, A-S-00057 bis A-S-00060 und A-S-00063 bis A-S-00067 erfasst. |
| A-S-00031 bis A-S-00036 | Alte sechs Lookup-Dateien und deren Parquet-Schemata werden durch das separate Modellformat A-S-00054 bis A-S-00057 und A-S-00068 ersetzt. |
| A-S-00037/A-S-00038 | Fingerprint- und Ausgabepolitik werden fuer den neuen Befehl durch A-S-00057 und A-S-00072 fortgefuehrt. |
| A-S-00039 bis A-S-00042 | Signed H1 als direkte Referenz bleibt; die alte Lookup-Achse und Manifestversion werden durch das neue Modell und A-S-00061/A-S-00063/A-S-00070 ersetzt. |

Die alten Anforderungen in Abschnitt 0 bleiben als Geschichte lesbar;
bei Konflikten fuer das neue Produkt gilt Abschnitt 13. Eine Anwendung,
Neuerzeugung oder Entfernung vorhandener externer Dateien wird durch
diese Spezifikation nicht beauftragt.

### 13.13 CLI, Ressourcen und Fingerprint

`segelmodell --auftrag <Datei>` erzeugt das neue Produkt. Der separate
`pruefen --auftrag <Datei>`-Aufruf prueft Eingaben, Geometrie und
Ressourcen ohne Schreiben oder Publikation. `--max-prozessorzahl` begrenzt
Erzeugungsworker global. Die bestehende Entscheidung ueber fachlich
geeignete GPU-Batches und die Modi `aus`, `automatisch`, `erforderlich`
bleibt erhalten; eine Funktion ohne geeigneten GPU-Batch muss nicht
kuenstlich auf die GPU verlagert werden. Fehler und CPU-Fallback sind
sichtbar, sofern fuer den jeweiligen Modus zulaessig.

`--output-policy create-only|overwrite` hat den Standard `create-only`;
bei vorhandenem Modell wird erfolgreich uebersprungen. `overwrite`
ersetzt nur nach erfolgreich abgeschlossenem, geprueftem Neulauf und
stellt den alten Stand bei Austauschfehler wieder her.
`--fingerprint skip|update` hat den Standard `skip`. `update` verwendet
ausschliesslich die bestehende SegelPhysik-Core-API fuer den Bereich
Aerodynamik, auch nach `create-only`-Ueberspringen; andere Bereiche
bleiben unveraendert. Ein Fingerprintfehler nach Publikation meldet Fehler
und den bereits geschriebenen Modellordner, nicht einen erfolgreichen
Gesamtablauf.

Stdout ist genau ein Abschluss-JSON, Fortschritt und Diagnosen stehen auf
stderr. Die vorhandenen Exitcode-Kategorien fuer Aufruf/Eingabe,
Ressourcen, Rechnung, Ausgabe und Abbruch bleiben erhalten. Tests fuer
Erzeugung, Austausch und Fingerprint verwenden isolierte Testverzeichnisse;
es werden keine externen Produktivmodelle aktualisiert.
