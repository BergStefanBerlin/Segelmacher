# SegelMacher: Offline-Generator fuer Segel-Lookups

Stand: 05.11.2026  
Status: **Zur Umsetzung freigegeben am 05.11.2026 durch ausdruecklichen Nutzerauftrag.**

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
| A-S-00012 | Volle 3D-Lasten fuer Neigungen 0..90 alle 10 Grad, identischer Pol und stetiger Anschluss (4.3). |
| A-S-00013 | Jede Einzelkraft mit realem Modellansatzpunkt, freien Eigenmomenten und exakter Momentensumme (8.2). |
| A-S-00014 | Zentrale Wirkungslinie, Restmoment, ungueltiger Ansatz bei kleiner Kraft und Momentenidentitaet (8.2). |
| A-S-00015 | Normierung q*S und q*S*c_ref sowie Hals-/Achsenbezuege dokumentiert (8.2). |
| A-S-00016 | Globales CPU-Limit aus Auftrag/CLI/Prozess, ohne vervielfachte Providerpools (6.1). |
| A-S-00017 | Fachlich geeignete GPU-Batches nutzen unterstuetzte Hardware, Modi/Fehler/numerischer Vergleich ohne Speedbenchmark (6.2). |
| A-S-00018 | Hoehenraster 1/2/4, feinste zwei vergleichen, Grenzen erhalten; Fehler sperrt Export (7). |
| A-S-00019 | Direkte Azimut-/Neigungs-/Zellenzwischenpunkte, Naht und Polstetigkeit geprueft (7). |
| A-S-00020 | Eindeutige lokale Profilpolaren samt Profil-/Streifen-/Re-Zuordnung (8.1/8.3). |
| A-S-00021 | Genau sechs Ausgabeprodukte, deterministisch sortierte CSV und 17-stellige Zahlen (8). |
| A-S-00022 | Selbsttragende Metadaten/Snapshots, Parameter, Grenzen, Versionen und SHA256 (8.1/8.5/8.6). |
| A-S-00023 | Exklusives Arbeitsverzeichnis, Schutz vorhandener Ergebnisse, atomare Veroeffentlichung, Fehlerdiagnose (8.7). |
| A-S-00024 | Reproduzierbare Ergebnisse unabhaengig von Workeranzahl (9). |
| A-S-00025 | Mehrere identifizierbare Segelflaechen/Transformationen intern darstellbar, keine gekoppelte Interferenz implementiert (5.3). |
| A-S-00026 | C++/CMake-CLI und eigenstaendig baubarer/testbarer Core ohne GUI (9). |
| A-S-00027 | Isolierte 420-Tests; Original/Sicherung weder geaendert noch bereinigt, Hashschutz (10). |
| A-S-00028 | Numerik und physikalische Validierung getrennt, Quellen/Annahmen/fehlende Referenzen ehrlich sichtbar (2/5/8). |
| A-S-00029 | Abbruch beendet nur eigene Worker, kein erfolgreiches Teilergebnis (3.3). |
| A-S-00030 | Fehler-/Ressourcen-/Schreib-/Konkurrenzfaelle mit passenden Codes und Nachweisen (10). |

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

- [Gemeinsame Konventionen](../documentation/aerodynamik_konventionen.json)
- [420-Herleitung](../documentation/420_segel_herleitung.json)
- [Folkeboot-/Optimist-Herleitung](../documentation/folkeboot_optimist_segel_herleitung.json)

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
        summen.csv
        profile.csv
        kraefte.csv
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
| 5 | Ausgabe gesperrt, bereits vorhanden oder Schreibfehler |
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

Im interaktiven Terminal darf eine Statuszeile aktualisiert werden.
Textmeldungen bleiben lesbar erhalten; Warnungen und Fehler unterbrechen
die Statuszeile sauber. Bei Umleitung in eine Datei oder Aufruf aus einem
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

Das Start-Windneigungsraster enthaelt
`[0, 10, 20, 30, 40, 50, 60, 70, 80, 90]` Grad als Array
`betriebspunkte.windneigung_deg`. Fuer jeden Azimut, jede Referenz-Re-Stufe
und jede Form werden alle zehn Neigungen berechnet.

Die Windneigung beta bezeichnet die Elevation der Windherkunft gegen die
lokale XY-Ebene des Segels, nicht den Kraengungswinkel des Bootes.
Mit Azimut alpha ist der Windherkunfts-Einheitsvektor
`(cos(beta)*cos(alpha), cos(beta)*sin(alpha), sin(beta))`;
trigonometrische Funktionen verwenden Radiant. Die Stroemung zeigt entgegen
diesem Vektor. Bei beta = 90 Grad wird exakt `(0,0,1)` verwendet.
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
| JSON-/CSV-Verarbeitung, Pfade, Pruefsummen, Berichte | CPU; ueberwiegend I/O und geringe Datenparallelitaet |
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

### 8.2 `summen.csv`

```text
form_id,alpha_deg,windneigung_deg,re_ref,CFx,CFy,CFz,CMx,CMy,CMz,angriff_x_m,angriff_y_m,angriff_z_m,angriff_gueltig,CM_rest_x,CM_rest_y,CM_rest_z,qualitaet
```

Eine eindeutige Zeile je Form, Azimut, Windneigung und Referenz-Re. Komponenten liegen im
lokalen Segelsystem vor, Momente um den Hals.
Mit `q = 0.5*rho*V^2`, Referenzflaeche `S` und Referenzlaenge `c_ref` gilt:
`F = q*S*CF`, `M = q*S*c_ref*CM`.
Die passende Re-Abhaengigkeit muss zuvor ausgewaehlt/interpoliert werden.

Jeder intern berechnete Streifenkraftvektor besitzt einen dreidimensionalen
Ansatzpunkt im lokalen Segelsystem relativ zum Hals. Dessen Definition folgt
dem gewaehlten physikalischen Modell; ein zusaetzliches Profilmoment darf
nicht durch die Punktwahl verloren gehen.

Diese Einzelbeitraege werden als Pflichtprodukt in `kraefte.csv` ausgegeben,
nicht nur intern zur Summenbildung verwendet. Mehrere Kraefte je Betriebspunkt
sind ausdruecklich zulaessig. Jede Kraft hat einen gueltigen raeumlichen
Ansatzpunkt; ohne belastbaren Ansatzpunkt darf sie nicht als positionierte
Einzelkraft exportiert werden.

```text
form_id,alpha_deg,windneigung_deg,re_ref,beitrag_id,CFx,CFy,CFz,ansatz_x_m,ansatz_y_m,ansatz_z_m,CM_eigen_x,CM_eigen_y,CM_eigen_z,qualitaet
```

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
Diese Summen muessen mit `summen.csv` innerhalb der dokumentierten numerischen
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

### 8.3 `profile.csv`

```text
profil_id,alpha_deg,re,CL,CD,CM,qualitaet
```

Profile sind eindeutig in `lookup.json` beschrieben. Gleiche Profile duerfen
wiederverwendet werden. Winkel/Re-Stuetzstellen muessen die tatsaechlich
verwendeten lokalen Abfragen und deren Interpolation abdecken.
Identische Datensaetze duerfen keine widerspruechlichen Werte enthalten.
Diese Tabelle beschreibt lokale zweidimensionale Profilpolaren, nicht die
raeumlichen Betriebspunkte des Gesamtsegels. Deren Neigung, Projektion und
3D-Kopplung gehoeren in den Gesamtsegelprovider; sie duerfen nicht durch
blosses Duplizieren der Profilpolaren als berechnet gelten.

### 8.4 CSV-Konvention

UTF-8, Komma als Feldtrenner, Punkt als Dezimalzeichen, Kopfzeile vorhanden,
keine lokalisierten Zahlen oder Kommentarzeilen. Numerische Werte werden
mit mindestens 17 signifikanten Dezimalstellen fuer binary64-Roundtrips
geschrieben. Unbenoetigte Nachkommastellen muessen nicht aufgefuellt werden.
Zeilen werden deterministisch nach ihren Schluesselspalten sortiert.
Keine NaN-/Infinity-Werte oder Nullkraft-Ersatzzeilen fuer gescheiterte Punkte.
Leere Ansatzpunktfelder sind ausschliesslich bei `angriff_gueltig = 0`
zulaessig und bedeuten nicht einen fehlgeschlagenen Kraft-/Momentenfall.

### 8.5 `pruefbericht.json`

Faelle mit Erfolg/Fehler, maximale absolute/relative Abweichungen,
fehlgeschlagene Betriebspunkte, Providerkonvergenz, Warnungen, Annahmen,
CPU/GPU-Entscheidung, Ressourcen, Laufzeiten und Grenzen.
Der Bericht trennt numerische Pruefung von physikalischer Validierung.

### 8.6 `auftrag_ausgefuehrt.json`

Originalauftrag und effektive Einstellungen, CLI-Limit, CPU/GPU-Backend,
Tool-/Modellversion, Eingabepruefsummen sowie Snapshots der Segelgeometrie
und Konventionen. Keine ausschliessliche Abhaengigkeit von externen Pfaden.

### 8.7 Schutz vor Teil- und Konkurrenzresultaten

Vorhandenes `LookUp` wird nicht ueberschrieben. Berechnung/Export erfolgen in
einem exklusiven temporaeren Geschwisterordner; erst nach vollstaendiger
Pruefung wird dieser als `LookUp` veroeffentlicht.
Zwei gleichzeitige Auftraege im gleichen Segelordner duerfen sich nicht
ueberschreiben oder vermischen. Der verlierende Aufruf meldet einen Konflikt.

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
[Sicherung](../Backups/420_main/main_20261105_195154_358.json) hat die SHA-256-Pruefsumme
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
| Gueltiger bereiter 420-Auftrag | Vollstaendiger Ordner mit allen sechs Ergebnisdateien einschliesslich positionierter Einzelkraefte |
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
| Zweiter Lauf/vorhandenes Ergebnis | Kein Ueberschreiben |
| Ergebnisordner auf anderen Rechner kopiert | Metadaten und Tabellen ohne Ursprungsrepository interpretierbar |
| Bibliothek/CLI spaeter gemeinsam genutzt | Gleicher numerischer Kern, keine GUI-Abhaengigkeit |

Diese Datei bleibt die massgebliche, zur Umsetzung freigegebene Spezifikation.
Der vorhandene Umsetzungsplan wird fortgefuehrt; fachliche Abnahme ist separat.
