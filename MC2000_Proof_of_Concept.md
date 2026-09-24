# Proof of Concept — Multi-Band Compressor ispirato a McDSP MC2000

**Documento tecnico di ricostruzione comportamentale / clean-room POC**  
**Target principale:** comportamento MC2000 / MC404 di generazione NextGen-v6, con supporto architetturale anche a 2 e 3 bande  
**Stack prevista:** C++ proprietario + JUCE + foleys_gui_magic + CMake  
**Formato plug-in principale:** VST3 tramite JUCE  
**Stato del documento:** POC tecnico basato su documentazione, ricerca algoritmica e misure black-box eseguite in DAW  
**Data:** 2026-08-28

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

**[IMPLEMENTED — v0.2.0]** Il DSP conserva gli offset relativi anche con
l'editor chiuso. Quando la GUI è aperta, il controller di link replica i valori
effettivi nei parametri slave, così knob, valori numerici, automazione host e
selettori TC rimangono visivamente sincronizzati con il master.

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

Per il prodotto Ponte MBC4 v0.2:

```text
Band Gain: -24.0 ... +24.0 dB
default: 0 dB
```

Step GUI: `0.1 dB`.

---

## 11.2 Threshold

```text
-48.0 ... 0.0 dB
```

Step GUI: `0.1 dB`.

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

Range pubblico Ponte MBC4 v0.2:

```text
1.00 ... 10.00
```

In `DSP_MODEL_4` il range pubblico viene normalizzato:

```cpp
biteNorm = (bite - 1.0) / 9.0;
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
0.25 ms ... 250.00 ms
```

Distribuzione GUI presumibilmente fortemente non lineare/logaritmica.

Parameter ID:

```text
bandN.attackMs
```

---

## 11.7 Release

```text
25.0 ms ... 2500.0 ms
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

Questo è il range documentato/storico usato nei render di ricerca. Il controllo
pubblico `DSP_MODEL_4` è limitato a `0.25 ... 250.00 ms`; i valori di ricerca
inferiori vengono clampati a 0.25 ms nel percorso di produzione.

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

**Aggiornamento 2026-09-23:** il fit seguente e storico (DSP_MODEL_2--6).
Le sonde isolate a 250/500 ms del 20 settembre supportano il rilascio del
controllo lineare, implementato in DSP_MODEL_7: vedere appendice N.
I tempi frazionari della GR dipendono ora dal livello iniziale e dal ratio;
1.336 R non e una costante universale del nuovo modello.

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

Le proposte seguenti documentano il vecchio modello; l'implementazione R1
corrente dal 23 settembre e descritta nell'appendice N. R2 resta invariato.

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

**[MEASURED]/[LIMITED]** il test pack aggiuntivo contiene render Auto con
Attack/Release manuali al minimo e al massimo: i due render sono
sample-identical, confermando che i controlli manuali vengono ignorati.

I parametri completi di ciascun test sono definiti dal relativo
`TEST_SPEC.md`; il suffisso del filename identifica soltanto la variante
manuale min/max. La prossima analisi deve quindi usare setup ed intervalli
temporali della specifica, non inferire il setup dal nome del WAV.

Il DSP mantiene una fallback **[CANDIDATE]** peak/RMS dipendente dal programma
finché il fitting segment-aware dei render Auto esistenti non è completo.

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

## 25.1 Validazione pack successivo

**[MEASURED]** Nel pack `MC2000_Next_DSP_Validation_Pack` i 39 confronti
min/max (13 sorgenti per MC202, MC303 e MC404) fanno null a precisione
numerica. Questo consolida una proprietà del POC: `Auto` ignora Attack e
Release manuali in tutti i modelli e nei segnali step, continuo, pulse,
crest-factor e noise del pack.

La stessa matrice non identifica ancora una formula unica per le traiettorie
Auto. Con BITE=5 e `All`, Auto è più attenuato di Type-1 all'inizio dei pulse
(circa -2.20 dB a 5 ms, -0.50 dB a 95 ms); il fallback crest-factor rimane
quindi **[CANDIDATE]**, non un fit dichiarato. Vedi
`Research/NEXT_VALIDATION_AUDIT.md` per misure e limiti del dataset.

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

## 26.1 Sidechain esterno

**[IMPLEMENTED]/[LIMITED]** il VST3 espone un bus Sidechain opzionale mono o
stereo. Quando è attivo:

```text
Key input
    ↓
stesso crossover LR4 del programma
    ↓
max(abs(L), abs(R)) per ciascuna banda
    ↓
detector della banda (in sostituzione del detector del programma)
```

Il key non entra mai nel percorso audio udibile e non viene sommato al
programma. Type-1, Type-2 e Auto ricevono il detector esterno senza cambiare
la topologia di gain reduction.

Il pacchetto finale fornisce i segnali Program/Key ma non render di riferimento
corrispondenti: il comportamento è testato funzionalmente, mentre la
corrispondenza esatta con l'originale resta **[TODO]**.

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

## 30.1 Pack successivo: matrice BITE

**[MEASURED]** Con tutte le bande attive, BITE 5 rispetto a BITE 1 lascia
passare in media `+0.127 dB` a 5 ms e `+0.008 dB` a 95 ms; BITE 10 produce
`+0.538 dB` e `+0.031 dB` negli stessi punti. La risposta è breve, ordinata e
compatibile con il relief fast/slow già implementato.

Il retest successivo Low/Mid chiarisce l'anomalia dei vecchi render BITE 5:
misura circa `+0.20 dB` a BITE 5 e `+0.82 dB` a BITE 10 a 5 ms. Il POC usa ora
una scala BITE non lineare con ancore 1/5/10/50, mantenendo il tetto di relief
da 3.2 dB e il detector fast/slow. Dettagli e numeri completi:
`Research/NEXT_VALIDATION_AUDIT.md`.

`DSP_MODEL_4` conserva l'andamento dolce calibrato fino a BITE 5, ma rimappa il
massimo pubblico BITE 10 sul tetto completo di relief da 3.2 dB. Le ancore fino
a 50 restano documentazione dei render storici e non sono più esposte dalla
GUI v0.2.

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

**[IMPLEMENTED — v0.2.0]** La GUI proprietaria corrente usa una palette
near-black/viola con quattro accenti di banda, layout adattivo 2/3/4 bande,
controlli numerici editabili e refresh dei dati live a 30 Hz. I controlli
inattivi vengono attenuati senza ridurre l'opacità dei pulsanti `IN` e `SOLO`,
che devono restare sempre leggibili e azionabili.

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

Per la misura della velocità dei meter e il confronto con la recensione
dell'esperto, usare [GUI_METER_TEST_PACK](Research/GUI_METER_TEST_PACK/README.md).
Stimoli pronti dal 2026-09-14; risposta visiva originale ancora da acquisire.

Il grafico deve mostrare:

- asse X logaritmico 20 Hz – 20 kHz;
- asse Y da -48 dB in basso a 0 dB in alto, comune a curve e spettro;
- curve delle bande;
- analizzatore di spettro in background come linea grigia;
- marker X1/X2/X3;
- drag orizzontale;
- text entry;
- fine control con modifier;
- ordine dei crossover sempre valido.

**[IMPLEMENTED — v0.2.0]** Lo spettro usa una FIFO SPSC lock-free alimentata
dal thread audio e una FFT Hann da 2048 campioni eseguita dalla GUI. Quando è
presente almeno un `SOLO`, vengono mostrate solo le regioni di spettro delle
bande in solo; altrimenti vengono mostrate le regioni con `IN` attivo. Le curve
delle bande inattive restano visibili ma attenuate.

**[IMPLEMENTED — UI 2026-09-14, allineamento e scala]** La caption CROSSOVER
parte dal bordo sinistro del visualizzatore delle bande. L'help sul wordmark
PonteDSP occupa 168 × 54 unità (prima 110 × 54), fino a quattro righe a 11
unità bold senza restringimento orizzontale. La scala verticale usa tacche
-48/-36/-24/-12/0 dB, con clamp delle curve e dello spettro ai bordi; i marker
crossover si trovano sulla linea superiore a 0 dB. Nessun cambiamento DSP.

Verifica di questa iterazione: CTest 2/2 superato in 4.81 s (DSP 1.64 s,
UI/stato 3.14 s), inclusi allineamento CROSSOVER, capienza di tutti i testi
contestuali al minimo e IN spenti durante SOLO. Rendering JUCE a 1100 × 738
controllato; riferimento build e commit nel catalogo CompanyGUI.

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
X: -48 ... 0 dB
Y: -48 ... 0 dB
```

Mostrare:

- linea unity;
- curve per banda;
- colore banda;
- threshold marker opzionale.

**[IMPLEMENTED — v0.2.0]** Ogni curva attiva mostra un marker live, nello
stesso colore della banda ma leggermente più scuro, posizionato dal livello
d'ingresso corrente. Il marker viene nascosto quando la banda è inattiva, così
non rimane congelato sull'ultimo valore del meter.

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

**Historical contract, superseded by Appendix J (0.2.2 / DSP_MODEL_5): IN off now mutes the band input and is independent of SOLO.**

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

Nella GUI corrente `SOLO` applica un override temporaneo senza riscrivere `IN`:

```text
Uno o più SOLO attivi -> tutti gli IN spenti e non modificabili, anche sulle bande in SOLO
Ultimo SOLO spento    -> IN torna alla configurazione salvata prima del SOLO
Esempio: IN 2/3 -> SOLO 4 -> tutti gli IN spenti -> fine SOLO -> IN 2/3
```

I due pulsanti restano sempre a piena opacità, anche quando il resto della
strip è attenuato. Nel layer DSP una banda in `SOLO` è sempre considerata
abilitata. I parametri host IN conservano il valore di base, anche salvando
il progetto durante SOLO. L'automazione di IN aggiorna questa base; l'override
visivo resta fino all'ultimo SOLO. Le bande nascoste non attivano il SOLO.
IN disattivato continua a bypassare la compressione, non muta la banda.

### SOLO

Una o più bande possono essere solo contemporaneamente.

Il solo deve agire dopo crossover e processing della banda.

**[IMPLEMENTED]** le transizioni Solo usano un crossfade di 5 ms per banda;
non vengono effettuati mute istantanei durante automazione o cambio di stato.

Più bande possono restare in `SOLO` contemporaneamente; in quel caso tutte le
bande non in solo vengono attenuate nella GUI e rimosse dalla visualizzazione
live dello spettro e dei marker Static I/O.

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
    0.25f,
    250.0f
};

attackRange.setSkewForCentre (10.0f);
```

Il centre va fittato alla sensazione/mapping originale.

Range e precisioni pubbliche correnti:

| Parametro | Range | Precisione GUI |
|---|---:|---:|
| Input / Output / Gain | -24.0..+24.0 dB | 0.1 dB |
| Threshold | -48.0..0.0 dB | 0.1 dB |
| Ratio | 1.00:1..10.00:1 | 0.01 |
| Knee | -10.00..+15.00 | 0.01 |
| BITE | 1.00..10.00 | 0.01 |
| Attack | 0.25..250.00 ms | 0.01 ms |
| Release | 25.0..2500.0 ms | 0.1 ms |

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

**[IMPLEMENTED — v0.2.0]** La posizione di `MODE` è indipendente dal numero di
crossover visibili: i tre slot di crossover rimangono riservati nel layout.
La dimensione minima è `1100 x 590` anche in mode 4-band, equivalente
all'altezza normale del mode 2-band; knob, meter e intestazione algoritmo si
comprimono senza sovrapporsi. Cambiando il numero di bande, l'editor conserva
un'altezza per-strip coerente entro i limiti della finestra.

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

Implementazione POC attuale:

```text
one-pole frequency smoothing: 20 ms, per sample
refresh coefficienti LR4: ogni 16 sample durante il movimento
```

Questo evita salti dipendenti dalla dimensione del buffer senza ricalcolare i
coefficienti ad ogni sample. Il comportamento originale con automation rapida
rimane da confrontare con render controllati.

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

Il secondo test pack è disponibile. Implementare e mantenere:

- crest-factor auto model.

Fittarlo con i black-box data disponibili usando setup e timing del
`TEST_SPEC.md`: i render min/max validano già l'indipendenza dai controlli
manuali.

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

1. Traiettoria Auto controllata, inclusa GR a regime.
2. Semantica esatta del sidechain esterno dell'originale.
3. Comportamento dell'automazione originale (crossover, Solo e IN) a buffer
   differenti.
4. Linking exact parameter scaling.
5. Behavior IN bypass della banda.
6. Sample-rate dependence dell'originale.

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

**[IMPLEMENTED — v0.2.0]** I selettori e i pulsanti non attivi (`PHASE`,
`SOLO`, `MODE`, `LINK`, `R1/R2/AUTO`) hanno fondo nero. Le strip usano
separatori viola chiaro fra Gain/Threshold, Ratio/Knee e BITE/Attack. Ogni
banda espone tre barre sottili `IN/OUT/GR`, ciascuna con scala dB viola e numeri
lime; il meter master usa lo stesso linguaggio visivo.

**[SUPERSEDED — UI 2026-09-14]** Il profilo minimale corrente usa superfici
neutre, controlli piatti e label/scale nel bianco `#F4F2F7` di R1. I meter
hanno cifre bold da 10 unità e label laterali; `MAIN OUTPUT` è a sinistra
della coppia stereo. Static I/O disegna per ultima la banda lavorata tramite
click, rotella o focus. Questa priorità non cambia IN/SOLO o il DSP.

---

# 72. Knob behavior

Tutti i knob:

- drag verticale;
- double-click default;
- Shift/Ctrl fine;
- nome del controllo sotto, sempre visibile;
- valore numerico in un overlay sopra il knob su interazione o hover;
- direct text entry;
- wheel opzionale;
- automation gesture corretta.

**[IMPLEMENTED — UI 2026-09-14]** La precedente casella sotto il knob,
anche quando trasparente, lasciava spazio vuoto ed è sostituita da un
riquadro editabile sovrapposto alla UI. Il gruppo contiene soltanto il
pomello quadrato e la caption da 15 unità, centrati nello spazio disponibile.
Il valore non partecipa al layout: overlay 100 × 24 unità, 3 unità sopra
il controllo, centrato e contenuto entro i bordi dell'editor. Non è una
finestra desktop e non viene tagliato dai limiti della strip.

Comparsa immediata con click, drag, rotella o nuovo focus da tastiera; primo
hover dopo 30 ms. Con un controllo già attivo, passaggio immediato al nuovo
elemento sotto il mouse. Un unico controller gestisce knob, selettori, pulsanti
e campi numerici: non possono restare più controlli accesi per focus tastiera
obsoleto. Rilascio dopo 100 ms fuori dai controlli, mantenimento durante drag,
editing o menu aperto. Il mouse fermo sopra un controllo mantiene il focus.
Timer GUI da 10 ms: le scadenze sono soggette alla disponibilità del message
thread, non sono tempi audio. Passare a un altro controllo chiude e conferma
l'eventuale editing dell'overlay precedente; Esc conserva l'annullamento JUCE.
Digitazione tramite lo stesso parser/formattatore dello slider, clamp e
quantizzazione del parametro esistente, gesture host begin/end alla conferma.
Le variazioni da automazione aggiornano il display senza interrompere il testo
in corso di modifica. Il reset con doppio click rimane sul knob.

Arco e indicatore a riposo conservano esattamente l'accento originale. Il focus
usa `brighter(0.08)`; corpo miscelato al 4% con l'accento e bordo al 25%.
Eliminata la precedente desaturazione ×0.35 / `darker(0.3)`.
Proprietà GUI condivise: `pontedspKnobActive` e `pontedspControlActive`,
non parametri audio. `ControlFocus.h` e `FocusTiming.h` sono replicati in
CompanyGUI; la copia locale mantiene compilabile il prodotto isolato.
CompanyGUI supporta questa proprietà opt-in nel LookAndFeel. Non modifica
gli altri prodotti finché non adottano il comportamento contestuale.

I tempi sono scelte di interazione PonteDSP, non misure dei plugin usati
come riferimento grafico. La proposta precedente «valore sotto» è superata;
non restano fasce di layout vuote riservate al numero.

Verifica storica prima della correzione focus/SOLO/resize: build VST3 x64 Release, CTest `MC2000Tests`
1/1 superato (4.27 s), diff-check superato. Binario 7370752 byte, SHA256
`4EE93ED775D4B5B2B62073E8E210884E3A71283187926259CA6A8A31C587588A`.
La verifica della revisione successiva è registrata nel catalogo CompanyGUI.
`MC2000UITests` aggiunge test del controller focus, dei pulsanti reali SOLO/IN,
automazione, riapertura, salvataggio e allineamento MAIN OUTPUT / STATIC I/O.

**[IMPLEMENTED — correzione UI 2026-09-14]** MAIN OUTPUT mantiene la caption
laterale, con l'intero componente largo e allineato come STATIC I/O. Header
ridistribuito per evitare sovrapposizioni con LINK alla larghezza minima.
Il minimo è fisso a 1100 × 738 per tutti i modi; una nuova istanza apre al
minimo per quattro bande. Cambiare 2/3/4 bande ridistribuisce le strip senza
cambiare la finestra. Le dimensioni scelte persistono nel processor e nello
stato salvato (`editorWidth`, `editorHeight`), con clamp 1100–1600 / 738–1100.
Stati precedenti senza queste proprietà usano il minimo iniziale.

Verifica della correzione 2026-09-14: VST3 x64 Release completato e CTest
**2/2 superato** (7.47 s: DSP 2.23 s, UI/stato 5.20 s). Test di editing con
conferma/annullamento, focus unico e alias, sequenze SOLO/IN, automazione,
bande nascoste, resize e riapertura, salvataggio durante SOLO. Confronto audio
con le bande del crossover a rapporto 1:1, dopo oltre 100 ms di assestamento
della costante esponenziale SOLO da 5 ms; errore massimo ammesso 2e-6.
Rendering JUCE a 1100 × 738 controllato; interazione manuale nella DAW da
provare nel proprio host. Binario 7388160 byte, SHA256
`9068957155717157DA517EC70CEB0E621DC22F53F3A19DFFC9AC7C6193227A0E`.

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

- valore accessibile con focus da tastiera o selezione, oltre all'hover;
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
DSP_MODEL_4 = public v0.2 ranges and BITE 1..10 mapping
DSP_MODEL_5 = independent IN input mute and SOLO output selection (public 0.2.2)
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
- [x] VST3 espone un sidechain esterno per-banda; matching dell'originale ancora da validare.
- [x] band linking funziona nel DSP e sincronizza i controlli GUI.
- [x] SOLO applica un override temporaneo; alla disattivazione ripristina gli IN salvati.
- [x] meter IN/OUT/GR e master completi di scale dB.
- [x] GUI crossover interattiva con assi e analizzatore FFT.
- [x] GUI compression curve con assi e marker live per banda attiva.
- [x] state/preset con schema versionato.
- [ ] pluginval.
- [ ] Steinberg Validator.
- [ ] no allocazioni realtime.
- [x] test regression automatici.
- [x] documentazione licenze third-party.

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
| Auto | MANUAL CONTROLS IGNORATI VERIFICATO; FALLBACK CREST-FACTOR, FIT TRAIETTORIA PENDENTE |
| BITE | FAST/SLOW CALIBRATO; CONTROLLO PUBBLICO 1..10 MAPPATO SUL RELIEF COMPLETO |
| Sidechain esterno | IMPLEMENTATO PER-BANDA; REFERENCE FIT PENDENTE |
| Automazione crossover/Solo | SMOOTHING IMPLEMENTATO; MATCHING ORIGINALE PENDENTE |
| band linking | IMPLEMENTATO DSP + SINCRONIZZAZIONE GUI |
| master controls | IMPLEMENTATI |
| GUI | IMPLEMENTATA, RESPONSIVE, FFT E METER LIVE |
| VST3 architecture | IMPLEMENTATA |

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
| Band | IN | On/Off | Conservato durante SOLO; tutti i pulsanti IN spenti e bloccati fino al termine |
| Band | Solo | On/Off | multiple solo |
| Band | Gain | -24.0..+24.0 dB | step 0.1 dB |
| Band | Threshold | -48.0..0.0 dB | step 0.1 dB |
| Band | Ratio | 1..10 | |
| Band | Knee | -10..+15 | |
| Band | BITE | 1..10 | step 0.01 |
| Band | Attack | 0.25..250 ms | log, step 0.01 ms |
| Band | Release | 25..2500 ms | log, step 0.1 ms |
| Band | TC Type | R1/R2/Auto | |
| Meter | Input | -48..0 dB | GUI con scala |
| Meter | Output | -48..0 dB | GUI con scala |
| Meter | GR | 0..48 dB | GUI con scala |

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
TYPE-1 RELEASE (historical DSP_MODEL_2--6; superseded by Appendix N)

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

# Appendice G — acquisizioni GUI 01–03, 2026-09-15

TEST ID: GUI-A1-A2-B0-B1-20260915.
PLUGIN VERSION: originale MC404 7.3.0.23 visibile; Ponte riferimento pack
`2b6e97e`, hash della build effettivamente caricata non certificato dal video.
SAMPLE RATE: WAV 48000 Hz, float32 stereo dual mono. Video 1920×1080/60 fps.
SETTINGS: protocollo R1, 2:1, knee 0, attack 2.5 ms, release 250 ms, BITE 1,
threshold -27.5, crossover 100/785/10000. Differenze confermate: MASTER 2 in
Ponte A1/A2, SOLO 2 spento nel video Ponte B0; il WAV B0 è compatibile con
SOLO ma la corrispondenza alla passata video non è dimostrata.
INPUT: sorgenti GUI_METER_TEST_PACK 01/02/03; eventi 02/03 trasformati nel
tempo a circa 0.780819/0.857656 del sorgente, sequenze complete, non troncate.
OUTPUT FILE: 8 WAV e 8 MP4 in `Research/GUI_METER_TEST_PACK/audio/`, inventario
con SHA256 e risultati nel [report](Research/GUI_METER_TEST_PACK/analysis_2026-09-15/REPORT.md).
MEASURED RESULT: discesa MAIN originale 10→90% dello step da 6 dB circa
467 ms (banda 2 483 ms), Ponte salto nello stesso frame; step da 12 dB
circa 683–750 ms originale e 0–33 ms Ponte. Originale cattura 30/30 picchi A2;
Ponte ne sottorappresenta quattro, di cui uno senza barra rilevabile.
GR audio B0/B1: plateau 10.319/10.527 dB originale/Ponte, T50 365–369/335–339 ms,
T10 895–905/849–855 ms. GR visiva plateau circa 9.9/10.5 dB;
discesa 90→10% circa 867–883/750–783 ms.
MODEL RESULT: candidata rampa di livello 14.3 dB/s + smoothing 130 ms,
stimata su A1/315 Hz e verificata su A1/2 kHz. Non modello recuperato univoco.
ERROR: RMSE held-out 0.16 dB; calibrazione MAIN massimo 0.13 dB sui plateau;
quantizzazione e trasferimento della scala GR aggiungono alcuni decimi.
CONCLUSION: confermata necessità di conservare i picchi fra letture GUI e
introdurre ballistics di livello. GR da trattare separatamente; nessuna
correzione DSP deducibile solo dalle barre. Ipotesi iniziale di semplici
export troncati `rejected`: tutti gli eventi sono presenti con tempi diversi.
CONFIDENCE: alta sulla differenza di discesa e sui picchi registrati;
condizionata dal setup per GR audio; non conclusiva su voce/R500/banda 3.
AAC dei video silenzioso, buffer e PRINT/offline ignoti: nessuna stima
della latenza audio/video assoluta. Nessuna nuova build o modifica DSP/UI.

# Appendice H — take 2, banda 3 e sonda neutra, 2026-09-15

TEST ID: GUI-FOLLOWUP-A2-B0-BAND3-NEUTRAL05-20260915.
PLUGIN VERSION: originale MC404 7.3.0.23; Ponte riferimento `2b6e97e`,
hash della build caricata non verificabile dalla cattura.
SAMPLE RATE: export float32 stereo 48000 Hz; buffer playback **512** dichiarato.
SETTINGS: A2 take 2 UNLINKED, B0 take 2 SOLO 2; 04 B0/B1 SOLO 3 R250;
05 **neutro**, IN 2/3 e nessun SOLO, confermato dall'utente. OBS e export
separati, non PRINT della stessa passata; vecchi WAV mantenuti nei take 2.
INPUT: sorgenti 02/03/04/05 del GUI_METER_TEST_PACK. Eventi 04/05 completi
ma con fattori onset 0.857710/0.84607 rispetto ai sorgenti; causa ignota.
OUTPUT FILE: 8 nuovi MP4 e 6 WAV; inventario/hash, CSV e grafici nel
[report del secondo giro](Research/GUI_METER_TEST_PACK/analysis_2026-09-15_take2_04_05/REPORT.md).
MEASURED RESULT: A2 take 2 quattro picchi sottorappresentati, due senza
barra di banda rilevabile. B0 take 2 GR zero e IN/OUT entro un pixel.
04 GR audio originale/Ponte 10.365/10.607 dB, T50 364–369/334–339 ms,
T10 899–905/849–855 ms; GR visiva circa 9.9/10.5 dB e discesa 90→10%
867–900/750–783 ms. 05 neutro: GR zero; delta RMS Ponte/originale circa
+0.12/+0.19/+0.18 dB a 90/120/180 Hz.
MODEL RESULT: confermata la priorità di accumulare massimi tra letture GUI;
il candidato per il meter di livello del primo giro non è stato implementato.
ERROR: audio circa 10 ms di risoluzione, video almeno 17–33 ms e alcuni
decimi di dB; allineamento A2 sui sei burst lunghi disperso entro 30.4 ms.
CONCLUSION: il difetto visivo persiste in UNLINKED; la piccola differenza
audio R250 si ripete sulla banda 3. 05 neutro non testa la compressione sulla
fondamentale. Mancano 05 compresso R250/R500, 03/04 R500 e voce dry reale.
CONFIDENCE: alta su difetto visivo e setup corretto; buona sulle differenze
audio nei test esportati; non conclusiva sul caso percettivo o sulla latenza A/V.
Verificati 35 hash invariati e 15 401 nuovi fotogrammi. Nessuna nuova build,
nessuna modifica al DSP/GUI. Guida completata con parametri e regole di riuso B0.

# Appendix I - Visual meter correction, 2026-09-15 (0.2.2)

STATUS: implementation supersedes the pending-meter status in appendices G/H.
SPEC: accumulate each band's IN/OUT/GR and both MAIN channel peaks between
GUI reads. Consume once per timer tick; paint only cached display values.
The raw per-block analysis API remains available. Lock-free atomic mailboxes,
no allocation or mutex in the audio producer; bounded strong-CAS retry with
one producer and one resetting consumer.
MODEL: immediate attack to the accumulated peak; IN/OUT/MAIN fall uses a
14.3 dB/s ramp followed by a 130 ms one-pole in dB. Integrate actual elapsed
time analytically, including arrival at target. GR uses a separate one-pole
return in GR dB, tau 150 ms. No fixed GR offset; constant plateaus preserved.
LIFECYCLE: consume hidden bands too; discard pending peaks when opening the
editor; drain towards silence without new callbacks; DSP reset clears raw
and pending readings. Repaint count never changes ballistics.
EVIDENCE: A1 level fit and held-out 2 kHz check, plus A2 take 2 peak losses.
GR engineering check uses export-derived 10 ms RMS envelopes: 03 normalized
return-interval RMSE improves from 83 to 13 ms, 04 from 89 to 18 ms. A grid
minimum at 160 ms is indistinguishable at capture resolution from the chosen
150 ms; this is not an identified McDSP internal constant.
RATIONALE: MC2000 is a digital plugin modeling multiple compressor behaviours,
not a verified replica of a physical MC2000 compressor. Analog peak-meter
capacitor discharge and mechanical pointer damping explain plausible meter
ballistics. A free RC discharge becomes a linear dB fall, 8.686/tau dB/s;
14.3 dB/s corresponds to approximately 0.61 s. This analogy does not prove a
specific circuit implementation; the added pole is in the display dB domain.
Readable short peaks and stable motion are plausible design purposes, not
statements of McDSP's unpublished intent. Audio release and display return
are separate. The display model alone cannot explain a steady GR offset.
VALIDATION: DSP and UI suites cover capture, concurrency, stereo, audio
invariance under GUI reads, clock-rate/jitter independence, repaint, stop,
reopen and existing focus/resize/SOLO regressions. Build/test outcomes are
recorded in the release verification note.
LIMITS: this meter-only stage did not change audio transfer or DSP_MODEL_4;
Appendix J subsequently changes IN routing to DSP_MODEL_5. R250 GR plateaus,
R500, compressed fundamental probe and real voice still require follow-up.
A new DAW capture must verify the complete meter after this implementation.

[Technical note, measurements and sources](Research/GUI_METER_TEST_PACK/meter_fix_2026-09-15/REPORT.md).

Verifica automatica 0.2.2: suite DSP/GUI **2/2 PASS**, modello numerico PASS,
35 file sorgenti invariati. Corretta la dipendenza della risorsa versione
Windows nelle build incrementali. Hash finali nel pacchetto di release.

# Appendix J - Unified visual response and independent controls (0.2.2)

This revision supersedes earlier IN/SOLO presentation rules and the raw
STATIC I/O marker policy. IN and SOLO are independent, including all four
IN and SOLO buttons simultaneously on. As explicitly confirmed by the user,
IN off mutes the band input, including its detector; SOLO only selects output
bands and never forces IN. IN-off/SOLO-on remains silent. Input transitions
use a 5 ms exponential time constant, clamped to exact silence below 1e-9.
Active SOLO has lime outline/text and ink fill. DSP_MODEL_5 identifies this
routing change; compression formulas and public version 0.2.2 are unchanged.
Stored IN/SOLO values are preserved, but sessions with IN off change sound.
This replaces the preliminary bypass interpretation, not a verified finding
about the original plugin. New neutral references are needed for changed routing.

All meter/graph dB scales end at -60 dB, GR at 60 dB reduction. Audio parameter
ranges are unchanged. Knob double click opens numeric editing without a
parameter reset; editing the overlay directly remains supported.
STATIC I/O dots reuse the same smoothed IN samples as band meters, projected
onto the static curve; muted bands are clamped to the graph floor. MAIN already
shares level ballistics. The FFT input spectrum uses stereo peak magnitudes,
continuous LR4 weighting of IN bands and the same timed level ballistics.
It retains filter skirts across crossover markers, rather than masking whole
frequency intervals. Its input tap remains before global INPUT gain.

FFT latency audit: 2048-sample Hann window/1024 hop (42.67/21.33 ms at 48 kHz),
plus block delivery and GUI scheduling. Remove the old 0.72/0.28 frame-based
smoothing, discard closed-editor/overflow backlogs, take recent bounded reads,
reset overlap on a discontinuity, and drain visual state without callbacks.
The response is not zero-delay and spectral short-burst amplitudes differ
from time-domain peak meters. The visual LR4 weighting is an approximation,
not per-band waveform capture. See the technical report for regression scope.

Commit and push are authorized; release publication awaits explicit user OK.

Verifica integrazione finale: suite DSP PASS e GUI PASS, compresi FIFO
recente, finestra FFT parziale a basso sample rate, punti/meter coerenti,
code crossover, stereo L=-R, doppio click e IN/SOLO indipendenti.

Verifica finale IN=mute: CTest 2/2 PASS (DSP 5.58 s, GUI 5.59 s; totale 11.21 s).
Copertura: intersezione IN/SOLO, tutti accesi/spenti, silenzio anche con SOLO,
transizioni di spegnimento/riaccensione e chiusura del detector esterno.

# Appendix K - CPU work and release header, 2026-09-18 (0.2.3 test build)

The Auto law is unchanged pending new original renders. This iteration caches
sample-rate coefficients for Auto/BITE, the manual attack coefficient and BITE
control mapping; band gain targets move outside the sample loop. The wrapper
resolves APVTS parameter atomics once instead of constructing IDs per block. DSP_MODEL_5
and parameter/state schemas are preserved. The spectrum producer skips FIFO
writes with no editor consumer; LR4 bin weights are cached until their inputs
change. This is not complete DSP sleep and does not change the FFT or ballistics.

The header shows the CMake version next to MBC4 and an optional newer stable
GitHub version. A shared cancellable background worker checks the public latest
release, at most hourly per loaded module, without credentials or audio data.
The notification alternates gray/yellow every second while visible; context
help replaces the whole header. No automatic installation. Oversampling text
is not shown until an oversampling implementation has passed its own gate.

[Implementation, benchmark method and results](Research/PERFORMANCE_2026-09-18.md).
The 0.2.2 baseline is published; 0.2.3 is for local evaluation, not a published
release. New original-render test pack and the remaining temporary checklist
are tracked with this work. Nap, Auto fitting and oversampling remain open.

# Appendix L - Measured Auto correction, 2026-09-19 (0.2.3 test build)

This appendix supersedes the Auto fallback described earlier. Of 47 original
renders delivered, seven automation exports are wholly silent, the noise B0
duplicates the multitonal B0, and BITE/manual/repeat conditions need checking.
The sidechain group was not acquired. A reproducible audit retains original
hashes and excludes those files from calibration without modifying them.

The valid MC404/48 kHz falling steps follow
`g(t) = 20 s log10(1 + A exp(-t/tau))`, `s = 1 - 1/ratio`,
with tau approximately 102 ms across four bands, levels, ratio 2/4 and threshold
changes at knee 0. Knee -5 also fits; positive knee has a remaining discrepancy.
This mathematical observation does not identify proprietary code or hardware.

Ballistics transforms instantaneous target GR into linear control normalised
by ratio, captures rising peaks with a 20 microsecond approximation, and lets
excess control over unity decay with tau 102 ms. Manual Attack/Release do not
control Auto. Ratio 1 is neutral; state remapping preserves current GR across
non-unity ratio changes and mode changes; prepare/reset clear memory. Conversion
is bounded near unity ratio. The public version stays 0.2.3; DSP_MODEL_6 records
the intentional Auto sound change. Parameter IDs and state schema are stable.
No recalibration of R1, R2, BITE, crossovers, routing or visual meter ballistics.

Full-engine comparison improves all 13 usable Auto conditions. Effective
attenuation MAE on the independent synthetic phrase changes from 0.514 to
0.099 dB; this includes audible low-level intervals and is not a waveform-null
or GUI-meter metric. Separate compression-active metrics and p95 are retained.
Do not reuse the older 54/54 bit-identity claim for the changed Auto model.

[Technical report, mathematical fit, acquisition audit and reproduction](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-19/REPORT.md).
Remaining validation: noise with correct B0, BITE/repeat clarification, audible
automation, other plugin models, native sample rates, external sidechain,
R1 expert probes at 500 ms, meter recordings and real voice. Local numerical
sample-rate and routing tests do not replace original-plugin acquisitions.

# Appendix M - Additional original measurements, 2026-09-20

Research only: production remains 0.2.3 / DSP_MODEL_6 at commit 5d8bd92.
The delivery contains 80 WAVs representing 75 planned IDs and four videos.
61 format-compatible audible conditions were compared with the frozen engine;
some are diagnostic comparisons with unresolved acquisition settings.

R1 isolated probes at ratio 2:1, knee 0 and BITE 1 fit
`g(t) = 10 log10(1 + A exp(-t/tau))`, with tau equal to the displayed
250/500 ms Release. Twelve release segments fit with RMSE below 0.0015 dB.
The current stretched exponential in dB is a different curve: at 500 ms
the original reaches half its initial GR around 733–735 ms, Ponte around
669 ms. This is an audio difference relevant to the expert's concern,
not proof of the full real-voice/meter explanation. Generalisation to other
ratios/knees and transition behaviour requires further verification.

Corrected BITE 5/10 recordings show substantially greater and shorter attack
relief than the current model. At BITE 5, the peak relief relative to BITE 1
is 3.32/3.80 dB on 315 Hz/2 kHz versus 0.19/0.17 dB in Ponte. Whole-render
MAE hides these short errors; 1 ms analysis is retained. Both amplitude and
timing need identification, with regression across R1/R2/Auto.

Auto BITE 1 release near 102 ms is confirmed by the MC202 isolated probe;
LOW probes at 44.1/48/88.2 kHz also support the current model. This does not
validate all modes/rates: twelve primary exports remain silent, MC303 isolated
and several ALL acquisitions show inconsistent routing/compression, and
96/192 kHz compressed cases and external sidechain remain incomplete.

Original 60 fps neutral videos support a 12 dB meter fall of 667–750 ms
(10–90%, median 717 ms), close to the current numerical profile's 714 ms.
For 6 dB, measured 467–600 ms exceeds its 441 ms. Pixel calibration and
frame uncertainty remain relevant. These are not new Ponte screen captures;
silent video audio prevents absolute latency measurement, and neutral runs
do not validate GR. No meter constants were changed from these observations.

[Full report, figures, reproducible scripts and acquisition limitations](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-20/REPORT.md).

# Appendix N - R1 linear-control release, 2026-09-23

The local 0.2.3 test build advances to **DSP_MODEL_7**. R1 replaces the
historical stretched exponential in dB with the release law supported by
the September 20 isolated 315 Hz / 2 kHz probes at 250 and 500 ms:

```text
s = 1 - 1/ratio
g(t) = 20*s*log10(1 + (10^(g0/(20*s)) - 1)*exp(-t/Release))
```

Release is in seconds. This is an excess-linear-control decay, not a
constant fractional decay of GR in dB. The measured original evidence is
ratio 2:1 / knee 0 / BITE 1. Ratio scaling elsewhere is an explicit model
extrapolation; numerical tests with other ratios and knees do not establish
original-plugin equivalence.

The implementation advances from current GR each sample. With
`scale = 20*s/ln(10)` and `delta = expm1(-1/(Fs*Release))`, it uses
`g += scale*log1p(-expm1(-g/scale)*delta)`. This avoids exponentiating
large positive values near unity ratio and avoids reinterpreting elapsed
release age when Release or ratio changes. The release coefficient is
cached until its inputs change; prepare invalidates the cache. Unity ratio
clears the release state; values below 1e-8 dB are flushed to zero.

Manual attack, the lower-event rule, R2, Auto, BITE, crossover and GUI
ballistics are unchanged. Mode transitions initialise from current GR.
Parameter IDs/state schema stay unchanged; saved state records model 7.
Existing R1 presets intentionally sound different; there is no legacy-model
selector or automatic recreation of the model-6 sound.

Regression coverage includes closed-form release, unchanged attack,
44.1/48/88.2/96/192 kHz, 25/250/500/2500 ms, ratio/knee combinations,
near-unity numerical stability, reset, parameter/mode transitions and
32/512/irregular host blocks. The separate renderer preserves model-6
binaries and outputs, verifies original/source hashes and checks unchanged
non-R1 cases sample for sample.

[Results and reproducible comparison](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-23-r1/REPORT.md).
Windows Release DSP/GUI tests pass 2/2. All eight available R1 comparisons
improve in MAE and p95; the other 53 cases are sample-identical to model 6.
All twelve measured release segments improve in GR trajectory RMSE.
Real voice, broader original ratio/knee acquisitions, BITE recalibration,
complete nap, oversampling decision, CPU benchmark and final DAW validation
remain open. This change is not a published release or a complete emulation claim.

# Appendix O - Corrected exports and Auto BITE, 2026-09-23

The twelve previously silent primary exports are now audible, stereo 48 kHz,
60 seconds long. The user confirms the automation values and times in the
pack plan. T002/T035/T036/T037 null exactly, as do T056/T057: Auto ignores
manual Attack/Release both statically and during the prescribed automation.
The new noise neutral is no longer a duplicate of the multitonal reference.
The suspect MC202/MC303 and ALL sample-rate acquisitions were not replaced;
their configuration limitations remain. Sidechain and T075–T081 are absent.

Production advances from model 7 (R1 already corrected in `4dadce7`) to
**DSP_MODEL_8**, retaining public version **0.2.3**. Only Auto with BITE > 1
changes its intended audio response. R1/R2 retain their existing BITE law.
Auto retains the model-6 excess-linear-control release with tau 102 ms.

For Auto BITE, a rising-only one-pole smoothing stage follows the normal GR:

```text
u = clamp((BITE - 1) / 9, 0, 1)
tau = 0.003 * u^1.875                  # seconds
a = exp(-1/(Fs*tau)), or 0 at BITE 1
g = a*g + (1-a)*normalGR              # when normalGR > g
g = normalGR                         # otherwise
```

Candidate identification on the original 315 Hz / 2 kHz onsets gives about
0.654 ms at BITE 5 and 2.987 ms at BITE 10. The rounded interpolation gives
about 0.656/3 ms. This is a measured approximation of the output behaviour,
not identification of McDSP's proprietary implementation. Intermediate BITE
values and other parameter combinations are extrapolations. Other onsets
in those same files are checks, not independent recording sessions.

Unlike changing the underlying peak detector's attack, this stage preserves
the long-term Auto memory and constant settled GR. Sinusoidal detector ripple
can still produce a small residual level difference; full-chain results are
reported rather than claiming an exact waveform match to the original.
At BITE 1 the normal GR passes unchanged. Manual BITE envelope states continue
updating in Auto; manual mode updates the Auto smoother's reference state.
Reset clears it, and prepare invalidates the sample-rate-dependent cache.
The new coefficient and power are calculated only when BITE or Fs changes.

The research renderer now accepts explicit automation profiles and splits
blocks at the prescribed 10-second event boundaries. It never infers the
automation from filenames. Both model-7 and model-8 outputs use the same
job settings. Parameter IDs/state schema are unchanged; saved state records
DSP model 8. Existing Auto presets with BITE > 1 intentionally sound different.

[Corrected acquisition audit, before/after measurements and validation](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-23-corrected/REPORT.md).
All four Auto BITE comparisons improve in whole-render MAE; the other
69 outputs are sample-identical to model 7. Across 36 transient windows,
20 improve and 16 are unchanged. On the four calibration onsets, relief
trajectory RMSE falls by approximately 63–80%. Windows Release DSP/GUI
tests pass 2/2. The corrected noise acquisition exposes a remaining Auto
error (0.348 dB overall MAE, 1.111 dB on compression-active windows);
this is retained as a limitation, not hidden by the BITE improvement.
Full DSP nap, oversampling decision, broader original BITE validation,
remaining acquisitions, real-voice/GR-video comparison and final DAW testing
remain open. No release is published by this work.

## P. DSP_MODEL_9: Auto, nap e ottimizzazioni (24 settembre 2026)

Versione candidata 0.2.3. Il controllo Auto normalizzato conserva la release
102 ms e usa in salita `a = exp(-1/(Fs*0.00032))`, al posto dei precedenti
20 us. E' un fit comportamentale congiunto su rumore e transienti: 640 us
riduce ulteriormente lo scarto del rumore ma peggiora alcuni attacchi BITE.
La nuova costante migliora 40 degli 80 confronti completi; 39 sono invariati
entro 2,53e-29 di differenza audio, T045 peggiora ed e' mantenuto diagnostico.
Nessuna modifica a R1/R2, release Auto, normalizzazione ratio o curva Auto BITE.
Il cambiamento sonoro intenzionale richiede DSP_MODEL_9; schema dei parametri
e identita' del plugin invariati. I preset Auto salvati usano il nuovo modello.

Il nap passa da Active a Draining e poi Sleeping solo su zero digitale
esatto, dopo esaurimento di filtri, GR, BITE e smoothing. Qualsiasi ingresso
programma/key non nullo, cambio parametro o canali risveglia l'intero blocco.
Nessuna soglia audio, nessun timeout della release, nessun azzeramento degli
stati inattivi. Il callback continua a osservare ingressi e parametri.
Tail dichiarata all'host: 2 s per la coda audio dei filtri, distinta dalla
memoria di controllo. Latenza di processing invariata: zero campioni.

Cache dei target gain, smoothing e coefficienti LR4, snapshot immutati
saltati. GUI: repaint delle aree modificate, FFT saltata sulle finestre
esattamente nulle, FIFO fermata senza editor visibile, testo knob aggiornato
solo se cambia. Curve STATIC I/O pubblicate tramite atomiche; paint passivo.
FFT 2048, ballistics e focus 30/100 ms restano invariati.

Oversampling completo: **NO-GO per 0.2.3**. Il prototipo mostra riduzione di
alcuni alias ma anche variazioni di gain/fase e beneficio non uniforme a 4x;
il costo CPU cresce circa 2-4x. Non vengono aggiunti parametro o label OS.
Il prototipo spettrale usa modello 8; il benchmark JUCE FIR/IIR usa modello 9.
Non e' una conclusione negativa su ogni possibile schema di oversampling.

Risultati, riproduzione, limiti e collaudi ancora aperti nella
[nota tecnica permanente](Research/NAP_CPU_AUTO_OVERSAMPLING_2026-09-24.md)
e nel [rapporto dei nuovi originali e della voce](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-24/REPORT.md).

## Q. Verifica tecnica locale finale della 0.2.3

DSP_MODEL_9 e schema invariati. Aggiunti strumenti diagnostici esclusi dal
VST3: matrice wrapper/DSP, guardie allocazioni e import lock/attese/I/O,
stress concorrente, profilo GUI/wrapper e verifica stato con tag modello 5.
2.160 casi e 21.600 callback: errore massimo zero; 1.024 callback concorrenti
senza violazioni. Suite locale 4/4 PASS. Corretto anche il target nap mancante
nella build CI. I picchi temporali osservati non sono nascosti e richiedono
collaudo sul driver/host reale. La semantica audio non cambia in questa revisione.
[Metodo, dati e limiti](Research/technical_validation_2026-09-24/REPORT.md),
[prove utente residue](Research/VALIDAZIONE_UTENTE_0.2.3.md).
