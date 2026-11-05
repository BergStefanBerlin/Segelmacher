# Quellenlinks – Segelmaße 420, Optimist, Folkeboot

Stand: 05.11.2026 · Zusammengestellt für das Segelkräfte-Projekt

Alle Links wurden während der Recherche aufgerufen oder in Suchergebnissen
verifiziert. Die Spalte **Status** sagt, was tatsächlich daraus entnommen
wurde – nicht jeder Link lieferte Maße.

## Legende

| Symbol | Bedeutung |
|---|---|
| ✅ | vollständig gelesen, Maße entnommen |
| 🟡 | teilweise gelesen (PDF brach ab, Seite gesperrt) – Maße aus anderer Quelle ergänzt |
| 🔒 | Zugriff verweigert (Bot-Sperre, HTTP 401/403) |
| 🔎 | nur als Suchergebnis-Snippet gesehen, nicht vollständig gelesen |
| 📄 | Dokument vorhanden, aber ohne Segelmaße |
| 💰 | kostenpflichtig |

---

# 1. International 420

## 1.1 Klassenvorschrift – Primärquelle

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [420 Class Rules, 01.12.2023 (PDF)](https://www.420sailing.org/uploaded_files/420%20Class%20Rules%20December%201st%202023.pdf_1902_en.pdf) | Int. 420 Class Rules, gültige Fassung | **Section G.3.3** Großsegel-Maßtabelle inkl. Querbreiten · **G.4.2** Fock · **Diagramm 17** · **F.2.5/F.3.5** Riggmaße (Mastlänge 6260 mm, Unterliekspunkt 1160 mm, Vorstaghöhe 4520 mm, Mastquerschnitt 50 × 45 mm) | ✅ (Section G per Seitenrender gelesen, Volltext brach vorher ab) |
| [420 Class Rules 2013 v2 (PDF)](https://420sailing.org/uploaded_files/International420ClassRules_2013_v2.pdf_405_en.pdf) | ältere Fassung | Gleiche Struktur, Section G vorhanden – als Vergleichsfassung brauchbar, wenn die aktuelle Ausgabe nicht zugänglich ist | 🔎 |
| [Änderungen der Class Rules, 13.01.2016](https://www.420sailing.org/default/news/new/text/change-to-420-class-rules-as-at-13-january-2016) | Änderungsmitteilung | Keine Maße, aber Hinweis auf die technische Dokumentenablage | 📄 |
| [420sailing.org – Startseite](https://420sailing.org) | Klassenvereinigung | Einstieg zu „Class Rules" und „Technical Documentation" | 📄 |

**Die entscheidenden Zahlen aus G.3.3** (Querbreiten des Großsegels, Abstand vom Schothorn aufwärts):

| Abstand | Querbreite max. |
|---|---|
| 0 mm | 1920 mm |
| 1350 mm | 2130 mm |
| 2700 mm | 1630 mm |
| 4050 mm | 995 mm |
| 4800 mm | 480 mm |
| 5400 mm | 115 mm |

Daraus integriert: **7,78 m²** – nicht 6,34 m² wie bei Dreiecksannahme.

## 1.2 Club 420 – falls dein Boot ein C420 ist

⚠️ **Wichtig:** Der Club 420 ist eine **andere Klasse** als der International 420.
Die Club-420-Regeln enthalten **keine Segelmaß-Tabelle**, weil die Segel nur
von lizenzierten Herstellern geliefert werden. Maße gibt es nur über
Herstellerangaben.

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [Harken – Club 420 Deck Layout](https://www.harken.com/en/support/one-design-deck-layouts/club-420/) | Rigg-Übersicht | **Großsegel 7,40 m² · Fock 2,80 m² · Spinnaker 8,83 m²** · LüA 4,24 m · Breite 1,68 m · drei zugelassene Hersteller | ✅ |
| [Zim Sailing – C420 Pro](https://zimsailing.com/c420---pro/) | Händlerseite | Segelausstattung, keine Maße | 📄 |
| [Zim Sailing – C420 Club](https://zimsailing.com/c420---club/) | Händlerseite | Segelausstattung, keine Maße | 📄 |

---

# 2. Optimist

## 2.1 Vermessungsformular – Primärquelle für die Segelmaße

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [IODA Sail Measurement Form (PDF)](https://www.optiworld.org/uploaded_files/Document_2045_20180605202313_en.pdf) | offizielles Vermessungsformular | **Head 1240 mm · Luff 1730 mm · Leech 2800 mm · Diagonale 2450–2580 mm · Peak→unterste Latte 1850–1950 mm · Peak→oberste Latte 900–1000 mm · Ply min. 0,15 mm · Augenabstände Vorliek 230–260 mm, Unterliek 270–300 mm** | ✅ |

**Nicht enthalten:** das Unterliek (die Fußlänge). Das Formular vermisst nur
Augenabstände, nicht die Fußlänge. Deshalb steht in `class_rules.yaml`
`confidence: verified_partial` und der Punkt **OI-3** ist offen.

## 2.2 Klassenvorschrift

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [IODA Class Rules 2022 (PDF)](https://www.optiworld.org/uploaded_files/2022%20IODA-Rules-Final%20March%201_22.pdf_2893_en.pdf) | aktuelle Vorschrift | Rahmenregeln, Verweis auf Pläne und Vermessungsformulare; Toleranzen ±2 mm | 🔎 |
| [IODA Class Rules 2015 (PDF)](https://www.optiworld.org/uploaded_files/2015%20Class%20Rules.pdf_2075_en.pdf) | ältere Fassung | Gleiches Prinzip, Verweis auf Messformulare | 🔎 |

## 2.3 Kostenpflichtige Vermessungsschablonen

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [IODA Store – Sail Measurement Template](https://www.optiworld.org/default/store/store) | Schablone | **400 USD** – physische Vermessungsschablone, keine digitale Maßtabelle | 💰 |
| [IODA Store – Class Info](https://www.optiworld.org/default/classinfo/store?type=0) | Übersicht | Daggerboard-Template 100 USD, Spar-Template 150 USD, Wood/Epoxy-Pläne 51,50 USD | 💰 |

---

# 3. Nordic Folkboat

## 3.1 Klassenvorschrift – Primärquelle

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [Folkboat Class Rules 2025–2028, inkl. Spinnaker (PDF)](https://folkboat.com/wp-content/uploads/2025/02/NORDIC-FOLKBOAT-CLASS-RULES-2025-2028-incl-spinnaker.pdf) | gültige Vorschrift | **9.22** Großsegel-Querbreiten (Halbbreite 2225 mm bei 4570 mm unter Kopf, Dreiviertelbreite 1310 mm bei 2280 mm) · **9.32** Fock (Kopfbreite 140 mm, Mittelgirth 1270 mm bei 2700 mm) · **6.1.11** Vermessungsbänder (unteres Mastband 1000 mm über Deck, oberes 8750 mm darüber) · **6.3.3** Baumpunkt 3380 mm hinter Mastachterkante · Vorstag 5500 mm über unterem Band · Doppelluff und loses Unterliek verboten · Mindestgewicht 1930 kg | ✅ |
| [Folkboat Class Rules 2025–2028, ohne Spinnaker (PDF)](https://folkboat.com/wp-content/uploads/2025/01/NORDIC-FOLKBOAT-CLASS-RULES-2025-2028.pdf) | gleiche Vorschrift, andere Datei | Inhaltlich identisch – als Ausweichquelle, falls der Spinnaker-Band nicht lädt | 🔎 |
| [Folkboat Class Rules – Übersichtsseite](https://folkboat.com/class-rules/) | Dokumentenindex | **Hier liegen alle Zeichnungen als eigene PDFs** (siehe 3.2) | ✅ |
| [Folkboat Class Rule Clarifications](https://folkboat.com/clarifications/) | Auslegungen | Vermessungspraxis, Segelbutton, keine neuen Maße | 📄 |

## 3.2 Offizielle Zeichnungen

Diese PDFs sind von [folkboat.com/class-rules/](https://folkboat.com/class-rules/)
verlinkt. **Direkt-URLs konnten nicht ermittelt werden** – die Seite hat eine
Bot-Sperre, die Downloads sind nur über den Browser erreichbar.

| Zeichnung | Was drin steht | Status |
|---|---|---|
| **8_Mainsail Rev 3, Juli 2024** | Großsegel-Vermessungsdiagramm: Vorliek, Unterliek, Querbreiten, Kopfbreite, Lattenpositionen | ✅ (als Upload) |
| **9_Headsail 9_1 Rev 4, Juli 2024** | Fock-Vermessungsdiagramm: Vorliek, Unterliek, LP, Mittelgirth | ✅ (als Upload) |
| 20170726_NF_Drawing_Mainsail | ältere Fassung des Großsegels | nicht geprüft |
| Drawing_Headsail_9_1_Rev3 | ältere Fassung der Fock 9.1 | nicht geprüft |
| Drawing_Headsail_9_2_Rev3 | **zweite Fock-Variante 9.2** – prüfen, ob deine Fock dieser entspricht | nicht geprüft |
| DRW-10-Spinnaker | Spinnaker – **für dieses Projekt nicht relevant** | – |
| DRW-11-revision-Mast-measurement-diagram | **Mastvermessung** – relevant für OI-4 (Mastdurchmesser) | nicht geprüft |
| 5_Construction plan wood English Rev 3, Juli 2024 | Bauplan Holzrumpf | – |
| GRP 7 · 236_03_Folkboat_GA_V025 | Generalplan GFK, Mai 2025 | – |

**Hinweis zu 9_2:** Es gibt zwei Fock-Zeichnungen (9.1 und 9.2). In
`class_rules.yaml` ist die Fock aus 9.1 mit `confidence: derived` eingetragen.
Falls deine Fock der Variante 9.2 entspricht, weichen Vorliek und Unterliek ab –
das ist der Punkt **OI-2**.

---

# 4. Bootsübergreifend

| Link | Dokument | Was drin steht | Status |
|---|---|---|---|
| [World Sailing – Classes & Equipment](https://www.sailing.org/classesandequipment/I420.php) | Dachverband | Verweis auf technische Dokumentation der International 420; in allen drei Vorschriften als übergeordnete Instanz referenziert (ERS – Equipment Rules of Sailing) | nicht geprüft |

**Zur ERS:** Alle drei Klassenvorschriften verweisen auf die *Equipment Rules
of Sailing* von World Sailing als übergeordnetes Regelwerk für
Vermessungsbegriffe. Ich habe die ERS **nicht** geöffnet – die konkreten
Segelmaße stehen ausschließlich in den Klassenvorschriften, nicht in der ERS.

---

# 5. Welcher Link löst welchen offenen Punkt

| Offener Punkt | Was fehlt | Link, der es enthält | Seite/Abschnitt |
|---|---|---|---|
| **OI-1** | 420 Fock: Tack-Höhe über Deck | [420 Rules 2023](https://www.420sailing.org/uploaded_files/420%20Class%20Rules%20December%201st%202023.pdf_1902_en.pdf) | F.2.5 / Diagramm 17 |
| **OI-2** | Folkeboot Fock: Vorliek exakt | [folkboat.com/class-rules](https://folkboat.com/class-rules/) → Zeichnung 9_1 oder 9_2 | Zeichnung |
| **OI-3** | Optimist: Unterliek | **in keinem Dokument** – nur am Boot oder am Baum messbar | – |
| **OI-4** | Mastdurchmesser je Boot | 420: G.3/F.3 (50 × 45 mm ✅ bereits belegt) · Folkeboot: DRW-11 Mastvermessung · Optimist: 30 mm angenommen | – |

**OI-3 ist der einzige Punkt, der sich nicht über Links lösen lässt.**
Das IODA-Formular vermisst bewusst nur Augenabstände. Die Fußlänge ergibt
sich aus der Baumlänge – ein Maßband, zwei Minuten.

---

# 6. Praktische Hinweise

**PDF-Abbruch beim Lesen.** Lange PDFs werden beim automatisierten Lesen
nach etwa 35.000 Zeichen abgeschnitten. Bei der 420er-Vorschrift liegt
Section G (S. 29 ff.) genau dahinter. Lösung: die entscheidenden Seiten
einzeln als Bild rendern oder die Datei lokal öffnen.

**Bot-Sperren.** `folkboat.com/class-rules/` antwortet automatisierten
Abrufen mit HTTP 401, `optiworld.org` teilweise mit 403. Über einen
normalen Browser funktionieren beide.

**Zwei Fassungen der Folkeboot-Regeln.** Die Dateien vom Januar und Februar
2025 sind inhaltlich identisch; die Februar-Fassung enthält zusätzlich den
Spinnaker. Für Großsegel und Fock ist es egal, welche du nimmst.

**Club 420 ≠ International 420.** Falls dein Boot ein Club 420 ist, sind
die 420er-Klassenmaße aus Abschnitt 1.1 **nicht** gültig. Dann gelten
7,40 m² Großsegel und 2,80 m² Fock (Harken-Angabe), und die Geometrie muss
aus Herstellerdaten rekonstruiert werden.

---

# 7. Nicht mehr verifizierbare Angaben

Zwei Zahlen aus der Recherche konnte ich **nicht** mit einem Link belegen
und habe sie deshalb nicht in `class_rules.yaml` verwendet:

| Angabe | Herkunft | Warum verworfen |
|---|---|---|
| 420er Baumlänge 2500 mm | österreichische 420er-Klassenvereinigung (Suchergebnis) | Direkt-URL nicht rekonstruierbar; der Wert passt aber zum belegten Unterliek von 1920 mm (Klassenmaximum) |
| 420er Großsegel 7,45 m² | mehrere Klassenvereinigungen | Widerspruch zur Geometrie: mit 4900 mm Vorliek sind nur 7,78 m² bei maximaler Achterlieksrundung darstellbar; 7,45 m² ist vermutlich eine Vermessungs- oder Bruttogröße |
