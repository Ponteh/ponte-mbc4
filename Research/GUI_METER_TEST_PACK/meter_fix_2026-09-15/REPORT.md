# Correzione meter — Ponte MBC4 0.2.2

La correzione conserva i massimi audio tra aggiornamenti GUI e applica una
dinamica visiva dedicata. Riguarda IN/OUT/GR delle quattro bande e MAIN OUTPUT
stereo. Il percorso audio, i parametri di compressione e DSP_MODEL_4 restano
invariati. Non introduce un parametro di velocità da regolare a orecchio.

## Implementazione e motivazione

Prima, `publishMeters()` sovrascriveva il massimo del blocco precedente:
con buffer 512 a 48 kHz arrivano circa tre blocchi in un aggiornamento GUI
da 33 ms. Un burst poteva sparire prima che il display lo leggesse.
Ora ogni canale ha un accumulatore atomico di massimo, consumato e azzerato
dal timer. Il producer audio non alloca memoria e non usa mutex. Le letture
grezze per blocco restano disponibili agli strumenti di analisi.

Il timer conserva i valori visualizzati; `paint()` li disegna senza consumare
picchi o avanzare lo smoothing. Il tempo è quello effettivamente trascorso,
non un decremento fisso per repaint. Anche le bande nascoste vengono lette.
Alla riapertura si scartano i massimi raccolti a editor chiuso; quando la DAW
sospende i callback, i meter scendono verso il fondo senza bloccarsi sull'ultima
lettura. Un reset del motore svuota anche letture grezze e accumulatori.

| Meter | Salita | Ritorno |
| --- | --- | --- |
| IN, OUT, MAIN OUTPUT | Massimo disponibile al prossimo tick GUI | Rampa 14.3 dB/s seguita da un polo, tau 130 ms |
| GR | Massimo disponibile al prossimo tick GUI | Un polo sulla GR in dB, tau 150 ms |

Le scale grafiche rimangono quelle Ponte. Non vengono sottratti dB alla GR:
uno smoothing a guadagno unitario conserva un plateau costante. I marker
STATIC I/O continuano a usare le letture grezze; non consumano gli accumulatori.

## Evidenza e limiti del modello

Il modello di livello proviene dalle discese A1/315 Hz, con verifica separata
a 2 kHz (RMSE circa 0.16 dB nel rapporto originale). Con i coefficienti
arrotondati l'intervallo 10–90% è circa 441 ms per uno step da 6 dB e 714 ms
per 12 dB, prima della quantizzazione dei frame. Non si rivendica l'identità
di ogni fotogramma o l'identificazione del codice McDSP.

`validate_meter_model.py` confronta gli intervalli normalizzati GR dei video
con una simulazione che usa la GR derivata dagli export su finestre RMS da
10 ms. Non sono i valori interni del detector, né audio sincronizzato a OBS.
Il test 03 orienta la scelta; il test 04 verifica il parametro separatamente.
La ricerca a passi di 10 ms trova 160 ms sul 03; si adotta **150 ms**, entro
l'incertezza video, evitando di attribuire precisione eccessiva al fit.

| Intervalli GR 90→50% e 90→10% | Surrogato senza smoothing: RMSE | Con tau 150 ms: RMSE |
| --- | ---: | ---: |
| 03, tre eventi | 83 ms | 13 ms |
| 04, tre eventi, verifica separata | 89 ms | 18 ms |

La simulazione restituisce circa 870 ms fra 90 e 10% della GR contro
867–900 ms nei video originali. Risultati completi: `model_validation.json`.
Questa è una verifica del candidato matematico, **non una nuova cattura del
plugin**. La differenza di plateau GR e la verifica R500/voce restano aperte.

## Perché un meter ha una propria dinamica

MC2000 nasce come software che emula diversi compressori vintage e moderni;
non è presentato da McDSP come replica di un singolo compressore hardware
chiamato MC2000. [Descrizione ufficiale](https://mcdsp.com/plugin-index/classic-pack/).
Il manuale locale descrive le letture e i peak LED, ma non specifica le
costanti della dinamica grafica. La motivazione precisa dei programmatori
non è documentata nelle fonti consultate.

I meter analogici possono usare un condensatore caricato sui picchi e
scaricato più lentamente; una lancetta introduce inoltre inerzia e
smorzamento. Sono circuiti di misura, distinti dal detector che comanda la
compressione. [Esempi di circuiti](https://sound-au.com/appnotes/an012.htm).
I PPM hanno anche specifiche di risposta ai burst e di ritorno:
[EBU Tech 3205](https://tech.ebu.ch/docs/tech/tech3205.pdf).
La nostra risposta non viene dichiarata conforme a quello standard.

Una scarica RC libera segue `V(t) = V0 exp(-t/tau)`; espressa come livello
di ampiezza diventa `L(t) = L0 - 8.686 t/tau` dB. La pendenza 14.3 dB/s
corrisponde quindi a tau circa 0.61 s **durante la scarica libera**.
La rampa con arresto al livello corrente e il polo aggiuntivo sono un modello
comportamentale plausibile, non la ricostruzione verificata di uno schema
elettrico McDSP. Il polo da 130 ms è applicato in dB: non equivale a mettere
semplicemente un secondo condensatore sul segnale audio.

La ragione funzionale plausibile è dare tempo all'occhio di leggere i picchi
e mantenere una risposta stabile e familiare. Questo resta un'inferenza:
molti algoritmi possono produrre curve simili. La release del compressore
decide la durata dell'attenuazione; il ritorno del meter decide la durata
dell'indicazione. Le due costanti non sono intercambiabili.

## Verifiche automatiche completate

- Test DSP: conservazione IN/OUT/GR/MAIN a buffer 64/512/1024, concorrenza
  publish/consume, stereo, reset, output identico con/senza letture GUI.
- Modello: gradini, monotonia, picchi immediati, 30/60 Hz, timer irregolare,
  svuotamento senza callback e assenza di offset del plateau GR.
- Test GUI: repaint senza consumo o decadimento extra, picchi visibili al
  tick successivo, stop callback e apertura senza picchi obsoleti; regressioni
  focus, resize, editing, SOLO/IN già presenti nella suite.
- Build Windows x64 Release VST3; CI aggiornata per compilare anche la suite
  GUI prima di eseguire CTest.

Validazione successiva in Ableton: ripetere 01/02 con la nuova build,
UNLINKED, buffer 512 e uno diverso (64/1024 se disponibili), Warp OFF e
durata originale. Riprendere anche 03/04 R250 per confrontare il nuovo
ritorno GR. Per chiudere la recensione servono ancora 05 compresso R250/R500,
03/04 R500 e una voce dry reale. Non occorre rifare il neutro se routing,
gain e timeline restano identici.

Esito CTest: **2/2 PASS**, DSP 7.72 s, GUI 7.09 s (14.86 s complessivi).
Verifica modello numerico PASS; verifica acquisizioni PASS (35 input invariati,
15 401 nuovi fotogrammi). Controllata anche la schermata al resize minimo.
Profilo condiviso e locale identici, SHA-256
`43A1DBE141FC6660FFA15938B284FDD8D3DCD0AB15E26028E608071D1CFAADE1`.

Corretto inoltre il metadato Windows della versione nelle build incrementali:
la regola RC di JUCE 9.0.1 non dipendeva da `Info.txt`, mantenendo 0.2.0 nelle
proprieta del file nonostante il manifest VST3 riportasse 0.2.2. La dipendenza
aggiunta in CMake rigenera la risorsa al cambio versione senza modificare JUCE.
Gli hash del binario e del pacchetto finale sono allegati alla release.
