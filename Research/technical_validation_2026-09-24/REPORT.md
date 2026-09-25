# Verifiche tecniche locali della candidata 0.2.3

Baseline di questa verifica: commit `72e474b`, DSP_MODEL_9. Non cambia la
legge audio. Gli strumenti diagnostici sono compilati solo nel test
`MC2000UITests`, con `MC2000_TECHNICAL_TESTS=1`; il VST3 non contiene hook
di import, override di new o cronometri attivi.

## Audit del callback

Il test confronta processBlock con il DSP diretto a nap disattivato, stessa
sorgente e snapshot, su 2.160 combinazioni: Fs 44,1/48/88,2/96/192 kHz;
buffer 32/64/128/256/512/1024/2048 e 257; programma mono/stereo; key
assente/mono/stereo; 2/3/4 bande e R1/R2/Auto. Ogni caso include prima
chiamata, silenzio, wake, zero/uno campione, blocco piu' corto, IN/SOLO,
cambio modo, key senza programma e NaN/Inf. Consumer FFT presente in meta'
dei casi. La suite nap separata copre il drenaggio lungo e 512 posizioni
di risveglio. Non si presume che dieci blocchi misurino una coda lunga.

Ulteriori 1.024 callback girano in un thread produttore mentre il thread
principale consuma FIFO, meter e curve atomiche. E' uno stress concorrente,
non una dimostrazione formale dell'assenza di ogni data race.

La guardia thread-local osserva new/delete C++, malloc/calloc/realloc e
gli ingressi importati a critical section, SRW lock, mutex MSVC, attesa e
ReadFile/WriteFile. Prima del test esegue una prova positiva: deve rilevare
allocazione, rilascio, lock, attesa e scrittura intenzionali fuori dal DSP.
Gli hook modificano soltanto l'import table dell'eseguibile diagnostico.
Non osservano chiamate interamente interne alle DLL CRT/OS o alla DAW:
**zero eventi misurati non e' una certificazione universale realtime**.

Revisione complementare del percorso chiamato:

- SnapshotReader conserva puntatori ad atomiche APVTS e array fissi; nessun
  ID costruito o ricerca parametro nel callback.
- getBusBuffer crea viste mono/stereo; AudioBuffer JUCE dispone di 32
  puntatori inline e non alloca per queste viste.
- FIFO JUCE usa indici atomici e storage preallocato; reset e cambio dimensioni
  restano fuori da processBlock. Atomiche float/double/int/bool verificate
  lock-free sul target tramite static_assert.
- DSP: array fissi, nessun file/network/GUI, mutex o container ridimensionato.
  MeterPeak ha un solo produttore e un consumer che riporta al floor;
  la CAS non e' un mutex. Il crossover del detector viene elaborato solo
  quando e' presente una key esterna: non c'e' doppio filtraggio interno.
- Le funzioni trascendenti restano esatte; il lavoro invariante e' gia'
  memorizzato. Nessuna approssimazione/SIMD aggiunta senza una necessita'
  dimostrata. Stato host, prepare e notifiche parametri sono fuori dalla
  guardia e non vengono dichiarati privi di allocazioni.

## Profilazione GUI e wrapper

Profilo GUI con parametri neutri (ratio 1:1, R1): un warm-up e cinque ripetizioni, 1/4 istanze, editor chiuso,
esistente ma nascosto, visibile fermo, animato e resize. Timer JUCE reali,
rete disattivata. Audio sintetico sul thread del test, cronometrato a parte;
il tempo CPU del thread comprende callback, dispatch e rendering forzato.
Il paint completo avviene su immagine software preallocata, non sulla GPU
della DAW. Nei casi animati si forza un frame completo per iterazione:
questo e' uno stress riproducibile, non il numero naturale di repaint.

Le sonde separate registrano snapshot, FIFO, DSP, timer principale, FFT,
risposta LR4, formattazione knob, layout e paint dei due grafici. I tempi
annidati non devono essere sommati come costi indipendenti. Private bytes
e tempi del thread sono osservazioni Windows; l'allocatore puo' mantenere
cache senza indicare una perdita di memoria.

Profilo callback: 54 condizioni, Fs 48/96/192 kHz, buffer 64/512/2048,
R1/R2/Auto, consumer FFT OFF/ON, quattro bande BITE 5, ratio 4:1,
threshold -30 dB, attack 10 ms, release 250 ms; cinque ripetizioni
di 64 blocchi dopo warm-up. Riporta mediana, p99 empirico (ordine 63 su 64),
massimo e deadline N/Fs. Sistema condiviso: le code dello scheduler non
sono attribuite automaticamente al DSP. Nessuna soglia CPU fragile in CI.

## CI e riproduzione

Corretto il workflow Windows: CTest registrava il nap, ma il comando build
non compilava MC2000NapValidation. Aggiunto quel target e il nuovo test
MC2000RealtimeMatrix; log e rapporti diagnostici vengono archiviati anche
in caso di fallimento. CI remota verificata sul commit `0cad3d5`: **build VST3 e 4/4 test PASS**.
[Job GitHub](https://github.com/Ponteh/ponte-mbc4/actions/runs/36048630878),
[risultati JUnit e matrice remota](ci.json).

```powershell
cmake --build build/MC2000-bite-2026-09-23 --config Release --target MC2000Tests MC2000UITests MC2000NapValidation PonteMC2000_VST3 --parallel 2
ctest --test-dir build/MC2000-bite-2026-09-23 -C Release --output-on-failure
# Eseguire i benchmark separatamente, senza build/render concomitanti:
build/MC2000-bite-2026-09-23/MC2000UITests_artefacts/Release/MC2000UITests.exe --gui-profile gui-profile.csv
build/MC2000-bite-2026-09-23/MC2000UITests_artefacts/Release/MC2000UITests.exe --callback-profile callback-profile.csv
```

## Risultati

- Suite locale **4/4 PASS**, 177,90 s: DSP 5,05 s, GUI 3,42 s,
  matrice realtime 28,66 s, nap 140,73 s. [Log](ctest.txt).
- **2.160 casi / 21.600 callback:** errore audio massimo **0** rispetto
  al DSP diretto. **Zero** allocazioni, rilasci C++ e ingressi osservati a
  lock, attese e I/O; 11 import hook installati, prova positiva superata.
  **1.024 callback concorrenti**, zero violazioni. Anche ripristino binario
  con tag DSP precedente e prepare ripetuti 48/192/44,1 kHz superati.
  [Rapporto macchina](technical-matrix.json).
- **GUI:** nessuna FFT in silenzio, nessun paint dei grafici a editor chiuso
  o nascosto, nessuna nuova formattazione knob a valori fermi dopo warm-up.
  Variazione private bytes massima osservata 933.888 byte fra prima e quinta
  ripetizione; non e' una misura di leak a lungo termine. Un valore 0 ms
  di CPU thread significa sotto la risoluzione del contatore Windows
  (qui 15,625 ms), non consumo esattamente nullo.
- Nel paint software completo forzato il costo dominante e' il rendering
  della finestra, non la sola FFT o le curve. Il prodotto gia' ridisegna le
  aree cambiate: non aggiungiamo cache raster di griglie/testi che richiedano
  nuove regole DPI/tema per ottimizzare questo scenario artificiale.
  Le cache dei dati esistenti restano; la cache completa di ogni disegno
  e' una possibile ottimizzazione futura, non una feature necessaria qui.

| Istanze | Scenario | CPU thread / passata (ms) | FFT / passata | Paint grafici (ms) | Variazione private bytes |
| ---: | --- | ---: | ---: | ---: | ---: |
| 1 | closed | 0.00 | 0 | 0.00 | 0 |
| 1 | hidden | 0.00 | 0 | 0.00 | 0 |
| 1 | idle | 15.62 | 0 | 1.17 | 847872 |
| 1 | animated | 375.00 | 60 | 25.91 | 417792 |
| 1 | resize | 390.62 | 60 | 24.90 | -290816 |
| 4 | closed | 0.00 | 0 | 0.00 | 0 |
| 4 | hidden | 0.00 | 0 | 0.00 | 0 |
| 4 | idle | 62.50 | 0 | 10.63 | 65536 |
| 4 | animated | 1468.75 | 240 | 93.51 | -290816 |
| 4 | resize | 1625.00 | 240 | 101.48 | 933888 |

La tabella riporta mediane di cinque passate con 20 iterazioni ciascuna.
Le passate hanno durate diverse: i millisecondi non sono percentuali CPU
direttamente confrontabili. [CSV GUI](gui-profile.csv),
[CSV callback](callback-profile.csv), [riepilogo](summary.json).

### Code temporali del callback

La prima misura conserva sette ripetizioni con almeno un blocco oltre la
deadline, fino a 2,66 volte N/Fs; le mediane dei p99 per condizione restano
sotto il 68% della deadline. [Prima misura integrale](callback-profile-first.csv).
Non e' quindi corretto promettere assenza di dropout su questa macchina.
La seconda misura aggiunge QueryThreadCycleTime a ogni blocco per osservare
quanta parte dei picchi comporti un aumento dei cicli del thread; il
contatore non e' un profiler esclusivo delle istruzioni DSP e non identifica
da solo quale driver o processo abbia causato un ritardo.

Seconda misura: nessuna delle 54 mediane dei p99 supera la deadline;
la peggiore e' il **59,6%**. Due delle 270 ripetizioni contengono almeno
un blocco oltre N/Fs; massimo osservato **1,094 volte** la deadline.
A 48 kHz/512 R2, il picco cresce 5,81x rispetto al tempo mediano ma i
cicli del thread solo 1,19x: forte indicazione di attesa/scheduling o
variazione della frequenza effettiva, non di lavoro DSP moltiplicato per sei.
A 192 kHz/64 R2 i rapporti sono 2,55x e 1,55x: c'e' anche variabilita'
del costo effettivo. Sono inferenze dalle misure, non un'identificazione
del driver responsabile. Non si modifica il modello sonoro per nascondere
queste code e non si dichiara garantita l'assenza di dropout.

La successiva aggiunta dei contatori di cicli ha modificato soltanto il
profiler opzionale; il VST3 e i percorsi dei quattro test restano quelli
che hanno superato la suite completa. La CI ricompila anche il profiler.

L'hardware locale rimane l'i3-3217U 1,8 GHz / 2 core / 4 thread, Windows
10 build 19045, MSVC 19.40 Release, JUCE 9.0.1. I tempi diagnostici includono
le sonde; il VST3 ne e' privo. Il budget sostenibile con altre tracce/plugin
e il scheduling audio del driver devono essere verificati in Ableton.


Le prove che richiedono la sessione dell'utente sono raccolte nella
[guida mirata](../VALIDAZIONE_UTENTE_0.2.3.md). Non occorre ripetere gli
80 render originali. T045 e il confronto visuale della GR restano aperti.
