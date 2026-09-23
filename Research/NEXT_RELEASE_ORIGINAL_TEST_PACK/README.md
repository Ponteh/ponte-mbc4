# Test originali MC2000 — prossima release

**Implementazione R1 del 23 settembre, DSP_MODEL_7:**
[confronto con la baseline, test e limiti](analysis_2026-09-23-r1/REPORT.md).
La legge del controllo lineare sostituisce il vecchio fit R1; Auto/R2/BITE
restano separati. Il rapporto del 20 settembre qui sotto e storico e mantiene
il riferimento congelato DSP_MODEL_6.

**Analisi aggiornata al 20 settembre:**
[nuovi risultati, R1/BITE, video e acquisizioni da controllare](analysis_2026-09-20/REPORT.md).
80 WAV per 75 ID e quattro video; 61 confronti audio, inclusi quelli diagnostici.
R1 mostra una diversa curva di rilascio e BITE un transiente troppo ridotto
in Ponte. Dodici export primari restano muti; alcuni preset ALL non risultano
equivalenti. T075–T081 ancora attesi. Codice di produzione invariato in questa
analisi; per lo stato attuale usare il nuovo rapporto.

**Storico della consegna del 19 settembre:**
[risultati, correzione Auto e file da rifare](analysis_2026-09-19/REPORT.md).
47 export ricevuti; T055–T061 muti, T023 duplica T021, condizioni BITE/repeat
da confermare. Sidechain non acquisita. Non occorre rifare tutti i test.
Il validatore ora accetta anche `T001.wav` e `T001 - note.wav`; il nome breve
identifica la riga ma non certifica le impostazioni effettive del plugin.

Preparato il 2026-09-17 per il **punto 2 dell'ordine di lavoro** della
[checklist](../../NEXT_RELEASE_CHECKLIST_TEMP.md): completare le misure
dell'originale, soprattutto Auto. Non è il benchmark CPU della sezione 2.

**Tutti i WAV sorgenti e tutti gli export richiesti durano esattamente
60.000 secondi.** I file sono già generati in `audio/`. Non serve estrarre
gli archivi originali per eseguire queste nuove prove.

Sono **22 sorgenti, circa 441 MiB**, e **88 export pianificati** (circa
2.30 GiB se tutti float32, esclusi video e voce reale). I cinque file del
test 18 hanno frequenze native diverse, ma sempre la stessa durata di 60 s.

Inizia con **P0: 12 export**, righe T001–T012 del
[piano impostazioni completo](RENDER_PLAN.md). Questo primo gruppo permette
di confrontare Auto con R1/R2 e il neutro sulle due bande centrali.
Poi prosegui con gli altri gruppi nell'ordine della tabella. Non occorre fare
tutto in una sessione: ogni gruppo ha riferimenti neutri propri o già acquisiti.

## File e cosa consegnare

| Percorso | Uso |
| --- | --- |
| `audio/` | WAV da importare, PCM 24 bit stereo; non modificarli. |
| `RENDER_PLAN.md` | Una riga per ogni export, con TUTTI i parametri comuni alle bande e nome finale. |
| `render_plan.json` | Stesse righe con configurazione esplicita di ogni banda, IN/SOLO, sorgente, Fs e automazione. |
| `manifest.json` | Hash, campioni, durata, eventi, livelli misurati e ruoli calibrazione/verifica. |
| `RENDER_LOG.csv` | Spuntare ogni acquisizione e annotare versione plugin, buffer, eccezioni e screenshot. |
| `SESSION_METADATA_TEMPLATE.json` | Copiare in `session_metadata.json` e compilare una volta per sessione. |
| `renders/` | Mettere qui gli export, con i nomi esatti del piano; per rifare una prova usare take2, take3 ecc. |
| `screenshots/` | Screenshot dei preset e dei routing speciali; creare la cartella quando serve. |
| `validate.py` | Controllo hash sorgenti, formato/durata/rate/canali dei render e file mancanti. Non è ancora il fit DSP. |

Per Auto servono principalmente **gli export audio**, non un video per ogni
riga. Gli screenshot evitano ambiguità di configurazione. Per P9 meter servono
anche video originali a 60 fps se disponibili; non ricomprimerli con app di chat.
Se OBS non cattura audio, consegnare comunque video e WAV offline distinti,
specificando che non provengono dalla stessa passata sincronizzata.

## 1. Preparazione del progetto Ableton

1. Nuovo progetto senza processing sul Master, return o gruppi. Una traccia
   audio stereo con **il solo MC2000 originale**; non concatenare Ponte.
2. Tempo **120 BPM**, metrica **4/4**, nessuna automazione del tempo. Mettere
   ogni sorgente all'inizio **1.1.1**. A questo tempo 60 secondi sono **30 battute**:
   selezione da 1.1.1 fino a 31.1.1 esclusa. Il tempo è solo un riferimento;
   la durata del WAV non deve essere cambiata.
3. **Warp OFF su ogni clip**, niente trasposizione, stretching, fade automatici
   del clip, crop o allineamento automatico dei transienti. Il file contiene
   già le transizioni progettate. Verificare che la clip intera sia di 60 s.
4. Clip gain 0 dB, fader traccia 0 dB, pan centrale, send chiusi; Master 0 dB,
   nessun limiter/normalizzatore. Plugin e traccia attivi, bypass host OFF.
5. Primo giro: progetto/driver **48 000 Hz**, buffer **512 campioni**. Per P7
   cambiare anche la frequenza del motore/progetto, non soltanto quella di export.
6. Prima di ogni passata ripartire da zero con una nuova istanza o con una
   procedura di reset verificata. Lo stato del detector non si azzera per forza
   premendo Stop. Ricaricare il preset della riga e verificare i valori.
   Il WAV ha 3 s iniziali di silenzio e gli ultimi 4 s sono silenzio esatto;
   non sostituire il reset con il solo silenzio iniziale.
7. Esportare **la traccia processata**, stereo, tutta la selezione di 60 s,
   WAV **32 bit float**, stesso sample rate del progetto, normalize OFF,
   dither OFF, conversione mono OFF, MP3 OFF, render-as-loop OFF.
   Non esportare solo il tratto che contiene suono e non aggiungere code oltre
   la selezione. Se la DAW offre solo PCM 24 bit, annotarlo: è accettato dal
   validatore, ma float32 è preferibile.
8. Non cambiare parametri durante un export, salvo le righe P6 con automazione
   espressamente prescritta. Disattivare vecchie envelope del clip e della traccia.

Fare prima **T001**, controllarlo, poi T002. Se il neutro è silenzioso o il
file non dura 60 s, correggere il progetto prima delle altre passate.

## 2. Impostazioni originali: riferimento comune

Usare la variante **stereo** del modello indicato nella riga. MC404, MC303
e MC202 sono rispettivamente 4, 3 e 2 bande: non simulare MC202 spegnendo
due bande di MC404. Annotare versione esatta McDSP, formato VST3/VST2/AU e DAW.

| Controllo originale | Valore comune, salvo riga esplicita |
| --- | --- |
| INPUT globale | **0.0 dB** |
| OUTPUT globale L e R, se separati | **0.0 dB su entrambi** |
| PHASE / inversione polarità | **OFF**, normale |
| Master bypass / bypass host | **OFF** |
| LINK / MSTR / Master band | **UNLINKED**, nessuna banda master |
| IN delle bande | **Tutti ON**, anche sulle bande non in SOLO |
| SOLO | Solo i numeri nella colonna SOLO; `nessuno` = tutti OFF |
| GAIN/OUTPUT/makeup di ogni banda | **0.0 dB** |
| THRESH di ogni banda | **−27.5 dB** |
| COMP / ratio di ogni banda | **2.00:1**; nelle righe B0 **1.00:1** |
| KNEE di ogni banda | **0.0** |
| BITE di ogni banda | **1.0** |
| ATTACK di ogni banda | **2.50 ms** |
| RELEASE di ogni banda | **250.0 ms** |
| Modalità TC | Secondo riga: AUTO, R1 oppure R2 |
| Sidechain esterna / key | **OFF/interna**, tranne P5 EXT verificato |
| Altri processori della DAW | Nessuno |

Inserire i numeri esatti tramite editing del valore, quando disponibile;
non stimare la posizione dei knob a occhio. Dopo il caricamento del preset,
controllare anche le bande non selezionate: il LINK può averle modificate.

### Crossover e bande da ascoltare

| Modello | X1 | X2 | X3 | Portanti usate e banda |
| --- | ---: | ---: | ---: | --- |
| MC404 | **100 Hz** | **785 Hz** | **10000 Hz** | 50→1; 315→2; 2000→3; 14000→4 |
| MC303 | **100 Hz** | **785 Hz** | non presente | 315→2; 2000/14000→3 |
| MC202 | **785 Hz** | non presente | non presente | 315→1; 2000/14000→2 |

`SOLO [2]` significa solo banda 2 accesa, tutte le altre SOLO spente.
`SOLO [2, 3]` significa accendere entrambi. **Non usare IN per isolare una
banda**: il manuale originale descrive IN come enable/bypass della compressione,
mentre Ponte v0.2.2 lo usa come mute d'ingresso. Tutti gli IN ON più SOLO
esplicito evita questa differenza nei confronti.

### TC, AUTO e riferimento B0

- **AUTO**: attivare A/Auto sulla banda; se il plugin presenta un selettore TC
  unico, scegliere Auto. Applicare la scelta a tutte le bande. Per i pulsanti
  separati impostare R1 come stato manuale di base e poi accendere A.
- **R1**: A/Auto OFF, R1/Type-1 ON. **R2**: A/Auto OFF, R2/Type-2 ON.
  Non basta scegliere R1/R2 lasciando Auto acceso.
- **B0**: stesso modello, sorgente, Fs, crossover, IN/SOLO e gain della prova,
  ma COMP 1:1 su tutte le bande, KNEE 0, BITE 1, Auto OFF, R1.
  Il compressore resta IN: non usare il bypass dell'intero plugin, perché
  serve il riferimento passato attraverso i crossover.
- In Auto Attack/Release manuali non dovrebbero influire, secondo il manuale
  e le precedenti acquisizioni. Impostarli comunque ai valori indicati.
  AUTO_MAN_LOW/HIGH usa **0.25/25 ms** e **25/2500 ms** (Attack/Release).
  Sono due coppie valide e distanti; non si afferma che 25 ms sia il massimo
  effettivo di ogni build, perché la documentazione e le UI possono differire.
- Se compare un ulteriore parametro, per esempio Release 2, non assegnare un
  valore inventato: annotare nome/valore e allegare lo screenshot prima di usare
  quella configurazione nel fit. Il piano si riferisce alla UI R1/R2/A già usata.

Fonte dei nomi e del significato dei controlli:
`../user_manuale_mcdsp_mc2000.md`, sezioni Controls, Attack and Release,
Compression Band Linking e tabella parametri. Il manuale resta locale.

## 3. Sorgenti: tutte da 60 secondi

| N. | Nome breve | Contenuto e scopo |
| --- | --- | --- |
| 01 | STEP_315 | Banda 2: base −42 dBFS, step −6 di 100 ms/500 ms/3 s; poi −18/−12/−6. Attacco, rilascio e dipendenza dal livello. |
| 02 | MEMORY_315 | Coppie di burst da 250 ms con pause 50/250/1000/3000 ms; treno finale di burst da 50 ms. Memoria di Auto/R2. |
| 03 | CREST_RMS_2000 | Sei segmenti di 6 s, tutti RMS −20 dBFS; carrier a 2 kHz modulata a 4 Hz, duty 100/50/20/50/100/20%. |
| 04 | CREST_PEAK_2000 | Stessa struttura del 03, ma picco di ogni segmento −8 dBFS: cambia l'RMS. |
| 05 | STEP_50 | Stessi eventi del 01, portante 50 Hz per banda 1. |
| 06 | STEP_2000 | Stessi eventi del 01, portante 2 kHz per banda 3. |
| 07 | STEP_14000 | Stessi eventi del 01, portante 14 kHz per banda 4. |
| 08 | MULTITONE | Somma a pesi uguali di 50/315/2000/14000 Hz con inviluppo a livelli e burst. Uscita completa. |
| 09 | NOISE_HOLDOUT | Rumore deterministico con inviluppo: verifica indipendente, non usarlo per calibrare Auto. |
| 10 | PROGRAM_SC_315 | Carrier 315 Hz a −18 dBFS; pausa 25–31 s per osservare key attiva senza program. |
| 11 | KEY_SC_315 | Chiave esterna per il 10: base −42 dBFS e burst −6; non esportare come uscita del compressore. |
| 12 | STEREO_315 | 3–15 s L solo; 15–27 R solo; 27–39 L=R; 39–55 L=−R. Step a −6 con base −42. |
| 13 | AUTOMATION_315 | Carrier −18 con burst −6 a 6/16/26/36/46 s, ciascuno lungo 2 s. |
| 14 | FUNDAMENTAL_90_120_180 | Tre frasi sintetiche con fondamentale dominante, prima 90, poi 120, poi 180 Hz. Non è una voce reale. |
| 15 | PHRASE_HOLDOUT | Frase armonica sintetica con modulazione lieve della fondamentale: seconda verifica indipendente. |
| 16 | METER_LEVELS | Plateau −36/−24/−12/−6/−12/−24/−36, prima 315 Hz poi 2 kHz. |
| 17 | METER_BURSTS | Burst 10/30/100/300/1000 ms con pause irregolari, due ripetizioni per portante. |
| 18 | RATE_PROBE | Multitono come 08, generato nativamente a 44100/48000/88200/96000/192000 Hz. |

I livelli sinusoidali sono **picco per canale**; −20 RMS del test 03 è RMS
digitale senza offset di calibrazione. Per multitono/rumore il livello
dell'inviluppo non è il picco di ogni singola banda: i livelli effettivi sono
nel manifest. Il crest factor dopo un crossover può differire da quello del
WAV: per il fit usare anche il B0 della banda.

Transizioni di ampiezza: mezzo coseno di **2 ms**, inizio all'istante evento.
Le discese al silenzio finiscono 2 ms dopo la fine nominale del segmento.
I segmenti crest hanno fade ai bordi compresi nei 6 s e normalizzazione
misurata su quei 6 s. Il manifest riporta gli eventi in secondi e campioni;
gli eventi “baseline” possono contenere i burst sovrapposti descritti sotto.

## 4. Gruppi e priorità

| Gruppo | Cosa cambiare rispetto al setup comune | Perché |
| --- | --- | --- |
| P0 | Solo sorgente, SOLO e TC/COMP secondo le 12 righe iniziali | Primo fit Auto e confronto R1/R2 sulle bande 2/3. |
| P1 | Sorgenti crest, banda bassa/alta, multitono e holdout | Separare crest, frequenza e generalizzazione. |
| P2 | Solo il parametro indicato: BITE 5/10, COMP 4:1, THRESH −18, KNEE −5/+10, tempi manuali o ripetizione | Evitare di attribuire a Auto gli effetti di BITE/knee. |
| P3 | Caricare realmente MC202 o MC303 e impostare i crossover della tabella | Capire se la legge cambia tra modelli. |
| P4 | Sorgente stereo 12; niente Utility che sommi a mono | Verificare detector stereo e opposizione di fase. |
| P5 | Solo se disponibile: key 11 su ingresso esterno del plugin | Isolare program e detector. |
| P6 | Automazioni definite nella tabella successiva | Tempi manuali in Auto, cambio TC, threshold e crossover. |
| P7 | Cambiare progetto/driver, sorgente nativa ed export allo stesso Fs | Eliminare i vecchi file etichettati ad alto Fs ma registrati a 48 kHz. |
| P8 | R1 release 250/500; sonda 14 in SOLO 2+3, e toni 01/06 a 500 | Riprendere il caso dell'esperto, senza ambiguità IN/mute. |
| P9 | Ratio 1:1, nessun SOLO; video IN e OUT separati se condividono il meter | Validazione meter. Le nuove timeline non sostituiscono alla cieca quelle storiche. |

Il piano contiene i nomi esatti anche per le righe con impostazioni particolari:
non rinominare un B0 come AUTO. `AUTO_REPEAT` va acquisito da nuova istanza,
stesse condizioni di AUTO base; serve a misurare la ripetibilità, non un nuovo preset.

### P5: sidechain, solo se il plugin originale la espone

Il manuale locale non documenta una procedura certa per una key esterna nella
variante usata. Verificare prima che **quella istanza** esponga un ingresso
sidechain e un modo effettivo di attivarlo. Se manca, scrivere
`NON SUPPORTATO` per P5 EXT nel log; non sostituire con un altro compressore.
I render interni del program 10 restano eseguibili.

Se disponibile: traccia PROGRAM con file 10 e plugin, traccia KEY con file 11,
entrambi a 1.1.1, Warp OFF e 0 dB. Instradare KEY solo al detector esterno;
nessuna sua copia deve arrivare all'uscita esportata. Non spegnere il segnale
key usando un mute che chiuda anche l'invio sidechain: verificare il routing
con il meter e acquisire uno screenshot. Esportare PROGRAM, non la somma.
Una prova esterna che nulla con quella interna non dimostra automaticamente
correttezza: il routing va verificato con le finestre in cui i due file differiscono.

### P6: automazioni, istanti assoluti dall'inizio del WAV

Creare cambi a gradino, senza rampe della DAW, sulle **tutte le bande** presenti
per i parametri di banda. Le impostazioni iniziali sono quelle della riga.
A 120 BPM: 10 s = 6.1.1, 20 s = 11.1.1, 30 s = 16.1.1,
40 s = 21.1.1, 50 s = 26.1.1. Salvare screenshot delle lane.

| Codice nel piano | Stato iniziale 0 s | 10 s | 20 s | 30 s | 40 s | 50 s |
| --- | --- | --- | --- | --- | --- | --- |
| MANUAL_TIMES | Auto; ATT 2.5 / REL 250 ms | ATT .25 / REL 25 | ATT 25 / REL 2500 | ATT 2.5 / REL 250 | ATT .25 / REL 2500 | ATT 25 / REL 25 |
| MODES | Auto (manuale R1) | A OFF, R1 | R2 | A ON | A OFF, R1 | A ON |
| THRESHOLD | −27.5 dB | −18 | −36 | −27.5 | −18 | −27.5 |
| CROSSOVER | X1 100 Hz | X1 200 | X1 100 | X1 200 | X1 100 | X1 100 |

In CROSSOVER X2=785 e X3=10000 restano fissi. Sono previsti un B0 e un Auto
con la stessa automazione: il B0 statico non sostituisce quello automatizzato.
MODES con pulsanti separati richiede di verificare l'ordine effettivo degli
eventi: annotare eventuali limiti dell'host invece di usare clic manuali a occhio.

### P7: sample rate nativo

Per ciascun Fs caricare il file 18 con quel numero nel nome. Cambiare il
sample rate del motore DAW prima di caricare/preparare l'istanza e scegliere
lo stesso rate nell'export. Buffer 512 campioni; annotare il valore reale se
il driver impone altro. Il tempo del blocco cambia con Fs: è intenzionale.
Se il driver non supporta un rate, annotarlo come non disponibile; non
rinominare un WAV 48 kHz e non usare il solo ricampionamento in export.

### P8: caso dell'esperto e voce reale

Il piano usa **tutti IN ON e SOLO 2+3** sulla sonda 14 per isolare le uscite
delle bande 2/3 in modo esplicito. Le altre bande non devono arrivare all'output.
Per il tono 01 SOLO 2; per il 06 SOLO 3. R1, COMP 2:1, THRESH −27.5,
KNEE 0, BITE 1, ATT 2.5, RELEASE secondo riga (250 o 500 ms).

Questo è un confronto controllato: non trattarlo come replica identica del
vecchio setup IN 1/4 OFF senza SOLO. La precedente semantica IN era diversa.
Per riprodurre anche quello storico, salvare un preset separato e descriverlo,
senza sostituire uno degli export del piano.

Resta necessaria una **voce dry reale**, che non posso ricavare dalla sonda.
Preparare a 48 kHz stereo dual mono un file da 60 s: 3 s silenzio, fino a 52 s
di voce, resto silenzio; niente EQ/compressore/normalizzazione/time-stretch.
Se il materiale è più corto, aggiungere silenzio, non allungarlo. Conservarne
l'originale e annotare livello di picco. Fare tre export con la stessa
configurazione della sonda 14: B0, R1_250, R1_500, chiamati
`VOICE_ORIG_MC404_B23_B0_48000Hz_take1.wav`,
`VOICE_ORIG_MC404_B23_R1_250_48000Hz_take1.wav`,
`VOICE_ORIG_MC404_B23_R1_500_48000Hz_take1.wav`; consegnare anche
`VOICE_DRY_48000Hz_60s.wav`. Questi file sono extra rispetto al piano sintetico.

### P9: video meter

Per entrambe le sorgenti 16/17: schermata intera plugin leggibile e trasporto,
60 fps se disponibile, nessun cambio vista nel mezzo. Se il meter originale
ha vista IN/OUT alternativa, fare due video per sorgente, uno IN e uno OUT,
con gli stessi parametri; basta un export neutro per sorgente.
Nomi: `ORIG_MC404_16_IN_48000Hz_take1.mp4`, equivalente OUT e 17.
Per GR si possono filmare T002 e T010 (Auto nelle bande 2 e 3);
indicare l'ID nel nome video.
Per confrontare specificamente il ritorno R1 usare le righe **T003 e T011**.
Non usare il meter come unica misura della GR: servono i B0/B1 audio.

## 5. Verifica, rigenerazione e consegna

Prima di inviare i file, dalla root PonteDSP:

```powershell
build/mc2000-meter-analysis-tools/python/python.exe products/MC2000/Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/validate.py
```

Il controllo accetta consegne parziali e riporta ciò che manca. Con `--complete`
richiede tutte le righe non dichiarate impossibili nel resoconto; non elimina
file. Gli extra VOICE vengono controllati se presenti. Un PASS del formato
non certifica che Warp, preset, automazione o sidechain fossero corretti:
per questo servono log e screenshot.

Gli audio sono già pronti. Per rigenerarli con Python 3 + NumPy:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File products/MC2000/Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/Generate.ps1 -Force
```

`-Force` sostituisce solo le sorgenti generate e rigenera piano/manifest/log:
prima salvare una copia del log compilato. Non modifica `renders/`.
Il generatore rilegge i PCM e verifica lunghezza, sample rate, canali, errori
di quantizzazione, silenzi, stereo e normalizzazioni crest. La sorgente 18
viene sintetizzata a ogni Fs, non ricampionata da 48 kHz.

Verifica iniziale completata: **22/22 sorgenti valide**, hash e durate
confermati, configurazioni e associazioni B0 coerenti. **0/88 export originali
acquisiti** al momento della preparazione: nessun risultato DSP viene dedotto
dalla sola generazione degli stimoli.

Per ogni gruppo consegnare WAV, log aggiornato, metadata e screenshot dei
preset. Annotare gli errori senza correggere nomi/tempi per nasconderli.
La cartella sorgenti e gli export sono ignorati da Git: conservare gli audio
localmente fino alla fine dell'analisi. Non pubblicare il manuale originale.

## 6. Come saranno usati i risultati

La colonna **B0 di riferimento** associa ogni render al neutro corretto;
non riusare un neutro con altro modello, SOLO, sample rate o automazione
crossover. Per una sola banda misurare l'inviluppo e i tempi GR audio tramite
finestre adatte alla portante, con esclusione degli intervalli quasi nulli.
Per ALL o SOLO 2+3 confrontare l'uscita complessiva: il rapporto dei due WAV
non identifica la GR separata di ciascuna banda.

Calibrare sui gradini/memoria/crest e controlli; lasciare 09 e 15 fuori dal
fit e usarli per la verifica finale. Controllare prima ripetibilità, durata,
routing e stato dei parametri; soltanto dopo confrontare il modello Auto.
Non interpretare l'allineamento di un export separato come sincronizzazione
con OBS. I file nuovi sono controllati ma non dimostrano ancora un difetto
del DSP: questo verrà stabilito dall'analisi delle acquisizioni originali.
