# C++-Schnittstellenregister

Vertrag 1, freigegeben durch den Implementierungsauftrag vom 05.11.2026.
Header: `Code/Core/include/segel/core.hpp`. Keine GUI-/Dateisystemabhaengigkeit.
Private Implementierungsdetails und Drittbibliothekstypen werden nicht exportiert.

| ID | Name | Status | Vertrag |
|---|---|---|---|
| CPPI-0001 | `segel::Context` | implementiert | Nicht kopierbarer Besitzer eines unveraenderlichen Einzelgeometriemodells; Lebensdauer bis Destruktor. Keine Segelinterferenz. |
| CPPI-0002 | `segel::Context::Context(std::string_view)` | implementiert | UTF-8-JSON mit Geometrie, numerik und expliziten Modellparametern gemaess Modellfestlegung; kopiert Daten, keine Referenzuebernahme. Wirft `std::exception` bei ungueltigen Werten. |
| CPPI-0003 | `segel::Context::~Context()` | implementiert | Gibt ausschliesslich eigenen Speicher frei, keine Exceptions oder externen Seiteneffekte. |
| CPPI-0004 | `segel::Context::geometry() const` | implementiert | UTF-8-JSON mit S [m²], c_ref [m], Hoehe, Streifen/Profilen je Faktor 1/2/4, Referenz X voraus/Y backbord/Z oben um Hals. |
| CPPI-0005 | `segel::Context::solve(double,double,double,int,bool,int) const` | implementiert | Azimut beliebig zyklisch [deg], signierte Elevation [-90,+90] deg mit direkter Vektorberechnung und beidseitiger `abs(beta)`-Ueberblendung, Re_ref positiv im Modellbereich, Faktor 1/2/4, Lastausgabe an/aus (Default true), Traglinienmoden 12/16/24/32/48/64 (Default 48). LU-Faktorisierungen werden je Context und Modenzahl wiederverwendet; RHS-Pivotierung bleibt mathematisch konsistent. UTF-8-JSON mit CF/CM und optional Einzelbeitraegen; q*S bzw. q*S*c_ref. Werte ausserhalb des Winkelbereichs sowie nicht endliche Eingaben werfen `std::exception`, keine Ersatzwerte. |
| CPPI-0006 | `segel::polar(std::string_view)` | implementiert | JSON mit m,p,alpha_deg,re und expliziten Parametern; liefert CL/CD/CM und Fourier-Koeffizienten. Re-Bereich, Einheiten und Vorzeichen gemaess Modellfestlegung. |
| CPPI-0007 | `segel::resultant(std::string_view)` | implementiert | JSON mit c_ref>0 und loads [{f,r,m}]; f=CF, r[m], m=CM_eigen. Liefert CF/CM/point/rest/valid; Kraftschwelle 1e-10. Auch reine Momente zulaessig. |
| CPPI-0008 | `segel::transform(std::string_view)` | implementiert | JSON mit loads, orthonormaler eigentlicher 3x3 rotation und translation[m]; transformiert r sowie freie f/m, behaelt segel_id. Keine Aerodynamik. |

Alle Funktionen sind synchron, ohne globale veraenderliche Zustandsdaten,
Dateizugriffe, Threads, Prozessausgabe oder GPU. Unabhaengige Aufrufe und
konstante Context-Aufrufe sind parallel sicher. JSON-Zahlen sind endliche
binary64-Werte; unerwartete/fehlende Felder werden nicht als erfolgreiche
Berechnung interpretiert. Aufrufer besitzt zurueckgegebene UTF-8-Strings.
Argumente werden nur waehrend des Aufrufs gelesen. Keine ABI-Stabilitaetszusage
ueber Compiler-/Standardbibliotheksversionen; statische Bibliothek.
Kopieren des Context ist geloescht und kein eigener Export.
Vertragstests: `Code/Core/tests/core_tests.cpp`.

## JSON-Feldvertrag (Version 1)

Context-Eingabe: genau die Pflichtobjekte `geometrie` (Geometrie 0.1),
`numerik` (Mittelpunkt-/Hoehenraster gemaess Beispielauftrag) und `parameter`
(H1-Parameter gemaess Modellfestlegung). Keine impliziten Modellparameter.
Die CLI prueft zusaetzlich die vollstaendige Auftragshuelle; Context prueft
Geometrie/Physik unabhaengig davon.

`solve`/`resultant` liefern:
`CF[3]`, `CM[3]`, `point[3]` oder null, `rest[3]`, `valid` (bool).
Mit Lastausgabe zusaetzlich `loads`, jeder Eintrag:
`id` (nichtnegativer Index pro Fall), `segel_id`, `kind`,
`f[3]` (CF), `r[3]` (Meter relativ Hals), `m[3]` (CM_eigen).
`kind`: `gebunden_plus_profilmoment` bzw.
`mittelflaechen_impuls_reibung`. `resultant` benoetigt nur f/r/m.
`transform` erhaelt alle anderen Felder unveraendert. Transformation des
Gesamtmoments erfolgt durch erneute Integration um den Zielursprung.
Eine Transformation erzeugt keine Segelinterferenz.

`geometry` liefert `segel_id`, `S_m2`, `c_ref_m`, `hoehe_m`, `grids`.
`grids` enthaelt `"1"`, `"2"`, `"4"` als Arrays mit:
`profil_id,z,front,chord,dz,m,p,twist,a0,cm`. Laengen Meter, Twist Grad,
a0 Radiant, cm lokales +Z-Viertelsehnenmoment; m/p dimensionslos.
Beiwoerte sind auf Gesamt-S/c_ref normiert, Profilpolaren dagegen auf
lokale Sehne. `polar` liefert CL (lokale positive Normalkraft),
CD (entlang Anstroemung), CM (lokale +Z-Achse), a0/A1/A2 (Radiant).

## Vorbereitetes Segelmodell: Vertrag 1

Die folgenden konkreten Schnittstellen sind fuer den beauftragten
Segelmodellumfang implementiert, aber numerisch nicht abgenommen.
Sie bilden eine von CPPI-0001 bis CPPI-0008
getrennte Bibliothek; der eigenstaendige H1-Core bleibt ohne Arrow und GUI
baubar. Massgeblich sind Abschnitt 13 der Spezifikation und die dort
festgelegte Modellgueltigkeit.
Fuer neu erzeugte Modelle gilt `N = ceil(H / 0.25 m)` gleich hohe
Streifen; je Streifen ist genau ein Windvektor zu uebergeben. Das ersetzt
die zuvor vorgesehene 0,5-m-Einteilung. Die numerische Abnahme bleibt offen.

| ID | Name | Status | Vertrag |
|---|---|---|---|
| CPPI-0009 | `segel::modell::Vektor` | implementiert; Abnahme offen | `std::array<double,3>`; x/y/z im segelfesten rechtshaendigen System. Einheiten ergeben sich aus dem jeweiligen Feld. |
| CPPI-0010 | `segel::modell::StreifenId` | implementiert; Abnahme offen | `std::uint32_t`; modellintern eindeutig und bei identischer Neuerzeugung stabil. |
| CPPI-0011 | `segel::modell::Punkt` | implementiert; Abnahme offen | `id` und `position_m` (m, Viertelsehne auf Streifenmitte). |
| CPPI-0012 | `segel::modell::Wind` | implementiert; Abnahme offen | `id` und `stroemung_m_s` (m/s, Richtung der Luftstroemung). |
| CPPI-0013 | `segel::modell::Last` | implementiert; Abnahme offen | `id`, `kraft_N` und vollstaendiges `moment_Nm` um den zugehoerigen Abfragepunkt; keine Doppelzaehlung mit Einzelbeitraegen. |
| CPPI-0014 | `segel::modell::Fehlerart` | implementiert; Abnahme offen | Kategorien `eingabe`, `modell`, `integritaet`, `gueltigkeit` und `numerik`. |
| CPPI-0015 | `segel::modell::Fehler` | implementiert; Abnahme offen | `std::runtime_error` mit typisierter Kategorie und Kontext im `what()`-Text. |
| CPPI-0016 | `segel::modell::Fehler::Fehler(Fehlerart,const std::string&)` | implementiert; Abnahme offen | Uebernimmt Kategorie und Kontext; allokationsbedingte Standardausnahmen bleiben moeglich. |
| CPPI-0017 | `segel::modell::Fehler::art() const` | implementiert; Abnahme offen | Gibt die Fehlerkategorie ohne Seiteneffekte und ohne Ausnahme zurueck. |
| CPPI-0018 | `segel::modell::Modell` | implementiert; Abnahme offen | Nicht kopierbarer, unveraenderlicher Besitzer eines geladenen Einzel-Segelmodells. Lebensdauer bis Destruktion; unabhaengige konstante Aufrufe sind parallel sicher. |
| CPPI-0019 | `segel::modell::Modell::~Modell()` | implementiert; Abnahme offen | Gibt eigenen Speicher frei; keine Dateiaenderungen oder Ausnahmen. |
| CPPI-0020 | `segel::modell::Modell::punkte() const` | implementiert; Abnahme offen | Liefert eine dem Aufrufer gehoerende Kopie der Punkte nach ID aufsteigend; neu erzeugte Modelle verwenden `ceil(H / 0.25 m)` gleich hohe Streifen. |
| CPPI-0021 | `segel::modell::Modell::berechnen(const std::vector<Wind>&,double,double) const` | implementiert; Abnahme offen | Genau ein endlicher Wind bis 50 m/s je ID, Reihenfolge beliebig; Dichte kg/m³ und kinematische Viskositaet m²/s innerhalb der manifestierten Bereiche. Liefert dem Aufrufer gehoerende Lasten nach ID sortiert. Ungueltige/fehlende/doppelte/fremde IDs, Werte ausserhalb des Bereichs und numerische Fehler werfen `Fehler` mit Kontext; kein Teilergebnis. Keine Dateizugriffe. |
| CPPI-0022 | `segel::modell::laden(const std::filesystem::path&)` | implementiert; Abnahme offen | Laedt aus einem Modellordner, prueft Manifest, Version, Hashes, Tabellen, gleichmaessige Geometrie und `ceil(H / 0.25 m)` Streifen, Luftbereiche und Matrixloesbarkeit und liefert exklusiven Besitzer `std::unique_ptr<const Modell>`. Dateifehler, Integritaets-/Versionsfehler und numerische Fehler werfen `Fehler` mit Kategorie und Kontext. Kein Schreiben; keine ABI-Zusage ueber Compiler-/Standardbibliotheksversionen. |
| CPPI-0023 | `segel::modell::Transformation` | implementiert; Abnahme offen | Starre Segel-zu-Ziel-Transformation. `rotation` enthaelt zeilenweise die drei Zielachsen als Vektoren im Segelrahmen; transformierter Vektor ist das Skalarprodukt jeder Zeile mit dem Eingangsvektor. `translation_m` liegt im Zielrahmen. Die Rotation muss endlich, orthonormal und rechtshaendig sein; die Translation muss endlich sein. |
| CPPI-0024 | `segel::modell::SdpSummen` | implementiert; Abnahme offen | Kompakte Kennzahlen eines Segels im durch CPPI-0023 definierten Zielrahmen: `segel_id`, `Q_N = Σ Fy`, `B_N = Σ |Fy|`, `X_Nm = Σ(x·Fy)` und `Z_Nm = Σ(z·Fy)`. `Fy` ist die Zielrahmen-y-Komponente jeder intern berechneten Einzelkraft; x/z sind die transformierten Angriffspunkte relativ zum Zielrahmenursprung. B summiert die Einzelbetragswerte, nicht den Betrag von Q. |
| CPPI-0025 | `segel::modell::Berechnung` | implementiert; Abnahme offen | Dem Aufrufer gehoerende Streifenlasten und die SDP-Summen dieses einzelnen Segels. `streifenlasten` entspricht CPPI-0021; `sdp.segel_id` identifiziert das Modell. |
| CPPI-0026 | `segel::modell::Modell::berechnen_mit_sdp(const std::vector<Wind>&,double,double,const Transformation&) const` | implementiert; Abnahme offen | Berechnet dieselben Streifenlasten wie CPPI-0021 und akkumuliert zusaetzlich Q/B/X/Z waehrend der Kraftbildung in einem Durchlauf. Die Transformation wirkt ausschliesslich auf die SDP-Auswertung, nicht auf Wind, Physik oder Streifenlasten. Es werden keine Einzelkraftlisten zurueckgegeben oder dauerhaft gespeichert. Unzulaessige Transformationen werfen `Fehlerart::eingabe`; nicht darstellbare Summen werfen `Fehlerart::numerik`. |

Alle Operationen sind synchron; Argumente werden nur fuer den Aufruf
gelesen. Ein geladenes Modell speichert Geometrie und Faktorisierung;
jede Rechnung besitzt ihre temporaeren Daten. Fehlende physikalische
Messvalidierung wird nicht als erfuellt behauptet. Die Vertragstests
muessen gueltige Nutzung, Grenzwerte, Dateifehler, parallele Nutzung und
alle Fehlerkategorien nachweisen. CPPI-0026 veraendert weder bestehende
Modellpakete noch die Semantik von CPPI-0021.
