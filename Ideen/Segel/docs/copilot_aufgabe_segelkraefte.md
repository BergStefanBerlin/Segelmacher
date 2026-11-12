# Arbeitsauftrag: Aerodynamik-Bibliothek fuer Segel (5 Segel, 3 Bootsklassen)

> ## ⚠ STATUS-UPDATE 2026-11-05 — Geometrie ist jetzt belegt
>
> `class_rules.yaml` liegt in **Version 3.0** vor und ist **nicht mehr Platzhalter**.
> Alle fuenf Segel sind aus den Original-Klassenvorschriften extrahiert:
>
> | Segel | Flaechenpruefung | Confidence |
> |---|---|---|
> | 420 Grosssegel | 7.78 m² (Klassennorm 7.45, +4.4 % durch Maximalmasse) | verified |
> | 420 Fock | 2.79 m² (Klassennorm 2.80, −0.4 %) | verified |
> | Folkeboot Grosssegel | 18.26 m² (Klassennorm 17.0, +7.4 %) | verified |
> | Folkeboot Fock | 6.99 m² (Klassennorm 7.00, −0.1 %) | derived |
> | Optimist | 3.20 m² (Klassennorm 3.50, −8.6 %) | verified_partial |
>
> **Quellen:** Int. 420 Class Rules 2023 (G.3.3, G.4.2, F.2.5, F.3.5, Diagram 17);
> Nordic Folkboat Rules 2025 (9.22, 9.32, 6.1.11, 6.3.3, Dwg 8/9); IODA Sail Measurement Form.
>
> **Wichtig fuer die Implementierung:** Die Chord-Verteilung ist NICHT dreieckig.
> `chord_profile_mm` in der YAML enthaelt die gemessenen Querbreiten aus der Vorschrift —
> diese bestimmen die Achterlieksrundung und damit die Flaechen. Wer stattdessen ein
> Dreieck ansetzt, liegt beim 420er-Grosssegel um **20 % zu niedrig**.
>
> **Geaenderte Annahmen gegenueber Erstentwurf:**
> - 420 Grosssegel: 6.34 m² → **7.78 m²** (Dreiecksannahme war falsch)
> - Folkeboot Grosssegel: Vorliek 8.22 m → **8.75 m**, Unterliek 2.90 m → **3.30 m**
> - Folkeboot Fock: 6.97 m² bleibt bestaetigt (6.99 m²)
>
> **Formatentscheidung vom 12.11.2026:** Ergebnistabellen werden ausschliesslich
> als Apache Parquet mit Zstandard und binary64-Werten geschrieben. CSV und
> ein eigenes Tabellen-Binaerformat sind keine Ausgabealternativen.


**Ziel dieses Dokuments:** Vollstaendige, in einem Rutsch abarbeitbare Spezifikation fuer einen Coding-Agenten (GitHub Copilot / Claude Code / Cursor). Keine Rueckfragen an den Auftraggeber noetig, ausser an den mit `TODO_HUMAN` markierten Stellen.

**Version:** 1.0
**Sprache im Code:** Englisch (Identifier, Docstrings). **Sprache der Dokumentation:** Deutsch und Englisch.
**Zielumgebung:** Python 3.11+ mit PyArrow, keine Netzwerkabhaengigkeiten zur Laufzeit, keine Cloud-Dienste.

---

## 1. Mission

Baue eine offline laufende Python-Bibliothek, die fuer **fuenf Segel** die aerodynamische Summenkraft, den Angriffspunkt (Center of Pressure) und das Moment in Abhaengigkeit von Windrichtung, Windgeschwindigkeit und Trimmzustand liefert.

Die Bibliothek erzeugt pro Segel **eine ZIP-Datei**, die alle Ergebnisdateien plus Nutzungsbeschreibung enthaelt. Jedes ZIP muss **eigenstaendig** verwendbar sein.

### 1.1 Umfang (genau fuenf Segel)

| ID | Boot | Segel | Anmerkung |
|---|---|---|---|
| `420_MAIN` | International 420 | Grosssegel | |
| `420_JIB` | International 420 | Fock | Interferenz mit Grosssegel |
| `OPT_MAIN` | Optimist | Grosssegel | Spritrig, Mast = Vorliek |
| `FOLK_MAIN` | Nordic Folkboat | Grosssegel | Fraktionalrigg |
| `FOLK_JIB` | Nordic Folkboat | Fock | |

**Nicht im Umfang:** Spinnaker, Hydrodynamik, Rumpf, Strukturanalyse, Flattern, GUI, Webdienst.

### 1.2 Harte Randbedingungen

- **Rein aerodynamisch.** Keine Rumpf-, Kiel- oder Ruderhydrodynamik. Keine Bootsgeschwindigkeitsberechnung (kein VPP). Die Windgeschwindigkeit ist **Eingabe**, nicht Ergebnis.
- **Keine erfundenen Klassenvorschrift-Zahlen.** Alle Maße kommen aus `config/class_rules.yaml`. Fehlt ein Pflichtwert, muss das Programm mit klarer Fehlermeldung abbrechen. Niemals Defaultwerte raten.
- **Keine Netzwerkaufrufe** im erzeugten Code.
- **Deterministisch.** Gleiche Konfiguration + gleicher Seed => bitgleiches Ergebnis.

---

## 2. Entscheidungsregister (verbindlich)

Diese Tabelle ist das Ergebnis einer abgeschlossenen Abstimmung. Sie ist **nicht** zu hinterfragen, sondern umzusetzen.

| Nr. | Thema | Entscheidung |
|---|---|---|
| Q1 | Dateiformate | Ausschliesslich Apache Parquet mit Zstandard |
| Q2 | Praezision | binary64 fuer Mess- und Rechenwerte; Integer-/Boolean-Typen fuer Indizes und Kennzeichen |
| Q3 | ZIP-Struktur | Ordner `/data`, `/docs`, `/plots`, `/examples` |
| Q4 | Namensschema | `<boot>_<segel>_v01.zip`; Formatversion und Metadaten in `manifest.json` |
| Q5 | Beigaben | Tabellen + README + 2 Diagramme + Python-Leser |
| Q6 | Winkelaufloesung | **1 Grad durchgehend, 360 Punkte** (-180 .. +179) |
| Q7 | Reynoldsstufen | **6 Stufen, logarithmisch** |
| Q8 | Trimm-Matrix | **4 Camber x 4 Twist x 2 Draft = 32 Faelle** |
| Q9 | Hoehenraster | 250 mm unten, ab 75 % Hoehe auf 125 mm verdichtet |
| Q10 | Ausgabeumfang | **Summen vollstaendig + Streifenlasten reduziert** (Details Abschnitt 6.2) |
| Q11 | Segelmass-Herkunft | Klassenmaxima + dokumentierte Ist-Annahme (Faktoren in Config) |
| Q12 | Camber/Twist | Als Matrix (Q8), Herleitung dokumentiert |
| Q13 | Blattmasse | Tabelle **und** gezeichnete Schnittkonturen (SVG + PNG) |
| Q14 | Mast/Interferenz | Pauschaler Korrekturfaktor, dokumentiert |
| Q15 | Koordinatensystem | **x voraus, y backbord, z oben** (rechtshaendig) |
| Q16 | Momentenbezug | **Hals des jeweiligen Segels** (Grosssegel: Gooseneck) |
| Q17 | Angriffspunkt | 3D-Punkt **plus** Momentenvektor um Bezugspunkt |
| Q18 | Winkeldefinition | -180 .. +180 Grad gegen lokale Sehne, 0 Grad = Wind vom Vorliek |
| Q19 | Geschwindigkeit | **scheinbarer Wind** (apparent wind) |
| Q20 | Verlaesslichkeit | Spalte `quality` je Zeile (0/1/2) |
| Q21 | Tabellenlayout | Long-Format-Parquet + zusaetzliche Pivot-Parquet-Tabelle |
| Q22 | README | Deutsch und Englisch |
| Q23 | Beispielcode | Python-Leser + Beispiel-Lookup mit Interpolation |

---

## 3. Architektur: vier Stufen

```
Stufe 0  Polar-Bibliothek      -> 2D-Profilpolaren (CL, CD, CM ueber alpha, Re)
Stufe 1  Geometrie             -> Schnitte, Streifenraster, Chord/Camber/Twist je Streifen
Stufe 2  Streifentheorie       -> Integration zur Summenkraft + Moment
Stufe 3  Backen                -> Parquet-Tabellen mit Zstandard
Stufe 4  Laufzeit              -> Lookup + Interpolation, < 10 us
```

**Wichtig:** Stufe 0 ist austauschbar. Es gibt zwei Provider:

- `PlaceholderPolarProvider` - analytisches Ersatzmodell, laeuft sofort, Genauigkeit ca. +/-25-30 %
- `XfoilPolarProvider` - ruft ein lokales XFOIL-Binary auf, Genauigkeit ca. +/-15-20 %

Beide implementieren dasselbe Interface. Der Rest der Kette darf keinen Unterschied kennen.

---

## 4. Verzeichnisstruktur des Repositories

```
sail-aero/
├── README.md
├── pyproject.toml
├── config/
│   ├── class_rules.yaml          # MASSGEBLICH, vom Menschen zu pflegen
│   ├── trim_matrix.yaml
│   ├── polar_grid.yaml
│   └── corrections.yaml
├── src/sailaero/
│   ├── __init__.py
│   ├── config.py                 # Laden + Validierung, fail-fast
│   ├── geometry/
│   │   ├── section.py            # parametrischer Schnittgenerator
│   │   ├── sail.py               # Silhouette -> Streifen
│   │   └── export_svg.py
│   ├── polars/
│   │   ├── base.py               # Protocol/ABC
│   │   ├── placeholder.py
│   │   ├── xfoil.py
│   │   └── cache.py
│   ├── physics/
│   │   ├── strip.py              # Streifentheorie
│   │   ├── induction.py          # induzierte Abwindung
│   │   └── interference.py       # Korrekturfaktoren
│   ├── bake/
│   │   ├── tables.py             # Erzeugung der Ergebnistabellen
│   │   └── parquet.py            # Schreiben/Lesen der Parquet-Tabellen
│   ├── runtime/
│   │   ├── lookup.py             # Interpolator
│   │   └── forces.py             # Kraftvektor + CoP
│   └── package_zip.py
├── scripts/
│   ├── build_all.py              # CLI: baut alle fuenf ZIPs
│   └── convergence_study.py
└── tests/
```

---

## 5. Koordinaten, Vorzeichen, Einheiten

### 5.1 Bootsfestes System

| Achse | Richtung | Einheit |
|---|---|---|
| x | **voraus** (Bug) | m |
| y | **backbord** (Port) positiv | m |
| z | **oben** | m |

Rechtshaendig: `x cross y = z`. **Achtung:** Steuerbord ist negativ. Das ist ungewoehnlich und muss in README, `manifest.json`, Parquet-Schemametadaten und Docstrings ausdruecklich stehen.

**Ursprung:** Mastfuss (Deck-Level am Mast). Fuer das Folkeboot: Mastspur. Fuer den Optimist: Mastfuss im Mastschuh.

### 5.2 Momentenbezugspunkt

**Hals (Tack) des jeweiligen Segels**, ausgedrueckt im bootsfesten System:

- Grosssegel: Gooseneck (Punkt Baum/Mast)
- Fock: unterer vorderer Segelpunkt (Beschlag an Deck bzw. Vorstagfuss)

Der Punkt wird pro Segel in `class_rules.yaml` als `tack_position_m: [x, y, z]` hinterlegt.

### 5.3 Winkel

`alpha` = Winkel zwischen dem Vektor des **scheinbaren Winds** und der **lokalen Sehne** des Streifens.

- `alpha = 0` : Wind stroemt vom Vorliek zum Achterliek (Segel voll im Wind)
- `alpha = +90` : Wind von Backbord quer einfallend
- `alpha = 180` : Wind von achtern (Segel killt / wird rueckwaerts angestroemt)
- Wertebereich: -180 .. +179 in 1-Grad-Schritten (360 Punkte)

### 5.4 Einheiten

Intern SI: Meter, Sekunde, Newton, Radiant. **An der Schnittstelle**
(Config, Parquet, API) Grad und Meter. Konvertierung nur in `config.py` und
dem Parquet-Schreiber.

Luftdichte: `rho = 1.225 kg/m^3` als Default, in Config ueberschreibbar.

---

## 6. Datenvertraege

Alle Ergebnistabellen werden ausschliesslich als Apache Parquet mit
Zstandard-Kompression geschrieben. Numerische Mess- und Rechenwerte sind
binary64; Indizes und Kennzeichen verwenden passende Integer- bzw.
Boolean-Typen. Es gibt weder eigene Tabellen-Binaerdateien noch alternative
Texttabellen. Jede Tabelle ist deterministisch nach ihren Schluesselfeldern
sortiert und in unabhaengig lesbare Row Groups aufgeteilt.

`manifest.json` enthaelt Format- und Schemanummer, Bibliotheks-/Codecversion,
Feldtypen, Einheiten, Achsen- und Momentenbezug, Trim-/Rasterachsen, Zeilen-
und Row-Group-Zahlen sowie SHA-256-Pruefsummen aller Parquet-Dateien.
Integritaets- und Schemainformationen werden nicht in Kommentarzeilen
repraesentiert.

### 6.1 Summen-Tabelle (`sums.parquet`)

Long Format. Alle Kraft-/Momentenwerte und Geometrieangaben bleiben erhalten.
`sail_id` ist UTF-8, die Indexfelder sind Integer, `cop_valid` ist Boolean,
`quality` ist ein Integer und alle Mess-/Rechenwerte sind float64.

| Spalte | Typ | Bedeutung |
|---|---|---|
| `sail_id` | string | Segelkennung |
| `alpha_deg` | float64 | -180 .. +179 |
| `re` | float64 | Reynoldszahl |
| `v_apparent_ms` | float64 | scheinbare Windgeschwindigkeit |
| `camber_idx` | int32 | 0..3 |
| `twist_idx` | int32 | 0..3 |
| `draft_idx` | int32 | 0..1 |
| `camber` | float64 | f/c |
| `twist_deg` | float64 | Root-zu-Tip |
| `draft` | float64 | x_f/c |
| `Fx_N`, `Fy_N`, `Fz_N` | float64 | Kraftkomponenten im bootsfesten System |
| `F_magnitude_N` | float64 | Kraftbetrag |
| `Mx_Nm`, `My_Nm`, `Mz_Nm` | float64 | Momentenkomponenten um den Segelhals |
| `cop_x_m`, `cop_y_m`, `cop_z_m` | float64 | Angriffspunkt im bootsfesten System |
| `cop_valid` | bool | Angriffspunkt gueltig, siehe 7.4 |
| `CL_global`, `CD_global` | float64 | globale Beiwerte |
| `quality` | int8 | 0 belegt, 1 plausibel, 2 geschaetzt |

### 6.2 Streifenlasten (`strips.parquet`) - reduzierte Aufloesung

**Definition "reduziert" (verbindlich):**

- Winkel: **alle 360** (1-Grad-Raster)
- Reynolds: **die beiden mittleren** der sechs Stufen (Index 2 und 3)
- Trimmfaelle: **genau drei**, definiert als

| Fall | camber_idx | twist_idx | draft_idx | Charakter |
|---|---|---|---|---|
| `R1` | 2 | 2 | 1 | mittlere Woelbung, mittlerer Twist |
| `R2` | 2 | 3 | 1 | mehr Twist |
| `R3` | 3 | 2 | 1 | mehr Woelbung |

Spalten und Typen: `sail_id` string, `alpha_deg`/`re`/`z_m`/`chord_m`/
`camber`/`draft`/`twist_deg`/`alpha_local_deg`/`CL`/`CD`/`dFx_N_per_m`/
`dFy_N_per_m`/`dFz_N_per_m` float64, `trim_case` string, `strip_index`
int32, `quality` int8.

### 6.3 Blattmasse (`geometry.parquet`)

Eine Zeile je Streifen, volle Aufloesung:

`sail_id` string, `strip_index` int32, `z_m`, `chord_m`, `camber`, `draft`,
`twist_deg`, `leech_offset_m` und `area_strip_m2` float64.

### 6.4 Pivot-Ansicht (`sums_pivot.parquet`)

Breites Format nur zur menschlichen Kontrolle: Schluesselspalten sind
`sail_id`, `alpha_deg`, `camber_idx`, `twist_idx` und `draft_idx`;
anschliessend folgt je ausgegebener Reynoldszahl eine float64-Spalte fuer
`F_magnitude_N`. Die Reynoldszahlen und ihre Spaltennamen stehen im Manifest.

---

## 7. Physik-Spezifikation

### 7.1 Parametrischer Schnittgenerator

Erzeuge eine geschlossene 2D-Kontur aus:

| Parameter | Symbol | Bereich |
|---|---|---|
| Woelbung | `f/c` | 0.06 .. 0.16 |
| Position max. Woelbung | `xf/c` | 0.28 .. 0.48 |
| Dicke | `t/c` | 0.015 .. 0.045 |
| Nasenradius | `rn/c` | 0.002 .. 0.008 |
| Hinterkanten-Reflex | `reflex` | 0.0 .. 0.02 |

**Wichtig:** Ein Segelschnitt ist **keine** NACA-Tropfenform. Umsetzung als duenne, stark gewoelbte Schale:

1. Skelettlinie: `y_c(xi) = f * g(xi)` mit `xi = x/c in [0,1]`, `g` als stueckweise definierte Funktion, die bei `xi = xf/c` das Maximum 1 erreicht, an beiden Enden 0 ist, und deren Kruemmung an der Hinterkante per `reflex` eingestellt wird.
2. Dickenverteilung `y_t(xi)`: duenn, mit endlichem Nasenradius, spitz an der Hinterkante.
3. Kontur: `y_upper = y_c + y_t`, `y_lower = y_c - y_t`.
4. Sicherstellen: `y_upper(0) == y_lower(0) == 0`, Tangentialstetigkeit an der Nase, `y_upper(1) == y_lower(1)`.

Export als SVG (Kontur + Skelettlinie + Sehne) und PNG (300 dpi).

### 7.2 Streifentheorie

Fuer jeden Streifen `i` auf Hoehe `z_i` mit Chord `c_i`:

1. `alpha_local_i = alpha_ref + twist_i + alpha_induced_i`
2. Polare liefert `CL_i, CD_i` durch Interpolation in `(alpha, Re, camber, draft)`
3. Induzierte Abwindung iterativ (3-5 Durchlaeufe, Relaxation 0.5):

   `alpha_induced_i = -(1/(8*pi)) * sum_j ( dGamma_j / (z_i - z_j) )` fuer `i != j`,
   mit `Gamma_j = 0.5 * V * c_j * CL_j`

   Fuer `i == j` entfaellt der Term (kein Selbstinduktions-Singularitaet; stattdessen
   `alpha_induced_i -= CL_i / (pi * AR_eff)` als globaler Anteil).

4. Streifenkraft:

   ```
   dF_i = q * c_i * dz_i * ( CL_i * e_normal_i + CD_i * e_tangential_i )
   ```

   mit `e_tangential_i` = Einheitsvektor der lokalen Anstroemrichtung,
   `e_normal_i` = dazu senkrecht in der Streifenebene.

5. Integration:

   ```
   F_total = sum_i dF_i
   M_tack  = sum_i ( (r_i - r_tack) cross dF_i )
   ```

6. Ergebnis in bootsfeste Koordinaten transformieren.

### 7.3 Korrekturfaktoren (Q14)

Aus `config/corrections.yaml`, multiplikativ auf `CD` bzw. additiv auf `cop`:

| Faktor | Default | Begruendung |
|---|---|---|
| `cd_multiplier_mast` | 1.08 | Mast im Profil |
| `cd_multiplier_interference` | 1.05 | Fock im Nachlauf des Grosssegels |
| `cop_shift_chord_fraction` | 0.02 | CoP-Wanderung durch Maststoerung |

Diese Werte sind **Schaetzungen** und in README als solche zu kennzeichnen (`quality = 2` fuer betroffene Zeilen).

### 7.4 Angriffspunkt und Singularitaet

Gegeben `F` und `M0` am Bezugspunkt `r0`:

```
r_cop = r0 + (M0 cross F) / |F|^2
```

**Vorzeichen pruefen:** Herleitung verlangt `(r - r0) cross F = -M0`, daraus folgt obige Formel. Unit-Test: Kraft in +x, Moment +Mz um Ursprung => CoP liegt bei negativem y.

**Klammerung:** Ist `|F| < F_min` (Default `1e-6 * q * A`), dann:

- `cop_valid = 0`
- `cop_* = NaN`
- `quality` auf 2 setzen

Der CoP ist bei verschwindender Kraft singulaer. Niemals ungeklammert ausgeben.

### 7.5 Interpolation zur Laufzeit

5-dimensionale multilineare Interpolation ueber `(alpha, log(Re), camber, twist, draft)`.

- `alpha`: zyklisch (Wrap-around bei 360 Grad), **nicht** clamped
- `log(Re)`: logarithmisch, clamped an den Raendern
- `camber/twist/draft`: linear, clamped

Rekursive 1D-Lerp ueber alle Achsen (2^5 = 32 Stuetzpunkte). Ergebnisvektor der Laenge 6.

**Performance-Ziel:** < 10 us pro Lookup in Python/NumPy, < 1 us in C++ (falls spaeter ergaenzt).

---

## 8. Konfigurationsdateien

### 8.1 `config/class_rules.yaml` - MASSGEBLICH

> **Regel fuer den Agenten:** Fehlt ein Wert oder steht `null`, MUSS das Programm mit
> `MissingClassRuleError` abbrechen und die fehlende Kennung nennen. **Niemals raten.**

```yaml
meta:
  schema_version: 1
  # Quellen sind Pflichtangaben, damit spaeter nachvollziehbar ist, woher die Masse kommen.
  sources:
    int420: "https://media.sailing.org/sailing/wp-content/uploads/2022/03/17092130/420_CR_2026-03-31.pdf"
    club420: "https://club420.org/"
    optimist: "IODA Sail Plan, March 1994"
    folkboat: "Nordic Folkboat International Class Rules"

sails:

  420_MAIN:
    class: "International 420"        # TODO_HUMAN: International 420 ODER Club 420?
    source_section: "Int. 420 Class Rules 2026, Section G.3 Mainsail (S. 29)"
    luff_m: null                      # TODO_HUMAN aus G.3
    foot_m: null                      # TODO_HUMAN aus G.3
    headboard_m: null                 # TODO_HUMAN aus G.3
    roach_m: null                     # TODO_HUMAN (Achterlieksrundung)
    area_m2: null                     # TODO_HUMAN aus G.3
    tack_position_m: [0.0, 0.0, 0.95] # TODO_HUMAN: Hoehe Gooseneck ueber Deck
    mast_diameter_m: 0.048            # TODO_HUMAN pruefen
    scale_factor_luff: 0.995          # Ist-Annahme: gefahren ~99.5 % des Maximums
    scale_factor_foot: 0.97

  420_JIB:
    class: "International 420"
    source_section: "Int. 420 Class Rules 2026, Section G.4 Headsail (S. 32)"
    luff_m: null
    foot_m: null
    area_m2: null
    tack_position_m: [null, 0.0, null]
    mast_diameter_m: 0.030
    overlap_to_main: null             # TODO_HUMAN: Ueberlappung in % (LP)
    scale_factor_luff: 0.998
    scale_factor_foot: 0.985

  OPT_MAIN:
    class: "International Optimist"
    source_section: "IODA Sail Plan (March 1994), sheets 1/5-5/5"
    luff_m: null
    foot_m: null
    leech_m: null
    foot_median_m: null
    area_m2: null
    tack_position_m: [0.0, 0.0, 0.62]
    mast_diameter_m: 0.029            # Spritrig: Mast IST das Vorliek
    sprit_length_m: null
    scale_factor_luff: 0.997
    scale_factor_foot: 0.975

  FOLK_MAIN:
    class: "Nordic Folkboat"
    source_section: "Nordic Folkboat Int. Class Rules, Section 9 + Measurement Diagram"
    luff_m: null
    foot_m: null
    headboard_m: null                 # frueher gelesen: max 140 mm -> PRUEFEN
    area_m2: null                     # Gesamtsegelflaeche frueher gelesen: 24.00 m2 -> PRUEFEN
    tack_position_m: [0.0, 0.0, null]
    mast_diameter_m: null             # Holz ODER Aluminium zulaessig -> beide Varianten
    scale_factor_luff: 0.996
    scale_factor_foot: 0.972

  FOLK_JIB:
    class: "Nordic Folkboat"
    source_section: "Nordic Folkboat Int. Class Rules, Section 9"
    luff_m: null
    foot_m: null
    area_m2: null
    tack_position_m: [null, 0.0, null]
    mast_diameter_m: null
    overlap_to_main: null
    scale_factor_luff: 0.999
    scale_factor_foot: 0.988
```

### 8.2 `config/trim_matrix.yaml`

```yaml
camber:  [0.08, 0.10, 0.12, 0.14]      # f/c, 4 Stufen
twist:   [0.0, -4.0, -8.0, -12.0]      # Grad, Root-zu-Tip, 4 Stufen
draft:   [0.30, 0.42]                  # xf/c, 2 Stufen

# Twist-Verteilung ueber die Hoehe: linear von 0 am Hals bis twist-Wert am Kopf
twist_distribution: "linear"

# Camber-Verteilung ueber die Hoehe: leicht abnehmend nach oben
camber_distribution:
  root_factor: 1.00
  tip_factor: 0.80
```

### 8.3 `config/polar_grid.yaml`

```yaml
alpha:
  start_deg: -180.0
  step_deg: 1.0
  count: 360

reynolds:
  values: [150000, 237000, 375000, 594000, 940000, 1490000]   # log-verteilt, 6 Stufen
  # Begruendung: deckt Opti-Fock (niedrig) bis Folkeboot (hoch) ab

provider: "placeholder"     # "placeholder" | "xfoil"
xfoil:
  binary_path: null         # TODO_HUMAN: Pfad zum lokalen XFOIL
  n_iter: 200
  transition: "free"        # "free" | "forced@0.10"
  roughness: 0.0002
```

### 8.4 `config/corrections.yaml`

Siehe Abschnitt 7.3.

---

## 9. Umsetzungs-Meilensteine

Arbeite in dieser Reihenfolge. Nach jedem Meilenstein muessen die Tests gruen sein.

### M1 - Grundgeruest und Konfiguration
- Repository-Struktur aus Abschnitt 4
- `config.py`: Laden, Schema-Validierung, `MissingClassRuleError`
- `pyproject.toml` mit PyArrow-Abhaengigkeit, Logging, CLI-Skelett
- **Test:** Fehlende `luff_m` => klarer Abbruch mit Segel-ID und Feldname

### M2 - Geometrie
- Parametrischer Schnittgenerator (7.1)
- Silhouette aus `luff_m`, `foot_m`, `roach_m`, `headboard_m`
- Streifenraster nach Q9 (250 mm / 125 mm ueber 75 %)
- Export `geometry.parquet`, SVG, PNG
- **Test:** Kontur geschlossen, `area(sum of strips)` weicht < 2 % von `area_m2` ab
- **Test:** Rasterverfeinerung 250->125->62.5 mm aendert Gesamtflaeche < 1 %

### M3 - Polar-Provider
- `base.py` mit Protocol
- `placeholder.py`: analytisches Modell
  - `CL(alpha) = CL_max * sin(2*alpha_eff)` fuer |alpha| < alpha_stall, danach flach abfallend
  - `CD(alpha) = CD0 + k*CL^2 + CD_sep(|alpha|)`
  - `CM(alpha)` linear um -0.10 bei alpha=0
  - Alle Parameter aus Camber und Re abgeleitet, dokumentiert
- `xfoil.py`: Aufruf, Parsing, Cache auf Platte
- **Test:** Placeholder liefert monotone CL-Kurve bis Stall, CD > 0 ueberall

### M4 - Streifentheorie
- `strip.py`, `induction.py`, `interference.py`
- **Test:** Symmetrie - spiegelt man die Geometrie, aendert sich `|F|` nicht
- **Test:** `alpha = 0` und `alpha = 180` liefern nahezu reine Widerstandskraft
- **Test:** Konvergenz der Induktionsschleife in < 10 Durchlaeufen

### M5 - Backen
- `parquet.py`: Parquet-Schreiben/Lesen mit Zstandard und Schemata aus Abschnitt 6
- Summen-Tabelle voll, Streifen-Tabelle reduziert (6.2)
- `manifest.json` enthaelt Version, Metadaten und SHA-256 aller Tabellen
- **Test:** PyArrow-Roundtrip erhaelt Schluessel und finite float64-Werte bitgleich
- **Test:** Schema-, Codec- und Pruefsummenfehler werden erkannt

### M6 - Laufzeit
- `lookup.py`: 5D-Interpolation, alpha zyklisch
- `forces.py`: `F = q*A*C`, `M = q*A*c_ref*CM`, CoP nach 7.4
- **Test:** Abfrage exakt auf einem Stuetzpunkt => identisch zum Tabellenwert (1e-6)
- **Test:** Abfrage zwischen zwei Stuetzpunkten => linearer Mittelwert
- **Test:** `alpha = 359.5` interpoliert korrekt zwischen 359 und 0
- **Test:** `|F| -> 0` => `cop_valid = 0`, keine NaN in `F`
- **Benchmark:** 10.000 Lookups < 100 ms

### M7 - Paketierung
- `package_zip.py`: ZIP je Segel mit `/data`, `/docs`, `/plots`, `/examples`
- README zweisprachig (Template unten)
- `SPEC.md` (dieses Dokument) ins ZIP
- **Test:** Jedes ZIP entpackt und ohne die anderen vier nutzbar

### M8 - Validierung
- `scripts/convergence_study.py`: Streifenanzahl 12/18/26/35, Bericht als Parquet + PNG
- Plausibilitaetspruefung: Kraft vs. Windgeschwindigkeit quadratisch (R^2 > 0.999)
- Hold-out-Test: 10 zufaellige `alpha`-Werte nicht in Tabelle, gegen direkte Rechnung pruefen
- `VALIDATION.md` mit Ergebnissen

---

## 10. README-Template (pro ZIP, zweisprachig)

```markdown
# Segel-Aerodynamik: <SAIL_ID>

## Was ist das? / What is this?
Vorberechnete Kraftbeiwerte fuer <SEGEL> auf <BOOT>.
Precomputed force coefficients for <SAIL> on <BOAT>.

## Koordinatensystem / Coordinate system
x = voraus / forward, y = BACKBORD positiv / PORT positive, z = oben / up.
Rechtshaendig / right-handed. STEUERBORD IST NEGATIV / STARBOARD IS NEGATIVE.
Ursprung / origin: Mastfuss / mast foot.

## Momentenbezug / Moment reference
Hals des Segels / sail tack: <x, y, z> m

## Dateien / Files
| Datei | Inhalt |
|---|---|
| data/manifest.json | Formatversion, Metadaten, Schemata und Pruefsummen |
| data/sums.parquet | vollstaendige Summen, siehe SPEC.md Abschnitt 6.1 |
| data/strips.parquet | Streifenlasten, reduzierte Aufloesung |
| data/geometry.parquet | Blattmasse je Streifen |
| data/sums_pivot.parquet | Kontrollansicht |
| plots/polar.png | CL/CD ueber alpha |
| plots/load_distribution.png | Lastverteilung ueber die Hoehe |
| examples/example_lookup.py | Minimalbeispiel |
| docs/SPEC.md | vollstaendige Spezifikation |

## Genauigkeit / Accuracy
Erwartet / expected: +/-15-20 % auf die Summenkraft.
Der Angriffspunkt streut staerker als die Kraft.
Spalte `quality`: 0 = belegt, 1 = plausibel, 2 = geschaetzt.

## Grenzen / Limitations
- Rein aerodynamisch, keine Hydrodynamik
- Starre Segelform (keine FSI)
- Interferenz nur als pauschaler Faktor
- Keine Aussage ueber Naehte, Tuchspannung, Flattern
```

---

## 11. Qualitaetsregeln fuer den Agenten

1. **Keine erfundenen Zahlen.** Jede Klassenvorschrift-Groesse kommt aus `class_rules.yaml`. Ist sie `null`, Abbruch.
2. **Keine stillen Defaults.** Jeder Default steht sichtbar in einer Config-Datei.
3. **Fail loud.** Lieber ein klarer Fehler als ein plausibles falsches Ergebnis.
4. **Docstrings mit Einheiten.** Jede Funktion, die Meter, Grad oder Newton nimmt, sagt es im Docstring.
5. **Tests vor Features.** Kein Meilenstein gilt als fertig ohne die genannten Tests.
6. **Keine Netzwerkaufrufe** im Paket.
7. **Reproduzierbarkeit.** Alle Zufallsquellen geseedet, Zeitstempel aus Config.
8. **Deutsch/Englisch trennen.** Code und Identifier englisch, Dokumentation zweisprachig.

---

## 12. Offene Punkte fuer den Menschen (`TODO_HUMAN`)

Diese Punkte MUSS der Auftraggeber entscheiden oder nachtragen. Der Agent legt sie als `null` an und bricht bei Benutzung ab.

| Nr. | Punkt | Warum kritisch |
|---|---|---|
| 1 | **Welche 420-Klasse?** International 420 oder Club 420 | Unterschiedliche Segelflaechen (Int. 420 vs. Club 420: Gross 7.40 m2, Fock 2.80 m2 laut Club-420-Quelle). Falsche Wahl => alle Kraefte falsch |
| 2 | Segelmass Int. 420 Gross | Abschnitt G.3, S. 29 der Vorschrift 2026 |
| 3 | Segelmass Int. 420 Fock | Abschnitt G.4, S. 32 |
| 4 | Segelmass Optimist | IODA Sail Plan, 5 Blaetter |
| 5 | Segelmass Folkeboot Gross + Fock | Abschnitt 9 + Vermessungsdiagramm |
| 6 | Gooseneck-Hoehe je Boot | Bezugspunkt fuer Momente |
| 7 | Fock-Hals-Position je Boot | Bezugspunkt fuer Momente |
| 8 | Mastdurchmesser (Folkeboot: Holz oder Alu?) | Korrekturfaktor |
| 9 | Ueberlappung Fock/Grosssegel (LP %) | Interferenz |
| 10 | XFOIL-Pfad, falls echter Provider gewuenscht | sonst nur Placeholder |

---

## 13. Abnahmekriterien (Definition of Done)

- [ ] Fuenf ZIP-Dateien erzeugt, jede eigenstaendig entpackbar und nutzbar
- [ ] Jedes ZIP enthaelt `/data`, `/docs`, `/plots`, `/examples` und README (DE+EN)
- [ ] `sums.parquet` enthaelt 360 x 6 x 4 x 4 x 2 = 69.120 Zeilen je Segel
- [ ] `strips.parquet` enthaelt 360 x 2 x 3 x n_strips Zeilen je Segel
- [ ] Alle Parquet-Dateien verwenden Zstandard; Schema und Metadaten stimmen mit `manifest.json` ueberein
- [ ] Es werden weder CSV-Dateien noch eigene Tabellen-Binaerdateien erzeugt
- [ ] Alle Tests aus M1-M8 gruen
- [ ] Benchmark: 10.000 Lookups < 100 ms
- [ ] `VALIDATION.md` mit Konvergenzstudie und Hold-out-Ergebnissen
- [ ] Kein `null` aus `class_rules.yaml` wird stillschweigend durch einen Default ersetzt
- [ ] README nennt Koordinatensystem, Momentenbezug, Genauigkeit und Grenzen

---

## 14. Erwartete Ergebnisgroessen (Plausibilitaetsanker)

Diese Werte stammen aus einem Lauf mit **Platzhalter-Physik** und dienen nur der Plausibilitaetspruefung, nicht als Referenz. Weicht das Endergebnis um mehr als Faktor 2 ab, liegt ein Fehler vor.

| Segel | v = 3 m/s | v = 5 m/s | v = 7 m/s |
|---|---|---|---|
| 420 Gross | ~60 N | ~170 N | ~340 N |
| 420 Fock | ~20 N | ~60 N | ~120 N |
| Opti Gross | ~30 N | ~80 N | ~160 N |

Quadratische Skalierung mit `v` muss gelten. Maximale Kraft im 360-Grad-Sweep typischerweise bei `alpha` um -100 Grad (Segel breitseitig zum Wind).
