# Release 0.2.5: implementazione e verifica

Data: 7 ottobre 2026. Candidato locale, DSP_MODEL_10, schema stato 3.
Nessuna pubblicazione. Le funzionalità sono implementate; la certificazione della
release resta distinta dai test automatici e dai gate host/hardware sotto indicati.

## Comportamento consegnato

- IIR predefinito: percorso audio originale, latenza zero; grafico con risposta
  digitale complessa, compensazioni all-pass e somma complessa delle bande.
  Durante lo smoothing la GUI dichiara la destinazione della frequenza.
- Dual Mono: detector, Ballistics e BITE indipendenti L/R, controlli comuni;
  key stereo L→L/R→R, key mono condiviso. Meter GR separati, aggregato max(L,R).
  Cambio Stereo/Dual Mono con trasferimento degli stati e transizione GR di 5 ms.
- Linear Phase: FIR simmetrici complementari, FFT partizionata con lavoro distribuito
  fra campioni; progettazione sul worker, richieste obsolete scartate, transizione
  di 20 ms sulla storia già acquisita. Nessun lookahead aggiuntivo del key.
- Modalità crossover richiesta persistente e non automatizzabile: si applica nel
  successivo prepareToPlay, dopo disattivazione/riattivazione del plugin nell'host.
  Lo stop del transport da solo non basta. La GUI mostra modalità attiva e pendente.
- Migrazione esplicita degli stati precedenti a IIR/Stereo, anche sopra un'istanza
  Linear Phase/Dual Mono; ID e indici dei parametri precedenti conservati.
- Bypass nativo con dry ritardato e storia wet mantenuta calda; nap dopo lo
  svuotamento delle code. Entrambi i segni IEEE dello zero sono silenzio,
  mentre i bit subnormal restano un motivo di risveglio anche sotto DAZ/FTZ.

## Profilo FIR e latenza

Scelta dell'utente: qualità alle basse frequenze accettando maggiore latenza.
Un solo profilo: sinc finestrato Blackman, N = 2·ceil(Fs·0.256)+1;
D=(N−1)/2. Il supporto dei low-pass più alti è abbreviato mantenendo il medesimo
centro D. Le bande sono L1, L2−L1, L3−L2, deltaD−L3 (adattate a 2/3 bande).
La latenza effettiva è D+2P, inclusi i buffer della convoluzione.

| Fs | Taps N | Partizione P | Latenza campioni | Latenza ms | Coda FIR dichiarata s |
| --- | ---: | ---: | ---: | ---: | ---: |
| 44.1 kHz | 22581 | 2048 | 15386 | 348.89 | 0.60490 |
| 48 kHz | 24577 | 2048 | 16384 | 341.33 | 0.59733 |
| 96 kHz | 49153 | 4096 | 32768 | 341.33 | 0.59733 |
| 192 kHz | 98305 | 8192 | 65536 | 341.33 | 0.59733 |

Il wrapper dichiara questi campioni tramite setLatencySamples. La coda FIR è
(N−1+2P)/Fs, conservativa; la memoria della dinamica non rappresenta una coda audio.
Il sample rate del motore è sanitizzato fra 8 e 384 kHz. Il dominio verificato qui
è 44.1/48/96/192 kHz. Frequenze ordinate/clamp condivisi con IIR, limite 0.45·Fs:
nessuna distanza minima aggiuntiva o degradazione nascosta. Bande 20/21/22 Hz sono
permesse ma possono non avere un passband piatto. La forma FIR differisce da LR4;
pre-ringing e compressione tempo-variante richiedono ascolto oltre alle misure.

## Misure e criteri automatici

Il prototipo misura il low-pass a 20 Hz: passband 0..10 Hz e stopband 30..100 Hz.
L'obiettivo è ripple ≤0.1 dB e attenuazione ≥60 dB in queste regioni, non all'interno
della transizione 10..30 Hz. Il riferimento FIR diretto usa tutti i coefficienti;
non riutilizza la convoluzione FFT. Il confronto di costo usa input precalcolato.

Misure finali: ripple 0.00113733..0.00113781 dB, stopband -79.7017..-79.7001 dB,
simmetria esatta, errore FFT/diretto massimo 4.88e-19, errore somma massimo
1.36e-20 per impulso double, zero overruns dello scheduler. Il FIR diretto costa
46.6 microsecondi/campione a 48 kHz e 199.6 a 192 kHz per un solo low-pass;
il budget disponibile è 20.8 e 5.21 microsecondi/campione. Il prototipo FFT
con tre low-pass e un solo lane attivo usa circa 0.10 e 0.40 della durata audio,
incluso il timer per campione. Questo confronto spiega la scelta della convoluzione
partizionata; il budget del compressore completo resta quello sotto.

I test obbligatori verificano impulsi e DTFT indipendente a sei frequenze per caso,
non ogni pixel della GUI: errore ≤0.1 dB sopra −80 dB; sotto il floor errore assoluto.
FIR diretto/FFT <1e−9, fase dopo compensazione del ritardo <1e−8 di parte immaginaria;
somma delle bande ≤1e−6 anche con blocchi irregolari e sovradimensionati. Le richieste
rapide di kernel sono verificate con un seno a 63 Hz: somma ≤1e−6, passo massimo
<0.01 FS, nessuno sforamento del budget interno dello scheduler.

La dinamica Dual Mono è confrontata con due compressori mono indipendenti per
IIR/FIR, 2/3/4 bande, key assente/mono/stereo e TC/BITE. Wrapper e GUI coprono
stato legacy, undo/redo, riapertura, indici parametri, latenza, bypass alternato,
blocco oltre la dimensione nominale e layout minimo. Nap verifica code FIR e
risveglio solo R ai quattro sample rate. Windows mantiene i quattro CTest;
Linux/macOS i tre, senza dipendenze da Research o archivi audio esterni.

| Piattaforma | Build | Test obbligatori | Tempo |
| --- | --- | --- | ---: |
| Windows x64 / MSVC 14.40 | VST3 Release, test /Ox | 4/4 PASS | 398.75 s |
| Ubuntu 24.04 x86_64 / GCC 13.3 | VST3 Release /O3 | 3/3 PASS | 80.03 s |
| macOS 12.7.6 Intel / Apple Clang 14 | VST3 Universal /O3 | 3/3 PASS nativi Intel | 77.50 s |

macOS: lipo conferma x86_64 e arm64; firma ad hoc verificata con codesign --deep
--strict, archivio di test. Esecuzione ARM64 e notarizzazione non sono certificate.
Linux: sessione GUI Xvfb/Openbox. Research assente dalle copie sorgenti remote.

Matrice Windows: 4320 casi IIR, 43200 callback più 1024 concorrenti; FIR 432
configurazioni e 31104 callback. Zero errori audio nel confronto wrapper/DSP,
zero allocazioni, deallocazioni, lock, attese o I/O rilevati nel callback.
Questo audit non misura una garanzia di deadline.

La prima suite Windows ha raggiunto il timeout storico di 300 s nella matrice
ampliata, mentre erano attivi i compilatori. Il limite è ora 900 s; nessun caso
rimosso. La matrice ripetuta a compilatori fermi è passata in 193.69 s. Il log
iniziale è conservato insieme al risultato finale, senza presentarlo come PASS.

Evidenze nel workspace: artifacts/MC2000/0.2.5-validation (log build/test,
JUnit, CSV CPU/nap, audit JSON, screenshot, sorgenti congelati e VST3 di test).
Gli archivi Windows/Linux/macOS sono per prova locale; nessuna pubblicazione.

## Budget CPU e memoria: limiti misurati

Benchmark del compressore completo: input e key stereo, Stereo/Dual Mono,
frequenze normali 100/1000/10000 Hz oppure 20/21/22 Hz, blocchi 64/512/2048;
48 casi da un secondo di audio, MSVC /O2. CPU Windows Intel Core i3-3217U 1.80 GHz.
La frazione CPU usa GetThreadTimes del thread chiamante (include la generazione
input, esclude il worker); la frazione wall misura il callback. Granularità del
contatore e scheduling OS limitano la precisione. Il CSV idle è conservato fra
le evidenze e non è una garanzia su tutte le macchine.

| Fs | Frazione CPU min..max | Casi con max callback oltre deadline |
| --- | ---: | ---: |
| 44.1 kHz | 0.094..0.234 | 2/12 |
| 48 kHz | 0.125..0.250 | 1/12 |
| 96 kHz | 0.234..0.344 | 0/12 |
| 192 kHz | 0.500..0.953 | 8/12 |

A 192 kHz, bande ravvicinate, Dual Mono: blocco 64 p99 0.421 ms contro deadline
0.333 ms; blocco 512 p99 5.992 ms contro 2.667 ms; blocco 2048 max 8.941 ms contro
10.667 ms in quel caso. A 44.1 kHz è stato osservato anche jitter OS di 89 ms.
**Il gate deadline realtime non è passato su questa macchina.** L'audit di assenza
allocazioni/lock/I/O e lo scheduler senza overruns verificano proprietà differenti.
Nessuna sostituzione silenziosa con IIR o riduzione della qualità per aggirare il limite.

RAM FIR: stima del payload dei vettori e tabelle, non misura RSS. Include quattro
bank completi, quattro lane preallocate, FFT/history e risposte atomiche; durante
la progettazione si aggiunge scratch del worker. Il wrapper aggiunge dry/bypass
buffer e JUCE/GUI. Capacità e allocator possono aumentare il consumo effettivo.

| Fs | Payload FIR persistente MiB | Scratch worker aggiuntivo MiB |
| --- | ---: | ---: |
| 44.1 kHz | 13.25 | 4.23 |
| 48 kHz | 14.28 | 4.25 |
| 96 kHz | 28.45 | 8.50 |
| 192 kHz | 56.80 | 17.00 |

Stima persistente: 256*partitions*fftSize + 32*(9P+latency+1) +
28*fftSize + 4*3*1205*8 byte. Scratch: 32*responseFFTSize + 8N +
16*fftSize byte; responseFFTSize è una potenza di due almeno 4N.

## Provenienza e riproducibilità

Working tree del prodotto basato su 60a812e, con modifiche non committate e
modifiche dell'utente conservate. Baseline 0.2.3 congelata da commit 5b9249c.
Hash SHA256 identico dei due render: 1697AA2D0C2AD84660C6C25E403691EA13029B3270400C4508EF4EB9F3B7A65B.
Il render compatibilità usa 36 configurazioni deterministiche, quattro Fs,
2/3/4 bande, tre TC, modifiche gain/crossover/IN/SOLO e key in Auto.
L'identità bit per bit riguarda questi casi, non ogni possibile input o host.

Snapshot di Source/Tests/CMake senza Research, usato per le build finali:
SHA256 F5762FA54D4BB2030593B80493E4FC2EED11AA254F7FFABB24CE1EABBCBD3992.
JUCE 9.0.1; Windows MSVC 14.40 x64; Linux GCC/Ubuntu 24.04 x86_64;
macOS Universal x86_64+arm64 con test nativi sul guest Intel Monterey.
La VM è rimasta integra e avviata: lo spegnimento ordinato loginwindow/ACPI non
è stato confermato e sudo -n richiede password; nessuno stop forzato o rimozione.
La presenza della slice ARM64 non equivale a un test su Apple Silicon.

I file di test ora descrivono il comportamento:
[CrossoverAndChannelModeTests.h](../Tests/CrossoverAndChannelModeTests.h),
[LinearPhaseCrossoverTests.h](../Tests/LinearPhaseCrossoverTests.h),
[CrossoverStateLatencyAndUITests.h](../Tests/CrossoverStateLatencyAndUITests.h),
[LinearPhaseNapValidation.h](../Tests/LinearPhaseNapValidation.h),
[LinearPhaseRealtimeValidation.h](../Tests/LinearPhaseRealtimeValidation.h).
Le descrizioni delle asserzioni e Tests/README.md dettagliano i casi.
I numeri di release sono usati per report e artefatti. IntelliSense è configurato
C++20 nel workspace/prodotto, coerente con CMake; gli operatori defaulted e
std::numbers richiedono questo standard.

## Gate ancora aperti

- Deadline e budget CPU su hardware target, GUI aperta/chiusa e automazione intensa.
- Esecuzione nativa Apple Silicon (la build Universal verifica solo entrambe le slice).
- DAW reale: PDC con tracce parallele, bypass host, riapertura sessione, render offline
  e procedura di riattivazione, registrando nome e versione dell'host.
- Ascolto su bassi/transienti, pre-ringing e transizioni di kernel.

Questi gate non sono dichiarati passati e non è stata pubblicata una release.