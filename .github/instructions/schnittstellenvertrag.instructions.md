---
description: "Use when adding, changing, reviewing, or documenting any public C++ API, Core-GUI boundary, exported type, function, or method in Segelmacher."
applyTo:
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cpp"
  - "**/*.cc"
  - "**/*.cxx"
  - "Dokumentationen/Schnittstellenregister_CPP.md"
  - "Spezifikationen/**/*Schnittstelle*.md"
---
# Pflege des C++-Schnittstellenvertrags

Für den Core-GUI-Architekturrahmen ist der
[Basisvertrag Simulationskern–GUI](../../Spezifikationen/Basisvertrag_Simulationskern_GUI.md)
maßgeblich. Das projektweite Register ist
[Schnittstellenregister_CPP.md](../../Dokumentationen/Schnittstellenregister_CPP.md).

## Vor der Änderung

- Öffentliche von privaten Headern und Exporten abgrenzen. Nur ausdrücklich
  freigegebene Schnittstellen bilden einen Implementierungsvertrag.
- Betroffene Registereinträge, IDs, Vertragsversionen, aufrufende
  Oberflächen und vorhandene Vertragstests prüfen.
- Ungeklärte Semantik nicht durch plausible Defaults ergänzen. Fachliche
  Änderungen oder Kompatibilitätsbrüche vor der Umsetzung klären.

## Bei jeder öffentlichen Schnittstelle

- Für jeden exportierten Typ, jede Funktion und jede qualifizierte
  Kontextmethode einen individuellen CPPI-Eintrag führen; keine Sammel-IDs.
- CPPI-IDs mindestens vierstellig, fortlaufend, versionsunabhängig und
  dauerhaft vergeben. IDs niemals wiederverwenden oder umnummerieren.
- Headerdeklaration, normative Dokumentation und Register müssen denselben
  Namen, Typen, Status und Vertrag wiedergeben.
- Zweck, Parameter, Rückgabe, Vor-/Nachbedingungen, Seiteneffekte, Einheiten,
  Koordinatenrahmen, Wertebereiche, Defaults, Fehlercodes, Besitz,
  Lebensdauer und Thread-Verhalten dokumentieren.
- Vertragstests für gültige Nutzung und Fehlerverhalten ergänzen, wo es
  technisch möglich ist. Tests nicht durch bloße Header-/Build-Präsenz
  ersetzen.
- Neue, geänderte, ersetzte oder stillgelegte Exporte im selben
  Arbeitsschritt in Register und Dokumentation nachführen. Status und
  Leistungsbehauptungen nur mit passenden Nachweisen ändern.

Die Spezifikation bleibt solange Entwurf, bis der Nutzer sie ausdrücklich
finalisieren lässt. Ein Entwurf allein autorisiert weder API-Implementierung
noch Vergabe von Anforderungs- oder Umsetzungsschrittkennungen.
