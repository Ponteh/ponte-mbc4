# Checklist operativa 0.2.3 - stato consolidato

Aggiornata il 24 settembre 2026. Candidata **0.2.3, DSP_MODEL_9**.
Questa e' la lista operativa: il piano dettagliato e gli stati storici
rimangono nella storia Git fino al commit `72e474b`. Non confondere una
funzione implementata con equivalenza perfetta all'originale o collaudo DAW.
Eliminare questo file soltanto alla chiusura delle voci applicabili.

## 1. Algoritmi e acquisizioni

- [x] Inventario, hash, riferimenti neutri e separazione video/export offline.
- [x] Legge R1 di release del controllo lineare, regressioni 250/500 ms,
  continuita', ratio/knee numerici e automazione (DSP_MODEL_7).
- [x] Auto BITE ricalibrato sui render corretti (DSP_MODEL_8).
- [x] Rumore Auto indagato con candidati e transienti di controllo:
  salita 320 us, release 102 ms (DSP_MODEL_9), nessun offset artificiale GR.
- [x] 80 confronti completi: 40 migliorano, 39 invariati entro code numeriche,
  T045 peggiora ed e' mantenuto diagnostico. R1/R2 invariati nel modello 9.
- [x] Nuovi Fs nativi 96/192 kHz e correzioni MC202/MC303 isolato/ALL ricevuti;
  buffer DAW 512 confermato. T087/T088 a 96 kHz sono supplementi diagnostici.
- [x] Voce R1 250/500 ms analizzata con MP3: MAE 0,134/0,125 dB. Routing
  tutte IN; manca un neutro originale e non coincide col primo esempio IN 2/3.
- [x] Sidechain originale T049-T054: **N/A nel setup disponibile**, non
  acquisita. Sidechain Ponte verificata internamente; nessuna richiesta di rifarla.
- [ ] Chiarire T045 e verificare il caso esatto della recensione: servono
  preset/sessione originale e catture mirate dell'utente, non altri 80 export.
- [ ] Ascolti su materiale reale e nuove sorgenti indipendenti per estendere
  la validazione. I dati attuali non certificano ogni ratio/knee/BITE originale.

[Analisi originali/voce](Research/NEXT_RELEASE_ORIGINAL_TEST_PACK/analysis_2026-09-24/REPORT.md).

## 2. CPU attiva e callback

- [x] Baseline, benchmark ripetibili, hardware/compilatore/hash archiviati.
- [x] Cache coefficienti, target gain e snapshot; niente approssimazioni exp/pow.
- [x] 54 confronti a parita' di modello bit-identici; guadagno mediano
  circa 3,1% nella misura precedente. Non e' il carico totale della DAW.
- [x] Audit del percorso callback e guardie runtime: allocazioni, ingressi
  importati a lock/attese/I/O; prova positiva e limiti della copertura documentati.
- [x] Verificato che il detector interno riusa il programma gia' filtrato;
  il secondo crossover elabora solo la key esterna.
- [x] Matrice completa scelta: 2.160 combinazioni, 21.600 callback,
  zero errore audio, zero violazioni osservate; stress concorrente separato.
- [x] Profilo snapshot/FIFO/DSP, p99/massimi/deadline e cicli del thread.
  Picchi temporali conservati nel rapporto, nessuna promessa di realtime assoluto.
- [ ] Carico sostenibile e dropout sul computer/driver/sessione Ableton dell'utente.

## 3. Nap

- [x] Active/Draining/Sleeping; zero digitale, nessun gate/timeout della release.
- [x] Drenaggio filtri, Auto/R1/R2/BITE, smoothing e meter; stati inattivi conservati.
- [x] Wake nel blocco corrente, impulsi in tutte le 512 posizioni, segnale 1e-30,
  key attiva senza programma, cambi di parametri/modo/bande e release massima.
- [x] 36 confronti nap ON/OFF; errore audio massimo osservato zero.
- [x] Riduzione costo DSP in silenzio 99,58%; tail audio dichiarata 2 s,
  distinta dalla memoria GR. Nessuna latenza di processing aggiunta.
- [ ] NAP-07 in host: live input, sospensione callback, stop/seek/loop,
  bypass, freeze/export. La parte numerica automatica e' coperta.

## 4. GUI, header e help

- [x] Repaint selettivo, cache risposta LR4 e testo knob; FFT nulle saltate.
- [x] FIFO ferma senza editor visibile, scarto dati vecchi e ripresa;
  test chiuso/nascosto/riaperto e audio identico con consumer ON/OFF.
- [x] Pubblicazione atomica delle curve, paint passivo e ballistics in secondi.
- [x] Focus 30/100 ms, editing doppio click e conservazione dei colori verificati.
- [x] Versione da CMake, avviso aggiornamento asincrono grigio/giallo;
  confronti versione, dati malformati e ripristino avviso dopo help testati.
- [x] Help sostituisce header e avviso; test di layout minimo e ripristino.
  Nessun controllo oversampling da nascondere in questa candidata.
- [x] Profilo timer/FFT/risposta/paint/layout/memoria, 1/4 istanze e cinque
  ripetizioni. Ridisegno software completo distinto dai repaint naturali.
- [ ] Verifica visiva DPI/display reali, resize e interazioni in Ableton.
- [ ] Confronto GR/IN/OUT/MAIN sincronizzato con l'originale sulla build nuova.

## 5. Oversampling

- [x] Prototipo completo 1x/2x/4x con riferimenti 8x/16x, 300 render;
  separati alias candidati, variazioni di livello/fase e incertezza di convergenza.
- [x] Misurati costo e latenza FIR/IIR JUCE; decisione **NO-GO 0.2.3**.
- [x] Parametri/UI, PDC, cambio fattore e OS-02..06: **non applicabili**
  senza integrazione. Non sono funzioni dimenticate.
- [x] Ulteriori prototipi parziali e ascolti OS: differiti a ricerca futura;
  nessuna affermazione che ogni schema OS sia inutile o inudibile.

## 6. Test, documentazione e consegna

- [x] Suite locale DSP/GUI/realtime/nap **4/4 PASS**; niente soglie CPU
  instabili nei test CI. Benchmark eseguiti separatamente.
- [x] Corretto workflow: compilazione target nap, test matrice realtime,
  archiviazione log/JUnit/rapporti anche in caso di errore.
- [ ] Esecuzione remota GitHub sul commit aggiornato: da registrare dopo push.
- [x] POC, audit, recensione, README, CompanyGUI, dati e script aggiornati.
- [x] Build VST3 candidata 0.2.3 verificata, pacchetto e hash archiviati.
- [ ] Collaudo utente e risoluzione/accettazione esplicita dei limiti residui.
- [ ] Pubblicazione release quando autorizzata; poi eliminazione di questa
  checklist. L'autorizzazione alla 0.2.2 non pubblica automaticamente la 0.2.3.

## Riferimenti operativi

- [Verifiche tecniche e loro limiti](Research/technical_validation_2026-09-24/REPORT.md).
- [Nap, CPU, Auto e oversampling](Research/NAP_CPU_AUTO_OVERSAMPLING_2026-09-24.md).
- [Solo le prove che richiedono l'utente](Research/VALIDAZIONE_UTENTE_0.2.3.md).

Il lavoro tecnico locale qui elencato e' completato nel perimetro misurato.
Le caselle residue distinguono host/originale, verifica remota e pubblicazione;
non attestano bug gia' dimostrati ne' richiedono di rifare la matrice audio.
