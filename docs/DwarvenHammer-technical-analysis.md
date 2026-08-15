# Dwarven Hammer (PedalPCB) — technikai leírás és DSP-fordítás

Forrás: `docs/DwarvenHammer.md`, `docs/PedalPCB-DwarvenHammer.pdf` (sematika + BOM + wiring diagram + drill template).
Ez a dokumentum a fizikai kapcsolás **komponensszintű, node-onkénti** visszafejtése, JUCE/DSP-implementációhoz igazítva. A sematikát pixelenként végignéztem (a PDF lapjai screenshotként, felnagyítva), az itt leírt topológia a rajz alapján, nem feltételezésből származik.

Típus: Tube Screamer-derivált overdrive/distortion, egy pluszban kapcsolható "Attack" tónuskontrollal a klasszikus TS808 gain-fokozatban, plusz egy második aktív "Bright" fokozattal és diszkrét tranzisztoros be/kimeneti pufferekkel (a sima TS808-ban ezek nincsenek).

## 1. Blokkdiagram / jelút

```
IN ──▶ [Input buffer: Q1 emitterkövető] ──▶ [Gain stage 1: IC1.1, TS-klón clipper, DRIVE + ATTACK] 
    ──▶ [Post-clip low-pass: R7/C7] ──▶ [Gain stage 2: IC1.2, BRIGHT aktív presence-boost]
    ──▶ [VOLUME pot] ──▶ [Output buffer: Q2 emitterkövető] ──▶ OUT
```

Négy JUCE-DSP-releváns blokk van: **input buffer** (gyakorlatilag transzparens, elhanyagolható vagy egyszerű unity-gain puffer), **clipping stage** (a lényegi nemlinearitás), **post-clip filter + bright stage** (lineáris/kvázi-lineáris szűrés, frekvenciafüggő erősítés), **output buffer** (transzparens).

## 2. Tápellátás / bias (nem audioút, de a modellhez kell a VREF fogalom)

- 9V DC, negatív középcsap; `D3` (1N5817 Schottky) sorosan a tápban, fordított polaritás elleni védelem.
- `C19` (100µF) a táp bulk-szűrő kondenzátora.
- **VREF** (a "virtuális föld" / mid-supply bias, ~VCC/2): `R18` (10K) + `R19` (10K) osztó VCC és GND között, `C20` (47µF) az osztó középpontján a földhöz — ez alacsony AC-impedanciájú referenciapont, amihez minden AC-csatolt fokozat vissza van blockolva.
- `IC1` (JRC4558DD, kettős op-amp — `IC1.1` és `IC1.2` a két fele) tápláb-entkopplung: `C18` (100n).
- LED + `R17` (4K7) áramkorlátozó, true-bypass footswitch (`SW`).

JUCE-ban ez egyszerűen **nem modellezendő explicit áramkörként** — a VREF csak azt jelenti, hogy minden AC jel 0V körül szimmetrikus a digitális domainben; a fizikai bias-hálózatot csak akkor kell figyelembe venni, ha valódi WDF-modellt építünk (pl. a bias ellenállások hozzájárulnak az egyes fokozatok tényleges impedanciájához).

## 3. Input buffer (Q1)

```
IN ──R9(1M, GND-re)── C2(27n) ──R5(1K)── [Q1 bázis]
VREF ──R1(510K)── [Q1 bázis]
Q1 (2N3904, NPN emitterkövető): kollektor→VCC, emitter→R10(10K, GND) és emitter→C3(1u)── tovább
```

- `R9` (1M): bemeneti terhelő ellenállás (a gitár pickupja szemszögéből ez az input impedance, ~1MΩ — tipikus TS-klón érték).
- `Q1`: NPN emitterkövető (common-collector), **unity-gain, magas bemeneti / alacsony kimeneti impedanciájú puffer**. Nem erősít, nem torzít érdemben normál jelszinten — funkcionálisan egy DC-blokkolt passthrough, ami a pickup terhelését szigeteli a következő fokozattól.
- **JUCE-fordítás**: elhanyagolható, vagy egy egyszerű első-rendű felül-áteresztő (a `C2`/`R5`/`R1` DC-blokkolás miatt, fc nagyon alacsony, gyakorlatilag a hallható sávban transzparens). Nem igényel nemlinearitást.

## 4. Gain stage 1 — a tényleges "Tube Screamer" clipper (IC1.1)

Ez a torzítás szíve, ide kell a legtöbb DSP-figyelem.

```
Nem-invertáló bemenet (pin3, +): C3(1u, AC-csatolás Q1-ről) + R2(10K → VREF, DC bias)

Invertáló bemenet (pin2, -) és kimenet (pin1) közötti visszacsatolás — HÁROM párhuzamos ág:
  (a) R15(10K) sorban DRIVE potméterrel (A500K, reosztátba kötve: a wiper rövidre zárva
      a bemenet-oldali lábbal) → variálható visszacsatoló ellenállás: 10K (min) .. 510K (max)
  (b) D1 ‖ D2 (1N4148, antiparallel) → szimmetrikus dióda-clipper
  (c) C8 (47p) → nagyfrekvenciás simítás / a clipper "keménységének" HF-korlátozása

Invertáló bemenet (pin2) és VREF közötti shunt-ág (ez a "gain-floor" beállítása):
  R16(1K) sorban [C9(120p, ÁLLANDÓAN bekötve) ‖ ATTACK-kapcsolóval kiválasztott kondenzátor]

  ATTACK (1P8T forgókapcsoló, wiper → pin2 node):
    pozíció 1..8 → C10(33n), C11(47n), C12(68n), C13(82n), C14(100n), C15(220n), C16(330n), C17(470n)
    C9(120p) mindig párhuzamosan van, függetlenül a kapcsoló állásától.
```

**Működés**: klasszikus TS-topológia — op-amp negatív visszacsatolásban antiparallel diódapár, a gain-t a Drive-potméterrel sorba kötött ellenállás állítja. Az invertáló bemenet földelő ága (a TS808-ban ez egy fix `R2=4.7K` lenne) itt **kondenzátorra van cserélve** (`R16` + kapcsolt C), ami frekvenciafüggő gain-floort hoz létre:

- AC feszültségerősítés (közelítőleg, kis jelre, klippelés előtt): `Gain(f) ≈ 1 + Zf(f) / Zg(f)`
  - `Zf(f)` = `(R15+Drive_pot) ‖ Zdiode ‖ Zc8` (a visszacsatoló ág)
  - `Zg(f)` = `R16 + 1/(jωC_selected‖C9)` (a földelő ág, ez az ATTACK-függő rész)
- Nagy ATTACK-kondenzátor (470n) → `Zg` alacsony már mélyfrekvencián is → a basszus is teljes erősítést/klippelést kap → vastag, tömör torzítás.
- Kis ATTACK-kondenzátor (120p+33n) → `Zg` csak magas frekvencián csökken le → csak a pengetés-tranziens/felharmonikusok kapnak extra gaint, a basszus viszonylag tiszta marad → "attack"-hangsúlyos, feszesebb karakter.
- A `C8` (47p) a diódák körül lágyítja a klipping HF-viselkedését (op-amp stabilitás + kevésbé "recsegő" felső oktáv).

**JUCE-fordítás**:
- Ez egy **feedback-hálózatos nemlineáris fokozat** — pontos modellhez ideális jelölt egy WDF-alapú megoldás (a projektben már ott a `chowdsp_wdf`), ahol a diódapár egy nemlineáris port-adaptor (pl. `chowdsp::WDF::DiodePair` vagy R-type adaptor), a `Zf`/`Zg` hálózat pedig lineáris WDF elemek (R, C, változtatható R a Drive potból, 8-utas kapcsolt C bank az Attacknek).
- Egyszerűbb (nem-WDF) közelítés: state-variable vagy egypólusú szűrőpár a frekvenciafüggő gain-hez (shelving filter az Attack-hez, ami a "floor gain" görbét adja vissza), + memoryless waveshaper (pl. `tanh`/dióda-clipper approximáció) a nemlinearitáshoz, oversampling-gel az aliasing ellen. Ez gyorsabb, de kevésbé pontos a dinamikus (feedback-függő) klipping-viselkedésben.
- **Paraméterek**: `drive` (0..1 → 10K..510K), `attack` (8 diszkrét pozíció vagy interpolált a 8 kapacitásérték között).

## 5. Post-clip low-pass filter

```
IC1.1 kimenet (pin1) ──R7(1K)── node X ──R3(10K → VREF) ‖ C7(220n → GND)── 
node X ──▶ IC1.2 pin5(+) és BRIGHT pot 1-es lába
```

- Egypólusú aluláteresztő: `R7=1K` sorban, `C7=220n` shunt VREF-re. Ez **pontosan megegyezik a klasszikus TS808 post-clip szűrőjével** (1K + 0.22µF, fc ≈ 720 Hz) — ez a klippelt jel élességét/harshness-ét tompítja, mielőtt a második fokozatba kerülne.
- `R3` (10K, VREF-re) csak DC-bias a `IC1.2` pin5-höz; AC-ban párhuzamos `C7`-tel, kis hatással a törésponti frekvenciára.
- **JUCE-fordítás**: egyszerű egypólusú IIR aluláteresztő (`fc ≈ 700-750 Hz`, R3‖C7 pontos hatását érdemes a végleges implementációnál Zg=R3‖(1/jωC7) alapján kiszámolni).

## 6. Gain stage 2 — "Bright" aktív presence/treble-boost (IC1.2)

```
pin5(+) = node X (az 5. pont kimenete)
pin6(-) = BRIGHT pot wiper
BRIGHT pot (B5K, lineáris): 1-es láb = node X, 3-as láb = C5(220n)──R12(220R)──GND sorban
R8(3K9): visszacsatoló ellenállás pin6 és pin7(kimenet) között
```

- Nem sima puffer/erősítő — a `BRIGHT` pot **blend-eli** az invertáló bemenetet a node X (közvetlen jel) és egy `C5+R12` felüláteresztő-jellegű, földre vezető ág között. Az op-amp virtuális-short viselkedése miatt minél inkább a `C5/R12` oldal felé van a wiper, annál nagyobb (és annál inkább magasfrekvencia-súlyozott, mert `C5` impedanciája csökken freknél) kompenzáló erősítést kell az `R8` visszacsatolásnak biztosítania → **frekvenciafüggő "presence" boost**, ami a pot állásával arányosan nő.
- Wiper a node-X végén (1-es láb felé): közel unity-gain, minimális boost.
- Wiper a `C5/R12` végén (3-as láb felé): maximális, magasakat hangsúlyozó boost.

**JUCE-fordítás**: ez egy **állítható shelving/presence filter**, viszonylag jól közelíthető egy hagyományos digitális high-shelf vagy peaking IIR szűrővel, aminek a boost-mértékét a `bright` paraméter (0..1, a potméter pozíciója) vezérli. Nem szükséges hozzá nemlinearitás — lineáris, feszültségvezérelt szűrő. WDF-ben is modellezhető (a pot egy 3-portos elem, `C5`+`R12` egy soros RC ág), de az egyszerűbb IIR-közelítés itt valószínűleg elég pontos, mert nincs klippelés ebben a fokozatban.

## 7. Volume és kimeneti puffer (Q2)

```
IC1.2 kimenet (pin7) ──C1(1u)── R4(1K) ──▶ VOLUME pot 3-as láb (jel)
VOLUME pot (A100K): 1-es láb = VREF, 3-as láb = jel, wiper(2) ──C4(100n)── [Q2 bázis]

VREF ──R6(510K)── [Q2 bázis]
Q2 (2N3904, NPN emitterkövető): kollektor→VCC, emitter→R13(10K, GND) és emitter→R11(100R)──C6(10u)──OUT
OUT ──R14(10K)── GND  (kimeneti bleeder/load)
```

- `VOLUME`: standard blend a jel és a VREF (csend) között — logaritmikus (A-taper) potméter, klasszikus hangerő-görbe.
- `Q2`: a `Q1`-gyel szimmetrikus NPN emitterkövető kimeneti puffer — alacsony kimeneti impedancia, hogy a pedál hosszú kábelt/következő pedált tudjon hajtani torzítás/veszteség nélkül.
- **JUCE-fordítás**: `volume` paraméter = egyszerű lineáris/log gain-szorzó a lánc végén. A pufferek (Q1, Q2) DSP szempontból elhanyagolhatók (nem adnak hallható színezést/torzítást normál üzemben).

## 8. Kezelőszervek → paraméter-mapping

| Fizikai kontroll | Típus | JUCE-paraméter | Hatás |
|---|---|---|---|
| DRIVE | A500K pot | `drive` (0..1) | Gain stage 1 visszacsatoló ellenállása (10K–510K), a klipping mértéke |
| ATTACK | 1P8T forgókapcsoló | `attack` (8 diszkrét fokozat) | Gain stage 1 föld-ági kapacitása (33n–470n + állandó 120p) → a gain frekvenciafüggő "floor"-ja, basszus/treble hangsúly a torzításban |
| BRIGHT | B5K pot (lineáris) | `bright` (0..1) | Gain stage 2 presence/treble-boost mértéke |
| VOLUME | A100K pot | `volume` (0..1) | Kimeneti szint |

## 9. Javasolt DSP-architektúra JUCE-ban

1. **Input stage**: átugorható vagy egyszerű DC-blokkoló HPF (~few Hz).
2. **Clipper (gain stage 1)**: a lényegi blokk.
   - *Pontosút*: `chowdsp_wdf` alapú komponensszintű modell — antiparallel dióda-pár nemlineáris adaptorral, a `Zf`/`Zg` hálózat linear WDF elemekként, a Drive potot és az Attack 8-állású kapcsolót futásidőben átszámolt komponensértékekként kezelve (blockonként, nem sample-önként, a zipper-zaj elkerülésére).
   - *Gyors közelítés*: feedback-es waveshaper vagy state-space modell, ami a `drive`/`attack` paramétereket a fent leírt `Gain(f)` képlet alapján egy shelving-szűrő + memoryless nemlinearitás kombinációjára képezi le, oversampling-gel (min. 4x) az aliasing ellen.
3. **Post-clip LPF**: egypólusú IIR, fc≈720 Hz (R3‖C7 pontosításával).
4. **Bright stage**: paraméteres shelving/peaking IIR, `bright` vezérli a boost mértékét.
5. **Volume**: lineáris gain.

A `chowdsp_wdf` modul jelenléte a repóban (`docs/components/chowdsp_wdf`) arra utal, hogy az 1. és opcionálisan 3-4. pont komponensszintű WDF-modellezéssel a cél — ebben az esetben a fenti node-lista (7.-8. pontban felsorolt hálózatok) közvetlenül átvihető WDF port-gráfba.
