# Technische Modellfestlegung H1 (05.11.2026)

**Ingenieurmodell, nicht physikalisch validiert.** Die Umsetzung ist durch
den Nutzer beauftragt. Die folgenden Schliessungsparameter sind explizite,
pruefbare Ingenieurannahmen, keine gemessenen Segelpolaren. Sie stehen im
Auftrag; es gibt keine versteckten Ersatzparameter.

## Verfahren, Bezuege und Quellen

Geometrie: polygonale XZ-Kontur, stueckweise parabolische NACA-Mittellinie,
lineare Stationsinterpolation, Twist um +Z am Vorliek. Grundflaeche S aus
Polygon, c_ref=S/H. Alle Kraefte beziehen sich auf q*S und Momente auf
q*S*c_ref; Windherkunft gemaess Spezifikation, Geschwindigkeit entgegen.

Anliegend: Fourier-Integration der Mittelliniensteigung mit
x/c=(1-cos(theta))/2; A0=alpha-integral(y')/pi,
An=2*integral(y'*cos(n theta))/pi,
CL=2*pi*(A0+A1/2), CM_c/4=pi*(A2-A1)/4.
Die Standard-Mittellinie hat positive Woelbung +Y und Sehnenrichtung -X.
Standard-CL wirkt +Y, Standard-Momentenachse ist -Z. Der ausgegebene
CM-Wert wechselt daher das Vorzeichen und bezieht sich auf lokales +Z.
Bei Wind aus +X ergibt positive Woelbung positive Y-Kraft.
Negativer Azimut erzeugt positiven Standard-CL; alpha_standard=-alpha.
Quelle: [UCF, Theory of Airfoil Lift Aerodynamics](https://pressbooks.online.ucf.edu/aerodynamicstext/chapter/chapter-5-theory-of-airfoil-aerodynamics/).
Geschlossene Integrale der parabolischen Mittellinie werden benutzt;
numerische Fourier-Quadratur mit 256/512 Punkten wird zusaetzlich gegen
die geschlossenen Integrale geprueft.

3D: gekoppelte Prandtl-Traglinie mit kosinusverteilten Kollokationspunkten.
Der Solver unterstuetzt 12, 16, 24, 32, 48 und 64 Sinusmoden; der oeffentliche
Core-Standardwert ist 48. Fuer H=gesamte Hoehe und z=H*(1-cos(theta))/2:

`alpha_standard-alpha0 = Sum An*sin(n*theta)*(4*H/(2*pi*c)+n/sin(theta))`

`Gamma=2*H*V_xy*Sum An*sin(n*theta)`,
`w_ind=V_xy*Sum n*An*sin(n*theta)/sin(theta)`.

Pivotiertes lineares Gleichungssystem; direkte Auswertung der Moden an
jedem Streifen. Die wiederverwendbare LU-Faktorisierung wird je Context und
Modenzahl gespeichert; bei jeder rechten Seite werden zuerst alle
Zeilenvertauschungen und danach Vorwaerts-/Rueckwaertssubstitution angewandt.
32/48 Moden werden **an jedem Betriebspunkt** als separate
Providerkonvergenz verglichen. Kein nachtraeglicher cL²/(pi AR)-Widerstand:
induzierte Geschwindigkeit geht unmittelbar in Kutta-Joukowski ein.
Grundlagen: [Prandtl-Traglinientheorie](https://en.wikipedia.org/wiki/Lifting-line_theory),
[NASA induzierter Widerstand](https://www1.grc.nasa.gov/beginners-guide-to-aeronautics/induced-drag-coefficient/).
Die Traglinie ist gerade, vertikal und verwendet die Queranstroemung.
Pfeilung wirkt auf reale Lastpositionen und Flaechennormalen, wird aber
in der **gebundenen Zirkulationskopplung vernachlaessigt**.
Das ist eine sichtbare Crossflow-/Tragliniennaeherung, kein vollraeumliches
Wirbelgitter und kein CFD; geringe Streckung und starke Pfeilung erhoehen
die Modellunsicherheit. Keine ungepruefte VLM-Fortsetzung am Pol.

Viskos: zweiseitige glatte Plattenreibung,
Cf_lam=1.328/sqrt(Re), Cf_turb=0.074/Re^0.2, stetige Mischung
um den expliziten Uebergangs-Re. Quellen:
[NASA, Skin friction](https://www.grc.nasa.gov/www/k-12/airplane/short.html)
(allgemeiner Widerstands-/Reynoldsbezug),
Schlichting/Gersten, *Boundary-Layer Theory*, Platten-Grenzschicht.
Zusaetzlicher Profilwiderstand k*CL² ist eine **unkalibrierte**
Abloese-/Druckwiderstandsschliessung. Re ist immer
Re_ref*c/c_ref, nicht still geklemmt; Bereich [1000,1e8].

Abriss: explizite Winkelgrenzen, Re-Verschiebung logarithmisch,
smoothstep-Mischung zwischen anliegend und abgeloest. Kein Anspruch auf
Vorhersage realer Abrisswinkel. Hochwinkel und rueckwaerts:
Newtonsche Impulsnaeherung fuer eine duenne Flaeche,
dCF=Cn_max*(u.n)*abs(u.n)*n*dS/S, Cn_max explizit (Beispiel 2).
Hintergrund: Anderson, *Hypersonic and High-Temperature Gas Dynamics*,
Kapitel Newtonian theory (Impulsflussargument, nicht Segelkalibrierung).
Newtonsche Theorie ist urspruenglich keine niedrig-Re-Segeltheorie;
hier **qualitative Hochwinkel-Schliessung**, keine validierte Extrapolation.
Kein synthetisches Sinus-Lookup: Normalkraft entsteht aus lokalem
dreidimensionalem Impulsfluss und Flaechennormalen.

Raeumlich: differenzierte Mittelflaechennormalen mit Camber-/Twist-
Gradient ueber Hoehe. Hochwinkeldruck und tangentiale Reibung verwenden den
vollen 3D-Vektor, auch bei exakt vertikalem Wind; gebundene Zirkulation
verwendet die explizite Crossflow-Annahme oben, Kutta-Joukowski den
vollen Vektor. Es wird nicht einfach der gesamte horizontale Lastvektor
mit cos(beta) skaliert. Anliegender Anteil wird
zwischen 60 und 80 Grad Elevation stetig ausgeblendet: die Traglinie
verliert bei spannweitiger Anstroemung ihre Gueltigkeit. Der Pol nutzt
exakt (0,0,-1), unabhaengig vom Azimut. Vertikale Lasten werden nicht
pauschal auf null gesetzt. Negative Elevationen werden abgelehnt.

Lastmodell: anliegende Kraft an Viertelsehne **plus unabhaengigem
Fourier-Profilmoment**; abgeloester Impulsdruck an vier Sehnen-Mittelpunkten
mit lokalen Normalen und der aus den beiden Flaechentangenten bestimmten
realen Flaechenmetrik (S bleibt die ebene Referenzflaeche).
Reibung ebenfalls an diesen Punkten. Keine Behauptung,
dass Viertelsehne ein physikalisches Druckzentrum ist. Integration erhaelt
Hebelarm und freie Eigenmomente; zentrale Wirkungslinie mit Restmoment
und dimensionsloser Kraftschwelle 1e-10.

## Parameter und Nachweise

Alle Zahlen im Beispielauftrag sind Modellannahmen und muessen explizit
vorliegen: stall_start_deg, stall_end_deg, stall_re_shift_deg,
transition_re, transition_width, viscous_cl2, normal_pressure,
re_min, re_max.
Gueltig nur inkompressibel, festes duennes Tuch, quasistationaer,
Camber |m|<=0.2, 0.05<=p<=0.95, |Twist|<=45 Grad.

Unit-Tests pruefen analytische Duennprofilgrenzfaelle, Reibungsformeln,
Geometrie, Momentenidentitaet, 3D-Symmetrien und stetige Uebergaenge.
Auftragspruefungen pruefen Fourier-/Provider-, Hoehenraster- und
Winkelinterpolationskonvergenz **getrennt**. Das geforderte Neigungs-
Grundraster 0, 10, ..., 90 Grad bleibt unveraendert. Zusaetzliche
Ausgabestuetzstellen liegen gezielt dichter im nichtlinearen Bereich:
52.5 bis 57.5, 62.5 bis 67.5, 72.5 bis 77.5 sowie 82.5 bis 87.5 Grad
in 2.5-Grad-Schritten und 88.75 Grad. Die Ausgabe-Neigungsachse enthaelt
damit 23 Werte statt der zehn Basiswerte; die 360er Azimutachse bleibt
unveraendert. Periodische tensorprodukt-kubische Hermite-Interpolation wird
an Winkel- und Zellenzwischenpunkten direkt gegen den Rechenkern geprueft.
Das Raster wird nur erweitert, nicht die Modellphysik oder die
Abnahmetoleranzen angepasst. Toleranzueberschreitungen sperren die
Veroeffentlichung, sie werden nicht kaschiert.
Es fehlen vermessene Segelpolaren, viskose Abrissreferenzdaten,
Hochwinkel-/Neigungsreferenzen und reale Hardwarevergleiche.
Eine erfolgreiche numerische Pruefung ersetzt diese nicht.

GPU: Unter Windows ist ein dynamisch geladenes OpenCL-FP64-Backend fuer
gebuendelte Profilpolaren implementiert. CPU und GPU werden fuer jeden
berechneten Profilwert in CL/CD/CM gegen dieselbe CPU-Referenzformel
verglichen; die binary64-Pruefschranke ist `2e-12 + 2e-12*|Referenzwert|`.
Ein Unterschied ausserhalb dieser Schranke bricht den Lauf ab. `automatisch`
faellt bei nicht verfuegbarer/fehlerhafter OpenCL-GPU transparent auf CPU
zurueck; `erforderlich` bricht dann mit Ressourcenfehler ab. Auf nicht
unterstuetzten Plattformen bleibt CPU das dokumentierte Fallback. Das
Pruefprotokoll nennt Backend, Plattform, Geraet, Anzahl der Polarwerte,
Vergleichsanzahl und maximale Abweichung. Traglinie und Streifenintegration
bleiben CPU; es wird kein Speedbenchmark ausgefuehrt.
Numerische CPU/GPU-Uebereinstimmung belegt weder GPU-Leistung noch
physikalische Modellgueltigkeit.
Mehrsegelflaechen sind intern als Identitaet/Transformation getrennt
darstellbar, aber werden nicht aerodynamisch gekoppelt.
