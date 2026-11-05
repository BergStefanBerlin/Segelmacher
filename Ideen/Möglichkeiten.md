D	Modellname	Segelmodell	360°	Genauigkeit	Umsetzung	Rechenzeit je Bezugsfall	Bezug =
M01	Handformel / Beiwerte	K0	ja	±20–25 %	Stunden–Tage	< 1 s	eine Windbedingung
M02	VPP / Polarenmodell	K0–K1	ja	±10–20 %	Wochen	1–10 s	ein Kurs/Windfall; ganze Polare: Minuten
M03	Streifentheorie	K1	teilweise	±10–20 %	Wochen	1–30 s	ein Fall; 5 Schnitte × 360°: Minuten
M04	Lifting-Line / VLM	K2	teilweise	±10–20 %	Wochen	10 s–5 min	eine Konfiguration; 360°-Sweep: 1–30 h
M05	3D-Panelverfahren	K2/K3	teilweise	±5–15 %	Wochen–Monate	10 min–2 h	ein Anstellwinkel; 36 Winkel: 1–2 Tage
M06	RANS 2D	K1	ja (sehr gut)	±5–10 %	Wochen	2–6 min (angelegt) / 20–60 min (abgelöst)	ein Anstellwinkel; Polare ±30° in 1°: 5–8 h; 360° in 1°: 2–4 Tage
M07	RANS 3D stationär	K2/K3	teilweise	±5–10 %	Monate	6–24 h	ein Betriebspunkt; 10-Punkt-Trimmstudie: 1–2 Wochen
M08	URANS 3D	K2/K3	teilweise	±5 %	Monate	3–14 Tage	ein Betriebspunkt mit Zeitverlauf
M09	DES / LES	K2/K3	teilweise	Referenz	Monate–Jahre	3–12 Wochen	ein Fall – auf dieser Hardware grenzwertig
M10	CFD–FEM (FSI)	K4	teilweise	±5–10 %	Monate–Jahre	1–4 Wochen	ein Trimmpunkt bis zum Gleichgewicht
M11	Windkanal	K5	ja	±5–10 %	Monate	keine CPU-Zeit – Messtage: 1–5 Tage je Reihe	eine Messreihe
M12	Vollmaßstab	K5	ja	Referenz	Monate	keine CPU-Zeit – Testfahrten: Wochen	eine Messkampagne
M13	Lattice-Boltzmann	K2/K3	teilweise	ähnlich M07	Monate	1–5 Tage – aber nur mit GPU sinnvoll	ein Fall
M14	ML-Surrogat	erbt (K2/K3)	nur Bereich	innen gut	Monate	Millisekunden (Training: 1–3 Tage)	eine Vorhersage
M15	Analytische Flügeltheorie	K0–K1	teilweise	±15–25 %	Tage	< 1 s	ein Fall
M16	Polaren-Lookup	K0–K1	ja	±15–20 %	Tage–Wochen	Millisekunden	eine Abfrage
M17	CFD mit Rumpf (overset)	K2/K3 + Rumpf	teilweise	±5–10 %	Monate	1–4 Tage	ein Betriebspunkt (1,5–4× M07)