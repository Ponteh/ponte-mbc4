# 0.2.3: prove che richiedono Ableton o l'originale

Non rifare gli 80 export gia' analizzati. Non servono prove di sidechain
esterna sul McDSP non disponibile nel tuo setup. I confronti automatici
Ponte e la diagnostica tecnica sono separati da queste acquisizioni.

## 1. Collaudo della build candidata in Ableton

Usa inizialmente 48 kHz, buffer 512; annota versione Ableton e versione
Ponte (0.2.3, con hash del pacchetto nel rapporto tecnico).
La versione 0.2.3 da sola non distingue le diverse build di prova.

| Passaggio | Cosa verificare |
| --- | --- |
| Voce, batteria e mix; R1/R2/Auto, BITE 1/5/10 | Nessun click, perdita di canali, distorsione inattesa o livello che cambia aprendo la GUI. |
| Lasciare 15 s di silenzio dopo il segnale, poi un transiente | Primo attacco completo, nessun fade aggiunto o picco vecchio nei meter. Con Release 2500 ms usare anche una pausa di 100 s: il nap deve attendere la memoria lunga, senza forzarla a zero. |
| Ingresso live con monitor attivo e trasporto fermo | Il segnale continua a passare; il nap non dipende dal tasto Play. |
| Play/stop, seek, loop e bypass host | Nessun blocco o coda anomala; meter e spettro ripartono con dati nuovi. |
| Automatizzare gain, crossover, IN/SOLO e cambio algoritmo | Niente dropout o stato errato, anche dopo il silenzio. |
| Salvare/chiudere/riaprire il progetto | Parametri, routing e dimensione finestra conservati. Provare anche una copia di una vecchia sessione. Auto e' stato ricalibrato: il suono puo' cambiare intenzionalmente. |
| Export e freeze/unfreeze dello stesso tratto | Nessun tratto mancante; confronto con la riproduzione live e livelli coerenti. |
| Una e poi piu' istanze, editor chiusi/aperti/nascosti e resize | Annotare numero di istanze, buffer, carico tipico/massimo e dropout. Ripetere a un buffer basso che usi davvero, per esempio 64/128. |

La sidechain esterna puo' essere provata **solo su Ponte**, se la usi:
programma muto e key attiva, poi ritorno del programma. Non serve un originale
equivalente. Per ogni problema salva progetto ridotto, impostazioni, breve
audio/video e passaggi per riprodurlo; un semplice esito PASS va bene altrimenti.

## 2. Verificare il caso T045 prima di rifare qualsiasi export

Apri il preset/sessione usato per **MC303 ALL** e controlla:

- MC303, tre bande, crossover **100 e 785 Hz**; LINK **UNLINKED**.
- Tutte le bande IN attive; nessun SOLO; fase normale.
- Input/output e gain di ogni banda **0 dB**.
- Su tutte le bande: **Auto, ratio 2:1, threshold -27,5 dB, knee 0,
  BITE 1, attack 2,5 ms, release 250 ms** (Auto ignora i tempi manuali).
- Sorgente `08_MULTITONE_48000Hz_60s.wav`, Warp OFF, clip/fader 0 dB,
  nessun altro effetto; motore DAW ed export **48 kHz**, buffer 512.

Manda una schermata leggibile o il preset con questi valori. Se trovi una
differenza, correggi e rifai **solo T044 (neutro ratio 1:1) e T045 (2:1)**,
60 s, WAV float32 stereo, senza normalizzazione/dither/fade aggiunti.
Se tutto coincide, confermalo: lo scarto resta da indagare nel modello e
non viene attribuito automaticamente a un tuo errore.

## 3. Meter: catture comparative sulla build nuova

Il problema dell'esperto riguarda anche i dB **mostrati**: i WAV offline
gia' consegnati non descrivono il comportamento del meter durante un video.
Usa 48 kHz nel motore, buffer 512, Warp OFF, GUI interamente visibile e
senza ridimensionarla durante la misura. Conserva i video originali,
preferibilmente 60 fps, senza montaggio o ricodifica.

Su MC404 originale e Ponte quattro bande, input/output/gain 0 dB,
crossover 100/785/10000 Hz, UNLINKED, tutte IN attive, nessun SOLO:

| Sorgente | Impostazioni su tutte le bande | Scopo |
| --- | --- | --- |
| `16_METER_LEVELS_48000Hz_60s.wav` | R1, ratio 1:1, knee 0, BITE 1, attack 2,5 ms, release 250 ms | IN/OUT/MAIN neutri. |
| `17_METER_BURSTS_48000Hz_60s.wav` | R1, ratio 2:1, threshold -27,5 dB, knee 0, BITE 1, attack 2,5 ms, release 250 ms | GR compressa e ritorno. |
| Stessa sorgente 17 | Come sopra, release 500 ms | Effetto della release lunga su audio e lettura GR. |

Avvia la cattura prima del Play e termina dopo la coda. Mostra anche il
trasporto della DAW. Registra l'audio della stessa passata nel video se
possibile; in alternativa conserva una traccia PRINT della stessa passata
e un riferimento temporale visibile per allinearla. Un nuovo export offline
non sostituisce l'audio sincronizzato per la latenza assoluta. Senza questo
allineamento si possono comunque misurare decadimenti relativi, dichiarando
il limite. Nomi suggeriti: `METER17_ORIG_R250_take1.mp4`,
`METER17_PONTE_R250_take1.mp4`, ecc.

I vecchi T087/T088 a 96 kHz restano validi come dati diagnostici: queste
catture servono per confrontare la nuova GUI in condizioni comuni.

## 4. Chiudere precisamente la recensione sulla voce

La voce gia' analizzata e' utile: non va scartata. Era pero' registrata con
tutte le IN attive e manca un neutro originale. Per riprodurre esattamente
l'esempio iniziale, usa la stessa voce, crossover 100/785/10000 Hz,
compressione sulle bande 2/3: R1, ratio 2:1, knee 0, BITE 1,
threshold -27,5 dB, attack 2,5 ms, release 250 e poi 500 ms.
Documenta IN/SOLO e il segnale effettivo: IN OFF non va assunto equivalente
nei due prodotti se uno bypassa e l'altro silenzia la banda.

Conserva un neutro ratio 1:1 con lo **stesso routing**, la sorgente dry
esportata dalla stessa clip e video/audio delle due compressioni. Se
l'esperto puo' ascoltare entrambe a pari livello, annotare anche in quali
passaggi la tenuta o la GR mostrata differiscono. Questo e' il controllo
mirato della recensione, non una richiesta di ripetere l'intera matrice.
