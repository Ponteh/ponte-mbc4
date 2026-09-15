# Test visivi MC2000 / Ponte MBC4 — pack 1

**Build 0.2.2, release in attesa di OK:** [implementazione, modello matematico e verifiche](meter_fix_2026-09-15/REPORT.md).
Le analisi sottostanti riguardano la build precedente. Per validare la nuova
GUI ripetere 01/02 e 03/04 R250; le prove R500/voce restano da completare.

Preparato il 2026-09-14 per la recensione dell'esperto: meter troppo rapidi e
meno GR **mostrata** dall'originale su voce ricca di fondamentale.

**Analisi del primo giro completata il 2026-09-15:**
[risultati dei test 01–03](analysis_2026-09-15/REPORT.md), con grafici,
misure e limiti del setup. Ricevuti 8 video e 8 WAV: confermate la discesa
troppo rapida dei meter Ponte e la sottorappresentazione di alcuni burst.
I tempi degli export 02/03 sono trasformati; il report usa quelli misurati.
La guida qui sotto conserva il protocollo previsto per la prossima acquisizione.

**Secondo giro completato:** [take 2 e test 04/05](analysis_2026-09-15_take2_04_05/REPORT.md).
A2 resta difettoso anche in UNLINKED; B0 take 2 ha SOLO 2 corretto. La banda 3
conferma le piccole differenze GR audio/visive. Il test 05 ricevuto è neutro.
Buffer confermato **512**, recorder OBS, WAV da export separati; non sono
registrazioni sincronizzate ai video. I WAV vecchi dei take 2 restano invariati.

### Prossime prove: impostazioni esplicite

| Prova mancante, per originale e Ponte | Impostazioni |
| --- | --- |
| **05 compresso a 250 ms** | 4 bande, crossover 100/785/10000; IN 2/3, IN 1/4 spenti, nessun SOLO; UNLINKED; bande 2/3 R1, ratio **2:1**, threshold **-27.5 dB**, knee **0**, BITE **1**, attack **2.5 ms**, release **250 ms** |
| **05 compresso a 500 ms** | Identico alla riga precedente, release **500 ms** su 2/3 |
| **03 R500** | Stessi parametri di compressione, release **500 ms**, **SOLO 2** |
| **04 R500** | Stessi parametri di compressione, release **500 ms**, **SOLO 3** |

In tutte le prove: input/output/gain delle quattro bande **0 dB**, fase normale,
sidechain esterno OFF, fader/clip gain 0, normalizzazione OFF, export stereo
48 kHz float32. Buffer **512 campioni**, così resta confrontabile con questo giro.
Per B0: identica timeline, gain e routing, ma ratio **1:1** sulle bande attive.
Il B0 si riusa per 250/500 ms se queste condizioni non cambiano.

Controllare **Warp OFF e durata effettiva del clip**: 03/04 = 25.65 s,
05 = 26 s. Se si corregge la timeline rispetto ai vecchi export, rifare
anche il relativo B0; una coppia con tempi diversi non dà una GR valida.
Per 05 la somma multibanda confronta l'uscita complessiva: non dividere
semplicemente B0/B1 per attribuire una GR a ciascuna banda.

Gli export separati sono utilizzabili per l'analisi DSP anche con video
OBS silenziosi; annotarli come tali. Conservare il trasporto visibile.
Una voce dry reale completa poi il caso dell'esperto. I test A2 a buffer
diversi sono la verifica successiva alla correzione dei meter.

## Audio pronti

WAV PCM 24 bit, 48 kHz, stereo dual mono (L = R), circa 49 MiB complessivi.
I livelli sono peak dBFS **per canale**. Il massimo è -6 dBFS; nessun file
è normalizzato dalla DAW. Gli eventi hanno rampe d'ampiezza a mezzo coseno
di 2 ms per evitare discontinuità brusche. I tempi nel manifest indicano
l'**inizio** della rampa; la durata nominale di un burst è fra inizio della
salita e inizio della discesa. I file iniziano con 2 s di silenzio.

| File in `audio/` | Durata | Scopo |
| --- | ---: | --- |
| `01_IN_OUT_LEVELS.wav` | 44 s | Portanti 315 Hz e 2 kHz, gradini -36/-24/-12/-6/-12/-24/-36 dBFS da 2.5 s: scala, salita, hold e discesa IN/OUT |
| `02_IN_OUT_BURSTS.wav` | 56.35 s | Burst -6 dBFS da 10/30/100/300/1000 ms, tre ripetizioni ciascuno, prima 315 Hz poi 2 kHz: eventi persi e mantenimento dei picchi |
| `03_GR_BAND2_315Hz.wav` | 25.65 s | Banda 2: portante residua -42 dBFS, eventi -6 dBFS lunghi 3 s, 150 ms e 500 ms, con code da 6/4/4 s |
| `04_GR_BAND3_2000Hz.wav` | 25.65 s | Stessa sequenza nella banda 3 |
| `05_FUNDAMENTAL_PROBE_90_120_180Hz.wav` | 26 s | Sonda armonica sintetica, fondamentale dominante a 90/120/180 Hz e inviluppo a frasi brevi: verifica intorno al crossover 100 Hz |

Il file 05 **non è una voce reale** né una ricostruzione dell'esempio ascoltato
dall'esperto. Isola una caratteristica utile; per chiudere il caso percettivo
servirà poi la voce dry dell'esempio, o un'altra registrazione dry concordata.

`manifest.json` contiene durata, campioni, livelli, eventi e SHA256 dei WAV.
`events.csv` contiene gli stessi riferimenti temporali in forma tabellare.
Il generatore rilegge i WAV e verifica formato, lunghezza, uguaglianza L/R,
assenza di clipping, silenzi dopo le rampe e picchi dei plateau sinusoidali
entro 0.02 dB dal livello previsto. Le pause irregolari dei burst riducono
la sincronizzazione accidentale con il repaint, ma tre prove non stimano
con precisione una probabilità di perdita.

## Primo giro: sei video, un plugin per passata

Usa lo stesso progetto e cambia soltanto il plugin fra originale MC404
quattro bande e Ponte MBC4. Salva due copie del progetto per preservare i
setup. Parti dal buffer 256 campioni; annota il valore effettivo del driver.

| Passata, per ciascun plugin | Audio | Impostazione | Acquisizione |
| --- | --- | --- | --- |
| A1 — livelli | 01 | NEUTRO | Video + WAV uscita |
| A2 — burst | 02 | NEUTRO | Video + WAV uscita |
| B1 — GR250 | 03 | R1, release 250 ms, SOLO banda 2 | Video + WAV uscita |
| B0 — riferimento | 03 | Stesso SOLO banda 2, ratio 1:1 | Solo WAV uscita |

Totale iniziale: **6 video e 8 WAV di uscita**. Mantieni anche i WAV sorgenti
del pack. Se l'originale mostra IN oppure OUT nello stesso meter, per A1/A2
registra prima IN e ripeti quelle passate selezionando OUT; indica la vista
nel nome del file. Non cambiare vista o parametri durante una passata.
Il manuale originale descrive proprio questa selezione e una GR distinta:
[manuale locale, sezione Metering](../user_manuale_mcdsp_mc2000.md).

### Impostazioni comuni

| Parametro | Valore |
| --- | --- |
| Numero bande / crossover | 4 / 100, 785, 10000 Hz |
| Input / Output / gain per banda | Tutti 0 dB |
| LINK / sidechain esterno / inversione fase | Disattivati |
| IN di base | Bande 2 e 3 attive; 1 e 4 disattive |
| NEUTRO | Ratio 1:1 su tutte le bande, nessun SOLO |
| R1 del confronto | Ratio 2:1, threshold -27.5 dB su 2/3, knee 0, attack 2.5 ms, release 250 ms, BITE 1 |

Gain a zero, LINK e sidechain disattivi sono **condizioni controllate scelte
per questi test**: non sappiamo ancora se coincidano con il setup storico
completo dell'esperto. Nel Ponte attuale SOLO spegne visivamente tutti gli
IN, ma la banda isolata continua a essere elaborata; lascia intatta la base
IN 2/3. Nell'originale assicurati che la banda in SOLO venga compressa.
Per B0 imposta ratio 1:1 mantenendo attivi plugin, crossover e SOLO: così
il riferimento conserva il filtraggio necessario al confronto audio.

## Preparazione in Ableton Live

1. Progetto a **48 kHz**, traccia audio stereo TEST con un solo plugin.
   Importa ogni WAV intero in Arrangement, con inizio esattamente sullo
   stesso riferimento di trasporto. Usa una passata separata per file.
2. Clip Gain, pitch e trasposizione a zero; **Warp OFF**, loop e inviluppi
   disattivi, nessun fade/crossfade aggiunto ai clip. Lascia i silenzi del
   file. Warp disattivato conserva la durata originale del campione;
   evita Consolidate, che può incorporare trasformazioni e normalizzare.
   [Clip View](https://www.ableton.com/en/live-manual/11/clip-view/),
   [Committing Audio](https://help.ableton.com/hc/en-us/articles/22998838817820-Committing-Audio-in-Live).
3. Fader traccia e Main a 0 dB, pan al centro, send a zero, nessun effetto
   aggiuntivo sulla traccia o sul Main. Metronomo e count-in spenti.
4. Per legare il WAV alla **stessa esecuzione del video**, crea una traccia
   PRINT: `Audio From = TEST`, punto `Post FX`, `Monitor = Off`, armata per
   registrare in Arrangement. Ascolta solo la traccia TEST durante la
   registrazione; evita di riprodurre stampe precedenti sul Main.
   [Routing e punti di prelievo](https://www.ableton.com/en/manual/routing-and-i-o/),
   [Monitoring FAQ](https://help.ableton.com/hc/en-us/articles/360006569179-Monitoring-in-Ableton-Live-FAQ).
5. Esporta la registrazione PRINT, con lo stesso inizio e tutta la durata
   del sorgente, in WAV stereo **48 kHz / 32 bit**, Normalize OFF, Convert
   to Mono OFF, Render as Loop OFF, dither OFF. Se fai invece un export
   offline del plugin, annotalo: può usare blocchi diversi dalla passata
   registrata, quindi non è la prova dello stesso istante visualizzato.
   [Esportazione degli stem](https://help.ableton.com/hc/en-us/articles/360000843404-Importing-and-exporting-stems),
   [Export Audio/Video](https://www.ableton.com/en/manual/managing-files-and-sets/).

I percorsi esatti dei menu possono cambiare con la versione di Live:
annotala in `CAPTURE_TEMPLATE.json`. Non è richiesto un file `.als` generato
automaticamente: contano questi parametri e il routing effettivamente usato.

## Registrazione dello schermo

- Preferisci **60 fps reali**, risoluzione nativa e testo/meter leggibili,
  senza ridimensionamento del video. Tieni visibili la finestra completa
  del plugin e, se possibile, il trasporto/riferimento temporale di Live.
- Se scegli OBS, imposta registrazione locale e 60 nei valori FPS video;
  verifica che la macchina mantenga il frame rate. Impostazioni e percorso:
  [OBS Overview](https://obsproject.com/kb/obs-studio-overview).
- Includi l'audio della DAW nel video quando disponibile e controllalo con
  una breve prova. Se il recorder non cattura l'uscita ASIO, conserva il WAV
  PRINT e il trasporto visibile: annota l'assenza di audio nel video.
- Avvia il recorder prima del play e termina almeno 3 s dopo la fine.
  Conserva i silenzi iniziali/finali. Fai vedere i settaggi prima della prova,
  poi sposta il mouse fuori dai controlli per non coprire i meter.
- Mantieni posizione/dimensione della finestra e scala display costanti
  nelle ripetizioni dello stesso plugin. I due plugin possono avere GUI di
  dimensioni diverse: ciascuna verrà calibrata sulla propria scala.
- Invia i file originali del recorder, con audio e timestamp, insieme ai
  WAV PRINT. Mantieni velocità e frame originali: niente montaggio,
  interpolazione frame o conversione tramite messaggistica.

Nomi consigliati: `ORIG_A1_IN_48k_B256_take1`, `PONTE_A1_48k_B256_take1`,
`ORIG_B1_R250_SOLO2_48k_B256_take1`, `PONTE_B0_NEUTRO_SOLO2_48k_B256_take1`.
Aggiungi estensione video o WAV e compila una copia del template per sessione.

## Secondo giro, dopo la prima analisi

- Ripeti B1 con **release 500 ms**: il primo commento dell'esperto citava
  almeno 500 ms, mentre il setup preciso successivo era 250 ms.
- Ripeti B0/B1 sul file 04 con **SOLO banda 3**, poi 250/500 ms.
- File 05: setup dell'esperto con entrambe IN 2/3 e nessun SOLO, 250/500 ms.
  Se l'audio diverge, aggiungiamo le stampe isolate delle bande e il loro
  riferimento ratio 1:1. La somma multibanda non rivela da sola la GR di
  ciascuna banda.
- Per verificare la dipendenza dal buffer, ripeti **solo A2** a 64 e 1024
  campioni, stessi sample rate e FPS. Se il driver non offre questi valori,
  usa quelli disponibili e annotali. Ripeti più volte i burst che risultano
  persi, prima di trarre una conclusione statistica.

## Che cosa misurerò e come distinguerò le cause

1. **Scala**: posizione dei pixel rispetto alle tacche e livelli dei plateau.
   Il meter di banda riceve la portante dopo il crossover, quindi può leggere
   meno del picco nominale del WAV. I numeri non si confrontano contando
   pixel grezzi fra GUI diverse; verifico anche differenze peak/RMS.
2. **Risposta IN/OUT** con ratio 1:1: salita, hold, discesa, picchi raggiunti
   e burst non rilevati. È la condizione per stimare il display senza la
   compressione; il breve transiente del crossover va comunque distinto.
3. **GR audio contro GR mostrata**: sui file 03/04 confronto ogni uscita
   compressa con il suo riferimento filtrato ratio 1:1, stesso SOLO e gain.
   Per le code uso rapporti RMS su finestre documentate, circa 10–20 ms,
   evitando le prime finestre contaminate dallo step del crossover.
   La portante -42 dBFS permette di continuare la misura dopo il picco.
4. **Modello del meter**: confronto hold + discesa lineare in dB, smoothing
   esponenziale e risposta adattiva; provo il modello su eventi diversi
   da quelli usati per stimarlo. Audio simile con GR visuale diversa indica
   una differenza di misura/display; audio diverso richiede anche indagine DSP.
5. **Buffer e frequenza GUI**: nel codice corrente i picchi IN/OUT e il
   massimo GR vengono pubblicati per blocco, e letti dalla GUI a circa 30 Hz,
   senza accumulare i picchi tra letture né ballistics visive dedicate.
   A 48 kHz, 64/256/1024 campioni durano 1.33/5.33/21.33 ms: i burst brevi
   possono sparire prima del repaint. È una pista supportata dal codice,
   non una misura già eseguita sul meter originale.

Con video a 60 fps ogni fotogramma dura circa **16.7 ms**: non certificherò
un attacco da 2.5 ms né un tempo visivo al singolo millisecondo. Riporterò
risoluzione, frame mancanti e incertezza A/V. Senza calibrazione della
sincronizzazione audio/video, un ritardo assoluto include recorder, host e
display; le forme delle code e i tempi relativi restano più affidabili.
Non modificherò release o compressione audio per far coincidere solo i meter.

## Riproducibilità

`Generate.ps1` compila il generatore C# locale e produce WAV, manifest ed
eventi usando soltanto .NET. Se la policy locale richiede script firmati,
la seguente esecuzione limita l'eccezione al singolo processo:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Generate.ps1
```

La rigenerazione sostituisce gli omonimi WAV generati. I WAV e il CSV sono
ignorati dal repository del prodotto; sorgenti, guida e manifest ne
conservano la provenienza. Non è stato necessario estrarre il vecchio ZIP.
Build Ponte di riferimento: commit `2b6e97e`, SHA256 del binario
`55535E64A4632E81A7FB4F3D320F5A20F614541F9A4035303DD7883593A95FAD`.

### Nuove catture dopo la revisione UI 0.2.2

Le scale Ponte ora coprono -60..0 dB (GR 0..60), mentre i CSV storici
riguardano la scala precedente. Non riusare la calibrazione pixel/dB Ponte
vecchia. IN e SOLO sono indipendenti: nei test B1 SOLO 2/3 mettere anche
IN 2/3 acceso per attivare la compressione; SOLO da solo non la attiva piu.
Il test B0 mantiene ratio 1:1 con lo stesso routing. Annotare il commit
caricato per distinguere le build preliminari con la stessa versione 0.2.2.

Per verificare lo spettro usare anche stereo L=-R, stop/riapertura e IN 1
spento con crossover a 100 Hz: devono restare le code delle bande IN
rimanenti a sinistra del marker. Il grafico mostra ingresso pre-gain INPUT
pesato per i filtri IN, non l'uscita compressa. La finestra FFT resta 2048
campioni; il ritorno lento e intenzionale, il replay di audio vecchio no.
