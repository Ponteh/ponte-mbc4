# MC2000 / Ponte MBC4 — memoria di progetto e recensione esperto

Data: 2026-09-09. Attività: lettura e analisi, nessuna modifica al codice,
nessuna nuova build. Aggiornamento: manuale letto e nuove misure sui render
originali T09/T10, dopo autorizzazione dell'utente ad accedere allo ZIP.
Questa nota conserva il contesto; non certifica il matching completo con v7.

## Richiesta e vincoli dell'utente

- Studiare la documentazione PonteDSP, POC, analisi in Research e manuale v7.
- Indagare una voce ricca di fondamentale: l'originale sembra ottenere la
  stessa stabilità con meno GR indicata dal meter (chiarimento dell'utente).
  Il messaggio iniziale cita release
  di almeno 500 ms; il setup specifico comunicato dopo usa 250 ms.
- Misurare prima la risposta temporale dei meter originali IN/OUT/GR.
  Solo se non è possibile determinarla, usare temporaneamente un controllo
  della velocità, trovare il valore e rimuovere il controllo.
- Proporre riferimenti e soluzioni per una GUI più minimalista. Per ora non
  scrivere codice.
- L'utente ha successivamente autorizzato il download dello ZIP e l'estrazione
  selettiva, chiedendo di liberare lo spazio dopo l'analisi. Autorizzazione
  acquisita: non richiederla nuovamente per questa attività.

## Setup comunicato dall'utente

| Controllo | Valore |
| --- | --- |
| Bande compresse | 2 e 3; le altre disabilitate |
| Crossover | 100 / 785 / 10000 Hz |
| TC | R1 |
| Ratio | 2:1 |
| Knee | 0 |
| Attack | 2.5 ms |
| Release del confronto specifico | 250 ms |
| BITE | 1 |
| Threshold | -27.5 dB |

Si assume che la threshold comunicata valga per entrambe le bande attive.
Non sono ancora noti Input/Output Gain, makeup per banda, LINK, sidechain,
versione esatta dell'originale e build effettivamente ascoltata.
L'utente ha chiarito che «meno compressione» significa **meno compressione
mostrata dal meter GR dell'originale**, a fronte della stabilità vocale
percepita nel confronto. Non significa necessariamente meno attenuazione
effettiva dell'audio e non indica impostazioni meno spinte.
Non inventare i dettagli ancora mancanti. Considerare 500 ms una seconda
condizione di verifica, senza sostituire i 250 ms del setup specifico.

## Contesto PonteDSP da conservare

Riferimento aziendale: `../../../PonteDSP_Gestione_Aziendale_Professionale.md`.

- Patrimonio DSP e logica proprietari separati dal framework; JUCE come
  integrazione per host, formati, GUI e servizi.
- DSP realtime senza allocazioni, lock, I/O o dipendenze dalla GUI; tempi
  espressi in campioni/sample rate, indipendenti dal buffer e dal repaint.
- Identità plugin e ID parametri stabili; schema di stato e migrazioni
  espliciti; compatibilità di preset, automazione e sessioni.
- CMake per prodotto, dipendenze condivise e fissate, test DSP e integrazione,
  corpus di regressione, validazione host e ascolti a livello confrontabile.
- Sistema grafico condiviso: palette, font, spaziature, componenti, resize e
  accessibilità. Estrarre componenti comuni quando c'è un riuso concreto.
- Ricerca distinta dal codice di produzione; distinguere osservazioni,
  ipotesi, modelli e implementazioni. Non trasformare le stime in misure.

Letti anche README degli altri prodotti, specifiche e manuale sorgente di
ReferenceLab, note PhaseComp, `shared/CompanyGUI/README.md` e `tests/README.md`.
Le note storiche di altri prodotti non dimostrano proprietà di MC2000.
La documentazione CompanyGUI propone grafite/viola/lime; la richiesta corrente
di maggiore minimalismo orienta le proposte senza imporre i vecchi ornamenti.

## Materiali MC2000 letti e loro precedenza

- `../README.md`, `../MC2000_Proof_of_Concept.md`, note di licenza e build.
- `BLACK_BOX_FIT.md`, `INTEGRATION_TEST_AUDIT.md`, `NEXT_VALIDATION_AUDIT.md`.
- `BlackBoxAnalysis.cpp`, `ExtendedPackAnalysis.cpp`, `NextPackAnalysis.cpp`.
- `user_manuale_mcdsp_mc2000.md`: letto integralmente dopo che l'utente ne
  ha indicato il percorso; introduzione v7, copyright 2022, corpo con parti
  storiche e alcune incongruenze. Non usare ogni range come contratto v7 provato.
- Tutti i 37 Markdown nello ZIP: README, TEST_SPEC e checklist dei cinque pack.
  Copia testuale e indice conservati in `EXPERT_REVIEW_2026-09-09_DATA/`.
- Codice dei parametri, processor, ballistics, percorso di elaborazione e
  metering, sezioni GUI e test pertinenti.
- `../../../artifacts/MC2000/Release/BUILD_INFO.md`: v0.2.0, 2026-08-29,
  DSP_MODEL_4, schema 2. È un resoconto storico, non un test eseguito oggi.

Il POC dichiara un target NextGen/v6 e contiene sezioni storiche non tutte
allineate agli audit successivi. Il README menziona ancora DSP_MODEL_2, mentre
il codice dichiara modello 4. Gli audit precedenti alla v0.2 non certificano
automaticamente la build corrente o la stessa versione v7 dell'esperto.

La cronologia Git locale non è interamente leggibile: `git log` mostra
`8228b8b Release v0.2.0 GUI and parameter update`, poi fallisce su un oggetto
precedente. Nessun confronto completo prima/dopo è stato possibile.

## Stato delle conoscenze DSP dagli audit

- Crossover LR4 e rete compensata delle bande interne: matching numerico
  riportato molto stretto; non c'è evidenza per cambiare topologia.
- Detector stereo: massimo dei valori assoluti L/R, condiviso fra i canali.
- Knee 0: legge statica standard threshold/ratio. Knee diversi da zero:
  correzioni empiriche Hermite/LUT, irrilevanti nel setup dell'esperto.
- R1: release stretched exponential ricavata da test sintetici. Il POC stesso
  segnala che la generalizzazione a materiale continuo deve essere validata.
- R2: adattamento empirico; costanti del codice più recenti di alcune sezioni
  candidate del POC. Non coinvolto nel setup dell'esperto.
- Auto: indipendenza dai controlli manuali confermata dai 39 confronti del
  Next Pack; la legge completa resta un fallback crest-factor. Non coinvolto
  nel setup dell'esperto.
- BITE: detector fast/slow e relief transiente; il retest Low/Mid corregge
  render storici BITE 5 non affidabili. Nel modello 4 il valore pubblico 10
  raggiunge il relief completo, diversamente dall'ancora storica a 10.
  BITE 1 produce fattore zero: questa rimappatura non spiega il caso riferito.
- Sidechain e automazione: implementazioni funzionali, matching originale
  ancora incompleto. Sample rate: parte dei render ha etichette incongruenti
  con gli header; non dichiarare validata tutta la matrice originale.

## Riscontri sul problema di compressione

Il chiarimento sulla lettura GR rende prioritaria la distinzione fra
attenuazione audio e rappresentazione del meter. I due rilievi dell'esperto
(GR maggiore nell'emulazione e indicatori troppo veloci) potrebbero avere
una causa comune nella misura/visualizzazione, ma non è ancora dimostrato.
Il codice corrente espone il massimo GR di ogni blocco senza ballistics
del display: questo può produrre letture diverse da un meter con altra
integrazione temporale, senza implicare una diversa attenuazione audio.
Non assumere quale integrazione usi l'originale e non correggere la release
o la profondità della compressione per far coincidere soltanto i meter.
Confrontare prima la GR ricavata dall'audio delle bande isolate e la lettura
del display sullo stesso evento; misurare separatamente scala e risposta
temporale. Se resta una differenza nell'audio, proseguire l'indagine DSP.

`Parameters.cpp` passa `releaseMs` dal valore fisico APVTS allo snapshot;
`MultiBandCompressor.cpp` lo passa a `Ballistics`; il tempo viene convertito
in secondi moltiplicando per 0.001. I controlli sono SliderAttachment standard.
Non è emersa una rimappatura erronea evidente nel percorso corrente normale.
Questo non esclude regressioni in vecchi stati, automazione o LINK.

Il gain computer calcola la GR istantanea, poi `Ballistics` filtra la GR in dB.
In R1, se il target supera la GR corrente riparte l'attacco; altrimenti viene
seguita la coda empirica. La fase di attacco può riavviare l'età della release
nel segnale continuo. Il matching di uno step non identifica univocamente
questa risposta su sinusoidi basse, armoniche, sillabe e picchi ripetuti.
È una pista da misurare, non un bug dimostrato e non una giustificazione per
moltiplicare arbitrariamente il parametro Release.

Legge attuale di release isolata: GR(t) = GR0 exp(-(t/(1.779 R))^1.286).
Il POC riporta circa t50 = 1.336 R e t10% = 3.40 R: con R=250 ms sono
circa 334 e 850 ms, con R=500 ms circa 668 e 1700 ms. Sono riferimenti della
coda isolata, non tempi garantiti su una voce continuamente eccitata.

IN disattivato nel codice esclude gradualmente la compressione ma la banda
continua a contribuire alla somma, con il suo gain. Non equivale a SOLO/mute.
Verificare nel manuale v7 e con un render neutro la semantica dell'originale.

## Riscontri sui meter

`MultiBandCompressor.cpp` azzera i massimi a ogni blocco e pubblica il picco
IN/OUT e il massimo della GR tramite semplici store atomici. `BandMeter::paint`
e `OutputMeter::paint` disegnano direttamente questi valori. Timer GUI: 30 Hz.

Conseguenze verificabili dal codice:

- nessuna ballistics separata di salita/discesa o hold;
- i blocchi intermedi possono essere sovrascritti prima che la GUI li legga;
- finestra di picco legata al buffer host, quindi visualizzazione potenzialmente
  diversa al cambiare del buffer;
- il valore visualizzato di GR non è una media calibrata sull'originale.

Non basta abbassare gli FPS. Proposta futura: misura temporale in un componente
meter separato, conservazione corretta dei picchi fra gli aggiornamenti,
risposta IN/OUT e GR misurata separatamente, trasferimento lock-free e repaint
indipendente. Qualunque smoothing visuale deve lasciare identico l'audio.

Il manuale letto non contiene tempi numerici dei meter v7. Descrive IN/OUT
selezionabili, una GR per banda, clip con reset e scale IN/OUT -60..0 dB,
GR 0..60 dB (sezioni Metering e Specifications); le scale correnti del plugin
sono diverse. Il pack v7 contiene T06_Meter_Impulse e T06_Meter_LevelSweep,
ma soltanto stimoli e una procedura: nessun risultato compilato o video.
Anche lo ZIP annidato nel primo pack contiene soltanto WAV. Non è quindi
possibile ricavare da questo archivio una velocità numerica dei meter.

Gli audio WAV da soli non identificano la loro animazione. Se il manuale non
la specifica, acquisire il display dell'originale durante segnali noti:
gradini/burst IN/OUT senza compressione e GR con portante bassa residua dopo
un evento forte. Confrontare livelli, hold e discese e dichiarare la risoluzione
temporale della cattura. Solo se non ricavabile, controllo temporaneo interno
di calibrazione, da rimuovere prima della versione finale, senza ID audio nuovo.

## Accesso ai dati e nuove misure

Lo ZIP da 343154828 byte è ora leggibile: il precedente errore sulla directory
centrale non dimostra un archivio corrotto. Indice: 2959 voci, di cui 2866 WAV,
37 Markdown, 21 CSV, uno ZIP annidato e 34 directory; circa 9.7 GiB non
compressi, esclusa l'espansione dello ZIP annidato. Sono stati estratti solo
15 WAV T09/T10 del primo pack (27310706 byte), oltre ai documenti piccoli.
Risultati, provenienza SHA256 e indice sono nella cartella
`EXPERT_REVIEW_2026-09-09_DATA`; rimossi tutti i WAV e documenti temporanei
(28012313 byte). Lo ZIP è tornato online-only tramite `attrib -p +u`:
attributi finali 5248544 e nessuna estensione dati allocata secondo
`fsutil file queryextents`. L'archivio remoto è conservato; i risultati
testuali occupano meno di 1 MiB.

**Distinzione essenziale nelle specifiche:** T09 del primo pack è R1, ratio
10:1, threshold -24, knee 0, BITE minimo, crossover 1 kHz, LOW Solo secondo
README; i nomi dei render annotano attack 2.5 ms. T10 usa ratio 10:1 e release
1000 ms. Il pack aggiuntivo `03_Dynamics_Validation` annota invece **Type-2**
per T03-Release, con ratio 2:1 e attack 2.5 ms. Non è una validazione R1 a
ratio 2:1. Anche Attack e Knee di quel gruppo sono annotati Type-2.

Metodo: eseguibile esistente `MC2000BlackBoxAnalysis --trace-gain`, rapporto
RMS uscita/ingresso del canale sinistro in finestre non sovrapposte di 10 ms.
Header verificati: stereo IEEE float32, 48 kHz, 6 s per T09 e 3.9 s per T10.
È una misura dell'audio dei riferimenti, non una lettura dei meter né un
render nuovo del DSP corrente. L'eseguibile del 28 agosto viene utilizzato
solo nella modalità di lettura WAV, senza il suo confronto col modello DSP.

T09: portante 100 Hz, -40 dBFS peak prima di 2 s, -6 fra 2 e 4 s, -40 dopo
4 s. Baseline 1.50..1.95 s e plateau 3.50..3.95 s; frazioni della GR in dB.
La prima finestra 4.00..4.01 s contiene il transiente del crossover: a R250
indica circa 4.92 dB contro circa 15.65 nella finestra seguente. La ricerca
del recupero inizia pertanto a 4.02 s, mantenendo t=4 s come origine.

| Release UI | GR plateau | Tempo GR al 50% | Tempo GR al 10% | Formula corrente: 50% / 10% |
| --- | --- | --- | --- | --- |
| 50 ms | 15.818 dB | circa 70 ms | circa 170 ms | 66.9 / 170.1 ms |
| 100 ms | 15.956 dB | circa 130 ms | circa 340 ms | 133.8 / 340.3 ms |
| 250 ms | 16.058 dB | circa 330 ms | circa 850 ms | 334.5 / 850.7 ms |
| 500 ms | 16.100 dB | circa 670 ms | circa 1700 ms | 668.9 / 1701.4 ms |
| 1000 ms | 16.125 dB | circa 1340 ms | oltre il file | 1337.8 / 3402.8 ms |

I tempi misurati sono l'inizio della prima finestra che supera la frazione:
risoluzione 10 ms, senza interpolazione. La formula è confrontata nella sola
coda, normalizzata alla GR originale misurata, non alla GR prodotta dal plugin.
Tra 4.02 e 5.99 s, usando il centro delle finestre, errore RMS della coda
normalizzata 0.130 dB a R250 e 0.183 dB a R500; massimo circa 0.46 dB.
Questo sostiene la scala dei tempi per il test isolato, non un matching
completo, specialmente su voce, ratio 2:1 e bande interne.

**Limite dell'analizzatore storico:** `measureFractionTime` cerca già da 4 s
e accetta il primo attraversamento. Sullo step T09 il transiente può far
restituire un t50 spurio di zero. Il codice non è stato modificato; questa
analisi usa le tracce con la finestra iniziale esclusa. Prima di futuri fit,
correggere il metodo di misura o confrontare con un riferimento neutro
filtrato, evitando di scambiare il transiente con la release.

T10: in R1 gli eventi secondari a -20 e -15 dBFS producono la stessa GR
misurata nelle finestre finali dell'evento (circa 12.306 dB) e dopo 2 s
(circa 11.511 dB); a -10 dBFS la GR è maggiore (12.537 e 11.827 dB).
In R2 anche -20 e -15 danno traiettorie distinte: a fine evento circa
11.668 e 11.866 dB. Le finestre riassuntive evitano le discontinuità di
livello. Il riscontro è coerente con la distinzione R1/R2 del manuale:
non identifica da solo l'intero algoritmo e non sostituisce il caso vocale.

## Altri riscontri dal manuale

- R1: eventi sotto il livello della release non ne modificano il decorso.
  R2: i nuovi segnali influiscono anche sotto quel livello. Auto ignora i
  controlli manuali di attack/release.
- LINK: la sezione dettagliata e la tabella finale descrivono un collegamento
  relativo nella scala dei controlli, esplicitamente senza offset fisso in
  millisecondi per attack/release. Il codice attuale somma invece offset in
  unità fisiche in `Parameters.cpp`. È una differenza documentale concreta,
  da verificare sulla GUI v7; non attribuirle il caso dell'esperto senza
  sapere se usava LINK e quali valori effettivi avessero le bande slave.
- Il manuale prevede che il TC/Auto di una slave modificata si sganci dal
  master; il codice corrente sovrascrive il TC delle slave col master.
- Il manuale mescola istruzioni storiche: una frase del Quick Tour nega il
  linking, poi lo descrive in dettaglio; Attack è tabellato 0.25..25 ms ma
  altri materiali riportano valori maggiori; BITE è tabellato 1..50.
  Il pack v7 stesso richiede verifiche sulla GUI reale. Non cambiare i range
  sulla base di queste sole discrepanze.

## Dati ancora necessari per chiudere il caso

Manuale, specifiche e render storici sono ora disponibili: non richiederli
di nuovo. Mancano nel corpus un confronto vocale e una matrice R1 con il
setup preciso dell'esperto, nonché una cattura temporale dei meter originali.

Passo mirato: breve voce dry più render originale/emulazione a livelli
documentati, setup della tabella, prima 250 ms e poi 500 ms. Controllare LINK,
gain, sidechain, versione/build. Per distinguere la dinamica dalla somma:
render aggiuntivi SOLO banda 2 e SOLO banda 3 con lo stesso routing.
Come test sintetico complementare usare una portante residua bassa dopo
burst/picchi ripetuti, R1 ratio 2:1 e frequenze nella banda 2 e vicino al
crossover 100 Hz. Non serve ripetere indiscriminatamente tutti i pacchetti.

Per i meter: video a frame rate noto di T06 Impulse/LevelSweep con IN/OUT
neutri, più un evento di compressione con portante residua per la GR.
Documentare hold, salita e discesa separatamente per livello e GR, con
buffer 32/256/1024/2048. La release audio va separata dalla ballistics del
display. Solo se la misura resta impraticabile, ricorrere al controllo
temporaneo di calibrazione richiesto dall'utente.

Prima leggere le TEST_SPEC; i soli nomi WAV non definiscono il setup completo.
Se mancano casi pertinenti, definire render controllati senza inventare il
contenuto dei pacchetti. Confrontare le bande isolate e poi la somma, tenendo
conto della fase del crossover: il semplice rapporto campione-per-campione fra
voce dry e uscita multibanda non restituisce la GR delle singole bande.

## Proposte GUI da discutere, non implementate

Direzione consigliata: quattro righe compatte, tutte leggibili simultaneamente.
Fondo grafite uniforme; numeri chiari; colore di banda usato per identificatore,
curve e stato, senza bordi colorati ovunque. Spaziatura al posto dei separatori
viola; lime riservato a pochi stati attivi. Valori numerici ben più leggibili
delle attuali scale meter da 5.5–7 punti.

Grafico crossover principale; curva statica comprimibile o vista alternativa.
Threshold e GR hanno priorità visiva. Attacco e release adiacenti; stessa griglia
per ogni banda; IN/SOLO sempre azionabili. Scale meno ripetute, lettura numerica
della GR; valutare una scala visiva 0–12/24 dB per compressioni moderate senza
cambiare i limiti del DSP. La curva statica non deve sembrare una misura della
GR dinamica.

Alternativa più essenziale: panoramica con meter di tutte le bande e controlli
dettagliati della banda selezionata. Occupa meno spazio ma rende meno immediato
confrontare e modificare bande 2 e 3; per questo il primo approccio è preferito.

Implementazione futura: componenti di strip/meter riutilizzabili, palette e
tipografia centralizzate, disegno vettoriale JUCE, layout per 2/3/4 bande,
aggiornamenti limitati alle zone dinamiche. Il CMake attuale di MC2000 usa GUI
JUCE diretta e copie locali di Palette/LookAndFeel; Foleys è nel POC, non nella
build MC2000 corrente. Non è necessario cambiare framework per semplificare
la UI. Conservare ID, range e valori dei parametri durante il redesign.

Riferimenti ufficiali da studiare (valutazione estetica proposta):

- [FabFilter Pro-MB](https://www.fabfilter.com/products/pro-mb-multiband-compressor-plug-in):
  gerarchia del grafico e interazione delle bande; la topologia non è quella MC2000.
- [TDR Nova GE](https://www.tokyodawn.net/tdr-nova-ge/): EQ dinamico, utile per
  combinazione grafico/knob, ordinamento e leggibilità dei controlli.
- [DMG Multiplicity](https://dmgaudio.com/dl/DMGAudio_Multiplicity_Manual.pdf):
  organizzazione per banda e accesso ai dettagli; più tecnico e denso.
- [Softube Weiss Compressor/Limiter](https://www.softube.com/user-manuals/weiss-compressor-limiter):
  riferimento per sobrietà e display alternabili, senza assumere equivalenza
  funzionale con un MC404 a quattro bande.
- [McDSP MC2000](https://mcdsp.com/plugin-index/mc2000/): pagina ufficiale
  consultata per le funzioni correnti; non sostituisce il manuale v7 richiesto.

Nessuna velocità definitiva dei meter e nessuna correzione DSP sono state
scelte: i riferimenti storici sono stati analizzati, ma resta necessario il
confronto controllato nel setup dell'esperto e la cattura dei meter originali.
