# Piano completo dei render originali

Generato da `generate.py`. Ogni riga = un export stereo di **60.000 s**.
Impostare tutti i valori indicati su **tutte le bande**; IN tutti ON.
Input/Output/Gain 0 dB, fase normale, LINK UNLINKED; leggere prima README.md.
B0 = riferimento ratio 1:1. AUTO = pulsante A/Auto ON; R1/R2 = Auto OFF e relativa legge.
Xover MC404: 100/785/10000 Hz; MC303: 100/785 Hz; MC202: 785 Hz.
I numeri sorgente corrispondono alla tabella audio nel README; il nome completo e ogni parametro sono in render_plan.json.


## P0_AUTO_PRIMA

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T001 | 01 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T002 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T003 | 01 | MC404 / 48000 | [2] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T004 | 01 | MC404 / 48000 | [2] | R2 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T005 | 02 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T005 |
| T006 | 02 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T005 |
| T007 | 02 | MC404 / 48000 | [2] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T005 |
| T008 | 02 | MC404 / 48000 | [2] | R2 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T005 |
| T009 | 06 | MC404 / 48000 | [3] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T009 |
| T010 | 06 | MC404 / 48000 | [3] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T009 |
| T011 | 06 | MC404 / 48000 | [3] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T009 |
| T012 | 06 | MC404 / 48000 | [3] | R2 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T009 |

## P1_AUTO_ESTESO

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T013 | 03 | MC404 / 48000 | [3] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T013 |
| T014 | 03 | MC404 / 48000 | [3] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T013 |
| T015 | 04 | MC404 / 48000 | [3] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T015 |
| T016 | 04 | MC404 / 48000 | [3] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T015 |
| T017 | 05 | MC404 / 48000 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T017 |
| T018 | 05 | MC404 / 48000 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T017 |
| T019 | 07 | MC404 / 48000 | [4] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T019 |
| T020 | 07 | MC404 / 48000 | [4] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T019 |
| T021 | 08 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T021 |
| T022 | 08 | MC404 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T021 |
| T023 | 09 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T023 |
| T024 | 09 | MC404 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T023 |
| T025 | 15 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T025 |
| T026 | 15 | MC404 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T025 |

## P2_CONTROLLI

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T027 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 5 | 2.5 | 250 | interna / nessuna | T001 |
| T028 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 10 | 2.5 | 250 | interna / nessuna | T001 |
| T029 | 06 | MC404 / 48000 | [3] | AUTO | 2:1 | -27.5 | 0 | 5 | 2.5 | 250 | interna / nessuna | T009 |
| T030 | 06 | MC404 / 48000 | [3] | AUTO | 2:1 | -27.5 | 0 | 10 | 2.5 | 250 | interna / nessuna | T009 |
| T031 | 01 | MC404 / 48000 | [2] | AUTO | 4:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T032 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -18 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T033 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | -5 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T034 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 10 | 1 | 2.5 | 250 | interna / nessuna | T001 |
| T035 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 0.25 | 25 | interna / nessuna | T001 |
| T036 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 25 | 2500 | interna / nessuna | T001 |
| T037 | 01 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T001 |

## P3_MODELLI

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T038 | 01 | MC202 / 48000 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T038 |
| T039 | 01 | MC202 / 48000 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T038 |
| T040 | 08 | MC202 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T040 |
| T041 | 08 | MC202 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T040 |
| T042 | 01 | MC303 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T042 |
| T043 | 01 | MC303 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T042 |
| T044 | 08 | MC303 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T044 |
| T045 | 08 | MC303 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T044 |

## P4_STEREO

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T046 | 12 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T046 |
| T047 | 12 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T046 |
| T048 | 12 | MC404 / 48000 | [2] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T046 |

## P5_SIDECHAIN_CONDIZIONALE

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T049 | 10 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | EXT 11 / nessuna | T049 |
| T050 | 10 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | EXT 11 / nessuna | T049 |
| T051 | 10 | MC404 / 48000 | [2] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | EXT 11 / nessuna | T049 |
| T052 | 10 | MC404 / 48000 | [2] | R2 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | EXT 11 / nessuna | T049 |
| T053 | 10 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T053 |
| T054 | 10 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T053 |

## P6_AUTOMAZIONE

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T055 | 13 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T055 |
| T056 | 13 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T055 |
| T057 | 13 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / MANUAL_TIMES | T055 |
| T058 | 13 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / MODES | T055 |
| T059 | 13 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / THRESHOLD | T055 |
| T060 | 13 | MC404 / 48000 | [2] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / CROSSOVER | T060 |
| T061 | 13 | MC404 / 48000 | [2] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / CROSSOVER | T060 |

## P7_SAMPLE_RATE

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T062 | 18 | MC404 / 44100 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T062 |
| T063 | 18 | MC404 / 44100 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T062 |
| T064 | 18 | MC404 / 44100 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T064 |
| T065 | 18 | MC404 / 44100 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T064 |
| T066 | 18 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T066 |
| T067 | 18 | MC404 / 48000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T066 |
| T068 | 18 | MC404 / 48000 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T068 |
| T069 | 18 | MC404 / 48000 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T068 |
| T070 | 18 | MC404 / 88200 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T070 |
| T071 | 18 | MC404 / 88200 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T070 |
| T072 | 18 | MC404 / 88200 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T072 |
| T073 | 18 | MC404 / 88200 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T072 |
| T074 | 18 | MC404 / 96000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T074 |
| T075 | 18 | MC404 / 96000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T074 |
| T076 | 18 | MC404 / 96000 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T076 |
| T077 | 18 | MC404 / 96000 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T076 |
| T078 | 18 | MC404 / 192000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T078 |
| T079 | 18 | MC404 / 192000 | nessuno | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T078 |
| T080 | 18 | MC404 / 192000 | [1] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T080 |
| T081 | 18 | MC404 / 192000 | [1] | AUTO | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T080 |

## P8_ESPERTO

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T082 | 14 | MC404 / 48000 | [2, 3] | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T082 |
| T083 | 14 | MC404 / 48000 | [2, 3] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T082 |
| T084 | 14 | MC404 / 48000 | [2, 3] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 500 | interna / nessuna | T082 |
| T085 | 01 | MC404 / 48000 | [2] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 500 | interna / nessuna | T001 |
| T086 | 06 | MC404 / 48000 | [3] | R1 | 2:1 | -27.5 | 0 | 1 | 2.5 | 500 | interna / nessuna | T009 |

## P9_METER

| ID | Sorgente | Modello / Hz | SOLO | TC | COMP | THRESH dB | KNEE | BITE | ATT ms | REL ms | SC / automazione | B0 di riferimento |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| T087 | 16 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T087 |
| T088 | 17 | MC404 / 48000 | nessuno | R1 | 1:1 | -27.5 | 0 | 1 | 2.5 | 250 | interna / nessuna | T088 |

## Nomi esatti degli export

| ID | Nome file |
| --- | --- |
| T001 | `T001_ORIG_MC404_01_B2_B0_48000Hz_take1.wav` |
| T002 | `T002_ORIG_MC404_01_B2_AUTO_48000Hz_take1.wav` |
| T003 | `T003_ORIG_MC404_01_B2_R1_48000Hz_take1.wav` |
| T004 | `T004_ORIG_MC404_01_B2_R2_48000Hz_take1.wav` |
| T005 | `T005_ORIG_MC404_02_B2_B0_48000Hz_take1.wav` |
| T006 | `T006_ORIG_MC404_02_B2_AUTO_48000Hz_take1.wav` |
| T007 | `T007_ORIG_MC404_02_B2_R1_48000Hz_take1.wav` |
| T008 | `T008_ORIG_MC404_02_B2_R2_48000Hz_take1.wav` |
| T009 | `T009_ORIG_MC404_06_B3_B0_48000Hz_take1.wav` |
| T010 | `T010_ORIG_MC404_06_B3_AUTO_48000Hz_take1.wav` |
| T011 | `T011_ORIG_MC404_06_B3_R1_48000Hz_take1.wav` |
| T012 | `T012_ORIG_MC404_06_B3_R2_48000Hz_take1.wav` |
| T013 | `T013_ORIG_MC404_03_B3_B0_48000Hz_take1.wav` |
| T014 | `T014_ORIG_MC404_03_B3_AUTO_48000Hz_take1.wav` |
| T015 | `T015_ORIG_MC404_04_B3_B0_48000Hz_take1.wav` |
| T016 | `T016_ORIG_MC404_04_B3_AUTO_48000Hz_take1.wav` |
| T017 | `T017_ORIG_MC404_05_B1_B0_48000Hz_take1.wav` |
| T018 | `T018_ORIG_MC404_05_B1_AUTO_48000Hz_take1.wav` |
| T019 | `T019_ORIG_MC404_07_B4_B0_48000Hz_take1.wav` |
| T020 | `T020_ORIG_MC404_07_B4_AUTO_48000Hz_take1.wav` |
| T021 | `T021_ORIG_MC404_08_ALL_B0_48000Hz_take1.wav` |
| T022 | `T022_ORIG_MC404_08_ALL_AUTO_48000Hz_take1.wav` |
| T023 | `T023_ORIG_MC404_09_ALL_B0_48000Hz_take1.wav` |
| T024 | `T024_ORIG_MC404_09_ALL_AUTO_48000Hz_take1.wav` |
| T025 | `T025_ORIG_MC404_15_ALL_B0_48000Hz_take1.wav` |
| T026 | `T026_ORIG_MC404_15_ALL_AUTO_48000Hz_take1.wav` |
| T027 | `T027_ORIG_MC404_01_B2_AUTO_B5_48000Hz_take1.wav` |
| T028 | `T028_ORIG_MC404_01_B2_AUTO_B10_48000Hz_take1.wav` |
| T029 | `T029_ORIG_MC404_06_B3_AUTO_B5_48000Hz_take1.wav` |
| T030 | `T030_ORIG_MC404_06_B3_AUTO_B10_48000Hz_take1.wav` |
| T031 | `T031_ORIG_MC404_01_B2_AUTO_R4_48000Hz_take1.wav` |
| T032 | `T032_ORIG_MC404_01_B2_AUTO_TH18_48000Hz_take1.wav` |
| T033 | `T033_ORIG_MC404_01_B2_AUTO_KM5_48000Hz_take1.wav` |
| T034 | `T034_ORIG_MC404_01_B2_AUTO_KP10_48000Hz_take1.wav` |
| T035 | `T035_ORIG_MC404_01_B2_AUTO_MAN_LOW_48000Hz_take1.wav` |
| T036 | `T036_ORIG_MC404_01_B2_AUTO_MAN_HIGH_48000Hz_take1.wav` |
| T037 | `T037_ORIG_MC404_01_B2_AUTO_REPEAT_48000Hz_take1.wav` |
| T038 | `T038_ORIG_MC202_01_B1_B0_48000Hz_take1.wav` |
| T039 | `T039_ORIG_MC202_01_B1_AUTO_48000Hz_take1.wav` |
| T040 | `T040_ORIG_MC202_08_ALL_B0_48000Hz_take1.wav` |
| T041 | `T041_ORIG_MC202_08_ALL_AUTO_48000Hz_take1.wav` |
| T042 | `T042_ORIG_MC303_01_B2_B0_48000Hz_take1.wav` |
| T043 | `T043_ORIG_MC303_01_B2_AUTO_48000Hz_take1.wav` |
| T044 | `T044_ORIG_MC303_08_ALL_B0_48000Hz_take1.wav` |
| T045 | `T045_ORIG_MC303_08_ALL_AUTO_48000Hz_take1.wav` |
| T046 | `T046_ORIG_MC404_12_B2_B0_48000Hz_take1.wav` |
| T047 | `T047_ORIG_MC404_12_B2_AUTO_48000Hz_take1.wav` |
| T048 | `T048_ORIG_MC404_12_B2_R1_48000Hz_take1.wav` |
| T049 | `T049_ORIG_MC404_10_B2_B0_EXT_48000Hz_take1.wav` |
| T050 | `T050_ORIG_MC404_10_B2_AUTO_EXT_48000Hz_take1.wav` |
| T051 | `T051_ORIG_MC404_10_B2_R1_EXT_48000Hz_take1.wav` |
| T052 | `T052_ORIG_MC404_10_B2_R2_EXT_48000Hz_take1.wav` |
| T053 | `T053_ORIG_MC404_10_B2_B0_48000Hz_take1.wav` |
| T054 | `T054_ORIG_MC404_10_B2_AUTO_48000Hz_take1.wav` |
| T055 | `T055_ORIG_MC404_13_B2_B0_48000Hz_take1.wav` |
| T056 | `T056_ORIG_MC404_13_B2_AUTO_48000Hz_take1.wav` |
| T057 | `T057_ORIG_MC404_13_B2_AUTO_MAN_MOVE_48000Hz_take1.wav` |
| T058 | `T058_ORIG_MC404_13_B2_MODE_SWITCH_48000Hz_take1.wav` |
| T059 | `T059_ORIG_MC404_13_B2_THRESH_MOVE_48000Hz_take1.wav` |
| T060 | `T060_ORIG_MC404_13_B2_B0_XOVER_48000Hz_take1.wav` |
| T061 | `T061_ORIG_MC404_13_B2_AUTO_XOVER_48000Hz_take1.wav` |
| T062 | `T062_ORIG_MC404_18_ALL_B0_44100Hz_take1.wav` |
| T063 | `T063_ORIG_MC404_18_ALL_AUTO_44100Hz_take1.wav` |
| T064 | `T064_ORIG_MC404_18_B1_B0_44100Hz_take1.wav` |
| T065 | `T065_ORIG_MC404_18_B1_AUTO_44100Hz_take1.wav` |
| T066 | `T066_ORIG_MC404_18_ALL_B0_48000Hz_take1.wav` |
| T067 | `T067_ORIG_MC404_18_ALL_AUTO_48000Hz_take1.wav` |
| T068 | `T068_ORIG_MC404_18_B1_B0_48000Hz_take1.wav` |
| T069 | `T069_ORIG_MC404_18_B1_AUTO_48000Hz_take1.wav` |
| T070 | `T070_ORIG_MC404_18_ALL_B0_88200Hz_take1.wav` |
| T071 | `T071_ORIG_MC404_18_ALL_AUTO_88200Hz_take1.wav` |
| T072 | `T072_ORIG_MC404_18_B1_B0_88200Hz_take1.wav` |
| T073 | `T073_ORIG_MC404_18_B1_AUTO_88200Hz_take1.wav` |
| T074 | `T074_ORIG_MC404_18_ALL_B0_96000Hz_take1.wav` |
| T075 | `T075_ORIG_MC404_18_ALL_AUTO_96000Hz_take1.wav` |
| T076 | `T076_ORIG_MC404_18_B1_B0_96000Hz_take1.wav` |
| T077 | `T077_ORIG_MC404_18_B1_AUTO_96000Hz_take1.wav` |
| T078 | `T078_ORIG_MC404_18_ALL_B0_192000Hz_take1.wav` |
| T079 | `T079_ORIG_MC404_18_ALL_AUTO_192000Hz_take1.wav` |
| T080 | `T080_ORIG_MC404_18_B1_B0_192000Hz_take1.wav` |
| T081 | `T081_ORIG_MC404_18_B1_AUTO_192000Hz_take1.wav` |
| T082 | `T082_ORIG_MC404_14_B2+3_B0_48000Hz_take1.wav` |
| T083 | `T083_ORIG_MC404_14_B2+3_R1_250_48000Hz_take1.wav` |
| T084 | `T084_ORIG_MC404_14_B2+3_R1_500_48000Hz_take1.wav` |
| T085 | `T085_ORIG_MC404_01_B2_R1_500_48000Hz_take1.wav` |
| T086 | `T086_ORIG_MC404_06_B3_R1_500_48000Hz_take1.wav` |
| T087 | `T087_ORIG_MC404_16_ALL_B0_48000Hz_take1.wav` |
| T088 | `T088_ORIG_MC404_17_ALL_B0_48000Hz_take1.wav` |

Totale: 88 export; ciascuno dura 60 secondi. La sidechain esterna e condizionata alla disponibilita effettiva.

## Stato acquisizioni 2026-09-24

Ricevuti i render nativi 96/192 kHz e la voce; buffer 512 confermato.
T049-T054: gruppo external-sidechain non eseguibile nell'originale disponibile
(inclusi i controlli T053/T054). T087/T088 a 96 kHz restano diagnostici
rispetto al piano a 48 kHz. Non ripetere l'intero pacchetto gia' acquisito.
[Risultati, caso T045 e verifiche residue](analysis_2026-09-24/REPORT.md).
