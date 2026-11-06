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
| CPPI-0005 | `segel::Context::solve(double,double,double,int,bool,int) const` | implementiert | Azimut beliebig zyklisch [deg], Elevation [0,90] deg, Re_ref positiv im Modellbereich, Faktor 1/2/4, Lastausgabe an/aus (Default true), Traglinienmoden 12/16/24/32/48/64 (Default 48). LU-Faktorisierungen werden je Context und Modenzahl wiederverwendet; RHS-Pivotierung bleibt mathematisch konsistent. UTF-8-JSON mit CF/CM und optional Einzelbeitraegen; q*S bzw. q*S*c_ref. Fehler werfen `std::exception`, keine Ersatzwerte. |
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
