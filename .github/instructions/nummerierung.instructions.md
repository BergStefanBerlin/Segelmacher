---
description: "Use when creating or updating specifications, requirement IDs, idea lists, implementation plans, backlog entries, or traceability in Segelmacher."
applyTo:
  - "Spezifikationen/**/*.md"
  - "Spezifikationen/**/*.json"
  - "Umsetzungsplaene/**/*.md"
  - "Umsetzungsplaene/**/*.json"
  - "Ideen/**/*.md"
---
# Projektweite Anforderungs- und Umsetzungsnummerierung

Diese Instruction ist die maßgebliche Regelung für Anforderungskennungen,
Entwurfspunkte, Backlog-Ideen und Umsetzungsschritte. Nummernkreise laufen
projektweit und versionsunabhängig; ein neuer Versions- oder Release-Schritt
setzt sie nicht zurück.

## Kennungsformate

- Finale Anforderungen: `A-S-<laufende-nummer>`, z. B. `A-S-00001`.
- Offene Punkte in Spezifikationsentwürfen: `IDEE-S-<laufende-nummer>`,
  z. B. `IDEE-S-00001`.
- Backlog-Ideen: `I-BL-<laufende-nummer>`, z. B. `I-BL-000001`.
- Umsetzungsschritte: `U-S-<laufende-nummer>`, z. B. `U-S-000001`.

Die Nummern werden mindestens so breit wie im Beispiel mit Nullen aufgefüllt
und bei Bedarf erweitert. Vor einer Vergabe alle aktiven und archivierten
Einträge durchsuchen und eine Nummer oberhalb der höchsten jemals vergebenen
Nummer wählen.

## Regeln

1. Anforderungen (`A-S-…`) stehen ausschließlich in freigegebenen
   Spezifikationen. Eine Kennung bezeichnet genau eine prüfbare Anforderung.
   Zusammengehörige Details dürfen gemeinsam abgenommen werden; unabhängig
   prüfbare Anforderungen nicht unnötig bündeln.
2. Entwürfe und ihre offenen Punkte sind nicht freigegeben und begründen
   keinen Umsetzungsauftrag. Offene Punkte stehen in einer getrennten
   Entwurfspunkteliste und erhalten `IDEE-S-…`, aber keine A- oder U-Kennung.
3. Erst nach ausdrücklicher Nutzerfreigabe zur Finalisierung werden
   angenommene Entwurfspunkte als Anforderungen übernommen. Die Liste hält
   die Zuordnung von `IDEE-S-…` zu `A-S-…` sowie den Status aller übrigen
   Punkte fest.
4. Wird eine Anforderung ins Backlog verschoben, erhält die Backlog-Idee
   eine neue `I-BL-…`-Kennung. Wird sie später spezifiziert, erhält sie eine
   neue `A-S-…`-Kennung; die frühere ID bleibt als Herkunftsverweis erhalten.
5. Umsetzungsschritte (`U-S-…`) stehen im versionsbezogenen Umsetzungsplan
   und verweisen auf mindestens eine passende Anforderung. Jeder aktive
   Anforderungspunkt muss am Ende einem Umsetzungsschritt und überprüfbaren
   Nachweisen zugeordnet sein.
6. Einmal vergebene Kennungen nie wiederverwenden, stillschweigend löschen
   oder umnummerieren. Zurückgezogene oder verworfene Einträge unter ihrer
   ursprünglichen Kennung mit Status und kurzer Begründung archivieren.
7. Versionsbezeichnungen wie `S-0.1` und Git-Tags sind keine Anforderungs-
   oder Umsetzungsschrittkennungen.
8. Für öffentliche C++-Schnittstellen führt dieses Projekt ein eigenes
   Schnittstellenregister. Jeder einzelne exportierte Typ, jede Funktion und
   jede qualifizierte Kontextmethode erhält eine dauerhafte Kennung
   `CPPI-xxxx`, beginnend mit `CPPI-0001`. Die Nummer wird mindestens
   vierstellig aufgefüllt und bei Bedarf erweitert. Der Zähler läuft
   projektweit und versionsunabhängig fort. Vor einer Vergabe aktive und
   archivierte Einträge prüfen und eine Nummer oberhalb der höchsten jemals
   vergebenen wählen. Kennungen nie wiederverwenden oder umnummerieren.
   Konkrete, freigegebene geplante Schnittstellen erhalten ihre ID vor der
   Implementierung; Entwurfsideen und bloße Arbeitsbereiche erhalten keine.
