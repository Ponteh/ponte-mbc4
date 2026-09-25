# Validazione utente ricevuta il 25 settembre 2026

Base codice `6c99ddd`, candidata 0.2.3, DSP_MODEL_9. Sono stati acquisiti
sei video METER16/17, due WAV e due video vocali SOLO 2/3, T044/T045 rifatti.
[Inventario e hash](inventory.json). Il collaudo Ableton del punto 1 resta
in attesa della conferma dell'utente; questi file non lo sostituiscono.

## Correzione implementata: solo risposta grafica GR

Il nuovo confronto distingue la scala non lineare dell'originale dalla
scala lineare in dB di Ponte. Sui burst 10/30 ms la GR Ponte sale troppo
rapidamente e mostra picchi maggiori, mentre i ritorni lunghi sono vicini.
Si sostituisce l'attacco visivo istantaneo con un inseguitore asimmetrico:

`display = target + (display - target) * exp(-dt / tau)`

- `tau = 45 ms` in salita, `90 ms` in discesa (prima: salita istantanea,
  discesa 150 ms).
- Il target rimane il massimo GR accumulato dal DSP tra due letture GUI.
- Guadagno statico unitario: nessun offset, moltiplicatore GR o modifica
  del valore finale. Nessuna modifica alla compressione audio, ai parametri
  salvati o alla latenza. DSP_MODEL_9 rimane appropriato.
- IN, OUT, MAIN, spettro e punti STATIC I/O mantengono il modello di livello
  gia' verificato; non si applica a un meter di livello la costante del GR.

Fit su R250 delle bande 2/3; R500 delle stesse bande tenuto fuori dal fit.
La stima continua e' 44,6/88,1 ms, arrotondata a 45/90 ms. La sonda usa
il motore completo a 48 kHz, snapshot 1 ms, mailbox simulata a 30 Hz;
le osservazioni originali sono i fotogrammi in cui cambia il valore GR.
L'offset visivo e' una variabile di allineamento, non latenza audio misurata.

| Confronto del modello con il video originale | Prima, MAE | Implementato, MAE | p95 implementato |
| --- | ---: | ---: | ---: |
| R250, fit | 0,745 dB | 0,239 dB | 0,583 dB |
| R500, verifica indipendente | 0,652 dB | 0,294 dB | 0,649 dB |

[Parametri e metriche](gr_fit.json), [tracce e confronto](gr_candidate.png).
Sono misure della simulazione grafica guidata dal DSP contro i video:
non una nuova cattura del VST corretto in Ableton, ne' una ricostruzione
dimostrata del codice proprietario. Scheduling e quantizzazione dei pixel
limitano l'identificazione; non si compensa lo scarto audio con un offset.

## Meter di livello: conferma del modello esistente

METER16 ha durate coerenti con la sorgente: fattori temporali 0,99948
originale e 1,00025 Ponte. Nessuna evidenza del precedente accorciamento.
Tutti i video sono 1920x1080, 60 fps. L'audio AAC e' interamente muto:
si misurano intervalli relativi, non la latenza assoluta audio-schermo.

Intervalli di discesa 10-90%, MAIN e banda attiva:

| Gradino | Originale | Ponte acquisito |
| --- | ---: | ---: |
| 6 dB | 0,467-0,500 s | 0,433-0,450 s |
| 12 dB | 0,684-0,734 s | 0,716-0,750 s |

Le salite attraversano 10-90% nello stesso fotogramma osservato: non
significa tempo fisico nullo. I ritorni GR finali R500 sono circa
1,70-1,73 s nell'originale e 1,65-1,67 s nella build acquisita.
Le discese interrotte dal burst successivo sono censurate, non forzate a zero.

Nei pannelli McDSP **IN e' selezionato**: le due barrette inferiori sono
canali L/R del meter IN, non IN e OUT separati. MAIN documenta l'uscita
globale; OUT per banda non e' verificato direttamente da queste catture.
ROI diverse per le finestre spostate (Ponte R500: -8/+1 pixel), calibrate
separatamente. Alle basse letture l'originale e' fortemente quantizzato.

[Misure livelli](level_measurements.json), [grafico](level_meter_comparison.png),
[burst misurati](burst_measurements.json), [grafico](burst_meter_comparison.png).

## T044/T045 corretti

Entrambi sono ora 60 s, float32 stereo **192 kHz**, anziche' i 48 kHz
richiesti per questi ID. Il rate del motore di questa nuova coppia e'
ancora da confermare; non si eredita automaticamente la conferma degli
altri test sample-rate. La misura e' diagnostica e conserva questo limite.

Confronto RMS in finestre 10 ms con il proprio neutro, dopo ricampionamento
esplicito 192 -> 48 kHz per la misura. Il candidato e' elaborato sia a 48
sia a 192 kHz; il secondo ingresso e' ricampionato dalla sorgente 48 kHz.
MAE attiva **0,301/0,292 dB**, complessiva **0,100/0,097 dB**.
Il precedente T045 aveva MAE attiva 1,050 dB e attenuazione quasi uniforme.

Ora i plateau originali 50/315/2000/14000 Hz hanno GR
4,135/4,413/7,069/7,141 dB, contro 4,364/4,677/7,291/7,365 dB del candidato
192 kHz. L'anomalia fra bande e' sostanzialmente risolta dal nuovo export.
Non si modifica Auto per inseguire il vecchio file; rimane uno scarto del
modello di qualche decimo di dB, non equivalenza perfetta.

## Voce SOLO 2/3

Nei video tutte IN accese, SOLO 2/3 attivi, R1 e crossover 100/785/10000 Hz.
Il confronto Ponte riproduce quel routing. Sorgente MP3 gia' fornita,
allineamento -50 ms e correzione costante di 57 campioni, verificata in
cinque posizioni di ogni take (correlazioni 0,994-0,997, nessuna deriva).

| Release | MAE della differenza di livello | Bias GR Ponte - originale | p95 |
| --- | ---: | ---: | ---: |
| 250 ms | 0,196 dB | +0,192 dB | 0,371 dB |
| 500 ms | 0,185 dB | +0,182 dB | 0,359 dB |

Non e' presente un nuovo neutro originale/dry WAV. Il riferimento neutro
Ponte e la decodifica MP3 non certificano la GR assoluta originale; il
livello della clip resta un'incertezza. I risultati non giustificano un
ritocco R1. [Confronti audio, allineamento e toni](audio_comparison.json).

## Riproduzione e stato

Da questa cartella superiore, con Python/numpy/scipy/Pillow/imageio-ffmpeg:

```text
python analyse_2026_09_25.py --inventory --extract --levels --bursts
python analyse_2026_09_25.py --build-probe --fit-gr
python analyse_2026_09_25.py --audio
```

La sonda standalone viene compilata con CMake/MSVC in `build/`; il confronto
audio usa il target `MC2000OriginalPackRender` della build locale indicata
nello script. I video/WAV originali restano intatti e non vengono pubblicati.
Cache e intermedi rimangono locali; le tabelle versionate hanno gli hash
della consegna. Per nuove sorgenti svuotare soltanto le cache derivate in
`build/mc2000-validation-2026-09-25`, non riutilizzarle per altri take.

Test, hash e pacchetto aggiornato: [validazione della build](build_validation.json).
Restano il punto 1, la conferma del rate T044/T045 e i limiti di acquisizione
descritti nella [guida utente aggiornata](../../VALIDAZIONE_UTENTE_0.2.3.md).
Nessuna release pubblicata.
