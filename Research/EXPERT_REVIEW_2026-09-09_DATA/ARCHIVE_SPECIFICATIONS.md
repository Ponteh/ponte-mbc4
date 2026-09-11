<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/00_Common/README_Common.md -->
# Common rendering setup
- Source/render: 32-bit float WAV, same sample rate.
- No normalization, limiter, dither or extra processing.
- Preserve source timing.
- Record exact MC version/build and actual UI values for clamped settings.
- Input/output 0 dB unless test says otherwise.


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/01_Crossover_MC303_MC404/TEST_SPEC.md -->
# T01 — MC303 / MC404 internal crossover

Render every source: Solo each band and all bands active.

MC303 configs: 100/1000, 800/1000, 100/1200, extreme legal limits 20/20000.
MC404 configs: 100/1000/10000, 100/900/1200, 100/8000/10000, closest legal non-crossing 1000/1000/1000.

Settings: Ratio 1:1, Gain 0 dB, BITE minimum, compression inactive, Input/Output 0 dB, Trashold 0dB, knee 0db, Attack 2.50 msec, release 250 msec, R1. Record actual crossover values.

Extract magnitude, phase, impulse response, total sum, split order, compensation and near-crossover stability.. 


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/02_Auto/TEST_SPEC.md -->
# T02 — Auto mode - MC404 100/1000/10000
Base: Threshold -24 dB, Ratio 10:1, Knee 0, BITE minimum.
For each source: render Auto with Attack/Release minimum (0.25msec/25msec), Auto with Attack/Release maximum (250msec/2500msec).
Measure T10/T50/T90 attack, T90/T50/T10 release, peak/RMS, repetition density, memory and whether Auto ignores manual Attack/Release.
A1 frequency, A2 same peak/different RMS, A3 same RMS/different peak, A4 event density 1/4/10/50 Hz, A5 event duration, A6 absolute level.


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/03_Dynamics_Validation/TEST_SPEC.md -->
# T03 — Dynamics validation MC404 
Knee: thresholds -36/-24/-12, ratios 2/4/8/10:1, knees -10/-5/0/+5/+10/+12.5/+15.
Attack: 100/1000/5000 Hz; 0.03/0.1/1/10/50/100/250 ms; ratios 2/4/10:1.
Release: levels -18/-12/-6; release 5/50/100/250/500/1000/2500 ms.
Type-2: intended D/E .1/.25/.5/.75/.9; release 50/250/1000/2500 ms; delays 100/250/500/750 ms. Record actual envelope E before second event.

T03-Attack: Threshold -9, knee 0,bite min, release 250 msec, type2. A=0.03 significa 0.25ms, A=0.1 significa 0.5ms nel plugin mc2000 mc404.

T03-Knee: bite min, attack 2.5msec, release 250 msec, type2.

T03-Release: Threshold -24, knee 0, ratio 2, bite min, attack 2.5msec, type2. R=5ms significa 25msec nel plugin mc2000 mc404.

T03-Type-2: Threshold -24, knee 0, ratio 2, bite min, attack 2.5msec, type2.

<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/04_BITE_Complete/TEST_SPEC.md -->
# T04 — Complete BITE
Sources: 100/500/1000/5000/10000 Hz; -24/-12/-6 dBFS; rise 0/1/10/100 ms. Each = 2 s silence + 100 ms burst + 3 s silence.
Render BITE 1/5/10. Base: MC404, T=-24, R=10:1, K=0, Type-1, Attack 10 ms, Release 300 ms, Input/Output 0 dB.
Measure relief, timing, decay, steady state, phase and THD/IMD.


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/05_Controls_IN_Solo_Link_Global/TEST_SPEC.md -->
# T05 — IN / Solo / Link / Global
Use strong compression. Toggle IN during sustained GR and observe state/reset/smoothing.
Solo: none, each band, multiple bands, and automation while playing; inspect audio, meters, compressor state.
Link: MC303/MC404, each master; change Gain/Threshold/Ratio/Knee/BITE/Attack/Release/TC Type; log slave offsets, relative/absolute behavior, limits and relink behaviour.
Global: Input/Output Gain -24/-12/0/+12/+24 dB and phase invert. Use sine + impulse.


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/06_Saturation_Headroom/TEST_SPEC.md -->
# T06 — Saturation / Headroom / Clip
Input Gain 0/+6/+12/+18/+24 dB; tones 20/100/700/1000/5000/10000 Hz; peaks -30/-18/-6/-1 dBFS. Use trashold -24, ratio 1:1, BITE minimum, knee 0, attacke 2.5msec, release 250msec, type2.
Two-tone IMD: 100+1000 and 1000+10000 Hz at Input Gain 0/+12/+24. Measure gain, harmonics, THD, IMD, clipping and clip-meter threshold. Repeat MC202/303/404 where practical.


<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/07_Sample_Rate/TEST_SPEC.md -->
# T07 — Sample rate
Repeat at 44.1/48/96 kHz with identical nominal parameters. Measure crossover frequency/slope/phase/latency, static curve, attack/release, BITE 10 and later Auto.



<!-- ARCHIVE: MC2000_Additional_BlackBox_Tests/MC2000_Additional_BlackBox_Tests/README.md -->
# MC2000 Additional Black-Box Test Pack

Second suite to add to the first pack.

P0: T01 crossover MC303/MC404, T02 Auto, T03 dynamics generalisation, T04 BITE.
P1: T05 controls/linking, T06 saturation/headroom, T07 sample rate.

WAV = input stimuli. Render through original MC202/303/404 according to each TEST_SPEC.md. Record exact MC version/build, sample rate and actual parameter values.


<!-- ARCHIVE: MC2000_BlackBox_TestPack_48k/MC2000_BlackBox_TestPack_48k/README_MC2000_TESTS.md -->
# MC2000 Black-Box Test Pack

Formato di tutti i WAV:
- 48 kHz
- WAV IEEE float 32-bit
- stereo
- dual-mono, tranne i file T15 stereo-link
- i livelli indicati nei nomi sono **peak dBFS** della sinusoide, non RMS

## Impostazione DAW consigliata
1. Sessione a 48 kHz.
2. Importa il WAV senza normalizzazione.
3. Inserisci MC2000 come unico processore sul canale.
4. Nessun limiter, clipper, dither, normalizzatore o gain automatico dopo il plugin.
5. Render/bounce a 48 kHz / 32-bit float, senza normalizzazione.
6. Mantieni esattamente il timing del file originale.
7. Se possibile, inviami i WAV renderizzati: posso estrarre curve, envelope, THD e tempi direttamente.

## Nomenclatura dei render
Aggiungi al nome sorgente le impostazioni chiave, per esempio:
- `T07_KneePositive__K+10_R4_T-24.wav`
- `T10_Type1_vs_Type2_SecondEvent_-15dBFS__Type1.wav`
- `T10_Type1_vs_Type2_SecondEvent_-15dBFS__Type2.wav`
- `T12_BITE_FrequencyBursts__BiteMin.wav`
- `T12_BITE_FrequencyBursts__BiteMax.wav`

## Setup base compressore (T05-T11, T15)
Usa MC202:
- Crossover: 1000 Hz
- SOLO: LOW
- Input Gain: 0 dB
- Output Gain: 0 dB
- Band Gain: 0 dB
- BITE: minimo disponibile, salvo test BITE
- Threshold: come specificato
- Ratio: come specificato
- Knee: come specificato
- TC: come specificato
- Attack/Release: come specificato

Se un valore esatto non è selezionabile, usa quello più vicino e annotalo nel nome o in un TXT.

---

## T01 — Baseline
File:
- `T01_Baseline_100Hz_-18dBFSpeak.wav`
- `T01_Baseline_5000Hz_-18dBFSpeak.wav`

Impostazioni:
- Ratio 1:1
- tutti i gain 0 dB
- BITE minimo
- per 100 Hz: LOW Solo, crossover 1 kHz
- per 5 kHz: HIGH Solo, crossover 1 kHz

Renderizza ciascun file con MC2000 attivo.

Scopo: verificare eventuale gain offset/non-linearità a compressione neutra.

## T02 — Risposta dei crossover
File:
- `T02_Crossover_SteppedSines_-24dBFSpeak.wav`

Impostazioni:
- MC202
- Crossover 1 kHz
- Ratio 1:1
- Gain 0 dB
- BITE minimo

Render A: LOW Solo.
Render B: HIGH Solo.

Il `MANIFEST.csv` indica l'intervallo temporale di ogni frequenza.

## T03 — Somma delle bande / impulso
File:
- `T03_CrossoverSum_SteppedSines_-24dBFSpeak.wav`
- `T03_Impulse_-6dBFSpeak.wav`

Stesse impostazioni T02, ma **nessuna banda in Solo**.

Renderizza entrambi.

Scopo: ricostruzione in frequenza, fase/impulse response e comportamento della somma LOW+HIGH.

## T04 — Saturazione/non-linearità del filtro
File:
- `T04_FilterSaturation_100Hz_LevelSeries.wav`
- `T04_FilterSaturation_700Hz_LevelSeries.wav`

Impostazioni:
- MC202
- Crossover 1 kHz
- LOW Solo
- Ratio 1:1
- Gain 0 dB

Renderizza i due file.

Scopo: confrontare THD lontano e vicino al crossover.

## T05 — Curva statica Knee 0
File:
- `T05_StaticCurve_Knee0_LevelSeries_100Hz.wav`

Impostazioni:
- Threshold -24 dB
- Ratio 4:1
- Knee 0
- Type-1
- Attack 1–10 ms
- Release 500 ms
- BITE minimo

Renderizza una volta.

## T06 — Knee negativo
File:
- `T06_KneeNegative_LevelSeries_100Hz.wav`

Renderizza tre versioni:
- Knee -10
- Knee -5
- Knee 0

Altre impostazioni:
- Threshold -24 dB
- Ratio 4:1
- Type-1
- Attack 1–10 ms
- Release 500 ms
- BITE minimo

## T07 — Knee positivo / overshoot / tail
File:
- `T07_KneePositive_LevelSeries_100Hz.wav`

Renderizza:
- Knee 0
- Knee +5
- Knee +10
- Knee +12.5 (o il valore più vicino disponibile)
- Knee +15

Altre impostazioni:
- Threshold -24 dB
- Ratio 4:1
- Type-1
- Attack 1–10 ms
- Release 500 ms
- BITE minimo

Questo è uno dei test più importanti.

## T08 — Attack reale
File:
- `T08_AttackStep_100Hz.wav`

Impostazioni:
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Type-1
- Release 1000 ms
- BITE minimo

Renderizza più volte con Attack:
- 0.1 ms
- 1 ms
- 10 ms
- 50 ms
- 100 ms

(o i valori più vicini disponibili).

## T09 — Release Type-1
File:
- `T09_ReleaseStep_100Hz.wav`

Impostazioni:
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Type-1
- Attack minimo/veloce
- BITE minimo

Renderizza con Release:
- 50 ms
- 100 ms
- 250 ms
- 500 ms
- 1000 ms

## T10 — Type-1 vs Type-2
File:
- `T10_Type1_vs_Type2_SecondEvent_-20dBFS.wav`
- `T10_Type1_vs_Type2_SecondEvent_-15dBFS.wav`
- `T10_Type1_vs_Type2_SecondEvent_-10dBFS.wav`

Impostazioni:
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Attack veloce
- Release 1000 ms
- BITE minimo

Per **ogni file**, fai:
- un render Type-1
- un render Type-2

Scopo: misurare come il secondo evento modifica la release.

## T11 — Type-2, dipendenza dalla distanza temporale
File:
- `T11_Type2_Delay_100ms_SecondEvent_-15dBFS.wav`
- `T11_Type2_Delay_250ms_SecondEvent_-15dBFS.wav`
- `T11_Type2_Delay_500ms_SecondEvent_-15dBFS.wav`
- `T11_Type2_Delay_750ms_SecondEvent_-15dBFS.wav`

Impostazioni:
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Type-2
- Attack veloce
- Release 1000 ms
- BITE minimo

Renderizza tutti e quattro.

## T12 — BITE: dipendenza dalla frequenza
File:
- `T12_BITE_FrequencyBursts_-6dBFSpeak.wav`

Per questo test usa:
- MC202
- Crossover al minimo disponibile, idealmente 20 Hz
- HIGH Solo
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Type-1
- Attack 10 ms
- Release 300 ms

Render A: BITE minimo.
Render B: BITE massimo.

Le frequenze dei burst sono 100, 500, 1000, 5000, 10000 Hz.

## T13 — BITE: dipendenza dal rise time
File:
- `T13_BITE_RiseTime_1kHz_-6dBFSpeak.wav`

Stesso setup T12.

Il file contiene tre eventi a 1 kHz:
- rise 0 ms
- rise 10 ms
- rise 100 ms

Render A: BITE minimo.
Render B: BITE massimo.

## T14 — BITE: onset o contenuto HF continuo?
File:
- `T14_BITE_Continuous_100Hz_-6dBFSpeak.wav`
- `T14_BITE_Continuous_10000Hz_-6dBFSpeak.wav`

Stesso setup T12.

Per ciascun file:
- BITE minimo
- BITE massimo

Scopo: verificare se l'effetto BITE è confinato all'onset o permane sul contenuto HF stazionario.

## T15 — Stereo linking
File:
- `T15_StereoLink_L-6_R-30.wav`
- `T15_StereoLink_L-30_R-6.wav`
- `T15_StereoLink_L-12_R-12.wav`
- `T15_StereoLink_LR-6_Antiphase.wav`

Impostazioni:
- Crossover 1 kHz
- LOW Solo
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- Type-1
- Attack veloce
- Release 300–500 ms
- BITE minimo

Renderizza tutti e quattro.

Scopo: distinguere max(L,R), somma stereo, media e altri tipi di linking.

---

## Ordine consigliato se vuoi partire con pochi test
Fai prima:
1. T05
2. T07
3. T10
4. T12
5. T13
6. T14

Con questi possiamo già ricostruire gran parte del gain computer, Type-2 e BITE.

## MANIFEST.csv
Il file `MANIFEST.csv` contiene gli intervalli temporali esatti dei segmenti nei WAV compositi. È utile per analisi automatica e per non dover individuare manualmente i cambi di livello/frequenza.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/08_Auto_Internal_Bands/TEST_SPEC.md -->
# T08 — Auto on MC303 / MC404 Internal Bands

MC303: X1/X2 = 300/3000 Hz.
MC404: X1/X2/X3 = 300/3000/8000 Hz.

Settings: Threshold -24 dB, Ratio 10:1, Knee 0, BITE 1, Input/Output 0 dB.

For every source:
- Auto with manual Attack/Release minimum
- Auto with manual Attack/Release maximum
- Solo every band
- all bands active

Sources:
- A1: 100/1000/5000/10000 Hz
- A4: pulse 1/10/50 Hz
- A6: -24/-6 dBFS

Measure GR, attack/release timing, Solo vs all-bands, manual min/max equality, and MC202 vs equivalent MC303/MC404 band.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/09_Stereo_Detector_Internal_Bands/TEST_SPEC.md -->
# T09 — Stereo Detector in MC303/MC404 Internal Bands

MC303: 300/3000 Hz, Solo B1/B2/B3.
MC404: 300/3000/8000 Hz, Solo B1/B2/B3/B4.

Settings: T=-24, R=10:1, Knee=0, Type-1, Attack 0.25ms, Release 500 ms, BITE min, Input/Output 0, Auto.

For each frequency 100/1000/5000/10000 Hz:
- L=-6 / R=-30
- L=-30 / R=-6
- L=-6 / R=-6 antiphase
- L=-12 / R=-12

Measure channel GR equality and compare internal bands to MC202.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/10_InterBand_Dynamic_Independence/TEST_SPEC.md -->
# T10 — Inter-Band Dynamic Independence

MC303 source: 100/1k/5k.
MC404 source: 100/1k/5k/10k.

Each source: 1 s silence; each isolated tone (1 s + .5 s); all tones (1 s); each adjacent pair (1 s + .5 s); 2 s silence.

Run Type-1 and Type-2, Ratio 4:1 and 10:1, T=-24, Knee 0, BITE 1, Attack 10 ms, Release 300 ms.

Render all bands and Solo each band. Compare each band's GR isolated vs other bands active.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/11_BITE_Internal_Bands/TEST_SPEC.md -->
# T11 — BITE Internal-Band Equivalence

MC404: 300/3000/8000 Hz, Solo B1/B2/B3/B4.

Sources 100/1000/5000/10000 Hz, rise 0/10 ms, -6 dBFS.

Render BITE 1/5/10. Base T=-24, R=10:1, Knee 0, Attack 10 ms, Release 300 ms. First Type-1; control at BITE 5 with Type-2 and Auto.

Compare relief max, duration, steady state and phase against the equivalent MC202 test.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/12_SampleRate_Auto_Internal_Crossover/TEST_SPEC.md -->
# T12 — Sample Rate: Auto + Internal Crossover

Native source rates: 44.1 / 48 / 88.2 / 96 / 192 kHz.

Auto: StepBurst 1 kHz and PulseTrain 10 Hz at -24 dBFS; Auto manual Attack/Release minimum then maximum.

MC303: 100/1000 Hz, Solo B1/B2/B3 + all.
MC404: 100/1000/10000 Hz, Solo B1/B2/B3/B4 + all.

Record crossover, phase, impulse position, host latency, Attack/Release and Auto trajectory. Do not resample.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/13_Automation_Smoothing/TEST_SPEC.md -->
# T13 — Automation / Parameter Smoothing

Source: 900 Hz + 1100 Hz, -18 dBFS per component, 12 s. Start crossover 1 kHz.

Automate crossover(s), Input/Output Gain, Band Gain, IN/OUT and Solo.
For important parameters use both slow (~2 s) and fastest practical DAW automation.
Repeat key cases at buffer 32 and 2048.

Measure clicks, transient spikes, ramp duration, phase/magnitude discontinuity, automated vs manual behavior, and crossover-neighbour constraint behavior.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/14_Sidechain_Type2_Auto/TEST_SPEC.md -->
# T14 — External Sidechain with Type-2 and Auto

Run only if v7 VST3 exposes a working external sidechain.

Use Program and Key 1 kHz files.

Configurations: MC202, MC303, MC404; Solo affected band and all bands.
Modes: Type-1, Type-2, Auto with manual Attack/Release minimum, Auto with maximum.
Settings: T=-24, R=10:1, Knee 0, BITE 1, Input/Output 0.

Compare internal detector/no key vs external key. Determine whether the key replaces program detection and whether Type-2/Auto adapt to key, program, or both.


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/FINAL_DSP_GAP_CHECKLIST.md -->
# Final DSP Gap Checklist
- [ ] T08 Auto internal bands
- [ ] T09 stereo detector internal bands
- [ ] T10 inter-band dynamic independence
- [ ] T11 BITE internal-band equivalence
- [ ] T12 sample-rate Auto + internal crossover
- [ ] T13 automation/smoothing
- [ ] T14 sidechain Type-2/Auto


<!-- ARCHIVE: MC2000_Integration_Final_DSP_TestPack/MC2000_Integration_Final_DSP_TestPack/README.md -->
# MC2000 Integration / Final DSP Validation Test Pack

Fourth and final integration suite for the remaining DSP gaps.

P0: T08 Auto internal bands; T09 stereo detector internal bands; T10 inter-band dynamic independence; T11 BITE internal-band equivalence; T12 sample-rate Auto + internal crossover; T13 automation/smoothing; T14 sidechain Type-2/Auto.

For each render record MC2000 build, host/version, sample rate, buffer, exact crossover values, exact parameters, realtime/offline and Solo/all state.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/01_AUTO_CROSSOVER/TEST_SPEC.md -->
# 01 â€” Auto and crossover interaction

All files are 12 s / 48 kHz. Fixed configuration: MC202 X1=300 Hz; MC303
X1/X2=300/3000 Hz; MC404 X1/X2/X3=300/3000/8000 Hz. All bands IN; render All
and every Solo. Threshold=-24 dB, Ratio=10:1, Knee=0, BITE=1, Input=0 dB,
Output=0 dB, internal detector, Time Constant=Auto. For every source render
manual Attack/Release both minimum and maximum.

Variables: A01 frequencies 100/300/1000/3000/5000/8000/10000 Hz with five
documented one-second level steps; A02 continuous 1 kHz; A03 pulse rates
1/10/50 Hz; A04 low-crest bed plus high-crest pulses; A05 noise-like source.
Measure GR, attack/release fractions, steady GR, min/max null and detector
behaviour at 300/3000/8000 Hz crossover boundaries.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/02_BITE/TEST_SPEC.md -->
# 02 â€” BITE matrix

All 48 files are 12 s / 48 kHz and have 100 ms events at 2, 5 and 8 s. Fixed:
MC404 X1/X2/X3=300/3000/8000 Hz; all bands IN; Threshold=-24 dB; Ratio=10:1;
Knee=0; Attack=10 ms; Release=300 ms; Input/Output=0 dB; internal detector.
First pass Type-1; control passes Type-2 and Auto.

No combination is omitted. Variables are carrier=100/1000/5000/10000 Hz,
source peak=-24/-12/-6 dBFS and rise=0/1/10/100 ms. For every source render
BITE=1/5/10, then BITE=5 in Type-2 and Auto. Render All and the carrier's Solo
band. Measure relief at 5/10/25/50/100 ms, decay, memory, GR and phase.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/03_AUTOMATION/TEST_SPEC.md -->
# 03 â€” Automation and smoothing

One 12 s / 48 kHz source. Fixed: MC404, all bands IN, X1/X2/X3 initially
300/1000/8000 Hz; Threshold=-24 dB; Ratio=4:1; Knee=0; BITE=1; Type-1;
Attack=10 ms; Release=300 ms; Input/Output=0 dB.

Use the full source for each pass. Slow moves: 2.0-4.0 s and 6.0-8.0 s. Fast
moves: 2.0-2.050 s and 6.0-6.050 s. Automate X2 1000â†’3000â†’1000 Hz, Input
0â†’+12â†’0 dB, Output 0â†’-12â†’0 dB, and Band 2 Gain 0â†’+12â†’0 dB. Toggle Band 2
IN/OUT at 2/4 s and Solo at 6/8 s. At 9 s command X1=3500 while X2=3000 Hz
and record the enforced neighbour. Repeat fast passes at buffers 32 and 2048.
Measure clicks, spikes, ramp duration and phase/magnitude discontinuity.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/04_SIDECHAIN/TEST_SPEC.md -->
# 04 â€” External sidechain

Four 12 s / 48 kHz program/key pairs; pair by frequency. Fixed: Threshold=-24
dB, Ratio=10:1, Knee=0, BITE=1, Input/Output=0 dB. Test MC202 X1=300,
MC303 X1/X2=300/3000 and MC404 X1/X2/X3=300/3000/8000; render All and the
carrier Solo band. Variables: carrier=100/1000/5000/10000 Hz, detector=internal
then external key, Time Constant=Type-1/Type-2/Auto (Auto manual min/max too).
Program is -24 dBFS continuous 1-11 s; key is -6 dBFS at 2-3,5-6,8-9 s.
Measure key-only GR, residual program detection and band routing.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/05_STEREO_DETECTOR/TEST_SPEC.md -->
# 05 â€” Stereo detector

Four 12 s / 48 kHz files. Fixed: MC404 X1/X2/X3=300/3000/8000 Hz; Threshold
=-24 dB; Ratio=10:1; Knee=0; BITE=1; Attack=0.25 ms; Release=500 ms; Type-1;
Input/Output=0 dB; internal detector; all bands IN. Variables are fully encoded
in names: L/R levels (-6/-30, -30/-6, -6/-6 anti-phase, -12/-12). Render All
and Solo Band 2. Measure left/right GR equality and detector link law.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/06_SAMPLE_RATE/TEST_SPEC.md -->
# 06 â€” Sample-rate invariance

Three 12 s stereo-float files exist at every native rate: 44.1/48/88.2/96/192
kHz. Do not resample. Auto fixed configuration: MC404 X1/X2/X3=300/3000/8000,
Threshold=-24 dB, Ratio=10:1, Knee=0, BITE=1, Input/Output=0 dB, all bands IN,
internal detector and Auto; render manual min/max. Variables are five-step 1 kHz
bursts and a 10 Hz / 50 ms 1 kHz pulse train. For impulse files use Ratio=1:1,
BITE=1, no gain, and render MC404 All plus every Solo. Measure timing in
seconds, impulse alignment, phase, crossover response and latency.


<!-- ARCHIVE: MC2000_Next_DSP_Validation_Pack/README.md -->
# MC2000 next DSP validation pack

This pack contains 89 neutral input stimuli. Every WAV is stereo IEEE-float,
32-bit, and exactly 12.000 seconds. There is no normalization, dither, limiter
or processing after MC2000. The Markdown specifications are authoritative;
MANIFEST.csv is an index only. Preserve each file's native sample rate.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/01_Parameter_Audit/TEST_SPEC.md -->
# T01 — Complete v7 Parameter Audit

## Goal
Do NOT adopt the old POC ranges. Establish the exact v7 contract from the GUI.

Audit separately:
- MC202
- MC303
- MC404

For every visible control record:
- minimum
- maximum
- default
- step/resolution
- displayed text format
- double-click/reset value
- whether it is automatable
- whether the control is visible in the selected configuration
- whether the range changes with TC mode or other state.

The v7 manual claims (to be verified in the actual demo):
- band Gain: -24 / +24 dB
- Threshold: -48 / 0 dB
- Attack: 0.25 / 25 ms
- Release: 25 / 2500 ms

Also record all controls not explicitly listed in the manual.

## Audit method
For each knob:
1. note displayed default;
2. double-click;
3. drag to exact min;
4. drag to exact max;
5. use fine-control gesture;
6. enter text if supported;
7. automate min->max->min;
8. reopen plugin/session and verify state.

## Deliverable
Fill `PARAMETER_AUDIT_TEMPLATE.csv`.
The v7 GUI result supersedes any historical range stored in the POC.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/02_External_Sidechain_VST3/TEST_SPEC.md -->
# T02 — External Sidechain VST3

## Host routing
Use the v7 demo in Studio One and verify whether the VST3 instance exposes an actual sidechain/key input bus.

If no bus is shown, record that fact with:
- Studio One version
- MC2000 build
- VST3 format
- channel/bus configuration.

## Base program
Use the 100 Hz / 1 kHz / 5 kHz program files.

Settings:
- Threshold -24 dB
- Ratio 10:1
- Knee 0
- BITE minimum
- Type-1
- Attack fast
- Release 500 ms
- Input/Output 0 dB

Route the key file to the external sidechain.

## Required questions
1. Does external key replace the internal detector or add to it?
2. Is the key filtered by each crossover?
3. Is the key full-band?
4. Does each band have an independent sidechain?
5. Does a signal on one key band trigger only that band?
6. Stereo key: max(L,R), sum, average, or another detector?
7. Antiphase key: does detection survive?
8. Does sidechain affect metering or only GR?

Run on:
- MC202
- MC303
- MC404
- isolated bands where practical.

The key source files are:
- T02_SC_Key_100Hz.wav
- T02_SC_Key_1kHz.wav
- T02_SC_Key_5kHz.wav
- T02_SC_Key_L6_R30.wav
- T02_SC_Key_L30_R6.wav
- T02_SC_Key_L6_R6_Antiphase.wav


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/03_Linking_Attack_Release/TEST_SPEC.md -->
# T03 — Exact Linking Law for Attack/Release and Other Parameters

## Scope
MC303 and MC404.

## Controls to audit
- Gain
- Threshold
- Ratio
- Knee
- BITE
- Attack
- Release
- TC Type

## Core experiment
For every possible master/slave pairing:
1. Start master and slave at deliberately distant values.
2. Record both values.
3. Move master across the complete usable range.
4. Record slave values at at least 10 points.
5. Repeat near min/max.
6. Edit slave manually.
7. Move master again.
8. Unlink.
9. Re-link.
10. Change the master again.

## Critical Attack/Release question
The manual says Attack/Release are linked relatively, but their GUI scale is nonlinear. Do not model it as:

`slave_ms = master_ms + constant_ms`

Instead record the GUI value at each master position and fit a relation in the **normalized parameter domain**.

Test separately:
- Attack
- Release

## Absolute-link exception
TC Type and Auto are absolute links.

When Auto or TC Type is changed on a slave:
- verify whether that parameter only detaches that slave from the master;
- verify whether the other linked parameters remain linked.

This exception must be implemented in the parameter-link layer, not the DSP.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/04_Crossover_Overlap_GUI/TEST_SPEC.md -->
# T04 — Crossover Overlap / GUI Constraint

## MC303
Test:
- X1 moved toward X2
- X2 toward X1
- X2 toward X3
- X3 toward X2
- direct text input crossing a neighbour
- rapid automation around the minimum separation

## MC404
Repeat for X1/X2/X3.

## Record
At every attempted crossing:
- commanded value;
- actual X1/X2/X3;
- which adjacent crossover moved;
- whether movement is immediate or smoothed;
- minimum separation;
- whether the GUI uses frequency-logarithmic or linear drag;
- behavior under text entry;
- behavior during automation.

## Critical implementation question
The manual says that when one crossover reaches another, adjacent points are updated so that the 2/3/4-band structure remains valid.

Our current POC only clamps the point being dragged. This test should determine the actual rule before replacing the current implementation.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/05_Master_Bypass_IN_OUT/TEST_SPEC.md -->
# T05 — Master Bypass and IN/OUT

## Master Bypass
Set up strong multiband compression.
During sustained audio:
- toggle Master Bypass on/off;
- inspect all band IN states;
- inspect Solo states;
- inspect meters.

Determine:
1. Does Master Bypass set all bands to OUT?
2. Is the crossover network bypassed too?
3. Does bypassed output become sample/bit identical to external dry?
4. Is there phase/all-pass coloration while bypassed?
5. Is there click/smoothing?
6. How long is the transition?

## IN interaction
With one band already OUT:
- toggle Master Bypass;
- restore Master Bypass;
- inspect that band's previous IN state.

## Solo interaction
Repeat with Solo active on a band.

Record state transitions and audio behavior.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/06_Metering_Clip_PeakHold/TEST_SPEC.md -->
# T06 — Metering / Clip / Peak Hold

## Meter selector
Verify:
- Band Ins
- Band Outs

On MC202/303/404:
- select input mode;
- record meter readings;
- select output mode;
- record readings.

## GR meters
Verify one GR indication per band.

## Clip LED
Test:
- normal peaks;
- repeated clipped/over peaks;
- band clip;
- master clip.

Record:
- exact threshold;
- latch/hold duration;
- whether click resets only selected LED or all.

## Reset
- click individual clip LED;
- press Alt+click on master clip;
- verify whether all peak holds reset.

## -inf
Use T06_Meter_LevelSweep and check exact display transition around -99.9 dB.

## Buffer-size stability
Repeat at:
- 32
- 256
- 1024
- >1024, e.g. 2048
at 44.1 kHz and 48 kHz.
Record meter update behavior, peak accuracy and GR display stability.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/07_Preset_State_2_3_4/TEST_SPEC.md -->
# T07 — Preset / State Compatibility for a Single 2/3/4-Band VST

Our product will use one VST with a selected band-count mode, unlike the historical separate MC202/303/404 products.

Create a distinctive preset:
- unique crossover frequencies;
- unique band gains;
- unique threshold/ratio/knee/BITE/Attack/Release/TC Type;
- unique Link master;
- unique Solo/IN;
- meter mode;
- master gains.

Save from:
- 2-band mode
- 3-band mode
- 4-band mode

Load into every other mode.

Verify:
- band count;
- crossover values;
- bands that exist in both source/target;
- missing bands return to target defaults;
- Link state;
- Solo/IN;
- meter mode;
- global values.

Also test:
1. save preset;
2. automate parameters;
3. close/reopen session;
4. save DAW project;
5. reopen;
6. compare state bit-for-bit / parameter-by-parameter.

Never assume the historical separate plug-in semantics map directly to the single-VST product; the test establishes our migration policy.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/08_Input_Headroom_Clip/TEST_SPEC.md -->
# T08 — Input Headroom / Clip (not saturation)

The v7 manual states there is no input-stage saturation/protection.

Test:
Input Gain = 0 / +6 / +12 / +18 / +24 dB
Sine levels = -30 / -18 / -6 / -1 dBFS

Measure:
- waveform deviation;
- THD;
- IMD;
- clip threshold;
- band clip LED;
- master clip LED.

Two-tone:
- 100 + 1000 Hz
- Input Gain 0 / +12 / +24 dB

Goal:
verify that nonlinear distortion does NOT gradually appear as an analog-style saturation curve before digital clipping.


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/09_Latency_VST3_Host/TEST_SPEC.md -->
# T09 — Zero Latency / VST3 Host

## Host
Use Studio One and at least one second DAW if available.

## Plug-in
Test:
- MC202
- MC303
- MC404

## Source
Use the corresponding single-sample impulse at -6 dBFS.

## Buffer sizes
32
256
1024
2048 samples

## Tests
1. Read the host's reported plug-in latency.
2. Record processed impulse position.
3. Compare with bypass.
4. Perform null test with neutral settings.
5. Repeat for all crossover configurations.
6. Repeat realtime bounce and offline bounce.

## Questions
- Is reported latency exactly zero?
- Does it remain zero across buffer sizes?
- Does offline render shift the impulse?
- Does bypass introduce/remove a phase/all-pass response?
- Is the crossover latency only mathematical phase or actual sample delay?


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/README.md -->
# MC2000 v7 Audit Black-Box Test Pack

Final audit suite intended to run against the **MC2000 v7 demo**.

This pack is intentionally different from the previous two:
- it does not primarily search for unknown DSP formulas;
- it establishes the actual v7 parameter contract, host routing, GUI behavior, state/preset behavior, meters, bypass, crossover constraints and zero-latency behavior.

## Priority
P0:
1. Parameter audit
2. External sidechain
3. Linking law
4. Crossover overlap
5. Master bypass
6. Latency / VST3 host behavior

P1:
7. Metering / clip / peak hold
8. Preset/state 2/3/4-band
9. Input headroom / clip

## Important rule
The **v7 demo GUI is the final authority** when its actual values differ from a historical manual. Record:
- exact v7 build/version;
- exact displayed min/max/default;
- actual value after reset;
- actual automation range;
- exact text formatting.

## Naming
Every render should preserve the source filename and add actual settings, e.g.:

`T02_SC_1k_KeyBurst_Band2__MC303_T-24_R10_T1.wav`

## Rendering
- 32-bit float WAV
- source sample rate unless the test specifies otherwise
- no normalization
- no limiter/dither
- preserve source timing
- MC2000 v7 as the only processor unless the test explicitly involves a routing/host function


<!-- ARCHIVE: MC2000_V7_Audit_TestPack/MC2000_V7_Audit_TestPack/V7_AUDIT_RESULTS_CHECKLIST.md -->
# v7 Audit Result Checklist

## P0
- [ ] T01 Parameter contract complete for MC202/303/404
- [ ] T02 VST3 sidechain bus confirmed or rejected
- [ ] T03 exact relative linking law measured
- [ ] T04 crossover neighbor-update rule measured
- [ ] T05 master bypass and IN/OUT state behavior measured
- [ ] T09 zero-latency host behavior confirmed

## P1
- [ ] T06 meter modes / clip / peak hold
- [ ] T07 2/3/4-band state migration
- [ ] T08 input headroom / clipping

## Required metadata for every test
- MC2000 version/build
- host and host version
- sample rate
- buffer size
- exact actual parameter values
- whether render is realtime/offline


