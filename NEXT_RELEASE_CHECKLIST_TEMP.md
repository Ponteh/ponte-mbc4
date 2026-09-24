# Prossima release — checklist temporanea DSP, nap e CPU

Creato il 2026-09-16. Base: **v0.2.2**, commit
`f8f2a9c4de1751ba98eb9bac0cefd46d3bc05cea`, DSP_MODEL_5.
Versione candidata assegnata: **0.2.3**; pubblicazione ancora separata.

**Questo file è un piano, non un elenco di funzioni già implementate.**
Spuntare le attività con riferimenti a commit, test e risultati. Eliminarlo
soltanto dopo aver trasferito specifiche definitive, misure e limiti nei
documenti permanenti e completato la checklist finale.

## Stato implementazione 2026-09-18 ? build di prova 0.2.3

Implementati senza nuove acquisizioni McDSP: cache dei coefficienti DSP e
risposte FFT, arresto della produzione FIFO a editor chiuso, versione vicino
MBC4 e avviso asincrono delle nuove release, sostituzione dell'header con help.
Aggiunti benchmark A/B contro 0.2.2 e regressioni locali.
[Dettagli e risultati](Research/PERFORMANCE_2026-09-18.md).

Aggiornamento 2026-09-19: ricevuti 47 render, analizzati con audit dei file e
confronto del motore completo. Auto ricalibrato nel DSP_MODEL_6: tutti i 13
confronti utilizzabili migliorano. Sette export muti, neutro rumore duplicato
e impostazioni BITE/ripetizione da chiarire impediscono di chiudere la matrice.
[Rapporto e priorita delle acquisizioni](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-19/REPORT.md).
Nap DSP completo e oversampling restano aperti; possono procedere con test locali.

Aggiornamento 2026-09-20, **analisi senza modifica del DSP**: 80 WAV/75 ID e
quattro video, 61 confronti audio anche diagnostici. R1 a ratio 2:1 segue un
rilascio del controllo lineare con tau pari alla Release; la curva attuale
torna a metà GR circa 9% prima nelle sonde 500 ms. BITE sottostima e prolunga
il transiente. Auto BITE 1 trova conferma sul MC202 isolato e sulle sonde
LOW a 44,1/48/88,2 kHz. Dodici export primari ancora muti; impostazioni ALL
non equivalenti in alcuni gruppi. T075–T081 attesi, sidechain non disponibile.
[Rapporto aggiornato e priorità](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-20/REPORT.md).

- [x] Implementare la nuova legge R1 e aggiungere regressioni numeriche:
  ratio/knee, 250/500 ms, attacco, continuità, cambi parametro e blocchi.
  DSP_MODEL_7, build locale 0.2.3; POC appendice N e
  [confronto riproducibile](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-23-r1/REPORT.md).
- [ ] Completare R1 su voce reale e nuove acquisizioni originali ratio/knee:
  le prove numeriche oltre ratio 2:1 non certificano equivalenza McDSP.
- [x] Ricalibrare Auto BITE dai T027–T030 corretti, confrontando la traiettoria
  a 1 ms e il motore completo; regressioni R1/R2/Auto. DSP_MODEL_8, POC O e
  [rapporto della consegna corretta](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-23-corrected/REPORT.md).
- [x] Recuperare i dodici export muti: ora tutti udibili, stereo 48 kHz, 60 s.
  Verificati hash, ripetizione Auto e automazioni manuali secondo il piano.
- [ ] Confermare/rifare i preset incongruenti MC202/MC303 e ALL 44,1/88,2 kHz:
  i file sono invariati rispetto al 20 settembre e restano diagnostici.
- [ ] Validare Auto BITE con sorgenti/sessioni indipendenti e altri ratio/knee;
  i valori intermedi del controllo restano un'interpolazione. R1/R2 BITE invariati.
- [ ] Quantificare incertezza pixel/frame e confronto GUI Ponte per i meter:
  ritorno a 12 dB già vicino, piccoli cali da approfondire; GR ancora aperto.

Aggiornamento 2026-09-23: evidenze del 20 settembre conservate nel commit
`4280b54`, baseline audio DSP_MODEL_6 invariata. R1 ora usa il decadimento
del controllo lineare con tau pari alla Release e aggiornamento dalla GR
corrente per gestire l'automazione senza reinterpretare l'eta del rilascio.
R2, Auto, BITE e meter non sono stati ricalibrati. Nessuna release pubblicata;
benchmark CPU aggiornato, nap, oversampling e collaudo DAW restano aperti.
Verifica Windows Release: DSP/GUI 2/2 PASS (11.04 s); migliorano tutti gli
8 confronti R1 e i 12 tratti di release, altri 53 casi identici alla baseline.

Seconda consegna del 2026-09-23: completato audit dei dodici render corretti
e aggiunto supporto alle automazioni nel renderer. Auto BITE usa ora uno
smussamento della salita GR con circa 0,656 ms a 5 e 3 ms a 10; mantiene
il rilascio Auto 102 ms. Confronto della nuova build contro DSP_MODEL_7;
risultati e limiti nel rapporto collegato sopra. Versione di prova 0.2.3,
DSP_MODEL_8. Le spunte seguenti riguardano gli strumenti o le acquisizioni
effettivamente completati, non la validazione di tutte le 88 righe del piano.
Verifica modello 8: **73 confronti, quattro Auto BITE migliorati, altri 69
output bit-identici al modello 7; DSP/GUI 2/2 PASS**. Rumore T024 ancora
da affinare (MAE 0,348 dB complessiva, 1,111 dB con compressione attiva).

- [x] Confermare nel motore l'indipendenza Auto dai tempi manuali statici e
  automatizzati: T002/T035/T036/T037 e T056/T057 identici, come nell'originale.
- [x] Verificare Auto BITE su 44,1/48/88,2/96/192 kHz con test numerici di
  stato/reset e su blocchi 32/512/irregolari; non equivale a validazione McDSP
  nativa completa. Report e hash nel gruppo 23-corrected.
- [x] Approfondire lo scarto Auto sul rumore ora acquisito: misure separate
  su attacco/plateau/code e detector, evitando compensazioni globali arbitrarie.

## Stato aggiornato 2026-09-24 - candidato 0.2.3 / DSP_MODEL_9

Le note datate precedenti sono storiche. Le spunte indicano le verifiche
effettive, non la chiusura automatica di tutta la release.
[Nota tecnica permanente](Research/NAP_CPU_AUTO_OVERSAMPLING_2026-09-24.md),
[nuove acquisizioni](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-24/REPORT.md).

- [x] Nap implementato e validato: 36 casi, 512 posizioni di impulso,
  key attiva, segnale 1e-30, cambi di parametri/bande e release R1 massima.
- [x] Ottimizzazioni DSP verificate a parita' di modello: 54 scenari bit-identici.
  Il confronto CPU e i suoi limiti sono nella nota tecnica.
- [x] GUI: repaint selettivo, FFT nulla saltata, consumer nascosto fermato,
  cache testo knob e pubblicazione atomica delle curve. Suite GUI PASS.
- [x] Indagato e corretto Auto sul rumore con controllo dei transienti BITE.
  80 confronti: 40 migliorano, 39 invariati, T045 peggiora e resta diagnostico.
- [x] Ricevute/certificate dall'utente le acquisizioni native 96/192 kHz,
  buffer 512; corretti MC202/MC303 isolato e ALL 44,1/48/88,2 kHz.
- [x] Analizzata voce R1 250/500 ms con sorgente MP3; tutti IN attivi.
  Manca neutro originale; non sostituisce il caso esperto IN 2/3 e i meter.
- [x] Valutato oversampling completo: NO-GO 0.2.3, beneficio non uniforme,
  risposta alterata e costo circa 2-4x. Controlli/UI e test OS-02..06 non
  applicabili a questo candidato. Prototipo parziale/ascolti non eseguiti.
- [ ] Collaudo Ableton live input, stop/seek/loop, bypass, stato, molte istanze.
- [ ] Benchmark GUI/host completo e verifica realtime dei picchi CPU.
- [ ] Confronto GUI compresso sincronizzato e chiarimento T045 MC303 ALL.

Le righe generali di seguito che includono ulteriori condizioni (preset
originali, ascolti, profiler/allocazioni, tutte le voci della matrice) restano
aperte anche quando una parte e' coperta. Non occorre rifare gli 80 render.

## 1. Obiettivi e ordine del lavoro

1. Congelare baseline audio/GUI e misurare il costo attuale.
2. Completare le misure dell'originale, soprattutto Auto; correggere gli
   algoritmi soltanto dove i dati mostrano una necessità riproducibile.
3. Ottimizzare il processing attivo senza modificarne la qualità.
4. Implementare il nap sicuro e ridurre il lavoro della GUI inattiva.
5. Misurare se l'oversampling è utile; implementarlo nel prodotto soltanto
   se il beneficio giustifica costo, latenza e cambiamenti di risposta.
6. Ripetere regressioni, benchmark e prove DAW sulla combinazione finale.

Tenere separate le modifiche di modello sonoro dalle ottimizzazioni:
una correzione Auto può intenzionalmente cambiare il suono; una semplice
ottimizzazione deve preservare il modello che sta ottimizzando.

### Riferimenti da leggere prima di iniziare

- [POC](MC2000_Proof_of_Concept.md), incluse appendici I/J e IN=mute.
- [Audit DSP](Research/NEXT_VALIDATION_AUDIT.md): Auto, BITE, stereo,
  sample rate e acquisizioni mancanti.
- [Recensione esperto](Research/EXPERT_REVIEW_2026-09-09.md).
- [Rapporto meter](Research/GUI_METER_TEST_PACK/meter_fix_2026-09-15/REPORT.md)
  e [guida acquisizioni](Research/GUI_METER_TEST_PACK/README.md).
- `Source/DSP/Ballistics.h`, `MultiBandCompressor.*`, `CrossoverNetwork.*`,
  `Source/Parameters.cpp`, `Source/PluginProcessor.*`, `Source/PluginEditor.*`.
- `Tests/MC2000Tests.cpp`, `Tests/MC2000UITests.cpp` e analizzatori Research.
- `../../shared/CompanyGUI/MC2000_UI_CATALOG.md` e profilo MeterBallistics.

## 2. Baseline e strumenti di misura

- [ ] Conservare binario v0.2.2, hash, preset, sorgenti WAV e output di riferimento.
- [ ] Eseguire prima le suite esistenti; archiviare esito e screenshot GUI minimo.
- [ ] Creare un harness riproducibile per misurare DSP e wrapper separatamente,
  senza file I/O, logging o allocazioni nel tratto cronometrato.
- [ ] Creare un benchmark GUI separato: CPU processo/thread GUI, durata FFT,
  durata paint, numero di repaint e memoria; distinguere dal thread audio.
- [ ] Registrare CPU/hardware, OS, compilatore, flag Release, sample rate,
  buffer, numero di istanze, preset, durata, warm-up e condizioni energetiche.
- [ ] Fare almeno 5 ripetizioni per scenario dopo warm-up; riportare mediana,
  p95/p99 e massimo dei tempi per blocco, dispersione e deadline `N / Fs`.
  Non dedurre l'efficienza dal solo indicatore CPU della DAW.
- [ ] Aggiungere contatori di allocazioni/lock nel callback in build diagnostica;
  misurare i tempi con strumentazione leggera, separata dalla build diagnostica.
- [x] Generare manifest con seed dei segnali, hash dei WAV e delle build,
  parametri, eventi temporali e associazione input/output.
  Pack del 17 settembre, rapporti 19/20/23 e rispettivi JSON di verifica.

Matrice comune: 44.1/48/88.2/96/192 kHz; blocchi 32/64/128/256/512/1024/2048
e dimensioni irregolari; mono/stereo; 2/3/4 bande; R1/R2/Auto; sidechain
assente/mono/stereo; editor chiuso/aperto/inattivo/in uso. Eseguire una matrice
ridotta rappresentativa in CI e quella estesa nel report prima della release.
Stressare anche blocchi vuoti, input non finiti e prepare/reset ripetuti.

## 3. Nuove misure e miglioramento degli algoritmi

**Pack acquisizioni preparato il 2026-09-17:**
[NEXT_RELEASE_ORIGINAL_TEST_PACK](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/README.md).
Contiene 22 WAV sintetici tutti da 60 s, piano di 88 export originali con
parametri espliciti, manifest e validatore. Iniziare con P0 (12 export).
La generazione delle sorgenti non equivale all'esecuzione delle acquisizioni
o alla validazione di Auto: le attività seguenti restano da completare.

### 3.1 Acquisizioni pulite

- [x] Inventariare i file già disponibili prima di richiedere altre registrazioni.
  Per archivi originali voluminosi, chiedere l'estrazione quando necessaria,
  estrarre solo i file utili e liberare i temporanei dopo la verifica.
- [ ] Escludere dal fit i file con sample rate reale diverso dal nome e completare
  la matrice nativa: l'audit segnala Solo Low etichettati 88.2/96/192 ma a 48 kHz
  e assenza di All a 192 kHz.
- [x] Acquisire i render originali di automazione T055–T061, con valori/tempi
  del piano confermati dall'utente; confronto quantitativo, non equivalenza perfetta.
- [x] Risolta la disponibilita' T049-T054: **N/A nel setup originale**,
  inclusi i controlli del gruppo. Non acquisiti; copertura Ponte interna separata.
- [ ] Usare sorgenti identiche, Warp OFF, normalizzazione/dither OFF, gain noti,
  UNLINKED salvo test del link; annotare versioni e preset dei due plugin.
- [ ] Acquisire per ogni banda un riferimento neutro ratio 1:1 con routing
  equivalente al test compresso. Non assumere che IN/SOLO abbiano la stessa
  semantica nei due prodotti: verificare l'audio effettivamente presente.
  In Ponte v0.2.2 IN spento silenzia; SOLO non riapre l'ingresso.
- [x] Separare export audio per il fit DSP e video per i meter. Un export offline
  non sincronizzato al video non misura da solo la latenza visiva assoluta.

### 3.2 Auto: identificazione prima della correzione

Stato iniziale: le 39 coppie manual min/max dell'audit confermano che Auto
ignora Attack/Release manuali. La legge crest-factor in `processAuto()` è
ancora un fallback, non un fit completo dell'originale. Questo è lo stato
iniziale della checklist: dal 19 settembre DSP_MODEL_6 usa il modello
misurato tau 102 ms, descritto nell'appendice L del POC. La matrice completa
di validazione resta aperta, come precisato nel rapporto del 20 settembre.

- [x] Creare `AUTO-01`: gradini con portante non nulla fra i picchi, livelli e
  durate diversi, per misurare attacco, plateau e rilascio senza dividere per zero.
  Sorgenti 01/05/06/07 del pack; fit e confronti nei rapporti 19/20/23 settembre.
- [x] Creare `AUTO-02`: impulsi/burst e treni con pause variabili; coppie di burst
  per rilevare memoria, recupero incompleto e dipendenza dalla storia.
  Sorgente 02_MEMORY, confronto T006 nel rapporto del 19 settembre.
- [x] Creare `AUTO-03`: segnali a RMS uguale con crest factor diverso e a picco
  uguale con RMS diverso; includere transizioni fra queste condizioni.
  Sorgenti 03/04_CREST, confronti T014/T016; scarti residui documentati.
  Queste tre spunte attestano creazione/misura, non equivalenza perfetta del DSP.
- [ ] Creare `AUTO-04`: tono basso/medio/alto, multitono e rumore a seed fisso;
  provare ogni banda e i modelli 2/3/4 bande, isolando il contributo crossover.
- [ ] Creare `AUTO-05`: BITE 1 prima, poi 5/10; ratio, threshold e knee diversi.
  Separare l'effetto BITE dalla legge Auto, evitando di compensare l'uno con l'altra.
- [ ] Creare `AUTO-06`: variazione degli Attack/Release manuali durante Auto,
  passaggio R1/R2/Auto, reset, riapertura sessione e cambio sample rate.
- [ ] Creare `AUTO-07`: stereo L-only/R-only/L=-R, sidechain esterna e segnali
  diversi fra program e key. Misurare anche il recupero dopo key senza program.
- [ ] Tenere fuori dalla calibrazione intere sorgenti/sessioni, non soltanto
  campioni contigui dello stesso render; usarle come verifica indipendente.
- [ ] Misurare traiettorie GR audio da riferimento filtrato: errori in dB su
  attacco/plateau/code, tempi 10/50/90%, overshoot e comportamento sui burst.
  Dichiarare finestre, allineamento, esclusioni vicino allo zero e incertezza.
- [x] Confrontare poche leggi candidate motivate dai dati; documentare parametri
  identificabili e ambiguità. Non imporre una legge unica se i modelli originali
  mostrano differenze ripetibili non spiegate dal routing.
  Auto: rapporto 19 settembre; R1: rapporto 23-r1; Auto BITE: quattro ipotesi
  e fit nel rapporto 23-corrected. I gruppi con routing dubbio restano esclusi dal fit.
- [ ] Modificare Auto solo se migliora sulle sorgenti indipendenti, senza regressioni
  materiali nelle altre famiglie. Congelare le tolleranze prima del fit finale.
  Se i dati non bastano, conservare il fallback e dichiarare il limite.

### 3.3 Questioni della recensione ancora aperte

- [ ] Registrare 05 compresso a release 250 e 500 ms, 03/04 a 500 ms e voce dry
  con molta energia nella fondamentale. Rifare B0 quando il routing cambia.
- [ ] Riprodurre R1, ratio 2:1, knee 0, attack 2.5 ms, BITE 1, threshold -27.5 dB,
  crossover 100/785/10000 Hz, bande 2/3; documentare IN/SOLO di entrambe le build.
- [ ] Distinguere la differenza di GR audio dalla differenza di GR mostrata.
  Non correggere un plateau del meter sottraendo un offset arbitrario.
- [ ] Ripetere 01/02 e 03/04 R250 sulla GUI aggiornata, poi confrontare il ritorno
  visivo con la cattura originale, rispettando l'incertezza dei fotogrammi.

## 4. Ottimizzazione del processing attivo

- [ ] Profilare prima di intervenire; ordinare i costi misurati per tempo totale
  e contributo al peggior blocco, non solo per numero di chiamate.
- [x] Verificare il beneficio di precalcolare coefficienti dipendenti soltanto da
  sample rate/parametri: Auto, BITE, gain e smoothing. Invalidare correttamente
  le cache su prepare, automazione, ripristino stato e cambio modalità.
- [x] Verificare aggiornamenti crossover: oggi il motore ricalcola ogni 16 campioni.
  Evitare lavoro a parametri stabilizzati senza cambiare la traiettoria durante
  automazione né introdurre dipendenza dalla dimensione dei blocchi.
- [ ] Profilare `exp`, `pow`, conversioni dB e ricostruzione snapshot/ID parametri.
  Prima spostare calcoli invarianti; valutare approssimazioni/SIMD solo dopo
  aver definito dominio, errore massimo e regressioni audio necessarie.
- [ ] Valutare se evitare lavoro duplicato nel detector senza sidechain esterna.
  Non saltare lo stato di bande non ascoltate se influenza il ritorno da SOLO.
- [ ] Confermare assenza di allocazioni, mutex, I/O e chiamate GUI nel callback.
- [x] Per ogni modifica confrontare con la baseline del modello corrente:
  null test allineato, errore massimo/RMS, inviluppi, crossover, stereo e automazione.
  Preferire equivalenza bit per bit ove possibile; se cambia l'ordine numerico,
  dichiarare e verificare la tolleranza anziché chiamarla equivalenza esatta.

## 5. Plugin nap: silenzio sicuro e risveglio

Obiettivo: ridurre fortemente il costo a riposo. **CPU esattamente zero non è
garantibile quando l'host continua a chiamare il plugin**: restano i controlli
minimi per rilevare segnale, sidechain e cambiamenti. La sospensione dell'host
è distinta dal nap interno. GUI chiusa, trasporto fermo, SOLO escluso o tutti
gli IN spenti non bastano da soli a dimostrare che lo stato DSP sia sacrificabile.

- [x] Definire gli stati Active / Draining / Sleeping e le condizioni verificabili
  di entrata/uscita. Partire da silenzio digitale esatto; un eventuale criterio
  sotto soglia richiede una prova separata su segnali debolissimi e gain massimo.
- [x] Ispezionare tutte le memorie: filtri, Auto/R1/R2, BITE, smoothing, meter,
  FIFO spettro e futuri filtri oversampling. Definire cosa deve continuare a
  decadere e quando può essere azzerato senza alterare il successivo attacco.
- [x] Attendere l'esaurimento delle code audio e della memoria rilevante del
  detector. Per stati non azzerabili, mantenere un aggiornamento ridotto oppure
  un avanzamento temporale dimostrato equivalente. Non congelare semplicemente
  la GR: il primo evento dopo il silenzio deve avere la memoria corretta.
- [x] Sidechain attiva con program silenzioso: continuare gli stati necessari;
  il solo silenzio dell'uscita non autorizza il sonno completo.
- [x] Rilevare un nuovo ingresso nello stesso blocco e processare anche il primo
  campione utile; nessun fade-in aggiunto che cancelli il transiente di risveglio.
- [x] Gestire parametri/preset/automazione in sleep e il successivo wake senza
  riprodurre vecchie rampe, picchi, dati FFT o code congelate.
- [x] Rivedere `getTailLengthSeconds()` (ora 2 s) sulla base delle code effettive;
  non confondere la memoria GR senza uscita audio con una coda audio udibile.
- [x] Evitare l'uso di una sospensione che impedisca al callback stesso di
  rilevare il risveglio, salvo un meccanismo host esplicitamente verificato.

Test da creare con confronto nap ON/OFF sul medesimo input:

| ID | Stimolo e verifica |
| --- | --- |
| NAP-01 | Silenzio lungo: ingresso in sleep, zero uscita, costo residuo e nessuna crescita di memoria. |
| NAP-02 | Impulso a ogni posizione nel blocco, dopo pause diverse: primo transiente preservato. |
| NAP-03 | Coda filtro, DC rimossa, release massima, Auto/BITE: nessun taglio o GR obsoleta al wake. |
| NAP-04 | Program zero/key attiva e viceversa, key mono/stereo: stato detector corretto. |
| NAP-05 | Segnali molto deboli, pause brevi, gain massimo, IN/SOLO: niente gating involontario. |
| NAP-06 | Parametri/preset/bande/sample rate/reset durante sleep: stato e smoothing coerenti. |
| NAP-07 | Callback sospesi/ripresi, seek/loop/stop, render offline e blocchi irregolari: nessun click/dropout. |
| NAP-08 | GUI aperta/chiusa: meter scendono, niente picchi o FFT storici alla riapertura. |

## 6. Oversampling: decisione misurata, poi eventuale implementazione

Un compressore può generare componenti aggiuntive tramite modulazione del gain;
la presenza di aliasing significativo in questo motore va misurata. Aumentare
Fs può anche cambiare detector, crossover e suono: non è automaticamente un
miglioramento né una replica più fedele dell'originale.

- [x] Creare `OS-01`: sweep, toni alti e multitoni a frequenze non coincidenti,
  segnale dinamico a forte compressione, transienti; R1/R2/Auto e BITE 1/5/10.
  Aggiungere ratio 1:1 per isolare gli effetti dei filtri di ricampionamento.
- [x] Creare un riferimento ad alto Fs con up/downsampling controllato e filtrato;
  verificare la convergenza fra due rate elevati. Non chiamarlo automaticamente
  ground truth, perché il modello stesso può dipendere da Fs.
- [ ] Misurare alias distinguendolo da armoniche legittime, sideband e leakage FFT;
  riportare energia delle componenti ripiegate, rumore, risposta ampiezza/fase,
  traiettorie GR, picchi, latenza, costo CPU e memoria.
- [ ] Prototipare 1x/2x/4x; usare rate maggiori solo come riferimento o se giustificati.
  Confrontare oversampling completo e parziale, incluso il percorso detector:
  sovracampionare solo il detector non garantisce l'eliminazione degli alias audio.
- [ ] Confrontare filtri FIR/IIR con ascolti allineati e level-matched, registrando
  latenza, ringing/fase e beneficio udibile oltre alle misure.
- [x] Scrivere decisione GO/NO-GO con budget CPU e benefici prima dell'integrazione.
  Se il beneficio è trascurabile o altera troppo il comportamento, non includerlo;
  il NO-GO documentato chiude questa attività senza una feature inutile.

Se GO:

- [ ] Progettare controlli, default 1x compatibile e persistenza/versione dello stato.
  Non cambiare automaticamente qualità nel render offline senza una scelta documentata.
- [ ] Preallocare buffer/stati; ricalcolare coefficienti al Fs interno e allineare
  program/sidechain. Conservare unità temporali in ms, frequenze in Hz e livelli.
- [ ] Gestire cambio fattore e ripristino sessione senza allocazioni nel callback,
  click, output non inizializzato o incoerenze di latenza. Se necessario applicare
  il cambio solo con una riconfigurazione sicura, dichiarandolo nella UI.
- [ ] Dichiarare correttamente la latenza alla DAW e verificare bypass/PDC,
  anche per latenza frazionaria. Aggiornare la descrizione attuale zero-latency.
- [ ] Specificare dove campionare meter/spettro, conservare la ballistics in secondi
  e verificare nap con memoria e code dei filtri oversampling.
- [ ] Creare `OS-02`: impulso e misura PDC; `OS-03`: cambio fattore/preset/bypass;
  `OS-04`: sidechain allineata; `OS-05`: stereo, rate/blocchi e interazione nap;
  `OS-06`: confronto 1x con percorso precedente e regressione alias a 2x/4x.

Riferimento implementativo locale: JUCE `juce_dsp/processors/juce_Oversampling.h`.
L'uso della classe va valutato mantenendo il confine attuale fra DSP puro e wrapper.

## 7. Ottimizzazione GUI senza degradare la risposta

- [ ] Profilare timer, FFT, curve STATIC I/O/LR4, letture parametri, layout e paint.
- [x] Non alimentare inutilmente la FIFO spettro quando non esiste un consumer
  visibile; gestire apertura/chiusura con comunicazione thread-safe senza
  dereferenziare componenti GUI dal thread audio. **Parziale 0.2.3:** completata
  la sospensione della FIFO a editor chiuso; resta il caso editor esistente
  ma nascosto dall'host, da verificare separatamente.
- [ ] Mettere in cache griglie, testo/layout e curve statiche, invalidandoli su
  parametri, scala, dimensione o tema pertinenti.
- [x] Limitare repaint alle aree cambiate; ridurre il lavoro a GUI inattiva dopo
  l'esaurimento delle animazioni, senza lasciare meter o help congelati.
- [x] Conservare salita ai picchi, discesa basata sul tempo reale e paint passivo.
  Un calo del frame rate non deve cambiare ballistics né perdere eventi.
- [x] Non degradare FFT/precisione o focus per risparmiare CPU senza una decisione
  esplicita: restano hover iniziale 30 ms, passaggio immediato, uscita 100 ms.
- [ ] Creare `GUI-PERF-01`: chiusa/aperta ferma/animata/resize e molte istanze;
  `GUI-PERF-02`: confronto traiettorie meter/FFT a clock regolare e irregolare;
  `GUI-PERF-03`: riapertura, focus, editing doppio click e cambio scala/DPI;
  `GUI-PERF-04`: nessuna variazione audio con GUI aperta o chiusa.

### 7.1 Intestazione: versione, aggiornamenti e oversampling

- [x] Mostrare la versione installata vicino a **MBC4**, ricavandola dalla
  versione della build, senza duplicare una stringa da aggiornare a mano.
- [x] Controllare le nuove release pubblicate nel repository GitHub del prodotto.
  Se esiste una versione stabile successiva, mostrare vicino a MBC4, per esempio,
  **0.2.2 → 0.2.3**, con lampeggio dal grigio al giallo della palette esistente.
  In assenza di aggiornamenti mostrare soltanto la versione corrente in grigio.
- [x] Confrontare le versioni numericamente, non come semplici stringhe; escludere
  draft e prerelease dal canale stabile. Un errore di rete non è un aggiornamento.
- [x] Eseguire il controllo fuori dal thread audio e senza bloccare la GUI:
  timeout, cache e intervallo di ricontrollo limitato, condiviso fra istanze.
  Verificare l'accessibilità del repository ai destinatari; non incorporare
  credenziali GitHub nel plugin. Se occorre, predisporre un manifest pubblico
  delle versioni alimentato dalla pubblicazione della release.
- [x] Animare solo l'indicatore visibile, con lampeggio lento da definire e
  verificare nel mockup; nessun repaint continuo dell'intera interfaccia.
  Fermare l'animazione quando l'editor è chiuso o l'intestazione mostra l'help.
  Questa funzione segnala la disponibilità: non installa automaticamente il plugin.
- [ ] **Solo se l'oversampling viene implementato**, aggiungere vicino a
  **PonteDSP** una scritta cliccabile, per esempio **Oversampling x4**.
  Da disattivato deve essere opaca/attenuata ma leggibile; da attivato gialla,
  usando i colori già presenti. Mostrare il fattore configurato effettivo.
- [ ] Collegare il click all'attivazione/disattivazione dell'oversampling e allo
  stesso stato salvato nel progetto. Per più fattori, definire anche la scelta
  del fattore senza confonderla con il toggle. Mostrare lo stato attivo solo
  quando il DSP ha applicato la modifica, rispettando riconfigurazione e latenza.
- [ ] Integrare il controllo nel focus comune, nell'accessibilità da tastiera e
  nell'help contestuale, mantenendo riconoscibili gli stati acceso/spento.

### 7.2 Priorità dell'help nell'intestazione

- [ ] Quando compare l'help, nascondere **PonteDSP, MBC4, versione/avviso di
  aggiornamento e controllo oversampling**, così che l'intera area sia dedicata
  al testo. L'help sostituisce l'intestazione: non si sovrappone alle scritte.
- [ ] Alla scomparsa dell'help ripristinare tutti gli elementi con il loro stato
  corrente, compreso l'avviso di aggiornamento e il fattore oversampling.
  Nascondere un controllo non deve disattivare la funzione audio associata.
- [ ] Disabilitare hit-test e attivazioni dei controlli nascosti senza perdere
  il contesto necessario a completare l'interazione che ha aperto l'help.
  Verificare che la sostituzione non provochi un ciclo mostra/nascondi quando
  il puntatore si trova sul controllo oversampling o sull'indicatore versione.
- [ ] Conservare spazio e leggibilità dell'help al resize minimo e con DPI diversi;
  aggiornare mockup, POC e catalogo CompanyGUI con i due stati dell'intestazione.

Test da aggiungere alla suite GUI e al controllo aggiornamenti:

| ID | Stimolo e verifica |
| --- | --- |
| HEADER-01 | Versione vicino a MBC4 uguale a build/manifest; layout minimo, resize e DPI. |
| UPDATE-01 | Risposte simulate: versione uguale, precedente, successiva, 0.2.9 → 0.2.10, draft/prerelease e dati malformati; avviso solo quando corretto. |
| UPDATE-02 | Offline, timeout, limite richieste, repository non accessibile e più istanze: nessun blocco audio/GUI, cache e richieste limitate. |
| UPDATE-03 | Nuova release: testo corrente → nuova e lampeggio grigio/giallo; animazione sospesa con help o editor chiuso, CPU misurata. |
| HEADER-OS-01 | Se implementato: click/tastiera, stato opaco/giallo, fattore, salvataggio progetto e applicazione DSP coerenti anche con cambio differito. |
| HEADER-HELP-01 | Help sostituisce tutti gli elementi, nessun click sui controlli nascosti; ripristino corretto senza cambiare audio o parametri. |
| HEADER-HELP-02 | Hover/focus dei nuovi elementi, spostamenti rapidi e arrivo asincrono di una nuova versione durante l'help: niente flicker o perdita del testo. |

Usare risposte locali simulate nei test automatici; verificare separatamente
il collegamento reale con GitHub dopo la configurazione della pubblicazione.

## 8. Criteri di accettazione e prove finali

Le soglie seguenti sono **obiettivi iniziali da confermare e congelare dopo la
baseline**, non risultati già ottenuti. Non allentarle dopo un test fallito
senza motivazione documentata.

| Area | Criterio |
| --- | --- |
| Auto | Miglioramento oltre incertezza sulle famiglie indipendenti; nessuna regressione materiale di R1/R2/BITE. Limiti numerici fissati dalle nuove acquisizioni prima del fit finale. |
| Ottimizzazione numerica | Bit-identico dove possibile; altrimenti obiettivo residuo RMS <= -120 dBFS e picco <= -100 dBFS sui test normalizzati, con controllo relativo sui segnali deboli. Nessun click o cambiamento percettivo. |
| Nap | Test transitori/stato superati rispetto a nap OFF; nessuna perdita del primo campione utile. Obiettivo >= 90% di riduzione del costo DSP su silenzio stabile, separato da wrapper e GUI. |
| CPU attiva | Guadagno superiore alla dispersione delle misure; nessuna regressione p99 > 5% senza beneficio di qualità motivato. Fissare il budget assoluto per l'hardware di riferimento. |
| GUI | Costo ridotto misurabile nei casi mirati; stessa leggibilità, interazioni e dinamica visiva, nessun backlog. |
| Oversampling | GO solo con beneficio misurabile e costo/latency accettati; 1x conserva il comportamento precedente. Nuovi default motivati separatamente. |

- [ ] Integrare test deterministici brevi nelle suite DSP/GUI e nella CI; tenere
  benchmark temporali su hardware controllato, senza soglie CPU fragili nei runner condivisi.
- [ ] Eseguire l'intera matrice estesa sul candidato finale, incluso nap + Auto +
  oversampling + automazione + GUI, non solo le feature isolate.
- [ ] Provare in Ableton: live input a trasporto fermo, stop/play/seek/loop,
  sidechain, bypass host, freeze/export offline, salvataggio/riapertura progetto,
  editor chiuso/aperto e più istanze. Registrare dropout e carico massimo sostenibile.
- [ ] Fare ascolti comparativi allineati e a pari livello su voce, basso, batteria
  e mix; includere transienti e passaggi quasi silenziosi. Separare test di
  trasparenza delle ottimizzazioni da preferenza per un nuovo modello Auto.

Comandi base esistenti, dalla root PonteDSP (aggiungere gli eventuali nuovi target):

```powershell
cmake --build build/MC2000-analysis-vs --config Release --target MC2000Tests MC2000UITests PonteMC2000_VST3 --parallel 4
ctest --test-dir build/MC2000-analysis-vs -C Release --output-on-failure
cmake --build build/MC2000-analysis-vs --config Release --target MC2000NextPackAnalysis
```

## 9. Documentazione, release ed eliminazione di questo file

- [ ] Archiviare in Research manifest, script riproducibili, dati di misura,
  benchmark prima/dopo, conclusioni Auto e decisione oversampling.
- [ ] Aggiornare POC, audit DSP, recensione esperto, README, CompanyGUI e note
  release con quanto effettivamente implementato e i limiti ancora aperti.
- [ ] Aggiornare DSP model/schema solo se necessario; verificare migrazione e
  suono delle sessioni precedenti, soprattutto nuovi parametri e default.
- [ ] Assegnare la nuova versione; verificare CMake, risorsa Windows, manifest
  VST3, latenza dichiarata, commit, hash e contenuto del pacchetto.
- [ ] Registrare PASS/FAIL e link ai risultati per ogni ID di test di questo piano;
  per i NO-GO riportare motivazione ed evidenza nei documenti permanenti.
- [ ] Eliminare questo file solo quando le attività applicabili sono chiuse e
  le informazioni utili sono state trasferite. Non cancellare dati o script
  necessari a riprodurre i risultati; eliminare solo temporanei ricostruibili.
- [ ] Build finale, commit/push e pubblicazione quando autorizzati per quella
  release. L'autorizzazione alla v0.2.2 non autorizza pubblicazioni future.
