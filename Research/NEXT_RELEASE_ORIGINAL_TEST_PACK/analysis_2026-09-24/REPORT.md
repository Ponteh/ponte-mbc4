# Consegna del 24 settembre: nuovi render e candidato 0.2.3

Baseline `8393751`, DSP_MODEL_8; candidato DSP_MODEL_9. Gli originali sono
conservati senza modifiche. [Inventario e hash](inventory.json),
[file sostituiti](delivery.json), [conferme della sessione](session_confirmation.json).
L'utente conferma Fs nativo della DAW a 96/192 kHz e buffer 512 campioni.

## Copertura e confronto completo

Sono disponibili 80 coppie di condizioni compatibili con il piano, tutte
di 60 secondi, confrontate con entrambi i modelli Ponte. T087/T088 sono a
96 kHz anziche' ai 48 kHz previsti: rimangono supplementi diagnostici.
T049-T054 non sono acquisiti: la prova di sidechain esterna non e' eseguibile
con l'originale nel setup disponibile; T053/T054 sono i controlli di quel
gruppo. Non e' richiesta la ripetizione di una funzione non disponibile.
La sidechain Ponte rimane coperta dai test numerici interni.

[Confronto dei modelli](model9_comparison.json): su tutte le 80 righe,
40 MAE migliorano, 39 restano invariate e T045 peggiora. Le 39 traiettorie
invariate hanno differenze audio massime di 2,53e-29, dovute a code numeriche
e FTZ/DAZ; non sono tutte bit-identiche ai vecchi file offline.
Le ottimizzazioni a parita' di algoritmo sono verificate separatamente.

La metrica e' la differenza di attenuazione RMS in finestre da 10 ms rispetto
al proprio neutro, con neutri sopra -65 dBFS; per la colonna attiva si richiede
anche GR originale >0,1 dB. Non e' un errore campione per campione ne' una
misura del meter grafico.

| Test | Condizione | MAE attiva modello 8 | Modello 9 |
| --- | --- | ---: | ---: |
| T024 | Rumore Auto | 1,111 dB | 0,293 dB |
| T041 | MC202 ALL corretto | 0,437 dB | 0,307 dB |
| T043 | MC303 isolato corretto | 0,208 dB | 0,141 dB |
| T063 | 44,1 kHz ALL corretto | 0,250 dB | 0,194 dB |
| T067 | 48 kHz ALL corretto | 0,313 dB | 0,238 dB |
| T071 | 88,2 kHz ALL corretto | 0,292 dB | 0,222 dB |
| T075 | 96 kHz ALL | 0,344 dB | 0,249 dB |
| T077 | 96 kHz LOW | 0,261 dB | 0,240 dB |
| T079 | 192 kHz ALL | 0,351 dB | 0,261 dB |
| T081 | 192 kHz LOW | 0,298 dB | 0,276 dB |
| T045 | MC303 ALL, diagnostico | 0,968 dB | 1,050 dB |

T045 non e' stato escluso dal conteggio del peggioramento: MAE complessiva
0,309 -> 0,335 dB, p95 1,189 -> 1,287 dB. Nei plateau, l'originale attenua
50/315/2000/14000 Hz di 7,07/6,90/6,73/7,02 dB; Ponte 9 di
4,36/4,68/7,33/7,40 dB. La quasi uniforme attenuazione originale e' poco
coerente con bande indipendenti e il preset pianificato; non dimostra da
sola un errore di export. Routing/link/preset e differenza di modello
rimangono ipotesi da distinguere. Nessuna compensazione specifica per T045
e' stata aggiunta. [Diagnosi per tono](model_groups_tones.json).

## Voce R1: release 250 e 500 ms

Sorgente `GAFITA Y CHOMBA_Vocal.mp3`, 44,1 kHz; originali a 48 kHz.
Nei video: MC404, crossover 100/785/10000 Hz, tutte le IN attive, nessun
SOLO. Impostazioni dichiarate: ratio 2:1, threshold -27,5 dB, attack 2,5 ms,
R1. Non coincide con il primo caso dell'esperto, che usava solo IN 2/3.

Allineamento di inviluppo con offset sorgente -50 ms, correlazione 0,959;
rifinitura di 57 campioni sull'uscita, costante nei cinque intervalli.
Nessuna deriva osservata. [Dettagli e hash](voice_comparison.json).

| Release | MAE GR audio | Bias Ponte - originale | p95 |
| --- | ---: | ---: | ---: |
| 250 ms | 0,134 dB | +0,101 dB | 0,318 dB |
| 500 ms | 0,125 dB | +0,088 dB | 0,297 dB |

L'errore del contrasto 500-250 ms e' 0,032 dB. Manca un neutro originale
della voce: per la GR assoluta viene usato il neutro Ponte; decoder MP3,
ricampionamento e gain della clip aggiungono incertezza. Queste misure non
chiudono il confronto visivo della recensione e non giustificano offset GR.

## Implementazione e verifiche ancora aperte

Auto usa ora una costante di salita di 320 us, mantenendo il rilascio a
102 ms: compromesso fra rumore e transienti BITE, non un parametro misurato
direttamente nel codice McDSP. R1/R2 invariati in questa revisione.
[Nota tecnica, nap, CPU/GUI e decisione oversampling](../../NAP_CPU_AUTO_OVERSAMPLING_2026-09-24.md).

Restano il collaudo reale in Ableton (live input, loop/seek, bypass,
salvataggio/ripristino, molte istanze), le acquisizioni GUI comparative e
la diagnosi T045. Non occorre rifare l'intero pacchetto audio. Ulteriori
sorgenti indipendenti e ascolti servono per estendere la validazione oltre
gli stimoli acquisiti, non per annullare i risultati gia' ottenuti.
