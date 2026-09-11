# Provenienza e riproduzione delle misure

Analisi del 2026-09-09. Nessuna modifica al plugin e nessun nuovo render del
DSP corrente. Conclusioni e limiti in `../EXPERT_REVIEW_2026-09-09.md`.

## Materiali

- ZIP originale: `../test_wav_raw_mc2000.zip`, 343154828 byte.
  SHA256: `8275B1C4FD19DF13D0F5A785E274EF8DF7C04AD449315308312178B7C5436097`.
- `archive-index.csv`: nomi completi, dimensioni espanse e compresse delle
  2959 voci. Totale espanso 10412134140 byte, prima di espandere lo ZIP interno.
- `ARCHIVE_SPECIFICATIONS.md`: copia dei 37 Markdown interni; ogni documento
  è preceduto dal percorso esatto nell'archivio. Contiene procedure storiche,
  non necessariamente risultati; preservati anche errori testuali originali.
- `baseline-MANIFEST.csv`: intervalli temporali degli stimoli del primo pack.
- `extracted-audio.csv`: i soli 15 WAV estratti, 27310706 byte complessivi.
- `audio-provenance.csv`: hash SHA256 e header dei 15 WAV. I CSV numerici
  conservati usano il punto decimale anche su sistema italiano.
- `traces/`: undici CSV prodotti leggendo le quattro sorgenti e gli undici
  render originali. I WAV estratti vengono eliminati dopo l'analisi.

## Strumento e procedura

Eseguibile esistente:
`.analysis-build-mc2000/MC2000BlackBoxAnalysis_artefacts/Release/MC2000BlackBoxAnalysis.exe`.
Data locale: 2026-08-28 15:38:43.
SHA256: `07D9BB445D1641504B62BF7C28B9C24E5E464AA6AD58B67ACD10BABD06BE7582`.

Per ogni render della selezione, modalità:

`MC2000BlackBoxAnalysis --trace-gain <source.wav> <render.wav>`

La sorgente si ottiene dal nome del render rimuovendo il suffisso che inizia
con `-with` e ripristinando `.wav`. Questa modalità legge i file e calcola
rapporti RMS; non esegue il compressore incluso nell'analizzatore storico.

Tracce: canale 0, finestre da 10 ms, `time_s` è l'inizio della finestra,
`input_dbfs` è RMS e `gain_db = 20 log10(RMSout/RMSin)`. Non sono letture dei
meter. Header controllati: IEEE float32 stereo, 48 kHz; durata 6 s T09,
3.9 s T10. Non è certificata qui la versione/build McDSP che creò i render.

## Release

`release-measurements.csv` usa la media dei `gain_db` delle finestre con
inizio fra 1.50 inclusi e 1.95 esclusi per la baseline; fra 3.50 inclusi e
3.95 esclusi per il plateau. GR iniziale = baseline meno plateau.

L'evento di discesa è a 4 s. Cercare la prima finestra con inizio >=4.02 s
e GR <=50% o <=10% del plateau. Riportare il suo inizio meno 4 s, in ms.
La prima finestra che include la discontinuità è contaminata dalla risposta
del crossover; non usarla come attraversamento della release. Risoluzione
10 ms, senza interpolazione. La misura al 10% per R1000 resta vuota perché
la registrazione finisce prima del recupero richiesto.

Confronto analitico limitato alla coda:

GR_formula(t) = GR_originale_plateau × exp(-((t-4)/(1.779 R))^1.286).

R espresso in secondi. Valutare t al centro di ciascuna finestra da 4.02 s
in poi; confrontare con baseline meno gain_db. RMS e massimo dell'errore
sono calcolati fino all'ultima finestra disponibile. La normalizzazione al
plateau originale esclude dal confronto eventuali errori di GR del plugin:
non interpretare questi numeri come errore del DSP completo o waveform null.

## Secondo evento

`second-event-measurements.csv`: medie dei gain_db con inizio finestra nei
tre intervalli [1.67,1.70), [1.87,1.90), [2.00,2.03) secondi. Il secondo
evento dura da 1.7 a 1.9 s. I valori sono guadagni negativi, non GR positive.
Il manuale e i TEST_SPEC spiegano la distinzione R1/R2 e i parametri usati.

## Spazio e conservazione

Conservati solo indice, specifiche e CSV piccoli; non duplicato l'archivio
originale. La directory `.expert-audit-temp-20260909` è stata rimossa:
28012313 byte temporanei eliminati e assenza della directory verificata.
Lo ZIP originale non va eliminato dalla
cartella sincronizzata: la liberazione della sua copia locale usa lo stato
cloud/unpinned, preservando l'originale remoto.

Eseguito `attrib -p +u` sul solo ZIP. Verifica finale: attributi 5248544
(placeholder online-only); `fsutil file queryextents` riporta un'unica
estensione sparse con LCN `0xffffffffffffffff`, nessuna estensione dati
allocata. Il contenuto può essere scaricato nuovamente quando necessario.
Riferimento operativo: [Microsoft Files On-Demand](https://learn.microsoft.com/en-us/sharepoint/files-on-demand-windows).
