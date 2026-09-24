# 0.2.3: nap, CPU/GUI, Auto e valutazione oversampling

Baseline: commit `8393751`, DSP_MODEL_8. Build candidata: **0.2.3, DSP_MODEL_9**.
Questo documento distingue le ottimizzazioni a parita' di algoritmo dalla
ricalibrazione intenzionale di Auto. Nessuna release pubblica e' implicita.

## Nap e costo a riposo

Il motore usa tre stati: Active, Draining, Sleeping. Il sonno richiede
**zero digitale esatto** su entrambi i canali di programma e su ogni canale
sidechain presente. Nessun gate basato sul livello e nessuna sospensione
basata su GUI, trasporto, IN o SOLO.

Il controllo di quiete avviene dopo l'elaborazione del blocco: stati attivi
LR4 sotto 1e-24, memorie BITE esaurite, GR sotto 1e-9 dB, rampe gain e
crossover numericamente stabili, routing completato. Non viene imposto un
timeout che tronchi il rilascio. Stati di bande, filtri e canali non utilizzati
rimangono conservati, come nel percorso normale. Il nap conserva anche la
fase dell'aggiornamento crossover ogni 16 campioni.

Il blocco successivo viene esaminato prima di saltare il DSP. Basta un solo
campione non nullo, anche molto debole, per elaborare **tutto quel blocco**.
Anche cambi effettivi dei parametri o del numero di canali risvegliano il
motore. Il key attivo impedisce il sonno completo con programma muto.
Le letture istantanee dei meter scendono al fondo; i picchi gia' pubblicati e
non ancora letti non vengono cancellati dal ramo Sleeping.

`getTailLengthSeconds()` passa da 0 a 2 s, come stima prudenziale della coda
audio dei crossover fino a 20 Hz. Questa non e' una latenza e non rappresenta
la durata della memoria GR. Il callback continua a verificare gli ingressi:
CPU esattamente zero dipende dalla sospensione effettuata dall'host.

[36 confronti e prove di risveglio](validation_2026-09-23-nap/nap_validation.json):
44,1/48/96/192 kHz, blocchi 64/512/2048, R1/R2/Auto, BITE 10, key interno/esterno.
Errore audio massimo osservato **0**. Aggiunti 512 impulsi, uno per posizione
nel blocco, segnale 1e-30, key attivo, cambio parametri, rilascio R1 massimo
per 100 s e cambio bande/modo/gain dopo il sonno. I test numerici non
sostituiscono il collaudo di seek/loop/stop e bypass nella DAW.

## CPU attiva e GUI

- Cache dei target gain e delle costanti di smoothing; snapshot identici
  evitano una nuova sanitizzazione. I coefficienti dipendenti da Fs vengono
  invalidati in prepare; nessuna approssimazione di exp/log/pow.
- Ogni LR4 riconfigura soltanto una frequenza effettivamente cambiata;
  l'interpolazione e la cadenza a 16 campioni restano identiche.
- Meter, STATIC I/O e spettro richiedono repaint quando cambia il contenuto,
  senza ridisegnare ogni intera striscia di banda a ogni tick.
- FFT delle finestre esattamente nulle saltate; dimensione 2048, overlap,
  stereo, risposta e ballistics temporali invariati. Le finestre parziali
  continuano a includere correttamente gli ultimi campioni non nulli.
- Editor nascosto: stop del consumer FFT, niente lavoro dei grafici nel timer
  principale, scarto dei picchi storici. Alla riapertura la FIFO riparte dai
  dati nuovi. Nessuna chiamata GUI nel thread audio.
- Testo dei knob riformattato solo al cambio valore o dopo una modifica
  testuale; restano doppio click, unita', editing e focus 30/100 ms.
- I parametri effettivi delle curve STATIC I/O sono pubblicati atomicamente;
  paint usa una copia del thread GUI e non legge strutture DSP mutabili.

Le prime due esecuzioni separate del benchmark hanno verificato **54/54
output bit-identici** sul modello 8, ma misure CPU molto instabili
(mediane -23,1% e +4,3% a ordine invertito). Non sono una prova di risparmio.
Il confronto conclusivo alterna i due motori nello stesso processo e per
ogni blocco, con cinque ripetizioni dopo warm-up. La baseline di quest'ultimo
confronto e' il modello 8 con la sola costante Auto portata a 320 us: confronta
quindi la stessa legge audio del modello 9 con/senza ottimizzazioni.

## Auto: rumore, transienti e limiti del fit

T024 evidenzia maggiore compressione Ponte soprattutto sopra 1,5 kHz:
nei plateau lo scarto e' circa 1,2-1,4 dB; il riferimento neutro differisce
molto meno. Il confronto non supporta una semplice correzione grafica della GR.

Sono state valutate 15 coppie attacco/rilascio, mantenendo separate le prove
sul rumore da quelle a tono. L'attacco 640 us minimizza l'errore sul solo
rumore ma peggiora alcuni attacchi BITE. **320 us** e' il compromesso che
migliora rumore, attacchi 315 Hz/2 kHz e tutti i quattro attacchi BITE
verificati, mantenendo la memoria di rilascio a **102 ms**.

Nel controllo lineare normalizzato q, la salita usa
`a = exp(-1 / (Fs * 0.00032))`; la discesa mantiene
`q = 1 + exp(-1 / (Fs * 0.102)) * (q - 1)`.
Ratio, knee, manual Attack/Release, leggi R1/R2 e curva Auto BITE non cambiano.
320 us e' un parametro del modello identificato, non una lettura dal pannello
McDSP ne' una dimostrazione della sua implementazione interna.

Nel prototipo, T024 passa da MAE 0,348 a 0,092 dB; limitando alle finestre
con compressione attiva, da 1,111 a 0,293 dB. Sui primi 50 ms dei quattro
attacchi BITE, RMSE passa da 0,323/0,238/0,359/0,281 a
0,109/0,099/0,135/0,098 dB.
[Misure per banda](validation_2026-09-23-nap/auto_noise_bands.json),
[candidati](validation_2026-09-23-nap/auto_noise_candidates.json),
[controllo BITE](validation_2026-09-23-nap/auto_noise_bite_holdout.json).

## Oversampling: prototipo e criterio di decisione

Eseguiti 300 render: 997/7001/16001 Hz, multitono, sweep e transienti;
R1/R2/Auto, BITE 1/5/10, ratio 10 e controlli neutri; 1x/2x/4x con
riferimenti 8x/16x. Il prototipo sovracampiona l'intera catena, inclusi i
crossover. FIR Kaiser beta 10, 64*k+1 coefficienti; compensazione offline
del ritardo. La versione streaming di questo FIR avrebbe 64 campioni di
ritardo complessivo, 1,33 ms a 48 kHz.

Le misure separano differenze audio generali dai candidati alias: solo sui
toni si integrano le armoniche dispari ripiegate previste, sottraendo prima
la fondamentale stimata per evitare leakage. I controlli neutri sono circa
-170 dBc. I risultati a 8x/16x sono confronti di convergenza, non una verita'
assoluta ne' una misura del motore originale.

L'oversampling riduce realmente alcuni alias: a 7001 Hz, R1 BITE 1 passa
da -58,1 dBc a -88,4 dBc a 4x. Il miglioramento non e' uniforme: a 16001 Hz
R1 passa da -43,9 a -57,2 dBc a 4x e -51,3 dBc a 8x; la dinamica del detector
puo' interagire con il rate. A 16001 Hz Auto, 2x modifica l'attenuazione media
di circa 0,36 dB e cambia la fase dei crossover. La sola differenza tra
waveform non viene chiamata aliasing.

[Misure del prototipo modello 8](validation_2026-09-23-nap/oversampling.json).
Il benchmark aggiuntivo usa l'effettiva classe JUCE Oversampling con FIR e
IIR, qualita' massima, latenza intera, intera catena DSP e buffer 512.
Nessuna modalita' parziale del solo detector viene presentata come soluzione
agli alias del prodotto audio gain*segnale.

## Acquisizioni aggiuntive e voce

[Rapporto della consegna del 24 settembre](NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-24/REPORT.md).
L'utente conferma Fs nativo della DAW uguale all'export a 96/192 kHz e
buffer 512. Il confronto external-sidechain con l'originale e' **non
eseguibile nel setup disponibile**, non un test originale fallito.
La sidechain Ponte continua a essere verificata nei test interni.

La voce MP3 richiede un allineamento costante di 50 ms piu' 57 campioni;
nessuna deriva osservata nei cinque intervalli controllati. R1 mostra MAE
0,134 dB a 250 ms e 0,125 dB a 500 ms; il contrasto fra release ha MAE
0,032 dB. Sono risultati audio, non misure pixel dei meter. Mancando un
render neutro originale della voce, decoder MP3, ricampionamento e gain
clip rimangono fonti di incertezza: nessun offset artificiale viene applicato
al DSP o alla grafica per compensarle.

## Riproduzione e risultati conclusivi

In PowerShell, dalla root PonteDSP, usare il Python con NumPy/SciPy gia'
preparato in `build/mc2000-meter-analysis-tools/python/python.exe`.
Gli script sono in `products/MC2000/Research`:

1. `AuditDelivery20260924.py` verifica gli originali senza modificarli.
2. `ValidateModel9.py --prepare`, poi `--render --measure`: 80 confronti;
   richiede i renderer CMake e la baseline Source modello 8 congelata.
3. `InvestigateAutoNoise.py --build` e `AutoNoiseBiteCheck.py`: ipotesi Auto.
4. `EvaluateOversampling.py --dsp-model 8 --renderer <renderer-modello-8>`: genera i 300 render; `--measure-only --dsp-model 8` riusa i
   dati esistenti. Il dataset archiviato riguarda DSP_MODEL_8; per riprodurlo
   esattamente usare quel renderer, non un nuovo modello implicitamente.
5. `build_performance_pair.py`, poi `PerformancePair.exe output.csv`:
   motori isolati in namespace distinti, stesso modello, ordine alternato.
6. `ctest --test-dir build/MC2000-bite-2026-09-23 -C Release --output-on-failure`;
   `MC2000NapValidation.exe --benchmark` e `MC2000UITests.exe --oversampling-cost`
   vanno misurati senza compilazioni/render concorrenti.

I renderer e il test nap impostano FTZ/DAZ come il wrapper VST. Inizialmente
il percorso di riferimento dei test lasciava vivere i denormali: era una
condizione di benchmark non rappresentativa, corretta prima delle misure
conclusive. Gli originali WAV e i dati storici non sono sovrascritti.

### Risultati finali e decisione 0.2.3

Host di misura: Intel Core i3-3217U, 1,80 GHz, 2 core/4 thread; Windows x64,
MSVC 19.40, CMake 3.30.1, JUCE 9.0.1, configurazione Release. Benchmark
eseguiti in sequenza senza compilazioni/render concomitanti di questa task.
Il sistema non e' un ambiente realtime isolato.

- **Nap:** mediana di sette ripetizioni da 2000 blocchi stereo 512/48 kHz:
  875,73 ms OFF, 3,6806 ms ON, **-99,58%**, circa 438 -> 1,84 us/blocco.
  E' il solo motore su zero, non il carico totale di Ableton.
  [Misura grezza](validation_2026-09-23-nap/nap_benchmark.csv).
- **DSP attivo:** 54 scenari, cinque ripetizioni dopo warm-up, sempre
  bit-identici; risparmio mediano elapsed **3,16%**, cicli del thread
  **3,09%**. Range elapsed -1,63%..+10,49%, nessuna regressione >5%.
  QueryThreadCycleTime affianca il timer per distinguere attese scheduler;
  non e' un contatore esclusivo delle istruzioni DSP. I p99 per blocco sono
  archiviati ma non certificano assenza di dropout in DAW.
  [CSV](validation_2026-09-23-nap/performance_paired.csv),
  [riepilogo](validation_2026-09-23-nap/performance_paired.json).
- **Oversampling JUCE, modello 9, 48 kHz/512:** 2x FIR/IIR circa
  1,86-2,16 volte il tempo 1x; 4x circa 2,75-4,27 volte. Latenza aggiunta
  FIR 49/61 campioni, IIR 4/6 campioni (2x/4x, latenza intera).
  Sono resampler diversi dal FIR offline delle misure spettrali modello 8:
  costo e alias non rappresentano lo stesso identico prototipo.
  [CSV costo/latenza](validation_2026-09-23-nap/oversampling_cost.csv).
- **Test:** DSP, GUI e nap **3/3 PASS** (210,94 s). Dopo la sola cache testo
  knob, suite GUI rieseguita: PASS (2,72 s). VST3 Release compilato, versione
  manifest 0.2.3. [Log e hash](validation_2026-09-23-nap/build_validation.json).

**NO-GO per l'oversampling completo in 0.2.3.** Esiste un beneficio su alcuni
alias, ma non uniforme a 4x; cambia anche la risposta e richiede ulteriore
calibrazione con un costo consistente. Nessun controllo OS nel prodotto.
Non sono stati fatti ascolti comparativi ne' un prototipo detector-only;
non si conclude che ogni futura soluzione OS sia inutile o inudibile.

Rimangono aperti il collaudo host e la profilazione GUI reale, la diagnosi
T045 (inclusa esplicitamente fra gli 80 confronti, con peggioramento noto)
e la misura GR visuale sincronizzata. La nuova build e' pronta per queste
prove; questa task non pubblica una release GitHub.
