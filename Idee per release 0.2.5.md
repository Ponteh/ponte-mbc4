# Idee e piano di lavoro — release 0.2.5

## Le tre idee

- Possibilità di lavorare con crossover a fase lineare.
- Disegno dei crossover attinente alla realtà.
- Possibilità di lavorare in Stereo o Dual mono.

Piano redatto il 04/10/2026 dopo l'ispezione del codice. Le funzionalità qui
descritte **non sono ancora implementate**. Le soglie indicate sotto sono
obiettivi proposti da validare, non risultati già ottenuti.

## 1. Punto di partenza e vincoli

- `Source/DSP/CrossoverNetwork.*`: rete IIR Linkwitz-Riley di quarto ordine,
  2/3/4 bande, con compensazioni all-pass. Va conservata come modalità predefinita.
- `Source/DSP/Biquad.h` e `LinkwitzRiley4.h`: filtri digitali dipendenti dal sample
  rate. La risposta grafica in `getBandMagnitudeDb()` usa invece formule basate
  sul rapporto frequenza/crossover, senza la stessa trasformazione digitale.
- `Source/PluginEditor.cpp`, `CrossoverPlot`: disegna le bande e calcola
  `inputResponseDb()` sommando magnitudini; serve una valutazione complessa della
  rete per rappresentare anche fase, compensazioni e combinazioni di bande.
- `Source/DSP/MultiBandCompressor.*`: il detector Stereo usa il massimo dei
  moduli L/R, con un solo stato Ballistics e BITE per banda e GR comune ai canali.
- `Source/Parameters.*`: `stateSchemaVersion = 2`; `global.linkMaster` collega
  parametri **tra bande**, non è il collegamento Stereo L/R.
- `Source/PluginProcessor.*`: wrapper audio, stato/preset e sidechain; la coda
  dichiarata è attualmente fissa a 2 secondi. La nuova latenza va gestita qui. Il plugin dichara correttamente la latenza alla DAW in qualsiasi modalità.
- La suite corrente include DSP, GUI, NapValidation e, su Windows,
  RealtimeMatrix: minimi 4 Windows / 3 Linux / 3 macOS. La verifica del 04/10
  ha completato il VST3 Windows e 4/4 test da checkout senza `Research`.

Vincoli comuni: mantenere ID e semantica dei parametri esistenti; niente
allocazioni, deallocazioni pesanti, lock, I/O o progettazione FIR nel callback;
nessuna dipendenza dei test obbligatori da `Research` o registrazioni esterne.
Non introdurre oversampling: è fuori dall'ambito di queste tre idee.

## 2. Decisioni da fissare prima del nuovo DSP

- [ ] **Dual mono — proposta:** due compressori L/R indipendenti nella dinamica,
  ma con gli stessi controlli di banda. Non significa pannelli e preset L/R
  separati, né elaborazione Mid/Side. Se si desiderano parametri L/R indipendenti,
  confermarlo prima: cambiano stato, UI, link tra bande e dimensione del lavoro.
- [ ] **Crossover — proposta:** scelta globale `IIR / Linear Phase`; IIR resta
  il comportamento dei vecchi preset e conserva latenza zero. Per Linear Phase
  partire con un solo profilo qualitativo, senza aggiungere subito un menu qualità.
- [ ] **Latenza — proposta:** fissa per sample rate/profilo in Linear Phase,
  indipendente dalle frequenze e dal numero di bande. Mostrare campioni e ms.
  Il cambio IIR/Linear Phase non è inizialmente automatizzabile: applicarlo solo
  con una procedura di riconfigurazione sicura e verificata negli host.
- [ ] **Grafico — proposta:** mostrare la risposta lineare dei crossover;
  eventuali curve con gain, IN e SOLO devono essere chiaramente distinte.
  Non presentare una curva statica come risposta esatta della compressione,
  che dipende dal segnale e dalla sua storia.
- [ ] Concordare CPU, RAM, latenza massima, pendenza/banda di transizione e
  attenuazione richieste per il FIR. Alle frequenze basse un FIR stretto può
  richiedere molti coefficienti: se il costo non è accettabile, fermarsi al
  prototipo e approvare un compromesso, senza degradazioni silenziose.

## 3. Ordine di implementazione

Procedere in piccoli cambiamenti verificabili:

1. Baseline, migrazione stato e specifiche condivise.
2. Risposta grafica corretta per la rete IIR esistente, senza cambiarne l'audio.
3. Stereo/Dual mono sulla rete IIR, isolando le regressioni della dinamica.
4. Prototipo e verifica quantitativa del crossover FIR.
5. Integrazione Linear Phase: latenza, sidechain, cambi di configurazione e nap.
6. Grafico FIR, UI definitiva, matrice combinata e build delle tre piattaforme.

Non sviluppare tutte le modalità contemporaneamente. Riutilizzare toolchain e
JUCE già disponibili; eseguire prima i test mirati e solo alle milestone la
suite completa. Le build lunghe devono salvare log, stato finale e provenienza
dei sorgenti, senza polling continuo e senza essere scambiate per build riuscite.

## 4. Fase A — baseline, stato e infrastruttura

- [ ] Conservare una baseline 0.2.3 riproducibile: commit, configurazione, hash e
  risultati. Salvare preset di prova vecchi, inclusi stati senza i nuovi campi.
- [ ] Aggiungere a `GlobalParameters` enum tipizzati per modalità crossover e
  canali. Proporre ID APVTS stabili `global.crossoverMode` e
  `global.channelMode`, senza rinominare né riordinare gli ID esistenti.
- [ ] Aggiornare `createLayout()` e `SnapshotReader`: risolvere i nuovi puntatori
  atomici alla costruzione, non cercare parametri tramite stringhe in audio.
- [ ] Definire migrazione esplicita verso schema 3: stato vecchio => IIR + Stereo;
  inserire i default mancanti anche quando il preset viene caricato in un'istanza
  già impostata su Linear Phase/Dual mono. Validare enum fuori range e stato
  incompleto. Versionare il modello DSP quando cambia realmente il suo contratto.
- [ ] Separare configurazione richiesta, configurazione attiva e versione della
  risposta grafica. Predisporre snapshot coerenti per la GUI; non leggere filtri
  mutabili dell'audio thread durante il disegno.
- [ ] Aggiungere round-trip save/load, apertura vecchi preset, undo/redo e
  ripristino sessione ai test. Nessun incremento di versione dichiarato completo
  prima della validazione e dell'aggiornamento di CMake/documentazione.

Criterio di uscita: vecchi preset e modalità IIR + Stereo riproducono la baseline;
i nuovi parametri sono persistenti e non modificano il DSP finché non attivati.

## 5. Fase B — disegno fedele dei crossover

### Implementazione

- [ ] Estrarre o condividere una funzione pura di progettazione dei coefficienti
  in `Biquad.h`, mantenendo invariato il percorso audio. Valutare la risposta
  complessa digitale `H(z)` sugli stessi coefficienti, sample rate e frequenze
  effettivamente applicate al DSP.
- [ ] In `CrossoverNetwork` ricostruire i prodotti dei filtri di ogni ramo e le
  compensazioni all-pass delle reti a 2/3/4 bande. Per la somma usare
  `abs(sum(H_banda))`, non assumere in generale `sum(abs(H_banda))`.
- [ ] Condividere clamp e ordinamento delle frequenze. Tenere conto del limite
  effettivo `0.45 * sampleRate` dei filtri attuali e del limite di Nyquist del
  grafico. Non mostrare una frequenza realizzata diversa da quella del motore
  senza segnalarlo; coprire anche crossover molto ravvicinati e sample rate bassi.
- [ ] Aggiornare `CrossoverPlot` e `inputResponseDb()` senza duplicare formule.
  Conservare il significato dello spettro di ingresso rispetto alla curva di
  uscita/solo: definire cosa rappresenta ciascun overlay prima di applicare i gate.
- [ ] Calcolare/cacheare le curve quando cambia la configurazione, non a ogni
  pixel di ogni repaint. Durante lo smoothing usare una vista coerente dello
  stato attivo, oppure indicare esplicitamente che si mostra la destinazione.
  Lo stesso vale durante il caricamento di un nuovo kernel FIR.

### Verifica

- [ ] Impulsi e sinusoidi generati nei test, compressione a rapporto 1:1 e gain
  unitari: confrontare risposta misurata e curva calcolata, banda per banda e
  nella somma. Includere combinazioni IN/SOLO previste dall'overlay.
- [ ] Obiettivo iniziale: errore grafico <= 0.1 dB sopra il floor di -80 dB;
  sotto il floor confrontare errore assoluto, non dB instabili vicino agli zeri.
  Usare durata/risoluzione sufficienti per 20 Hz e compensare la latenza nei test.
- [ ] Testare 44.1/48/96/192 kHz, 2/3/4 bande, estremi e frequenze vicine a Nyquist;
  UI ridimensionata, drag dei crossover, inserimento numerico e automazione.

Criterio di uscita: la nuova GUI concorda con misure indipendenti del filtro;
il rendering audio IIR precedente non cambia. Non usare la sola stessa formula
in produzione e nel test come prova di correttezza.

## 6. Fase C — Stereo / Dual mono

### DSP e sidechain

- [ ] Conservare in Stereo la legge attuale: massimo dei detector L/R, stato
  dinamico comune per banda e identico guadagno applicato a entrambi i canali.
- [ ] In Dual mono separare **tutta** la memoria dinamica L/R per banda:
  Ballistics, BITE, detector e ogni eventuale smoothing della riduzione di gain.
  Verificare le classi prima di assumere che basti duplicare il valore di GR.
- [ ] Mantenere comuni crossover e parametri nella proposta iniziale. Non
  confondere `linkMaster`, IN e SOLO tra bande con il collegamento dei canali.
- [ ] Definire sidechain esterna: stereo => L controlla L e R controlla R in
  Dual mono; mono => lo stesso key pilota entrambi, quindi può produrre GR uguali
  pur con stati indipendenti. In Stereo conservare l'aggregazione attuale.
  In assenza di key usare il programma corrispondente; il key non va mai in uscita.
- [ ] Gestire input mono come singolo percorso valido. Non accedere al canale
  destro assente e non riutilizzare stati residui di un precedente layout stereo.
- [ ] Progettare il cambio modalità: inizializzazione/trasferimento degli stati
  e breve transizione controllata, senza reset bruschi. Misurare il costo di
  mantenere caldi percorsi paralleli prima di sceglierlo; non raddoppiare CPU
  per default senza una necessità dimostrata.
- [ ] Aggiornare nap: dormire solo quando tutti gli stati L/R e tutte le code
  sono quieti; risvegliare anche su segnale solo destro, key o cambio modalità.

### UI, meter e test

- [ ] Aggiungere selettore Stereo/Dual mono e indicazione inequivocabile dello
  stato attivo. Per Dual mono preferire GR L/R leggibili; se si mantiene un solo
  meter, dichiararne l'aggregazione e rendere verificabili i due valori separati.
- [ ] Test decisivo: forte segnale solo L e segnale debole R. In Dual mono la
  dinamica di R deve coincidere con il suo riferimento mono indipendente; in
  Stereo deve seguire la legge di collegamento esistente.
- [ ] Confrontare ogni canale Dual mono con un'istanza mono di riferimento;
  verificare scambio L/R, segnali identici, opposta polarità, key mono/stereo,
  tutti i modi TC e BITE, cambi modalità e ripristino preset.
- [ ] Inserire i casi principali in `MC2000Tests`, GUI e `NapValidation`;
  estendere la matrice realtime a entrambe le modalità.

Criterio di uscita: nessuna interazione dinamica indesiderata fra i canali in
Dual mono con detector indipendenti; Stereo resta compatibile con la baseline.

## 7. Fase D — crossover a fase lineare

### D1. Prototipo verificabile prima dell'integrazione

- [ ] Progettare FIR reali simmetrici con lunghezza dispari e ritardo comune.
  Una struttura da valutare usa low-pass cumulativi `L1, L2, L3`, tutti allineati
  al medesimo impulso ritardato `deltaD`:
  - 2 bande: `L1`, `deltaD - L1`;
  - 3 bande: `L1`, `L2 - L1`, `deltaD - L2`;
  - 4 bande: `L1`, `L2 - L1`, `L3 - L2`, `deltaD - L3`.
  La somma dei kernel deve essere `deltaD`. Questa proprietà non garantisce da
  sola pendenza, attenuazione o forma desiderata delle singole bande: misurarle.
- [ ] Non promettere la stessa forma del LR4 IIR senza verificarla. Definire
  risposta desiderata, larghezza delle transizioni, ripple e attenuazione;
  come punto di partenza valutare ripple <= 0.1 dB e stopband >= 60 dB, solo
  nelle regioni dove una stopband è definita. Concordare eventuali revisioni.
- [ ] Misurare costo e memoria a 20 Hz, crossover ravvicinati e 192 kHz. Definire
  la distanza minima sostenibile o una politica esplicita per bande strette;
  non nascondere clamp aggiuntivi rispetto all'IIR.
- [ ] Confrontare FIR diretto e convoluzione partizionata. Scegliere dopo misure
  di worst-case callback, non solo CPU media. Nessun valore definitivo di taps,
  partizione o latenza finché questo passaggio non è concluso.

### D2. Integrazione realtime

- [ ] Introdurre un backend separato, per esempio `LinearPhaseCrossover`, con
  contratto comune per preparazione, reset, processamento, latenza, coda e
  quiete. Se serve elaborazione a blocchi, adattare l'interfaccia senza alterare
  il ramo IIR sample-by-sample e senza dipendere dal block size dell'host.
- [ ] Preallocare buffer audio/key, storia, partizioni e spazio delle transizioni
  in preparazione. Gestire blocchi variabili e più grandi del valore previsto
  tramite chunking; niente resize nel callback.
- [ ] Progettare i kernel fuori dall'audio thread. Accorpare richieste ravvicinate
  dell'automazione, scartare risultati superati e pubblicare configurazioni
  complete/versionate. Non liberare il vecchio kernel sul thread audio.
- [ ] Usare la storia d'ingresso per inizializzare il nuovo percorso, allineare
  i ritardi e applicare una transizione controllata fra kernel. Non ricostruire
  un FIR ogni 16 campioni come avviene per i coefficienti IIR attuali.
- [ ] Allineare programma e detector esterno con la stessa rete/ritardo previsto
  dal progetto. Non introdurre lookahead accidentale, doppio ritardo del key o
  compressione attivata su un tempo diverso da quello dichiarato.
- [ ] Misurare la latenza totale: `(N-1)/2` vale per il solo FIR simmetrico; una
  convoluzione a blocchi può aggiungere altro ritardo. Riportare all'host quello
  effettivo tramite `setLatencySamples` e verificarlo con impulsi.
- [ ] Definire una procedura host-safe per il cambio IIR/Linear Phase. Una
  semplice crossfade non risolve una modifica della compensazione di latenza.
  Durante playback mantenere la modalità attiva e mostrare la richiesta pendente
  se non è possibile una riconfigurazione sicura. Il solo transport stop non
  garantisce che ogni host chiami nuovamente `prepareToPlay`.
- [ ] Gestire bypass del plugin, compensazione dry e coda dichiarata all'host;
  verificare separatamente il bypass nativo della DAW. Aggiornare il valore fisso
  di `getTailLengthSeconds()` secondo il nuovo contratto, senza confondere la
  memoria del detector con una coda audio udibile.
- [ ] Estendere nap a storia FIR, output differito, code e transizioni. Silenzio
  all'ingresso non significa che non restino campioni da emettere. Verificare
  anche che il risveglio conservi la latenza e non perda il primo transiente.

### D3. Grafico e criteri di uscita

- [ ] Estendere il modello di risposta della fase B usando i kernel realmente
  attivi. Nessuna FFT pesante nel paint o nel callback; cache fuori da entrambi.
- [ ] A rapporto 1:1 e gain unitari, somma delle bande equivalente all'ingresso
  ritardato: obiettivo iniziale errore massimo <= 1e-6 per segnali normalizzati,
  verificato anche con partizionamento e blocchi irregolari.
- [ ] Misurare simmetria dell'impulso e ritardo di gruppo costante nelle regioni
  utili; non stimare fase in corrispondenza di zeri/nulli numerici.
- [ ] Testare click/transienti di cambio kernel e interruzioni host. Definire
  soglie quantitative del residuo e del tempo di assestamento prima di accettare
  il risultato; affiancare ascolto su transienti e bassi per il pre-ringing.

Criterio di uscita: ricostruzione, fase, PDC, sidechain, bypass, nap e budget
realtime verificati. La dicitura “fase lineare” riguarda il crossover, non rende
lineare o a fase lineare l'intero compressore tempo-variante.

## 8. Matrice di regressione e gate della release

- [ ] Incrociare IIR/Linear Phase × Stereo/Dual mono × 2/3/4 bande × input mono/
  stereo; key assente/mono/stereo, IN/SOLO, TC R1/R2/Auto e BITE.
- [ ] Usare 44.1/48/96/192 kHz e blocchi 1/17/64/512/2048, sequenze irregolari,
  zero campioni dove ammesso e blocchi oltre la dimensione nominale.
- [ ] Coprire silenzio, impulsi a diverse posizioni, segnali minuscoli, livelli
  estremi, input non finiti, cambi frequenze/bande/modo, reset e nuovo sample rate.
- [ ] Nap on/off deve essere equivalente a parità di modalità e latenza; nessun
  sonno anticipato durante code FIR, release lunga o transizioni pendenti.
- [ ] Verificare assenza di allocazioni/lock/I/O nel callback, limiti di memoria,
  deadline del callback e costo con GUI aperta/chiusa e automazione intensa.
  I benchmark restano in `Research`; le asserzioni di correttezza in `Tests`.
- [ ] Se si creano nuovi target CTest obbligatori, aggiornare insieme CMake,
  `.github/workflows/windows.yml`, `ci/release/MC2000.json`, helper Unix e minimi
  della suite. Non ridurre i minimi per aggirare un test mancante o fallito.
- [ ] Ripetere una build da checkout nuovo senza `Research` né archivi audio.
  Registrare commit/patch, hash degli input, log, JUnit e configurazione.
- [ ] Verificare Windows e Linux x86_64 e macOS Universal. Sul Mac Intel eseguire
  i test x86_64 e controllare entrambe le slice con `lipo`; l'esecuzione ARM64
  richiede un Apple Silicon reale o un runner appropriato, non è provata dal
  solo cross-build. Segnalare questo gate come aperto se l'hardware manca.
- [ ] Prova in DAW: preset 0.2.3, riapertura sessione, automazione, PDC su tracce
  parallele, bypass, rendering offline e cambio modalità. Registrare host e
  versione testati; non affermare compatibilità universale dopo una sola DAW.

## 9. Consegne e chiusura

- [ ] Un cambiamento/commit logico per ciascuna fase, con test associati e nessuna
  modifica estranea; conservare la baseline prima dei refactor.
- [ ] Aggiornare `MC2000_Proof_of_Concept.md`, README e documentazione UI con
  semantica Dual mono, differenza tra link di bande e canali, latenza, pre-ringing,
  limiti del cambio modalità e misure effettivamente raccolte.
- [ ] Aggiornare versione prodotto e release notes a 0.2.4 solo nella fase di
  confezionamento; includere migrazione dello stato e limiti ancora aperti.
- [ ] Registrare qui gli esiti di ogni fase, distinguendo implementato, compilato,
  testato e da verificare. Nessuna pubblicazione finché i gate richiesti non sono
  passati o finché eventuali esclusioni non sono approvate esplicitamente.

**Prima azione alla ripresa:** confermare le scelte della sezione 2, fissare la
baseline e implementare la fase B sul solo IIR. Non iniziare dal FIR dentro
`processBlock`: è la parte con più rischi e dipende dalle specifiche precedenti.
