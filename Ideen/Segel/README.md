# Segelkraefte-Paket - 420, Optimist, Folkeboot

Stand: 05.11.2026 - Version 1.0

**Formatentscheidung vom 12.11.2026:** Ergebnistabellen werden ausschliesslich
als Apache Parquet mit Zstandard-Kompression und binary64-Werten bereitgestellt.
Es gibt weder CSV- noch ein eigenes Binaerformat als Tabellenalternative.

Dieses Paket enthaelt alles, was fuer die Umsetzung der Segelkraefte-Simulation
erarbeitet wurde: die verifizierten Klassenmasse, die vollstaendige
Arbeitsanweisung fuer GitHub Copilot und die Originalvorschriften als Belege.

---

## 1. Inhalt

| Ordner / Datei | Inhalt | Status |
|---|---|---|
| `README.md` | diese Datei | - |
| `config/class_rules.yaml` | **5 Segel, alle Masse aus Originalvorschriften** | **fertig, v3.0** |
| `config/class_rules.template.yaml` | leeres Geruest mit TODO-Feldern | Referenz |
| `docs/copilot_aufgabe_segelkraefte.md` | Arbeitsanweisung, Meilensteine M1-M8 | **fertig** |
| `quellen/` | die 5 Original-PDFs | Belege |
| `_verworfen/` | Platzhalter-Ergebnisse aus Testlauf | **NICHT VERWENDEN** |

---

## 2. Die fuenf Segel - verifizierte Masse

| Segel | Vorliek | Unterliek | Achterliek | Flaeche gerechnet | Klassennorm | Abw. | Confidence |
|---|---|---|---|---|---|---|---|
| 420 Grosssegel | 4,90 m | 1,92 m | 5,40 m | 7,78 m2 | 7,45 m2 | +4,4 % | verified |
| 420 Fock | 3,50 m | 1,75 m | 3,20 m | 2,79 m2 | 2,80 m2 | -0,4 % | verified |
| Folkeboot Grosssegel | 8,75 m | 3,30 m | 9,14 m | 18,26 m2 | 17,00 m2 | +7,4 % | verified |
| Folkeboot Fock | 5,50 m | 2,54 m | - | 6,99 m2 | 7,00 m2 | -0,1 % | derived |
| Optimist | 1,73 m | 1,90 m | 2,80 m | 3,20 m2 | 3,50 m2 | -8,4 % | verified_partial |

Vier von fuenf treffen die Klassennorm auf unter 8 %. Die positiven
Abweichungen sind systematisch: Es wurden die Klassenmaxima angesetzt,
echte Segel sind etwas kleiner.

### Wichtigster Befund: Segel sind keine Dreiecke

Das 420er-Grosssegel wurde zunaechst als Dreieck gerechnet: 6,34 m2.
Das ist falsch. Die Klassenvorschrift gibt in G.3.3 Querbreiten vor,
die die Achterlieksrundung beschreiben:

| Abstand vom Schothorn | Querbreite (max) |
|---|---|
| 0 mm | 1920 mm |
| 1350 mm | 2130 mm |
| 2700 mm | 1630 mm |
| 4050 mm | 995 mm |
| 4800 mm | 480 mm |
| 5400 mm | 115 mm |

Integriert ergibt das 7,78 m2 statt 6,34 m2. Wer ein Dreieck ansetzt,
liegt um 20 % zu niedrig - und damit auch bei der Kraft. Deshalb steht
in der YAML `chord_profile_mm` mit diesen sechs Stuetzstellen.

---

## 3. Reynoldszahlen (belegt, nicht geschaetzt)

| Segel | Re bei 3 m/s | 5 m/s | 7 m/s |
|---|---|---|---|
| 420 Grosssegel | 0,37 Mio. | 0,61 | 0,85 |
| 420 Fock | 0,20 Mio. | 0,34 | 0,48 |
| Optimist | 0,29 Mio. | 0,48 | 0,67 |
| Folkeboot Grosssegel | 0,66 Mio. | 1,10 | 1,54 |
| Folkeboot Fock | 0,51 Mio. | 0,85 | 1,19 |

Spanne 0,20-1,54 Mio. Die sechs Stufen in der YAML
(0,20 / 0,30 / 0,45 / 0,70 / 1,05 / 1,55) decken das ab.

---

## 4. Getroffene Entscheidungen (verbindlich)

| Thema | Entscheidung |
|---|---|
| Formate | Ausschliesslich Apache Parquet mit Zstandard |
| Praezision | binary64 fuer Mess- und Rechenwerte; passende Integer-/Boolean-Typen fuer Indizes und Kennzeichen |
| ZIP-Aufbau | /data, /docs, /plots, /examples |
| Naming | <segel>_v01.zip, Formatversion in `manifest.json` |
| Winkelaufloesung | 1 Grad durchgehend, 361 Punkte |
| Reynoldsstufen | 6, log-verteilt |
| Trimm-Matrix | 4 Camber x 4 Twist x 2 Draft = 32 Faelle |
| Hoehenraster | 25 cm unten, ab 75 % Hoehe auf 12,5 cm verdichtet |
| Ausgabentiefe | Q10 = B: Summen voll, Streifenlasten reduziert |
| Masse | Klassenmaxima + dokumentierte Ist-Annahme |
| Blattmasse | Tabelle UND gezeichnete Schnittkonturen (SVG/PNG) |
| Mast/Interferenz | pauschaler Faktor, dokumentiert |
| Koordinaten | x voraus, y BACKBORD positiv, z oben, Ursprung Mastfuss |
| Momentenbezug | Hals des jeweiligen Segels (Gross = Gooseneck, Fock = Vorstagfuss) |
| CoP | 3D-Punkt + Momentenvektor |
| Winkeldefinition | -180..+180 Grad gegen lokale Sehne, 0 = Wind vom Vorliek |
| Geschwindigkeit | scheinbarer Wind |
| Qualitaet | Spalte `quality` je Zeile |
| Layout | Long-Format-Parquet + zusaetzliche Pivot-Parquet-Tabelle |
| README | Deutsch und Englisch |
| Code | Python-Leser + Beispiel-Lookup |

### Achtung: Vorzeichenkonvention

y ist BACKBORD positiv. Das ist ungewoehnlich - die meisten Programme
nutzen steuerbord positiv. Das System ist rechtshaendig (x mal y = z), aber
wer die Tabelle ungeprueft uebernimmt, dreht alle Querkraefte um.
Die Konvention steht deshalb zusaetzlich in `manifest.json` und den
Parquet-Schemametadaten.

---

## 5. Was noch offen ist

| ID | Was | Auswirkung | Loesung |
|---|---|---|---|
| OI-1 | 420 Fock: Tack-Hoehe ueber Deck | gering | am Boot messen |
| OI-2 | Folkeboot Fock: Vorliek exakt (5500 angenommen) | mittel | Segel messen |
| OI-3 | Optimist: Unterliek (nicht im IODA-Formular) | mittel | Baumlaenge messen |
| OI-4 | Mastdurchmesser je Boot fuer Interferenzfaktor | mittel | messen |

Keine davon blockiert den Start. `strict_mode: false` ist gesetzt.

---

## 6. Naechste Schritte

1. `config/class_rules.yaml` ins Repo legen
2. Syntax pruefen:
   python -c "import yaml; yaml.safe_load(open('config/class_rules.yaml'))"
3. `docs/copilot_aufgabe_segelkraefte.md` ins Repo legen
4. Copilot starten mit: "arbeite Abschnitt 9 ab, beginnend mit M1"

Die Meilensteine M1, M3 und M6 (Config, Polar-Provider, Laufzeit-Kern)
sind unabhaengig von den offenen Massen testbar.

---

## 7. Warnung zu `_verworfen/`

Die Dateien in `_verworfen/` stammen aus einem Testlauf mit
Platzhalter-Geometrie (Dreiecksannahme, 6,34 m2 statt 7,78 m2 beim
420er-Grosssegel) und Platzhalter-Physik. Sie dienten nur dazu, das
Datenformat zu zeigen.

NICHT WEITERVERWENDEN. Der 20-%-Geometriefehler wuerde sich sonst in
die Auswertung fortschleppen.

Die sechs grossen CSV-Dateien des Testlaufs (je ~7 MB) wurden aus diesem
Paket entfernt - sie sind falsch und wuerden das Paket unnoetig aufblaehen.
Auch die aktuelle Paketspezifikation erzeugt keine CSV- oder eigenen
Binaertabellen; Tabellenprodukte liegen ausschliesslich als Parquet vor.

---

## 8. Quellen

| Dokument | Verwendung |
|---|---|
| Int. 420 Class Rules, 01.12.2023 | Section G.3.3, G.4.2, Diagram 17, F.2.5, F.3.5 |
| Nordic Folkboat Class Rules 2025-2028 | Section 9.22, 9.32, 6.1.11, 6.3.3 |
| IODA Sail Measurement Form | Opti-Maximalmasse |
| Folkboat Dwg 8 (Mainsail Rev 3, Juli 2024) | Grosssegel-Zeichnung |
| Folkboat Dwg 9 (Headsail Rev 4, Juli 2024) | Fock-Zeichnung |

Alle PDFs liegen in `quellen/`.

---

## 9. Was dieses Paket nicht enthaelt

Die Umsetzung - Streifentheorie, Tabellenerzeugung, ZIPs pro Segel,
Laufzeit-Kern. Das ist der Inhalt der Copilot-Anweisung und wird dort
abgearbeitet.

Der Grund: Die Erzeugung braucht Langlaeufe (XFOIL-Batches ueber Stunden),
die in der Erzeugungsumgebung nicht moeglich sind.
