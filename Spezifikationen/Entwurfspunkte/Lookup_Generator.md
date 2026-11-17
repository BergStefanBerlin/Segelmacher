# Entwurfspunkte: Lookup-Generator

Stand: 15.11.2026
Status der Ursprungsliste: am 05.11.2026 durch ausdruecklichen
Umsetzungsauftrag uebernommen. Die Status der spaeteren Erweiterungen
stehen in ihren jeweiligen Abschnitten.

Diese Liste ergaenzt die
[Spezifikation](../Spezifikation_Lookup_Generator.md).
Die nachfolgende Ursprungsliste bleibt als Historie erhalten; finale
Anforderungen stehen nur in der freigegebenen Spezifikation.

| Herkunft | Entscheidung / finale Anforderungen |
|---|---|
| IDEE-S-00001 | Angenommen; A-S-00006 bis A-S-00015, A-S-00025, A-S-00028. H1 technisch festgelegt; physikalische Validierung offen. |
| IDEE-S-00002 | Angenommen; A-S-00016/A-S-00017. CPU implementiert; echter GPU-Backend und Hardwarenachweis offen. |
| IDEE-S-00003 | Angenommen; A-S-00001 bis A-S-00005, A-S-00013 bis A-S-00015, A-S-00020 bis A-S-00024, A-S-00029/A-S-00030. |
| IDEE-S-00004 | Angenommen; A-S-00026; C++17/CMake, statischer Core, Windows-CLI; CPPI-0001 bis CPPI-0008. |
| IDEE-S-00005 | Angenommen; A-S-00007/A-S-00012/A-S-00018/A-S-00019. Startraster/Toleranzen unveraendert; numerische Abnahme separat zu belegen. |

## Historische Entscheidungen vor dem Implementierungsauftrag

| Kennung | Status | Entscheidung zur Freigabe |
|---|---|---|
| IDEE-S-00001 | Umfang entschieden, technische Ausarbeitung offen | Nutzerentscheidung: physikalisches Hybridmodell mit der Faehigkeitseinordnung aus Abschnitt 5.1 im Erstumfang; kein vorgeschalteter synthetischer Ersatzmodell-Rechenpfad. Der fruehere Testmodell-zuerst-Vorschlag ist zurueckgezogen. Zunaechst Einzel-Lookups; Grosssegel-/Fock-Interferenz folgt spaeter ueber einen Konfigurationsauftrag mit Einbaulagen und Segelstellungsrastern (Abschnitt 5.3). Jetzt internen Rechenkern auf mehrere identifizierbare Segelflaechen und getrennte Transformationen vorbereiten; keine Einbaulagen in intrinsische Geometrien aufnehmen. Konkrete Verfahren, Schliessungen und Referenzpruefungen werden technisch ausgearbeitet, ohne den vereinbarten Umfang erneut zur Auswahl zu stellen. |
| IDEE-S-00002 | teilweise geklaert | Nutzerentscheidung: GPU-Eignung je Funktion fachlich einschaetzen, geeignete Funktionen bei vorhandener unterstuetzter GPU dort ausfuehren; vorerst keine Speedpruefung. Der fruehere Vorschlag einer 10-Prozent-Laufzeitschwelle ist zurueckgezogen. Offen bleiben die finale Freigabe des gesamten Ressourcenvertrags und die konkrete Backend-Technologie. |
| IDEE-S-00003 | offen | CLI-, Auftrags- und Ergebnisvertrag einschliesslich Entwurf/Bereit-Status, Ergebnisformatversion, Fehlercodes und atomarer Ergebnisablage bestaetigen. |
| IDEE-S-00004 | teilweise geklaert | Nutzer hat C++/CMake und die vorgeschlagene GPU-Richtung akzeptiert. Konkrete Distribution, Backend-Technologie und direkte Bibliotheksschnittstelle bleiben festzulegen. Noch keine exportierten C++-Schnittstellen oder CPPI-Kennungen. |
| IDEE-S-00005 | teilweise geklaert | Nutzerentscheidung: Windneigung von 0 bis einschliesslich 90 Grad in 10-Grad-Schritten aufnehmen. Winkel-/Poldefinition und Ergebnisvertrag stehen in Abschnitt 4.3 der Spezifikation. Geeigneter raeumlicher Provider noch festzulegen; negative Elevationen nicht abgedeckt. Uebrige Startparameter und Toleranzen offen: sechs Re-Stufen, 1-Grad-Azimutraster, Hoehenraster 0.25/0.125 m, Verfeinerung 1/2/4, absolut 0.0001 plus relativ 0.01. Keine physikalische Genauigkeitszusage. |

Bei Finalisierung angenommene Punkte neuen A-S-Anforderungen zuordnen.
Nicht angenommene Punkte unter ihrer Kennung mit Entscheidung erhalten.

Nachtrag zu IDEE-S-00005 (Nutzerentscheidung 05.11.2026): absolute
Beiwerttoleranz 0.0005, relative Toleranz weiterhin 0.01. Der fruehere
Startvorschlag 0.0001 ist damit fuer neue Laeufe ersetzt.

## Erweiterung: kompaktes Lookup-Format (06.11.2026)

Status: **offener Spezifikationsentwurf, nicht zur Umsetzung freigegeben**.
Die historischen Freigaben oben bleiben unveraendert. Neue Punkte beziehen
sich auf [Abschnitt 11](../Spezifikation_Lookup_Generator.md#11-kompaktes-parquet-ausgabeformat).
Noch keine A-S-, U-S- oder CPPI-Kennungen.

| Kennung | Status | Entscheidung vor Finalisierung |
|---|---|---|
| IDEE-S-00006 | Angenommen; A-S-00031 bis A-S-00036 | Parquet/Zstandard als einziges Tabellenformat festgelegt; Apache Arrow 21.0.0 fuer C++17 gepinnt. Selektive Row-Group-Zugriffe und Ressourcenwerte werden praktisch abgenommen. |
| IDEE-S-00007 | Angenommen; A-S-00031 und A-S-00033 | Genau sechs Ergebnisdateien, drei Parquet-Tabellen, Manifest und Berichte. CSV-Ausgabe, CSV-Export und Formatwahl werden entfernt; bestehende CSV-Ergebnisse werden nicht automatisch migriert. |
| IDEE-S-00008 | Teilweise angenommen; Messwerte offen; A-S-00034 | Messung auf Tabellenkopie mit Dateigroesse, Erzeugung, Vollscan, kalten/warmen selektiven Abfragen, p95 und Spitzen-RAM. Kein CSV-Produktvergleich; quantitative Grenzen erst nach Messung festlegen. |
| IDEE-S-00009 | CORE-Konsumentensicht angenommen; Umsetzung separat | Lookup ist ein aerodynamisches Segel-Lookup, keine allgemeine Windfelddatei. Der Konsumentenpfad transformiert relativen Wind einschliesslich Bootsbewegung; der Core wertet Lookup-Daten derzeit noch nicht als Windphysik aus. Keine Spiegelung/Begrenzung negativer Windneigungen und keine behauptete Grosssegel-/Fock-Wechselwirkung. Keine Core-Aenderung beauftragt. |

## Erweiterung: negative lokale Windneigung (14.11.2026)

Status: **am 14.11.2026 finalisiert und zur Umsetzung freigegeben.** Die
angenommenen Punkte sind den Anforderungen A-S-00039 bis A-S-00042
zugeordnet. Die Core-Bestaetigung der vier Vertragsdetails ist in Abschnitt
12.4 der [Spezifikation](../Spezifikation_Lookup_Generator.md#12-vorzeichenbehaftete-lokale-windneigung)
dokumentiert. Umsetzungsstand und Abnahmen stehen im
[Plan](../../Umsetzungsplaene/Entwurf_Lookup_Generator.md#10-vorzeichenbehaftete-lokale-windneigung).

| Kennung | Status | Nutzerentscheidung und verbleibende Klaerung |
|---|---|---|
| IDEE-S-00010 | Angenommen; A-S-00039 | -90 bis +90 Grad direkt mit vorzeichenbehaftetem 3D-Wind berechnen; beide Pole exakt und azimutinvariant. H1-Grenzen einschliesslich beidseitiger Crossflow-Ueberblendung sind dokumentiert; keine Lastspiegelung. |
| IDEE-S-00011 | Angenommen; A-S-00040/A-S-00041 | Auftraege auf die 19-Werte-Achse umstellen, alte Lookup-Formatversionen nicht unterstuetzen, neue Lookups mit Version 3 ausgeben; Ausgabeachse, Ressourcenlimit und Fehlersperre nachweisen. Die fuenf produktiven Auftraege im separaten Modelle-Repository sind umgestellt und ihre Lookups atomar durch gepruefte Version-3-Ausgaben ersetzt. |
| IDEE-S-00012 | Angenommen; A-S-00041 | Core bestaetigt Referenzpunkt Segelhals mit Translation/Rotation, lineare log(Re)-Interpolation ohne Extrapolation, vollstaendige Halsmomententransformation und Lookup-Formatversion 3 ohne Legacy-Leser. |
| IDEE-S-00013 | Angenommen; A-S-00042 | Numerik, Parquet-Integration, Bestandsregression und Core-Vertragstests sind verpflichtend; physikalische Validierung ist nicht verlangt und darf die Nutzbarkeit nicht einschraenken. |

## Erweiterung: vorbereitetes Segelmodell fuer lokale Windfelder (15.11.2026)

Status: **am 15.11.2026 auf Nutzerauftrag finalisiert; Umsetzung nicht
beauftragt.** Die Anforderungen stehen in
[Abschnitt 13](../Spezifikation_Lookup_Generator.md#13-vorbereitetes-segelmodell-fuer-lokale-windfelder),
ihre Umsetzungsschritte im
[Plan](../../Umsetzungsplaene/Entwurf_Lookup_Generator.md#11-vorbereitetes-segelmodell-fuer-lokale-windfelder).
Die alten Kennungen bleiben als Herkunft erhalten. Das festgelegte
Verfahren und seine Abnahme sind noch nicht implementiert oder nachgewiesen.

| Kennung | Status und Anforderungen | Entscheidung und verbleibende Nachweise |
|---|---|---|
| IDEE-S-00014 | Angenommen; A-S-00043/A-S-00048/A-S-00049/A-S-00052 | H1-Kopplung und Geometrie-/Windabhaengigkeit in 13.8 festgelegt; Loesbarkeit, H1-Paritaet und Konvergenz noch nachzuweisen. |
| IDEE-S-00015 | Angenommen; A-S-00044 bis A-S-00046/A-S-00055 | Gesamte Konturhoehe, gleich hohe Streifen, eindeutige Viertelsehne und interne Unterteilung festgelegt; Integration und ungueltige Schnitte noch zu pruefen. |
| IDEE-S-00016 | Angenommen; A-S-00047 | Lineare Hoeheninterpolation mit konstanter Fortsetzung festgelegt; Anwendung an allen internen Punkten und Rasterabnahme noch zu pruefen. |
| IDEE-S-00017 | Angenommen; A-S-00049 bis A-S-00051/A-S-00070 | 0--50 m/s, analytisches Niedrig-Re-Modell und lokale Nullgeschwindigkeit festgelegt; Grenzuebergaenge und geforderter Bereich noch nachzuweisen. |
| IDEE-S-00018 | Angenommen; A-S-00054 bis A-S-00058/A-S-00066 bis A-S-00069/A-S-00072 | JSON/Parquet-Schema, Integritaet, Schutz und spaetere Abloesung festgelegt; alte externe Dateien bleiben unberuehrt. Roundtrip, Determinismus und Publikation noch zu pruefen. |
| IDEE-S-00019 | Angenommen; A-S-00052/A-S-00053/A-S-00059/A-S-00060 | Typisierter Laufzeitvertrag, unveraenderliches Modell und Fehlerkategorien festgelegt; konkrete Exporte mit CPPI-IDs sind vor ihrer Implementierung zu definieren und zu testen. Eine Core-Freigabe konkreter Signaturen liegt nicht vor. |
| IDEE-S-00020 | Angenommen; A-S-00061/A-S-00062/A-S-00064 | Vergleichsnorm, Bezug, additive Toleranz und Referenz festgelegt; H1-Referenzkonvergenz und Einhaltung von 0,1 %/5 % noch nachzuweisen. |
| IDEE-S-00021 | Angenommen; A-S-00063 bis A-S-00065/A-S-00069/A-S-00070 | Testsegel, Winkel, Null-/Gradientfaelle und Messungen festgelegt; numerische Abnahme und Leistungsmessung fehlen, fehlende physikalische Validierung ist separat sichtbar. |
