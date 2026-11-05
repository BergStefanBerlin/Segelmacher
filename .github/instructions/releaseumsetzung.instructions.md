---
description: "Use when preparing or implementing a Segelmacher version: maintain the authoritative specification, implementation plan, milestones, evidence, and acceptance."
applyTo:
  - "Spezifikationen/**/*.md"
  - "Umsetzungsplaene/**/*.md"
---
# Arbeitsanweisung Releaseumsetzung

Diese Instruction regelt die fachliche Vorbereitung und Umsetzung eines
Versionsschritts. Releaseumsetzung umfasst Spezifikation, Umsetzungsplan,
Implementierung und Nachweise; sie ist keine Freigabe für einen GitHub-Release.

## Verbindliche Dokumentstruktur

- Pro sachlogischem Thema genau eine maßgebliche Spezifikationsdatei führen.
  Bestehende Spezifikationen desselben Themas fortführen, statt parallele
  Hauptdateien anzulegen.
- Für jeden Versionsschritt genau einen maßgeblichen Umsetzungsplan führen.
  Umsetzungsschritte, Abhängigkeiten, Meilensteine und tatsächliche Stände
  dieses Schritts gehören in diesen Plan.
- Vor dem Anlegen oder Ändern vorhandene Dateien im passenden Themen- oder
  Versionsordner prüfen. Eindeutige Hauptdateien nicht stillschweigend
  ersetzen, aufteilen oder umbenennen. Bei konkurrierenden Kandidaten erst
  die Abgrenzung klären.
- Entwürfe deutlich als nicht freigegeben kennzeichnen. Offene fachliche
  Punkte in einer separaten Entwurfspunkteliste festhalten; sie begründen
  weder Anforderungen noch eine Umsetzung.
- Anforderungen erst nach ausdrücklicher Freigabe finalisieren und gemäß der
  [Nummerierungs-Instruction](nummerierung.instructions.md) kennzeichnen.
- Unterstützende Nachweise wie Testergebnisse und Messdaten dürfen separat
  liegen, sind aber keine zusätzlichen Spezifikationen oder Pläne.
- Änderungen an öffentlichen C++-Schnittstellen zusätzlich im
  [Schnittstellenregister](../../Dokumentationen/Schnittstellenregister_CPP.md)
  und gemäß der
  [Schnittstellen-Instruction](schnittstellenvertrag.instructions.md)
  dokumentieren.

## Nachverfolgbarkeit und Abnahme

- Jede aktive Anforderung muss ein prüfbares Ergebnis oder Abnahmekriterium
  haben. Jeder Umsetzungsschritt verweist auf mindestens eine passende
  Anforderung.
- Pläne weisen tatsächliche Abhängigkeiten und eine umsetzbare Reihenfolge
  aus. Meilensteine erhalten überprüfbare Eintritts- und Abnahmekriterien.
- Umsetzung, Test, Integration und fachliche Abnahme getrennt bewerten.
  Status nur mit passenden Nachweisen ändern; fehlende Belege und offene
  Punkte ausdrücklich dokumentieren.
- Zurückgezogene oder verworfene Einträge nicht stillschweigend löschen:
  ihre ursprüngliche Kennung und Begründung erhalten. Übergänge ins Backlog
  während einer laufenden Umsetzung nur mit Nutzerzustimmung.
- Ideen und historische Dokumente sind Quellen, aber keine automatisch
  freigegebenen Anforderungen. Verbindlich sind nur die finalisierte
  Spezifikation und der zugehörige freigegebene Plan.
- Umfang, fachliche Verträge oder Abnahmekriterien nicht eigenständig
  erweitern oder ändern; dafür die erforderliche Nutzerentscheidung einholen.

## Umsetzung und Abschluss

- In der Reihenfolge des freigegebenen Plans arbeiten und ihn mit tatsächlichem
  Status, Testergebnissen und Belegen fortschreiben. Geplante oder teilweise
  erledigte Arbeit nicht als abgeschlossen darstellen.
- Zum Abschluss aktive Anforderungen gegen Plan, Tests und Nachweise
  abgleichen. Nicht erledigte Punkte, bekannte Abweichungen und fehlende
  Belege sichtbar lassen; keine Vollständigkeit ohne Beleg behaupten.
- Commit, Push, Tag und Veröffentlichung richten sich ausschließlich nach
  der [Release-Instruction](release.instructions.md) und benötigen den dort
  festgelegten ausdrücklichen Auftrag.
