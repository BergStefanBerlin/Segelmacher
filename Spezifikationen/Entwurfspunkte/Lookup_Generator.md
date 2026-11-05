# Entwurfspunkte: Lookup-Generator

Stand: 05.11.2026  
Status: am 05.11.2026 durch ausdruecklichen Umsetzungsauftrag uebernommen.

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
| IDEE-S-00003 | offen | CLI-, Auftrags- und Ergebnisvertrag einschliesslich Entwurf/Bereit-Status, CSV-Praezision, Fehlercodes und atomarer Ergebnisablage bestaetigen. |
| IDEE-S-00004 | teilweise geklaert | Nutzer hat C++/CMake und die vorgeschlagene GPU-Richtung akzeptiert. Konkrete Distribution, Backend-Technologie und direkte Bibliotheksschnittstelle bleiben festzulegen. Noch keine exportierten C++-Schnittstellen oder CPPI-Kennungen. |
| IDEE-S-00005 | teilweise geklaert | Nutzerentscheidung: Windneigung von 0 bis einschliesslich 90 Grad in 10-Grad-Schritten aufnehmen. Winkel-/Poldefinition und Ergebnisvertrag stehen in Abschnitt 4.3 der Spezifikation. Geeigneter raeumlicher Provider noch festzulegen; negative Elevationen nicht abgedeckt. Uebrige Startparameter und Toleranzen offen: sechs Re-Stufen, 1-Grad-Azimutraster, Hoehenraster 0.25/0.125 m, Verfeinerung 1/2/4, absolut 0.0001 plus relativ 0.01. Keine physikalische Genauigkeitszusage. |

Bei Finalisierung angenommene Punkte neuen A-S-Anforderungen zuordnen.
Nicht angenommene Punkte unter ihrer Kennung mit Entscheidung erhalten.

Nachtrag zu IDEE-S-00005 (Nutzerentscheidung 05.11.2026): absolute
Beiwerttoleranz 0.0005, relative Toleranz weiterhin 0.01. Der fruehere
Startvorschlag 0.0001 ist damit fuer neue Laeufe ersetzt.
