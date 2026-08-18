# Proof of Concept — Multi-Band Compressor ispirato a McDSP MC2000

**Documento tecnico di ricostruzione comportamentale / clean-room POC**  
**Target principale:** comportamento MC2000 / MC404 di generazione NextGen-v6, con supporto architetturale anche a 2 e 3 bande  
**Stack prevista:** C++ proprietario + JUCE + foleys_gui_magic + CMake  
**Formato plug-in principale:** VST3 tramite JUCE  
**Stato del documento:** POC tecnico basato su documentazione, ricerca algoritmica e misure black-box eseguite in DAW  
**Data:** 2026-08-18

---

## 0. Scopo del documento

Questo documento raccoglie in un'unica specifica il lavoro svolto per comprendere e ricreare il comportamento di un compressore multibanda della famiglia **McDSP MC2000**.

L'obiettivo non è decompilare, disassemblare o copiare il binario originale. L'approccio è:

1. studiare la documentazione pubblica;
2. osservare il comportamento esterno del plug-in tramite segnali di test;
3. formulare modelli DSP indipendenti;
4. confrontare numericamente il nostro output con quello originale;
5. iterare finché l'errore diventa sufficientemente basso;
6. implementare un prodotto con codice C++ autonomo e una GUI originale.

Il POC deve essere abbastanza dettagliato da permettere a uno sviluppatore C++ audio di partire da zero e ricostruire progressivamente:

- MC202 — 2 bande;
- MC303 — 3 bande;
- MC404 — 4 bande;
- crossover;
- compressor gain computer;
- Knee;
- BITE;
- Type-1;
- Type-2;
- Auto;
- stereo linking;
- band linking;
- metering;
- gestione parametri;
- GUI completa;
- preset/state;
- test automatici;
- build VST3.

---

# 1. Convenzioni di evidenza

Ogni informazione tecnica importante dovrebbe essere classificata con una delle seguenti etichette.

| Etichetta | Significato |
|---|---|
| **[OFFICIAL]** | dichiarato da McDSP o da documentazione ufficiale/di prodotto |
| **[MANUAL]** | presente nel manuale storico MC2000/CompressorBank |
| **[MEASURED]** | ricavato dai render WAV del plug-in originale |
| **[INFERRED]** | deduzione coerente con i dati ma non ancora dimostrata |
| **[CANDIDATE]** | modello algoritmico da provare |
| **[TODO]** | comportamento ancora da misurare o fittare |

Questa distinzione è fondamentale: il codice finale non deve trasformare automaticamente un'ipotesi in una “verità” solo perché sembra plausibile.

---

# 2. Fonti principali

## 2.1 McDSP

Fonti ufficiali correnti:

- MC2000:
  - https://mcdsp.com/plugin-index/mc2000/
- CompressorBank:
  - https://mcdsp.com/plugin-index/compressor-bank/
- SPC2000:
  - https://mcdsp.com/plugin-index/spc2000/
- download McDSP:
  - https://mcdsp.com/downloads/plugin-downloads/
- CompressorBank pagina storica:
  - https://mcdsp.com/plugin-index-deprecated-3/compressor-bank-deprecatedapr2022/

Fonti manuali/storiche usate come riferimento tecnico:

- manuale MC2000 storico indicizzato online:
  - https://www.scribd.com/document/1028389533/toaz-info-mcdsp-mc2000-plug-in-manual-pr-d7e83a734a8a669490b92083278b3653
- manuale McDSP/CompressorBank storico indicizzato:
  - https://manuals.plus/m/5762f74cfd3ffc21739ff3512cb0928f64e0492000fa300343f448db70dad097.pdf

Informazioni NextGen utili per distinguere la GUI legacy dalla linea v5/v6:

- https://s3-eu-west-1.amazonaws.com/s3-euw1-ap-pe-ws4-cws-documents.ri-prod/9780415814591/Pro_Tools_APP12_Online_2PP.pdf

## 2.2 Airwindows

Repository ufficiale:

- https://github.com/airwindows/airwindows
- licenza:
  - https://github.com/airwindows/airwindows/blob/master/LICENSE

Candidati DSP studiati:

- Pressure5:
  - `plugins/WinVST/Pressure5/Pressure5Proc.cpp`
- Logical4:
  - `plugins/WinVST/Logical4/Logical4Proc.cpp`
- Pyewacket:
  - `plugins/WinVST/Pyewacket/PyewacketProc.cpp`
- ButterComp2
- Pop / Pop2
- Spiral / Spiral2
- PurestSaturation
- BiquadNonLin
- Isolator2
- StoneFireComp

Airwindows è MIT. Se porzioni sostanziali di codice vengono riutilizzate direttamente, il relativo copyright e testo di licenza devono essere preservati.

## 2.3 Letteratura DSP

Riferimento principale per compressori digitali e automazione dei parametri:

D. Giannoulis, M. Massberg, J. D. Reiss  
**Parameter Automation in a Dynamic Range Compressor**, JAES, 2013

- https://joshreiss.github.io/documents/2013/Giannoulis%20Massberg%20Reiss%20-%20dynamic%20range%20compression%20automation%20-%20JAES%202013.pdf

Riferimento utile per grey-box modeling:

- Wright et al., **Grey-Box Modelling of Dynamic Range Compression**
- https://acris.aalto.fi/ws/portalfiles/portal/89253381/Wright_et_alii_GREY_BOX_MODELLING_OF_DYNAMIC_RANGE_COMPRESSION.pdf

---

# 3. Architettura del prodotto

## 3.1 Configurazioni

**[OFFICIAL]** MC2000 è una famiglia di tre configurazioni:

- MC202 → 2 bande;
- MC303 → 3 bande;
- MC404 → 4 bande.

Il nostro progetto dovrebbe supportare una di queste due strategie.

### Strategia A — un solo binario con modalità 2/3/4 bande

Vantaggi:

- un solo prodotto;
- preset condivisi;
- meno manutenzione;
- più semplice per il cliente.

### Strategia B — tre target distinti

Esempio:

- `Product202.vst3`
- `Product303.vst3`
- `Product404.vst3`

Vantaggi:

- replica più fedele al concetto storico;
- numero parametri fisso per target;
- automazione host più pulita.

Per il POC si consiglia **un solo DSP core** parametrizzato con `numBands`, ma è possibile produrre tre target finali.

---

# 4. Stack software

## 4.1 Core DSP

Il core audio deve essere **C++ proprietario indipendente da JUCE**.

Dipendenze consentite nel core:

- C++ standard library;
- eventualmente piccole utility matematiche interne.

Da evitare nel core:

- `juce::AudioBuffer`;
- `juce::dsp`;
- `AudioProcessorValueTreeState`;
- foleys;
- tipi VST3.

Interfaccia ideale:

```cpp
namespace product::dsp
{
    class MultiBandCompressor
    {
    public:
        void prepare (double sampleRate,
                      int maxBlockSize,
                      int numChannels);

        void reset() noexcept;

        void process (float** channels,
                      int numChannels,
                      int numSamples) noexcept;

        void setParameters (const Parameters&) noexcept;
    };
}
```

Questo rende possibile:

- unit test senza DAW;
- benchmark;
- eventuale porting AU/AAX/CLAP;
- applicazioni standalone;
- server processing;
- confronto offline con file WAV.

---

## 4.2 JUCE

JUCE gestisce:

- wrapper VST3;
- bus audio;
- automazione;
- stato;
- host communication;
- editor;
- preset host;
- gestione formato;
- cross-platform.

Il DSP non deve dipendere da JUCE.

---

## 4.3 Steinberg VST3 SDK

Decisione progettuale:

> **non usare Steinberg VST3 SDK come secondo framework parallelo a JUCE.**

JUCE rimane il wrapper principale.

Lo Steinberg SDK può essere mantenuto come:

- documentazione;
- Validator;
- VST3 Plugin Test Host;
- Inspector;
- test di conformità;
- eventuali estensioni specifiche via `juce::VST3ClientExtensions`.

Dal VST3 SDK 3.8 la licenza principale è MIT.

Fonti:

- https://steinbergmedia.github.io/vst3_dev_portal/pages/VST%2B3%2BLicensing/VST3%2BLicense.html
- https://steinbergmedia.github.io/vst3_dev_portal/pages/What%2Bis%2Bthe%2BVST%2B3%2BSDK/Validator.html
- https://docs.juce.com/master/structjuce_1_1VST3ClientExtensions.html

---

## 4.4 foleys_gui_magic

Usato sopra JUCE per:

- layout;
- DOM GUI;
- CSS;
- connessione parametri;
- meter;
- plot;
- editor rapido della GUI.

Repository:

- https://github.com/ffAudio/foleys_gui_magic

Licenza corrente del repository:

- BSD 3-Clause.

In release il WYSIWYG/editor di sviluppo non deve essere incluso.

---

# 5. Struttura repository consigliata

```text
MultibandCompressor/
│
├── CMakeLists.txt
├── cmake/
│   ├── CompilerWarnings.cmake
│   ├── Sanitizers.cmake
│   └── Packaging.cmake
│
├── Source/
│   ├── DSP/
│   │   ├── Core/
│   │   │   ├── MultiBandCompressor.h
│   │   │   ├── MultiBandCompressor.cpp
│   │   │   ├── BandProcessor.h
│   │   │   └── BandProcessor.cpp
│   │   │
│   │   ├── Crossover/
│   │   │   ├── Biquad.h
│   │   │   ├── LinkwitzRiley4.h
│   │   │   ├── CrossoverNetwork.h
│   │   │   └── CrossoverNetwork.cpp
│   │   │
│   │   ├── Compression/
│   │   │   ├── PeakDetector.h
│   │   │   ├── GainComputer.h
│   │   │   ├── KneeModel.h
│   │   │   ├── Type1Ballistics.h
│   │   │   ├── Type2Ballistics.h
│   │   │   ├── AutoBallistics.h
│   │   │   ├── BiteProcessor.h
│   │   │   └── CompressorBand.h
│   │   │
│   │   ├── Metering/
│   │   │   ├── MeterBallistics.h
│   │   │   └── MeterData.h
│   │   │
│   │   └── Utilities/
│   │       ├── Db.h
│   │       ├── Smoother.h
│   │       └── Math.h
│   │
│   ├── Plugin/
│   │   ├── PluginProcessor.h
│   │   ├── PluginProcessor.cpp
│   │   ├── Parameters.h
│   │   ├── Parameters.cpp
│   │   ├── State.h
│   │   └── VST3Extensions.h
│   │
│   └── GUI/
│       ├── PluginEditor.h
│       ├── PluginEditor.cpp
│       ├── GuiState.cpp
│       ├── CrossoverPlot.*
│       ├── CompressionPlot.*
│       ├── BandMeter.*
│       └── Theme/
│
├── Resources/
│   ├── GUI/
│   │   ├── magic.xml
│   │   └── style.css
│   └── Presets/
│
├── Tests/
│   ├── Unit/
│   │   ├── TestBiquad.cpp
│   │   ├── TestLR4.cpp
│   │   ├── TestGainComputer.cpp
│   │   ├── TestType1.cpp
│   │   └── TestStereoDetector.cpp
│   │
│   ├── Golden/
│   │   ├── ReferenceWav/
│   │   └── ExpectedCurves/
│   │
│   └── Integration/
│
├── Research/
│   ├── MC2000/
│   │   ├── TestPack/
│   │   ├── Renders/
│   │   ├── Analysis/
│   │   └── Measurements.md
│   │
│   ├── AirwindowsCandidates/
│   └── Papers/
│
└── third_party/
    ├── JUCE/
    ├── foleys_gui_magic/
    └── optional-research-only/
```

---

# 6. Signal flow generale

Il modello generale previsto è:

```text
                 INPUT
                   │
                   ▼
             Master Input Gain
                   │
                   ▼
             Crossover Network
                   │
     ┌─────────────┼─────────────┬─────────────┐
     ▼             ▼             ▼             ▼
   BAND 1        BAND 2        BAND 3        BAND 4
     │             │             │             │
     ▼             ▼             ▼             ▼
 Stereo Peak    Stereo Peak    Stereo Peak    Stereo Peak
 Detector       Detector       Detector       Detector
     │             │             │             │
     ▼             ▼             ▼             ▼
 Time Constant / Ballistics per band
     │
     ▼
 Static Gain Computer
 Threshold + Ratio + Knee
     │
     ▼
 BITE transient GR modification
     │
     ▼
 Gain smoothing / gain application
     │
     ▼
 Per-band Makeup Gain
     │
     └─────────────┬─────────────┬─────────────┐
                   ▼
                 SUM
                   │
                   ▼
            Master Output Gain
                   │
                   ▼
                 OUTPUT
```

L'esatta posizione matematica del BITE nella catena di controllo è ancora da validare. Nel POC deve quindi essere una classe separata facilmente spostabile prima/dopo il gain computer.

---

# 7. Controlli globali

## 7.1 Input Gain

### Generazione storica

**[MANUAL]**

- range: `-12 dB ... +12 dB`.

### NextGen / v5-v6

La documentazione NextGen indica:

- `-24 dB ... +24 dB`.

Per il target v6 si adotta:

```text
Input Gain: -24.0 ... +24.0 dB
default: 0.0 dB
```

**[TODO]** verificare step esatto e skew nella build originale target.

---

## 7.2 Master Output Gain

Manuale storico:

```text
-24 ... +24 dB
```

Da implementare inizialmente:

```text
Master Output: -24.0 ... +24.0 dB
default: 0.0 dB
```

---

## 7.3 Master polarity / phase

La GUI storica esponeva polarità del master output.

Funzione:

```text
phase normal: y = x
phase invert: y = -x
```

Per stereo, il manuale storico descrive controlli di output linkabili.

**Target POC:** implementare un controllo globale `Phase Invert`.

**[TODO]** verificare se la specifica v6 target presenta esattamente lo stesso controllo/posizione grafica.

---

## 7.4 Band Link

**[OFFICIAL]/[MANUAL]**

Modalità:

- Unlinked;
- Master 1;
- Master 2;
- Master 3;
- Master 4, se presente.

Comportamento storico:

- i normali parametri delle bande vengono linkati **relativamente**;
- Auto e TC Type vengono linkati in modo **assoluto**;
- Solo e In/Out rimangono indipendenti;
- modificare manualmente uno slave può aggiornare il suo offset relativo.

Esempio:

```text
Band 1 Threshold = -24 dB
Band 2 Threshold = -30 dB

Band 1 diventa Master.

Band 1 +2 dB:
Band 1 = -22
Band 2 = -28
```

Questa logica deve vivere nel **Parameter/Control layer**, non nel DSP core.

---

# 8. Crossover

## 8.1 Dati ufficiali

**[OFFICIAL]/[MANUAL]**

- filtri: `24 dB/oct`;
- frequenze crossover: `20 Hz ... 20 kHz`;
- controllo da:
  - text field;
  - trascinamento nel Crossover Display;
- i punti non devono incrociarsi in modo da distruggere la distinzione delle bande.

La documentazione storica dichiara che i filtri derivano da FilterBank.

---

## 8.2 Risultato black-box

**[MEASURED]**

I render a 48 kHz con crossover a 1 kHz mostrano una corrispondenza praticamente esatta con:

> **Linkwitz–Riley di 4° ordine ottenuto da due Butterworth 2° ordine in cascata.**

A 1 kHz:

```text
LOW  ≈ -6.020599 dB
HIGH ≈ -6.020601 dB
```

La somma LOW+HIGH mostra magnitude piatta a unity con la normale fase all-pass della rete LR4.

Il confronto con un impulso LR4 calcolato indipendentemente ha prodotto un errore nell'ordine di:

```text
~ 9.1e-9
```

compatibile con precisione float dei file di test.

---

## 8.3 Biquad base

Definire:

```text
K = tan(pi * fc / fs)
Q = 1 / sqrt(2)
norm = 1 / (1 + K/Q + K^2)
```

Low-pass 2° ordine:

```text
b0 = K^2 * norm
b1 = 2*b0
b2 = b0

a1 = 2*(K^2 - 1)*norm
a2 = (1 - K/Q + K^2)*norm
```

High-pass 2° ordine:

```text
b0 = norm
b1 = -2*norm
b2 = norm

a1 = 2*(K^2 - 1)*norm
a2 = (1 - K/Q + K^2)*norm
```

Per LR4:

```text
LP4 = LP2 -> LP2
HP4 = HP2 -> HP2
```

---

## 8.4 Coefficienti di riferimento a 48 kHz / 1 kHz

Singola sezione LP2:

```text
b0 =  0.003916126660547
b1 =  0.007832253321095
b2 =  0.003916126660547

a1 = -1.815341082704568
a2 =  0.831005589346758
```

Singola sezione HP2:

```text
b0 =  0.911586668012832
b1 = -1.823173336025663
b2 =  0.911586668012832

a1 = -1.815341082704568
a2 =  0.831005589346758
```

Questi coefficienti sono un test di regressione, non valori hard-coded.

---

## 8.5 Crossover network multibanda

Per 2 bande:

```text
x ─┬─ LP4(fc1) ── Band1
   │
   └─ HP4(fc1) ── Band2
```

Per 3/4 bande non assumere automaticamente che una semplice cascata sequenziale sia già identica al plug-in originale.

**[TODO]** misurare separatamente le bande interne per capire:

- ordine di splitting;
- all-pass compensation;
- fase delle bande 2 e 3;
- ricombinazione completa;
- comportamento quando X1/X2/X3 sono vicini.

Implementazione POC suggerita:

```cpp
class CrossoverNetwork
{
public:
    void setBandCount (int n);
    void setFrequency (int crossoverIndex, double hz);
    void process (...);
};
```

La topologia interna deve poter essere sostituita senza toccare il compressore.

---

# 9. Saturazione del crossover

## 9.1 Dichiarazione storica

Il manuale storico dichiara “analog saturation modeling” nelle sezioni del filtro.

## 9.2 Misure eseguite

**[MEASURED]**

Test a:

- 100 Hz;
- 700 Hz;
- livelli:
  - -30;
  - -24;
  - -18;
  - -12;
  - -6;
  - -3;
  - -1 dBFS.

Risultato:

- fondamentale lineare;
- nessun aumento significativo di THD;
- armoniche nell'ordine del rumore numerico, circa `-165 ... -170 dB`.

Conclusione POC:

> **non inserire saturazione nel crossover della V1.**

Il comportamento misurato del target testato è essenzialmente lineare in queste condizioni.

Possibili spiegazioni:

- la funzione storica è stata rimossa/modificata;
- si attiva solo in altre condizioni;
- dipende dall'Input Gain;
- il livello del test non raggiunge uno stadio interno critico.

**[TODO]** test futuro con Input Gain positivo e diverse sample rate.

---

# 10. Architettura per banda

Ogni banda deve essere modellata come un compressore indipendente con:

- Enable/In;
- Solo;
- Makeup Gain;
- Threshold;
- Compression/Ratio;
- Knee;
- BITE;
- Attack;
- Release;
- TC Type:
  - Type-1;
  - Type-2;
  - Auto;
- meter:
  - input;
  - output;
  - gain reduction.

Il manuale MC2000 dichiara che le bande derivano dal CompressorBank e possiedono il comportamento della configurazione CB1.

---

# 11. Parametri per banda — target v6

## 11.1 Gain

### Legacy

```text
-24 ... +24 dB
```

### NextGen / v5-v6

La revisione NextGen porta il makeup gain a:

```text
0 ... +48 dB
```

Per il target v6:

```text
Band Gain: 0 ... +48 dB
default: 0 dB
```

**[TODO]** verificare step/esatto comportamento del punto zero.

---

## 11.2 Threshold

```text
-45 ... 0 dB
```

Default da scegliere in base al preset iniziale del prodotto.

Parameter ID:

```text
bandN.threshold
```

---

## 11.3 Compression / Ratio

Manuale:

```text
1:1 ... 10:1
```

Internamente memorizzare come numero:

```text
1.0 ... 10.0
```

Parameter ID:

```text
bandN.ratio
```

---

## 11.4 Knee

Range:

```text
-10.0 ... +15.0
```

Regioni:

```text
-10 ... 0    undershoot
0           hard knee
0 ... +10   overshoot
+10 ... +15 overshoot + compression tail
```

Parameter ID:

```text
bandN.knee
```

---

## 11.5 BITE

Range storico:

```text
1.0 ... 50.0
```

Il minimo storico è `1`, non `0`.

Nel DSP conviene normalizzare:

```cpp
biteNorm = (bite - 1.0) / 49.0;
```

ottenendo:

```text
0 ... 1
```

Parameter ID:

```text
bandN.bite
```

---

## 11.6 Attack

```text
0.03 ms ... 250 ms
```

Distribuzione GUI presumibilmente fortemente non lineare/logaritmica.

Parameter ID:

```text
bandN.attackMs
```

---

## 11.7 Release

```text
5 ms ... 2500 ms
```

Parameter ID:

```text
bandN.releaseMs
```

---

## 11.8 TC Type

Target v6:

```text
Type-1
Type-2
Auto
```

Release2 appartiene alla generazione legacy ed è stato rimosso nelle revisioni NextGen.

Parameter:

```text
bandN.tcMode
```

enum:

```cpp
enum class TCMode
{
    type1,
    type2,
    automatic
};
```

---

# 12. Stereo detector

## 12.1 Dato documentato

Il manuale specifica che in stereo viene utilizzato il massimo fra i canali L e R come input del compressore.

## 12.2 Misura black-box

**[MEASURED]**

Test:

```text
L = -6 dBFS
R = -30 dBFS
```

output:

```text
L ≈ -21.174 dBFS
R ≈ -45.174 dBFS
```

La differenza di 24 dB viene preservata.

Invertendo i canali, il risultato si inverte.

Test:

```text
L = -6
R = -6
fase relativa 180°
```

produce la stessa GR del caso senza cancellazione.

Conclusione:

```cpp
double detectorInput =
    std::max (std::abs (left),
              std::abs (right));
```

e una singola gain reduction viene applicata a entrambi i canali.

Questo blocco è considerato **risolto**.

---

# 13. Gain computer — Knee = 0

Per input in dB `x`, threshold `T`, ratio `R`:

```text
x <= T:
    y = x

x > T:
    y = T + (x - T)/R
```

Gain reduction statica:

```text
GR = x - y
```

ovvero:

```text
GR = max(0, (x - T) * (1 - 1/R))
```

## Misura

**[MEASURED]**

Con:

```text
Threshold = -24 dB
Ratio = 4:1
Knee = 0
```

la compressione inizia esattamente vicino a `-24 dBFS peak`.

Questo è un forte indizio di detection di picco, non RMS.

Il ratio steady-state misurato appariva circa `3.89:1`, ma questa differenza è probabilmente spiegabile dalle ballistics Attack/Release sulla sinusoide di prova.

Per la V1:

> implementare il ratio matematico esatto.

---

# 14. Knee negativo — undershoot

## 14.1 Dato documentato

Il Knee negativo crea una transizione graduale fra:

```text
1:1
   ↓
ratio selezionato
```

ed è descritto come comportamento tipo dbx “OverEasy”.

## 14.2 Misure

**[MEASURED]**

Con test centrati attorno a:

```text
T = -24 dB
R = 4:1
```

risultati indicativi:

### Knee -5

Massima compressione addizionale rispetto a Knee 0 attorno alla threshold:

```text
~ 2.515 dB
```

### Knee -10

Massima compressione addizionale:

```text
~ 4.357 dB
```

A input `-36 dBFS`, Knee -10 produce già circa:

```text
~ 1.382 dB
```

di GR addizionale rispetto alla curva hard-knee.

Conclusione:

> Knee non è semplicemente una `softKneeWidth = abs(K)` espressa in dB.

---

# 15. Implementazione POC del Knee

Non conviene inventare immediatamente una formula “elegante” se i dati possono essere riprodotti più fedelmente con una superficie misurata.

## 15.1 Strategia consigliata V1 — normalized curve + spline

Definire:

```text
u = x - threshold
```

La deformazione del Knee può essere descritta come:

```text
deltaY = F(u, ratio, knee)
```

La curva finale:

```text
y = hardKneeOutput + deltaY
```

Costruire una LUT normalizzata con:

- valori di `u`;
- alcuni ratio;
- Knee.

Interpolazione:

- monotonic cubic / Hermite;
- eventualmente bicubic fra ratio e Knee.

Vantaggi:

- alta fedeltà;
- facile fitting;
- nessun rischio di inventare la formula sbagliata;
- costo DSP molto basso.

Successivamente, se necessario, derivare una formula analitica.

---

# 16. Knee positivo — overshoot

## 16.1 Correzione concettuale importante

Le misure mostrano che “overshoot” non significa più compressione.

Nel nostro dataset:

> la curva di output sale **sopra** la curva hard-knee, quindi viene applicata temporaneamente **meno gain reduction**.

## 16.2 Misure

Con:

```text
Threshold = -24
Ratio = 4:1
```

### Knee +5

Massima differenza rispetto a Knee 0:

```text
~ +1.364 dB
```

circa a:

```text
input ~ -18 dBFS
```

A input `-3 dBFS`:

```text
differenza ~ +0.051 dB
```

quindi la curva ritorna quasi alla ratio normale.

### Knee +10

Massimo:

```text
~ +1.924 dB
```

circa a:

```text
input ~ -17 dBFS
```

A `-3 dBFS`:

```text
~ +0.098 dB
```

Quindi la zona `0 ... +10` può essere vista come un hump localizzato della transfer curve.

---

# 17. Compression tail — Knee > +10

## 17.1 Dato documentato

Il manuale indica che oltre +10 compare una “tail” che riduce la ratio per segnali molto sopra threshold.

## 17.2 Misure

### Knee +12.5

Overshoot massimo:

```text
~ +2.125 dB
```

A input `-3 dBFS` rimangono ancora circa:

```text
+2.062 dB
```

rispetto a Knee 0.

### Knee +15

A input `-3 dBFS`:

```text
~ +4.069 dB
```

rispetto a Knee 0.

Conclusione:

```text
Knee positivo =
    hump overshoot
    +
    tail che diventa progressivamente importante sopra +10
```

Implementare come due componenti separate è una buona architettura interna.

---

# 18. Gain computer proposto

```cpp
class GainComputer
{
public:
    double computeOutputDb (double inputDb,
                            double thresholdDb,
                            double ratio,
                            double knee) const noexcept;

    double computeGainReductionDb (...) const noexcept;
};
```

Internamente:

```text
1. hard curve
2. undershoot/overshoot correction
3. optional tail correction
```

Pseudo:

```cpp
double GainComputer::computeOutputDb (...)
{
    const double hard = hardKnee (...);

    if (knee < 0.0)
        return hard + negativeKneeCorrection (...);

    if (knee <= 10.0)
        return hard + overshootCorrection (...);

    return hard
         + overshootCorrection (..., 10.0)
         + tailCorrection (...);
}
```

Per il primo POC le correction possono provenire da LUT/Hermite.

---

# 19. Attack

## 19.1 Dato documentato

Attack è la velocità con cui il compressore segue un segnale crescente sopra threshold.

Range:

```text
0.03 ... 250 ms
```

## 19.2 Test eseguiti

Nel test pack sono stati inclusi step a:

```text
-40 dBFS
→ -6 dBFS
→ -40 dBFS
```

con diversi valori di Attack.

**[TODO]**

Il dataset esiste, ma il POC corrente non fissa ancora una formula definitiva dell'Attack.

Prima implementazione:

```text
one-pole peak attack
```

con mapping di tempo parametrico calibrabile.

Non assumere che il valore visualizzato corrisponda direttamente a:

```text
63.2%
90%
10 dB
```

senza ulteriore fitting.

---

# 20. Type-1 — Pure Peak Detection

## 20.1 Comportamento documentato

**[MANUAL]/[OFFICIAL]**

Durante la release:

> un nuovo segnale più basso dell'envelope di release corrente non modifica la traiettoria di release.

Quindi:

```text
new detector <= release envelope
    -> ignore for the release trajectory

new detector > envelope
    -> nuovo evento / attack
```

---

## 20.2 Release misurata

Test con Release:

```text
50 ms
100 ms
250 ms
500 ms
1000 ms
```

Le curve mostrano una forma simile scalata nel tempo.

Rapporti empirici:

```text
t50   ≈ 1.336 * R
t1/e  ≈ 1.799 * R
t10%  ≈ 3.40  * R
```

Un buon fit empirico ottenuto è:

```text
GR(t) ≈ GR0 *
        exp( - ( t / (1.779 * R) ) ^ 1.286 )
```

dove:

- `R` è il valore Release in secondi;
- `GR0` è la gain reduction all'inizio della release.

Questa non è necessariamente la formula originale, ma è una descrizione efficace dei render misurati.

---

# 21. Type-1 implementation candidate

### Approccio POC A — release age

Mantenere:

```cpp
double releaseStartGrDb;
double releaseAgeSeconds;
```

Quando parte una release:

```text
releaseStartGrDb = currentGR
releaseAge = 0
```

A ogni sample:

```text
GR = releaseStartGr *
     exp( -pow(releaseAge/(1.779*R), 1.286) )
```

Se un nuovo detector richiede più GR dell'envelope corrente:

- entra in attack;
- azzera/resetta lo stato release.

Vantaggio:

- riproduce bene il test isolato.

Svantaggio:

- richiede validazione con materiale continuo;
- potrebbe non corrispondere all'implementazione interna originale.

### Approccio POC B — state-dependent one-pole

Derivare un coefficiente istantaneo equivalente alla stretched exponential.

Più adatto a materiale continuo, ma più complesso.

Entrambe le strategie devono essere presenti come prototipi intercambiabili.

---

# 22. Type-2 — Adaptive Release

## 22.1 Dato documentato

Type-2 reagisce a nuovi eventi anche se il nuovo segnale è sotto l'envelope di release corrente.

Questa è la differenza fondamentale da Type-1.

---

## 22.2 Misure

In assenza di nuovi eventi forti, Type-2 si comporta grossolanamente come una release più rapida:

```text
R_type2_base ≈ 0.77 * R_type1
```

cioè circa 23% più veloce nel dataset misurato.

Quando arriva un secondo evento, la pendenza cambia.

Test con secondo evento `-15 dBFS` a diversi tempi della release hanno prodotto envelope stimati:

```text
100 ms -> ~ -6.79 dB
250 ms -> ~ -8.12 dB
500 ms -> ~ -10.25 dB
750 ms -> ~ -12.25 dB
```

Un modello empirico candidato:

```text
v = v0 * (1 - k * D/E)
```

con:

```text
k ≈ 0.42
```

dove:

- `D` = detector corrente in ampiezza lineare;
- `E` = envelope corrente;
- `v0` = velocità base della release.

I quattro test hanno suggerito valori di k nell'ordine:

```text
0.40
0.41
0.42
0.44
```

quindi `0.42` è un buon punto iniziale, **non** una costante considerata già dimostrata.

---

# 23. Airwindows per Type-2

## Pressure5

Pressure5 mantiene stati interni di velocità (`muSpeed`) dipendenti dal segnale e dal controllo release.

Interessante perché:

```text
nuovo livello
    ↓
modifica velocità interna
    ↓
modifica envelope/gain
```

È una famiglia concettualmente compatibile con Adaptive Release.

## Logical4

Logical4 include un comportamento di release/chase variabile dipendente dallo swing del segnale.

È un secondo modello molto utile.

## ButterComp2

Modello più semplice di release dipendente dal livello.

### Strategia di sviluppo

Implementare tre prototipi:

```text
Type2Model_A_Measured
Type2Model_B_PressureStyle
Type2Model_C_LogicalStyle
```

Confrontarli automaticamente con i WAV originali.

Il modello con errore più basso diventa la base del DSP definitivo.

---

# 24. AUTO

## 24.1 Dato documentato

Auto:

- calcola Attack e Release dal segnale;
- disabilita i controlli manuali Attack/Release;
- è program-dependent.

## 24.2 Stato

**[TODO]** nel primo test pack Auto non è stato caratterizzato in modo sufficiente.

---

# 25. Auto candidate — crest factor

Il paper Giannoulis/Massberg/Reiss propone un sistema di automazione basato su caratteristiche estratte dal sidechain.

Una prima versione può usare:

```text
Peak envelope
RMS envelope
      ↓
Crest Factor
      ↓
Attack / Release dinamici
```

Con:

```text
C = Peak / RMS
```

L'Auto originale McDSP non deve essere assunto uguale a questa formula.

Questa è una **reference implementation scientifica** utile finché il black-box fitting non identifica una struttura migliore.

---

# 26. Test futuri per Auto

Creare segnali con:

- stesso RMS;
- crest factor diverso;
- stessa peak;
- densità transienti diversa.

Test minimi:

1. sine continua;
2. square/pulse;
3. sine burst;
4. noise;
5. drum-like synthetic train.

Misurare:

- tempo di attacco;
- tempo di release;
- GR a regime;
- risposta a cambi di densità.

Scopo:

capire se Auto dipende principalmente da:

- peak/RMS;
- slope;
- event density;
- envelope history;
- altra feature.

---

# 27. BITE — significato

**[OFFICIAL]/[MANUAL]**

Aumentando BITE:

- i transienti vengono lasciati passare di più;
- la quantità complessiva di compressione rimane sostanzialmente invariata.

Il manuale associa il BITE anche a rapide variazioni / “high-frequency data”, ma questo non implica necessariamente un vero ramo audio high-pass.

---

# 28. Airwindows inizialmente candidato per BITE

Pressure5/PawClaw è stato inizialmente considerato il candidato principale.

Pressure5 calcola informazione di slew/inter-sample e modifica il comportamento del compressore in funzione dei transienti.

Altri candidati concettuali:

- Pyewacket;
- Pop;
- Spiral2.

Dopo i test black-box, il semplice modello `sample slew = |x[n]-x[n-1]|` è diventato meno probabile come replica diretta.

---

# 29. BITE — risultati misurati

## 29.1 Tono continuo 10 kHz

Il file originariamente rinominato come `1000Hz` è stato verificato e contiene realmente `10 kHz`.

Con BITE massimo rispetto al minimo:

- differenza transiente iniziale circa `+3 dB` nelle condizioni di test;
- dopo circa 100 ms le due versioni convergono quasi allo stesso livello;
- a regime la differenza diventa trascurabile.

## 29.2 Fase

Durante l'onset a 10 kHz:

```text
differenza fase BITE min/max < ~0.05°
```

e spesso molto inferiore.

Questo è un forte indizio contro un vero ramo audio parallelo filtrato che venga sommato al segnale.

## 29.3 Rise time

A 1 kHz:

```text
rise 0 ms:
    forte BITE effect

rise 10 ms:
    effetto ancora importante

rise 100 ms:
    effetto molto ridotto
```

Valori indicativi misurati:

```text
0 ms   -> ~ +1.31 dB
10 ms  -> ~ +1.20 dB
100 ms -> ~ +0.22 dB
```

a regime:

```text
~ 0.002 dB
```

---

# 30. Interpretazione BITE

Il comportamento misurato favorisce:

```text
input
  │
  ├── fast envelope ─┐
  │                  ├─ transient measure
  └── slow envelope ─┘
                         │
                         ▼
                       BITE
                         │
                         ▼
                temporary GR relief
```

più che:

```text
input ─┬─ HPF dry path ─────┐
       │                    ├─ sum
       └─ compressor ───────┘
```

---

# 31. BITE candidate V1

Definire due envelope:

```text
Efast
Eslow
```

con:

```text
transient =
    max(0, Efast - Eslow)
```

Normalizzare:

```text
tNorm =
    transient / max(Efast, epsilon)
```

Calcolare una riduzione temporanea della GR:

```text
reliefDb =
    biteNorm *
    maxReliefDb *
    shape(tNorm)
```

Vincolo:

```text
reliefDb <= currentGainReductionDb
```

Gain finale:

```text
GRfinal =
    max(0, GRnormal - reliefDb)
```

Costanti iniziali candidate:

```text
fast attack/envelope:  sub-ms / pochi ms
slow/transient memory: ~15-20 ms
```

Dal dataset, la protezione BITE decade grossolanamente nell'ordine di:

```text
15 ... 20 ms
```

nelle condizioni di test.

Questi valori devono essere fittati.

---

# 32. Nota sul test BITE in frequenza

Nel test T12 i burst erano separati da solo:

```text
400 ms
```

con Release:

```text
300 ms
```

Il compressore non era completamente resettato tra tutti gli eventi.

Quindi T12 non deve essere usato per concludere definitivamente l'indipendenza dalla frequenza.

Tuttavia i burst da 500 Hz a 10 kHz mostrano risposte molto simili, che non favoriscono un semplice detector di sample slew puro.

**[TODO]** rifare ogni frequenza in un file separato con almeno 2-3 s di reset.

---

# 33. Metering

Per ogni banda:

- Input;
- Output;
- Gain Reduction.

Master:

- Output L;
- Output R;
- clip indication opzionale.

Implementazione consigliata:

```cpp
struct BandMeterData
{
    std::atomic<float> inputDb;
    std::atomic<float> outputDb;
    std::atomic<float> gainReductionDb;
};
```

Mai usare mutex nel realtime thread.

Il DSP scrive valori atomici/lock-free, la GUI legge a ~30-60 Hz.

---

# 34. GUI — obiettivo

La GUI deve riprodurre **tutte le funzioni**, ma non deve copiare logo, marchio o trade dress McDSP.

Design consigliato:

- identità visiva proprietaria;
- stessa chiarezza funzionale;
- 4 colori per le bande;
- layout adattivo;
- knob based;
- grafici grandi;
- meter leggibili.

---

# 35. GUI completa — wireframe MC404

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│ PRODUCT NAME            MODE: 4 BAND       PRESET       BYPASS              │
├──────────────┬───────────────────────────────────────────────────────────────┤
│              │                                                               │
│ INPUT        │       CROSSOVER DISPLAY             COMPRESSION DISPLAY       │
│ [-24..+24]   │                                                               │
│   KNOB       │  Band1 | Band2 | Band3 | Band4      I/O curves 1..4          │
│              │        X1      X2      X3                                     │
│ OUTPUT       │  X1 [   Hz ] X2 [   Hz ] X3 [   Hz ]                         │
│ [-24..+24]   │                                                               │
│   KNOB       │  LINK [Unlinked/M1/M2/M3/M4]      meter master               │
│ PHASE        │                                                               │
├──────────────┴───────────────────────────────────────────────────────────────┤
│ BAND 1                                                                       │
│ [IN] [SOLO]  GAIN  THRESH  RATIO  KNEE  BITE  ATTACK  RELEASE  [R1][R2][A]  │
│                 [ GR meter | IN meter | OUT meter ]                           │
├──────────────────────────────────────────────────────────────────────────────┤
│ BAND 2                                                                       │
│ [IN] [SOLO]  GAIN  THRESH  RATIO  KNEE  BITE  ATTACK  RELEASE  [R1][R2][A]  │
│                 [ GR meter | IN meter | OUT meter ]                           │
├──────────────────────────────────────────────────────────────────────────────┤
│ BAND 3                                                                       │
│ [IN] [SOLO]  GAIN  THRESH  RATIO  KNEE  BITE  ATTACK  RELEASE  [R1][R2][A]  │
│                 [ GR meter | IN meter | OUT meter ]                           │
├──────────────────────────────────────────────────────────────────────────────┤
│ BAND 4                                                                       │
│ [IN] [SOLO]  GAIN  THRESH  RATIO  KNEE  BITE  ATTACK  RELEASE  [R1][R2][A]  │
│                 [ GR meter | IN meter | OUT meter ]                           │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

# 36. GUI — comportamento Crossover Display

Il grafico deve mostrare:

- asse X logaritmico 20 Hz – 20 kHz;
- asse Y almeno da circa +12 a -48 dB;
- curve delle bande;
- area colorata opzionale;
- marker X1/X2/X3;
- drag orizzontale;
- text entry;
- fine control con modifier;
- ordine dei crossover sempre valido.

Colori esempio:

```text
Band 1 = yellow
Band 2 = green
Band 3 = orange
Band 4 = purple
```

I colori sono indicativi; per il prodotto commerciale scegliere una palette proprietaria.

---

# 37. GUI — Compression Display

Visualizzare per ogni banda:

```text
Input dB -> Output dB
```

La curva deve aggiornarsi quando cambiano:

- Threshold;
- Ratio;
- Knee.

BITE e ballistics non devono necessariamente cambiare la curva statica se il loro effetto è time-dependent.

Range grafico consigliato:

```text
X: -60 ... 0 dB
Y: -60 ... 0 dB
```

Mostrare:

- linea unity;
- curve per banda;
- colore banda;
- threshold marker opzionale.

Il plot deve usare la stessa `GainComputer` del DSP, non una formula duplicata nella GUI.

---

# 38. GUI — controlli per banda

Ordine raccomandato:

```text
IN
SOLO
GAIN
THRESHOLD
COMPRESSION
KNEE
BITE
ATTACK
RELEASE
R1
R2
AUTO
METER
```

### IN

Bypass del compressore della banda.

Definire con precisione se:

- bypassa solo gain reduction;
- oppure bypassa l'intera processing band.

Nel manuale è descritto come compressor enable/disable.

Per POC:

```text
IN = off
-> nessuna compression GR
-> band gain rimane applicato
```

**[TODO]** validare sull'originale.

### SOLO

Una o più bande possono essere solo contemporaneamente.

Il solo deve agire dopo crossover e processing della banda.

---

# 39. GUI — parametri e mapping JUCE

ID globali:

```text
global.inputGainDb
global.outputGainDb
global.phaseInvert
global.bandCount
global.linkMaster

xover.1.frequencyHz
xover.2.frequencyHz
xover.3.frequencyHz
```

Per banda:

```text
band1.enabled
band1.solo
band1.gainDb
band1.thresholdDb
band1.ratio
band1.knee
band1.bite
band1.attackMs
band1.releaseMs
band1.tcMode

band2....
band3....
band4....
```

Gli ID non devono mai cambiare dopo una release pubblica.

---

# 40. JUCE parameter layout

Esempio:

```cpp
juce::AudioProcessorValueTreeState::ParameterLayout
createParameterLayout();
```

Per i time controls usare `NormalisableRange` con skew/log mapping.

Esempio concettuale:

```cpp
NormalisableRange<float> attackRange {
    0.03f,
    250.0f
};

attackRange.setSkewForCentre (10.0f);
```

Il centre va fittato alla sensazione/mapping originale.

---

# 41. foleys_gui_magic structure

Schema DOM indicativo:

```text
Root
├── Header
├── Master
│   ├── InputKnob
│   ├── OutputKnob
│   ├── PhaseButton
│   └── LinkCombo
├── Displays
│   ├── CrossoverPlot
│   ├── CrossoverFields
│   └── CompressionPlot
└── Bands
    ├── Band1View
    ├── Band2View
    ├── Band3View
    └── Band4View
```

Ogni `BandView` contiene:

```text
InButton
SoloButton
GainSlider
ThresholdSlider
RatioSlider
KneeSlider
BiteSlider
AttackSlider
ReleaseSlider
TCSelector
Meter
```

---

# 42. Componenti GUI custom

foleys può gestire i controlli standard, ma servono componenti custom per:

1. `CrossoverPlot`
2. `CompressionPlot`
3. `BandMeter`
4. eventuale multi-handle crossover editor.

Registrare i componenti bespoke nel GUI builder.

---

# 43. Responsive layout

La GUI deve supportare almeno:

- 100%;
- 125%;
- 150%;
- 200%.

Layout:

- desktop largo → bande orizzontali;
- finestra stretta → riduzione dimensioni knob;
- non nascondere parametri fondamentali.

Per MC202/MC303:

- rimuovere fisicamente le righe non usate;
- CrossoverPlot usa 1 o 2 marker;
- Link menu mostra solo master disponibili.

---

# 44. Stato e preset

Salvare:

- tutti i parametri;
- numero bande se un solo binario;
- impostazioni GUI non-audio:
  - zoom;
  - eventuale modalità meter;
- versione schema.

Struttura ValueTree:

```text
STATE
├── version
├── parameters
└── ui
    ├── zoom
    └── meterMode
```

Aggiungere migration:

```cpp
if (savedVersion < currentVersion)
    migrateState (...);
```

---

# 45. Compatibilità preset 2/3/4 bande

Storicamente i preset fra configurazioni potevano essere condivisi con limiti dovuti al numero di bande.

Nel nostro prodotto:

- le prime N bande vengono caricate;
- le bande mancanti rimangono default;
- crossover non validi vengono riordinati/clampati.

---

# 46. Threading realtime

Regole:

- nessuna allocazione in `processBlock`;
- nessun mutex;
- nessun file I/O;
- nessun log diretto;
- nessuna GUI operation;
- parametri letti atomicamente;
- coefficienti crossover aggiornati in modo sicuro;
- smoothing su gain/frequency per evitare zipper noise.

---

# 47. Precisione

McDSP dichiara double precision / zero latency nella documentazione corrente.

Strategia consigliata:

- calcoli detector/gain/crossover in `double`;
- audio I/O può rimanere float;
- supportare `processBlock(double)` se richiesto;
- nessun lookahead;
- latency host dichiarata = 0.

---

# 48. Crossover parameter smoothing

Quando l'utente trascina X1/X2/X3:

non aggiornare brutalmente coefficienti sample-by-sample senza strategia.

Possibilità:

1. smoothing frequenza;
2. interpolazione coeff;
3. dual-filter crossfade.

Per preservare stabilità e assenza di zipper:

POC:

```text
frequency smoothing 10-30 ms
```

ma verificare il comportamento originale con automation rapida.

---

# 49. Gain smoothing

Separare:

- detector envelope;
- target gain;
- applied gain.

Non applicare direttamente un target dB discontinuo.

Per Type-1/2 la ballistics è parte fondamentale del suono, quindi il gain smoothing **non deve introdurre una seconda Attack/Release non voluta**.

---

# 50. Test pack già realizzato

È stato generato un pacchetto WAV a:

```text
48 kHz
32-bit float
```

con test T01-T15.

I test coprono:

- baseline;
- crossover;
- crossover sum;
- impulse;
- saturazione;
- static curve;
- negative Knee;
- positive Knee;
- attack;
- release;
- Type-1 vs Type-2;
- Type-2 timing;
- BITE frequency;
- BITE rise time;
- BITE continuous;
- stereo link.

Sono stati analizzati 49 render del plug-in originale.

---

# 51. Test di regressione che devono entrare nel repository

## 51.1 LR4 impulse test

A:

```text
fs = 48000
fc = 1000
```

confrontare impulse response con il reference.

Tolleranza iniziale:

```text
max abs error <= 1e-7
```

---

## 51.2 Static curve

Test:

```text
T = -24
R = 4
K = 0
```

Livelli:

```text
-48
-42
-36
-30
-27
-24
-21
-18
-15
-12
-9
-6
-3
```

Verificare il modello.

---

## 51.3 Stereo detector

Test antiphase:

```text
L = sine
R = -sine
```

La GR non deve annullarsi.

---

## 51.4 Type-1 second event

Il secondo evento sotto l'envelope non deve alterare la release.

---

## 51.5 Type-2 second event

Lo stesso evento deve alterare la release.

---

# 52. Metriche di confronto

Per ogni algoritmo candidato misurare:

### Waveform

```text
max abs error
RMSE
NRMSE
```

### Gain

Ricostruire:

```text
gain(t) = y(t) / x(t)
```

quando numericamente stabile.

Confrontare:

```text
GR_ref(t)
GR_model(t)
```

### Static curve

```text
mean abs dB error
max dB error
```

### Crossover

```text
magnitude error
phase error
impulse error
```

---

# 53. Obiettivi POC di accuratezza

Prima milestone:

```text
Crossover:
    max magnitude error < 0.05 dB
```

ma il blocco già identificato dovrebbe ottenere molto meglio.

Static curve:

```text
< 0.1 dB medio
< 0.3 dB max
```

Ballistics:

```text
GR trajectory MAE < 0.2 dB
```

BITE:

```text
onset GR difference < 0.3 dB
```

Questi target possono diventare più severi in seguito.

---

# 54. Airwindows — ruolo corretto

Airwindows non deve diventare il “motore” del prodotto intero.

Uso consigliato:

```text
Airwindows
    ↓
identificazione di primitive/famiglie DSP
    ↓
prototipo
    ↓
confronto MC2000
    ↓
modello proprietario finale
```

Candidati:

| Blocco | Airwindows |
|---|---|
| Type-2 | Pressure5, Logical4 |
| adaptive release | Pressure5, Logical4, ButterComp2 |
| transient concepts | Pressure5, Pyewacket, Pop |
| waveshaping eventuale | Spiral, PurestSaturation |
| nonlinear filter research | BiquadNonLin |
| crossover | nessun candidato necessario |

---

# 55. Perché Pressure5 non è più il candidato diretto n.1 per BITE

Pressure5 usa meccanismi correlati allo slew/sample movement.

Se BITE fosse direttamente proporzionale a:

```text
abs(x[n] - x[n-1])
```

a parità di ampiezza ci aspetteremmo una forte dipendenza dalla frequenza.

I test disponibili non mostrano una differenza sufficiente per sostenere questa ipotesi come modello primario.

Quindi Pressure5 resta:

- utile per Type-2;
- utile come reference transiente;
- non da copiare direttamente come BITE.

---

# 56. Clean-room engineering

Separare idealmente:

## Research notes

Contengono:

- comportamento osservato;
- grafici;
- parametri;
- misure.

## Implementation spec

Contiene:

- formule indipendenti;
- API;
- test.

## Production code

Non deve richiedere:

- binari McDSP;
- file proprietari;
- reverse engineering del formato interno.

---

# 57. Licenze e distribuzione

## Airwindows

MIT.

Se si incorpora codice:

- mantenere notice/licenza.

## foleys_gui_magic

BSD 3-Clause.

## Steinberg VST3 SDK

Dalla versione 3.8: MIT per il core SDK, rispettare anche eventuali regole di trademark se si usa il marchio/logo VST.

## JUCE

Verificare la licenza JUCE appropriata al modello commerciale del prodotto prima della distribuzione.

Questa sezione non è consulenza legale.

---

# 58. Non copiare la GUI McDSP pixel-perfect

Dal punto di vista tecnico si possono replicare:

- controlli;
- workflow;
- grafici;
- funzioni.

Per un prodotto indipendente usare:

- nome originale;
- logo originale;
- palette;
- knob design;
- font;
- texture;
- icone.

Il POC GUI deve quindi essere **functional equivalent**, non una copia grafica del prodotto McDSP.

---

# 59. Roadmap di implementazione

## Fase 1 — Skeleton

Implementare:

- CMake;
- JUCE;
- foleys;
- VST3;
- APVTS;
- DSP core vuoto;
- pass-through.

Done quando:

- VST3 Validator passa;
- Studio One/Reaper caricano;
- stato viene salvato.

---

## Fase 2 — Crossover

Implementare:

- Biquad;
- LP2/HP2;
- LR4;
- 2 band network;
- generalized network.

Done quando:

- impulse 1 kHz/48k coincide con reference;
- somma piatta;
- no denormal.

---

## Fase 3 — Band infrastructure

Implementare:

- 4 buffer interni;
- Solo;
- In;
- Gain;
- sum;
- meter.

---

## Fase 4 — Detector

Implementare:

```text
max(abs(L), abs(R))
```

Done con test stereo/antiphase.

---

## Fase 5 — Hard Knee Compressor

Implementare:

- threshold;
- ratio;
- gain computer K=0;
- attack iniziale;
- release iniziale.

---

## Fase 6 — Type-1

Implementare:

- Pure Peak rules;
- measured release model;
- attack fitting.

Done quando T09/T10 Type-1 passa.

---

## Fase 7 — Knee Model

Importare dataset:

- -10;
- -5;
- 0;
- +5;
- +10;
- +12.5;
- +15.

Implementare LUT/Hermite.

Done quando static curve error rispetta target.

---

## Fase 8 — Type-2

Implementare modello measured V1:

```text
base release ~0.77R
adaptive term k~0.42
```

Poi confrontare Pressure/Logical candidates.

---

## Fase 9 — BITE

Implementare:

```text
fast envelope
slow envelope
transient difference
GR relief
```

Fittare:

- onset;
- decay;
- rise-time sensitivity;
- max relief.

---

## Fase 10 — Auto

Generare secondo test pack.

Implementare prima:

- crest-factor auto model.

Poi sostituire/fittare con black-box data.

---

## Fase 11 — GUI completa

Implementare:

- master section;
- crossover display;
- compression plot;
- 4 band strips;
- meters;
- link;
- resize;
- 2/3/4 mode.

---

## Fase 12 — Production quality

- parameter smoothing;
- automation;
- stress test;
- denormal handling;
- sanitizer;
- CPU benchmark;
- preset migration;
- pluginval;
- VST3 Validator;
- multiple DAW test.

---

# 60. Ordine delle classi da scrivere

```text
01 Db.h
02 Biquad.h
03 LinkwitzRiley4.h
04 PeakDetector.h
05 GainComputer.h
06 Type1Ballistics.h
07 CompressorBand.h
08 CrossoverNetwork.h
09 MultiBandCompressor.h
10 Type2Ballistics.h
11 KneeModel.h
12 BiteProcessor.h
13 AutoBallistics.h
14 Metering
15 JUCE wrapper
16 GUI
```

---

# 61. API DSP proposta

```cpp
struct BandParameters
{
    bool enabled = true;
    bool solo = false;

    double gainDb = 0.0;
    double thresholdDb = -24.0;
    double ratio = 4.0;
    double knee = 0.0;
    double bite = 1.0;
    double attackMs = 10.0;
    double releaseMs = 250.0;

    TCMode tcMode = TCMode::type1;
};

struct GlobalParameters
{
    double inputGainDb = 0.0;
    double outputGainDb = 0.0;

    bool phaseInvert = false;

    int numBands = 4;

    std::array<double, 3> crossoverHz {
        100.0,
        1000.0,
        10000.0
    };

    std::array<BandParameters, 4> bands;
};
```

---

# 62. Process loop conceptual

```cpp
process()
{
    applyInputGain();

    splitIntoBands();

    for each active band
    {
        detector = maxAbsStereo();

        targetStaticGR =
            gainComputer(detector,
                         threshold,
                         ratio,
                         knee);

        switch (tcMode)
        {
            case Type1:
                gr = type1.process(targetStaticGR, detector);
                break;

            case Type2:
                gr = type2.process(targetStaticGR, detector);
                break;

            case Auto:
                gr = autoTC.process(targetStaticGR, detector);
                break;
        }

        gr = bite.modify(gr, detector);

        applyGainReduction(gr);

        applyBandGain();
    }

    soloLogic();

    sumBands();

    applyOutputGain();

    applyPhase();

    updateMeters();
}
```

La posizione di BITE nel pseudocodice è ancora provvisoria.

---

# 63. Compression display API

Non duplicare matematica.

La GUI deve chiedere al DSP:

```cpp
double getStaticOutputDb (int band,
                          double inputDb) const;
```

Per disegnare 128-512 punti.

Questo garantisce che il grafico rappresenti realmente il gain computer attivo.

---

# 64. Crossover display API

Esportare dal core/utility:

```cpp
double getBandMagnitudeDb (int band,
                           double frequencyHz) const;
```

oppure calcolare analiticamente la magnitude dei biquad.

Non processare sweep audio in GUI.

---

# 65. Meter update rate

Audio thread:

```text
per block:
    peak detector meter accumulations
```

Message thread:

```text
30-60 Hz
```

Ballistics GUI separate dalle ballistics audio.

---

# 66. CPU target

MC404 stereo:

- 4 crossover bands;
- 4 stereo compressors;
- double precision control;
- zero latency.

Obiettivo moderno realistico:

```text
< 1% di un singolo core moderno a 48 kHz / buffer 128
```

da misurare su hardware target.

Non ottimizzare prematuramente prima della fedeltà.

---

# 67. SIMD

Non necessario per il primo POC.

Possibile in seguito per:

- filter bank;
- multiple channels;
- analyzer.

Detector e ballistics hanno dipendenze temporali e non sono il miglior candidato iniziale per SIMD.

---

# 68. Oversampling

Non necessario nella V1 se il comportamento misurato non mostra nonlinearità.

Aggiungere oversampling solo se:

- futuro saturation stage;
- nonlinear BITE;
- distortion misurata.

Zero-latency e phase behavior originale hanno priorità.

---

# 69. Dati ancora mancanti

## Critici

1. Attack exact law.
2. Type-2 exact function.
3. Auto.
4. BITE intermediate values.
5. BITE frequency test isolato.
6. bande interne MC303/MC404.
7. comportamento dei crossover durante overlap.
8. linking exact parameter scaling.
9. behavior IN bypass della banda.
10. sample-rate dependence.

## Secondari

- clip LED exact threshold;
- meter ballistics;
- GUI knob taper esatto;
- preset factory values.

---

# 70. Secondo test pack consigliato

## BITE

Per ogni frequenza, file separato:

```text
100 Hz
500 Hz
1 kHz
5 kHz
10 kHz
```

Ogni file:

```text
2 s silence
100 ms burst
3 s silence
```

BITE:

```text
1
5
10
20
30
40
50
```

---

## Type-2

Second event relativo a envelope:

```text
D/E =
0.1
0.25
0.5
0.75
0.9
```

con diversi release.

---

## Auto

Segnali normalizzati per peak/RMS come descritto sopra.

---

## Crossover internal bands

MC404:

- solo Band1;
- solo Band2;
- solo Band3;
- solo Band4;
- tutte attive.

Usare:

```text
X1 = 100
X2 = 1000
X3 = 10000
```

e impulso.

Questo permetterà di identificare esattamente il phase compensation network.

---

# 71. GUI styling specification

## Tema

Proposta proprietaria:

```text
background:
    near-black / charcoal

panel:
    dark neutral

band accent:
    B1 warm yellow
    B2 green
    B3 amber
    B4 violet

text:
    off-white

meter:
    band color + warning
```

Non usare il verde McDSP come identità predominante.

---

# 72. Knob behavior

Tutti i knob:

- drag verticale;
- double-click default;
- Shift/Ctrl fine;
- valore numerico sotto;
- direct text entry;
- wheel opzionale;
- automation gesture corretta.

Attack/Release:

- display `ms` sotto 1000 ms;
- display `s` sopra 1000 ms.

Ratio:

```text
1.00 : 1
...
10.0 : 1
```

---

# 73. Accessibility / usability

- valore leggibile senza hover;
- contrasto alto;
- resize;
- tooltip opzionale;
- keyboard focus;
- no dipendenza esclusiva dal colore;
- Solo/IN con icona e testo.

---

# 74. Preset architecture

Categorie consigliate:

```text
Factory/
├── Broadband Styles
├── Drums
├── Bass
├── Vocals
├── Mix Bus
├── Mastering
└── Experimental
```

Evitare nomi che implichino endorsement di brand terzi.

È possibile creare preset tecnici:

```text
Smooth Undershoot
Transient Forward
Adaptive Pump
Opto Tail
Brick Curve
```

---

# 75. Factory default

Configurazione iniziale neutra consigliata:

```text
Input = 0
Output = 0

Band Gain = 0
Threshold = 0
Ratio = 1:1
Knee = 0
BITE = 1
Type-1

crossover:
100
1000
10000
```

In questo stato l'unica colorazione deve provenire dalla fase del crossover network, come nell'architettura multibanda, non da compressione.

---

# 76. Versioning

DSP model versions:

```text
DSP_MODEL_1 = initial POC
DSP_MODEL_2 = fitted Type2/BITE
DSP_MODEL_3 = Auto verified
```

Salvare il model version nel preset per garantire backward compatibility.

---

# 77. Logging di ricerca

Per ogni modifica DSP mantenere:

```text
date
test name
reference file
model commit
metric before
metric after
notes
```

Esempio:

```text
2026-08-xx
T10 Type2 second event -15
model: adaptive-v3
MAE before: 0.42 dB
MAE after: 0.11 dB
change: k 0.37 -> 0.418
```

---

# 78. Definition of Done del POC

Il POC è considerato completato quando:

- [ ] VST3 carica nelle DAW target.
- [ ] 2/3/4 band mode funzionano.
- [ ] LR4 coincide con reference.
- [ ] stereo detector coincide.
- [ ] hard knee coincide.
- [ ] Knee -10...15 coincide entro tolleranza.
- [ ] Type-1 coincide.
- [ ] Type-2 coincide.
- [ ] BITE coincide su test sintetici.
- [ ] Auto è stato caratterizzato e implementato.
- [ ] band linking funziona.
- [ ] Solo/In funzionano.
- [ ] meter completi.
- [ ] GUI crossover interattiva.
- [ ] GUI compression curve.
- [ ] state/preset.
- [ ] pluginval.
- [ ] Steinberg Validator.
- [ ] no allocazioni realtime.
- [ ] test regression automatici.
- [ ] documentazione licenze third-party.

---

# 79. Priorità immediata di sviluppo

Con le informazioni già ottenute, si può iniziare immediatamente da:

1. `Biquad`;
2. `LinkwitzRiley4`;
3. `CrossoverNetwork`;
4. stereo `PeakDetector`;
5. `GainComputer` hard knee;
6. `Type1Ballistics`;
7. `KneeModel` LUT;
8. band processing;
9. JUCE wrapper;
10. GUI base.

Non serve aspettare Auto/BITE per costruire l'80% dell'infrastruttura.

---

# 80. Stato dei blocchi

| Blocco | Stato |
|---|---|
| 2/3/4 band product concept | DEFINITO |
| Crossover slope | RISOLTO |
| LR4 2-way behavior | RISOLTO |
| crossover saturation | NON RILEVATA / DISABILITATA V1 |
| stereo detector | RISOLTO |
| hard knee | QUASI RISOLTO |
| Knee negative | MISURATO, DA FITTARE |
| Knee positive | MISURATO, DA FITTARE |
| tail | MISURATA, DA FITTARE |
| Attack | DATI PRESENTI, FIT PENDING |
| Type-1 release | MODELLO EMPIRICO DISPONIBILE |
| Type-2 | MODELLO EMPIRICO V1 DISPONIBILE |
| Auto | DA MISURARE |
| BITE | TOPOLOGIA PROBABILE, FIT PENDING |
| band linking | DOCUMENTATO |
| master controls | DOCUMENTATI |
| GUI | SPECIFICATA |
| VST3 architecture | SPECIFICATA |

---

# 81. Nota finale di progettazione

Il punto più importante del progetto è mantenere distinti tre livelli:

```text
1. Verhalten / comportamento osservato
2. modello matematico
3. implementazione C++
```

Il clone comportamentale non richiede di conoscere il codice originale.

Se una funzione indipendente produce:

```text
stessa curva statica
stessa gain trajectory
stesso crossover
stesso phase response
stesso transient behavior
```

allora, dal punto di vista del prodotto audio, il comportamento è stato ricostruito.

La strategia consigliata per tutto ciò che resta ignoto è:

```text
documentazione
      +
stimulus black-box
      +
Airwindows/paper come famiglie candidate
      ↓
parameter fitting
      ↓
regression test
      ↓
C++ proprietario
```

Questo POC deve rimanere un documento vivo: ogni nuova misura deve aggiornare la sezione relativa, insieme alla confidenza dell'algoritmo.

---

# Appendice A — tabella completa parametri target

| Scope | Parameter | Target POC | Note |
|---|---|---:|---|
| Global | Input Gain | -24..+24 dB | NextGen/v6 |
| Global | Output Gain | -24..+24 dB | storico, verificare target |
| Global | Phase | On/Off | storico |
| Global | Link Master | Unlinked / 1..N | documentato |
| Xover | X1 | 20..20000 Hz | ordine vincolato |
| Xover | X2 | 20..20000 Hz | MC303/404 |
| Xover | X3 | 20..20000 Hz | MC404 |
| Band | IN | On/Off | independent |
| Band | Solo | On/Off | multiple solo |
| Band | Gain | 0..+48 dB | NextGen/v6 |
| Band | Threshold | -45..0 dB | |
| Band | Ratio | 1..10 | |
| Band | Knee | -10..+15 | |
| Band | BITE | 1..50 | |
| Band | Attack | 0.03..250 ms | |
| Band | Release | 5..2500 ms | |
| Band | TC Type | R1/R2/Auto | |
| Meter | Input | approx -60..0 dB | GUI |
| Meter | Output | approx -60..0 dB | GUI |
| Meter | GR | 0..60 dB | GUI |

---

# Appendice B — parametri legacy da NON esporre nel target v6

La generazione precedente includeva:

```text
Release2:
5 ms ... 5 s
```

attivo per Type-2.

La documentazione NextGen indica che Release2 è stato rimosso.

Non inserirlo nella GUI principale target v6.

Può essere mantenuto solo come:

- nota di ricerca;
- possibile stato interno concettuale;
- modalità “legacy research” non commerciale.

---

# Appendice C — risultati numerici black-box principali

```text
CROSSOVER
fc = 1000 Hz
fs = 48000 Hz

LOW(fc)  ~ -6.020599 dB
HIGH(fc) ~ -6.020601 dB
sum magnitude ~ 0 dB

impulse comparison error ~ 9.1e-9
```

```text
SATURATION TEST
100 Hz / 700 Hz
-30 ... -1 dBFS

THD ~ numerical floor
~ -165 ... -170 dB
```

```text
STATIC CURVE
T = -24 dB
R = 4:1
K = 0

onset ~ -24 dBFS peak
```

```text
KNEE -5
max additional GR ~ 2.515 dB

KNEE -10
max additional GR ~ 4.357 dB
additional GR at -36 dBFS ~ 1.382 dB
```

```text
KNEE +5
max output delta ~ +1.364 dB
near input -18 dBFS

KNEE +10
max output delta ~ +1.924 dB
near input -17 dBFS
```

```text
KNEE +12.5
overshoot max ~ +2.125 dB
delta at -3 dBFS ~ +2.062 dB

KNEE +15
delta at -3 dBFS ~ +4.069 dB
```

```text
TYPE-1 RELEASE

t50  ~ 1.336 R
t1/e ~ 1.799 R
t10% ~ 3.40 R

fit:
GR(t) ~ GR0 * exp(-(t/(1.779R))^1.286)
```

```text
TYPE-2

base equivalent release ~ 0.77 * Type1 R
candidate adaptation constant k ~ 0.42
```

```text
BITE

strong compression test:
transient relief up to ~3 dB

phase difference min/max BITE at 10 kHz onset:
< ~0.05 degrees

steady-state difference:
approximately zero

transient memory candidate:
~15-20 ms
```

---

# Appendice D — candidate algorithms matrix

| MC2000 feature | Preferred approach | Secondary research |
|---|---|---|
| LR4 crossover | proprietary exact implementation | JUCE reference only |
| Stereo detector | proprietary max abs | Pressure5/Pyewacket reference |
| Hard knee | proprietary | standard DRC paper |
| Knee negative | measured LUT/Hermite | DRC soft-knee theory |
| Knee positive | measured LUT/Hermite | analytic hump fit |
| Tail | measured LUT/Hermite | opto static curve models |
| Type-1 | measured release model | classic peak follower |
| Type-2 | measured adaptive model | Pressure5 / Logical4 |
| Auto | black-box + crest-factor model | spectral flux model |
| BITE | fast/slow envelope GR relief | Pressure5/Pyewacket |
| Saturation | none in V1 | Spiral/Purest if later measured |

---

# Appendice E — strumenti di validazione

Build/test:

```text
CMake
CTest
Catch2 or JUCE UnitTest
pluginval
Steinberg VST3 Validator
VST3 Plugin Test Host
```

DAW:

```text
Studio One
Reaper
Cubase/Nuendo
Ableton Live
FL Studio
```

Audio regression:

```text
Python/NumPy/SciPy analysis
WAV 32-bit float
48 kHz baseline
96 kHz validation
```

---

# Appendice F — regola di aggiornamento del documento

Ogni nuova misura deve aggiungere:

```text
TEST ID:
PLUGIN VERSION:
SAMPLE RATE:
SETTINGS:
INPUT:
OUTPUT FILE:
MEASURED RESULT:
MODEL RESULT:
ERROR:
CONCLUSION:
CONFIDENCE:
```

Non cancellare le ipotesi precedenti: marcarle come `rejected` quando un test le esclude. In questo modo rimane tracciabile il percorso di reverse engineering comportamentale.
