---
description: "Use when committing, pushing, tagging, publishing, or preparing a Segelmacher release, including Tagesstart, Tagesende, or vor großer Änderung."
---
# Git-, Commit- und Release-Regeln

Diese Instruction ist die maßgebliche Regelung für Git- und Release-Aktionen.
Sie darf normale Entwicklungsarbeit nicht blockieren. Antworte bei diesen
Aktionen kurz und auf Deutsch.

## Geltungsbereich und Schutzregeln

- Entwicklungsaufträge erlauben Änderungen auf dem aktuell ausgecheckten
  Branch. Nicht eigenmächtig Branches wechseln, mergen oder rebasen.
- Ein Entwicklungsauftrag ist keine Freigabe für Commit, Push, Tag oder
  Release. Diese Aktionen nur auf ausdrücklichen Auftrag ausführen. Die unten
  genannten Stichwörter gelten nicht, wenn sie nur zitiert oder besprochen
  werden.
- Vor Git-Aktionen Branch, Arbeitsbaum und Staging-Bereich prüfen. Nur die
  zum Auftrag gehörenden, freigegebenen Dateien stagen; bei fremden oder
  nicht freigegebenen Änderungen kein pauschales `git add -A`.
- Vor jedem Commit den vorhandenen nativen C++-Build und CTest ausführen:
  `cmake -S . -B build -DBUILD_TESTING=ON`,
  `cmake --build build --config Release`,
  `ctest --test-dir build -C Release --output-on-failure`.
  Der Core muss auch ohne GUI separat baubar und testbar bleiben
  (`cmake -S Code\Core -B build-core -DBUILD_TESTING=ON` mit anschließendem
  Build/CTest). Bei fehlendem `cmake`/`ctest` auf PATH den installierten
  Visual-Studio-CMake-Pfad oder eine Developer PowerShell verwenden.
  Python/pytest und Qt sind kein Testpfad dieses C++-Projekts.
  Fehlen Testsetup oder Testdateien, das ausdrücklich melden und keine
  Tests als bestanden darstellen. Schlagen Build oder Tests fehl, nicht committen.
- Nie `--force` oder `--no-verify` verwenden, keinen Verlauf umschreiben und
  keine Änderungen verwerfen. Bei fehlgeschlagenem Push abbrechen und melden;
  nicht mergen oder force-pushen.

## Versionsschema

- Tags haben das Format `S-X.Y` ohne `v`, zum Beispiel `S-0.1`, `S-0.2`
  und `S-1.0`. Der erste Versionsstand dieses eigenständigen Repositories
  ist `S-0.1`.
- Ein normaler Tag erhöht `Y` um eins: `S-0.1` → `S-0.2`.
- Ein formelles Release erhöht `X` um eins und setzt `Y` auf null:
  `S-0.7` → `S-1.0`, `S-1.4` → `S-2.0`.
- Den letzten passenden Tag ermitteln:
  `git tag --list --sort=-v:refname | Where-Object { $_ -match '^S-[0-9]+\.[0-9]+$' } | Select-Object -First 1`.
  Tags mit anderen Präfixen oder mehr als zwei Versionssegmenten sind
  Historie und werden ignoriert.
- Commits tragen keine Versionsnummer. Ein Tag ohne neue Commits seit dem
  letzten passenden Tag wird nicht gesetzt.
- Versionstags und GitHub-Releases sind auf `main` beschränkt. Auf anderen
  Branches keine Tags oder Releases veröffentlichen und nicht automatisch
  den Branch wechseln.

## Ausdrücklich beauftragte Abläufe

**„commit“**

Arbeitsbaum und Änderungen prüfen, Tests ausführen, nur freigegebene Dateien
gezielt stagen und committen. Gibt es nichts zu committen, melden und enden.
Kein Push und kein Tag.

**„Tagesstart“**

1. Offene, freigegebene Änderungen committen (wie oben).
2. Ungepushte Commits des aktuellen Branches zu seinem konfigurierten
   Upstream pushen. Fehlt ein Upstream, Ziel vor dem Push klären.
3. Nur auf `main` und nur wenn seit dem letzten Tag neue Commits vorliegen,
   den nächsten normalen Tag `S-X.(Y+1)` annotiert setzen und pushen. Kein
   GitHub-Release anlegen.

**„Tagesende“**

Offene, freigegebene Änderungen committen, dann den aktuellen Branch zu
seinem konfigurierten Upstream pushen. Fehlt ein Upstream, Ziel vor dem Push
klären. Zugehörige Versionstags nur auf ausdrücklichen Auftrag mitpushen.

**„vor großer Änderung“**

Offene, freigegebene Änderungen committen und den aktuellen Branch wie bei
„Tagesende“ pushen. Nur auf `main` und sofern neue Commits vorliegen, den
nächsten normalen Tag `S-X.(Y+1)` annotiert setzen und pushen. Kein Release.

**„Release“**

Nur auf ausdrücklichen Zuruf. Auf einem anderen Branch als `main` abbrechen
und melden; keine Teilveröffentlichung und kein automatischer Branchwechsel.

1. Offene, freigegebene Änderungen committen, Tests ausführen und pushen.
2. Den nächsten Major-Tag `S-(X+1).0` annotiert setzen und pushen.
3. Release-Notizen aus `git log <letzter Release-Tag>..HEAD --format="- %s (%h)"`
   erstellen. Letzter Release-Tag ist der letzte passende Tag auf `main`, der
   auf `.0` endet. Gibt es keinen, den gesamten Verlauf verwenden.
4. Ein GitHub-Release mit `gh release create <Tag> --title <Tag> --notes-file <Datei>`
   anlegen. Ist `gh` nicht verfügbar, Notizen ausgeben und den Link
   `https://github.com/BergStefanBerlin/Segelmacher/releases/new?tag=<Tag>`
   nennen.

## Aussagekräftige Einträge

- Commit-Betreff kurz, auf Deutsch und ohne Versionsnummer verfassen. Keine
  nichtssagenden Betreffe wie „Update“ oder „Änderungen“ verwenden.
- Commit-Nachrichten enden mit:
  `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`
- Unabhängige Themen getrennt committen. Bei mehreren zusammenhängenden
  Änderungen im Nachrichtentext knapp festhalten, was erreicht wurde und
  warum.
- Annotierte Tags beschreiben in der Tag-Nachricht das Hauptergebnis seit
  dem vorigen Tag. Release-Notizen nennen Änderungen und bekannte
  Einschränkungen, statt nur eine ungefilterte Commit-Liste zu wiederholen.

## Abschluss

Nach einem ausgeführten Ablauf kurz angeben, was committet, getaggt oder
gepusht wurde, den Stand aus `git describe --tags --always --dirty` nennen
und den nächsten normalen Tag beziehungsweise das nächste Release angeben.
Abbruchgründe wie fehlgeschlagene Tests, ein abgelehnter Push oder ein
konkurrierender Tag sind ausdrücklich zu melden; nichts erzwingen.
