# Zajszint-vizsgálat: háttérzaj a Drive/Attack fokozatban

Felhasználói jelzés: valós DI-felvételen (`Measurements/di - 25 Earth Elemental - rythm gtr 2.wav`) a füllel alig hallható háttérzaj a plugin által drasztikusan felerősödik, különösen erősítő-modell mögé kötve.

## Diagnózis (mérve, nem tippelve)

A `juce-dsp-measurement` skill módszertana szerint egy scratch mérőeszközzel (a tényleges `ClipperStage`/`ToneStage`/`PluginProcessor` production kódot futtatva a valós DI-fájlon) két gyanús okot vizsgáltunk külön-külön:

**1. A gain-fokozat frekvenciafüggő kisjelű erősítése (domináns ok, NEM hiba)**

A csendes (kb. −55dBFS, lecsengő hang) és a hangos (kb. −15dBFS, játék) szakaszok eltérő dB-erősítést kapnak:

| Drive | Attack | Csendes gain | Hangos gain | Különbség |
|---|---|---|---|---|
| 0.0 | 0 | +2.6dB | +3.3dB | ~0 |
| 0.0 | 7 | +12.8dB | +3.1dB | +9.7dB |
| 1.0 | 7 | +19.6dB | +3.2dB | +16.4dB |

Ez **fizikailag hiteles** viselkedés: a `docs/DwarvenHammer-technical-analysis.md`-ben levezetett kisjelű erősítési képlet (`1+Zf/Zg`) szerint a stage magas frekvencián akár +54dB-t is elérhet (mivel R16=1k kisebb, mint a stock TS808 R2=4.7k-ja, ez a konkrét áramkör MÉG zajérzékenyebb egy sima TS808-nál). Minden valódi TS-stílusú overdrive ismert erről a jellemzőről magas gain-en.

A valódi digitális-csend szakaszokon (kb. −124dBFS, felvételi szünetek) ez konkrétan azt jelenti: a plugin önmagában +19.6dB-lel emeli ezt fel (−124 → −118dBFS, önmagában alig hallható), DE egy utána kötött, magas gain-ű erősítő-modell további +20-40dB-t adhat hozzá, ami −78 és −98dBFS közé tolja — ez már egyértelműen hallható zaj. Ez magyarázza a felhasználó pontos megfigyelését ("erősítő mögött még jobban hallható").

**2. Aliasing (másodlagos, valós, de kisebb hozzájárulás)**

Kezdeti broadband-zaj-alapú méréssel talált "excess HF energy" metrika félrevezető volt (valós felharmonikus-tartalmat és aliasing-terméket egyaránt a >12kHz sávba mér, nem különböztethető meg). A klasszikus, egyértelmű teszt (szinusztónus, aminek felharmonikusai szándékosan a Nyquist fölé esnek, mérés a valós harmonikus sorból matematikailag kizárt frekvencián) egyértelmű eredményt adott:

- 8kHz bemeneti tónus, mérés 4100Hz-en (= |5×8000 − 44100|, egy 8kHz tónus valódi felharmonikus-sorában soha nem szereplő frekvencia)
- **Oversampling nélkül: −24.57dBFS** (hangos, egyértelműen hallható parazita tónus)
- **4x oversamplinggel: −100.17dBFS** (−75.6dB javulás)

## Megoldás

**1. AdaptiveGate-VST a MothBite elé kötve** (a felhasználó saját, már meglévő pluginja)

A domináns ok (a fokozat magas kisjelű erősítése) NEM MothBite-on belüli hiba, hanem a valódi áramkör tulajdonsága — ezért a helyes, topológiailag is korrekt megoldás egy noise gate a clipper ELÉ kötve (pontosan ahogy egy pedalboardon is: gate → overdrive → amp), nem egy új, a valódi áramkörben nem létező gate beépítése MothBite-ba.

Mérve (teljes fájl, folytonos feldolgozás, hogy a gate envelope/noise-floor trackerei valós előzménnyel rendelkezzenek a teszt-ablakokhoz érve):

| Beállítás | Igazi csend kimenet | Lecsengő hang (érintetlen?) |
|---|---|---|
| Nincs gate | −118.0dBFS | (baseline) |
| AdaptiveGate, alapértelmezett "Guitar" profil | **−144.0dBFS** (−26dB javulás) | 16.38→15.94dB, gyakorlatilag változatlan |
| AdaptiveGate, gyorsabb attack/szigorúbb threshold | −174.2dBFS | szintén gyakorlatilag változatlan |

Az alapértelmezett "Guitar" forrás-profil (ami a magas sávban, 8-20kHz, már eleve a leggyorsabb attack-et [3ms] és legagresszívebb beállítást használja — pont ott, ahol a MothBite clipper a legtöbb zajt generálja) **külön hangolás nélkül** ~26dB-lel csökkenti az igazi csendet, miközben a valódi lecsengő hangot érintetlenül hagyja. Nincs szükség extra tuningra — a defaultok már jól illeszkednek.

**Tranziens-megőrzési teszt (a "elég gyors legyen" kérdésre):** külön megmértem, hogy a gate nem vágja-e le egy új hang pengetés-attackjének csúcsát (nem csak a lecsengést). Valódi note-onset pontot keresve a DI-fájlban (csend → hirtelen hangos átmenet), a klippelt kimenet csúcsértékét hasonlítottam ungated vs. gated esetben, az Attack-szorzó 1.0x (alapértelmezett) és 0.5x/0.3x/0.15x (gyorsított) beállításainál egyaránt:

| Beállítás | Idle zaj kimenet | Onset csúcs-veszteség |
|---|---|---|
| Guitar defaults (1.0x) | −143.97dBFS | **0.00dB** |
| 0.5x / 0.3x / 0.15x attack | −143.97dBFS | **0.00dB** |

Minden tesztelt beállításnál **nulla** mérhető veszteség a pengetés-csúcson — az alapértelmezett (1.0x) attack-sebesség is bőven elég gyors ahhoz, hogy teljesen kinyisson, mielőtt a tranziens csúcsa megérkezik. **Nincs szükség gyorsításra sem** — a Guitar profil defaultjai egyszerre optimálisak a zajelnyomásra ÉS a tranziens-megőrzésre.

**2. 4x oversampling a ClipperStage körül** (`Source/PluginProcessor.h/.cpp`)

`juce::dsp::Oversampling<float>` (4x, `filterHalfBandPolyphaseIIR`), kizárólag a WDF-diódapáros nemlineáris fokozat köré csavarva — a `ToneStage` lineáris, nem igényli. A latency-t `setLatencySamples()`-szel jelenti a host felé. Mérve: −75.6dB aliasing-csökkenés a fent leírt klasszikus teszttel.

## Módszertani tanulság

A broadband-zaj alapú "magas frekvenciás energia" metrika NEM alkalmas aliasing kimutatására, mert nem különbözteti meg a valódi felharmonikus-tartalmat az aliasing-termékektől (mindkettő ugyanabba a frekvenciasávba esik). A helyes teszt: kontrollált szinusztónus, aminek felharmonikusai szándékosan túllépik a Nyquist-et, mérés egy, a valós harmonikus sorból matematikailag kizárt frekvencián.
