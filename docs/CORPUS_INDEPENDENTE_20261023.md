# Corpus independente — 2026-10-23

Foram obtidas duas falas de vozes distintas do [conjunto público de fala
conversacional do WebRTC](https://webrtc.googlesource.com/src/+/798e45187873d45b9251e56b9ff3345909af958e/resources/audio_processing/conversational_speech/README.md).
Os arquivos são WAV PCM mono, 16 bits, 48 kHz, sem conversão. A origem usa
arquivos `.sha1` para apontar ao bucket `chromium-webrtc-resources`; o SHA-1
de cada download foi conferido com o valor publicado.

| Arquivo local | Voz | Duração | SHA-1 |
|---|---|---:|---|
| `analysis/independent_20261023/EN_script2_F_sp1_B1.wav` | feminina | 21,136 s | `a6a0265063f9e7b1ba830a35dda2f05399dec3c1` |
| `analysis/independent_20261023/EN_script2_M_sp1_B1.wav` | masculina | 17,000 s | `abdbd2a518c2fa26ffb922b4279aa32f6a1c7366` |

Comando de comparação: `py -3.10 build/v0.2.2/independent_compare_v022.py`.
Cada sinal recebeu 1 s de preroll e três renders do CEDAR VoicEX 2.0.26603,
com Voice 0 dB, Noise -50 dB, buffer 512. Resultado completo:
`analysis/v022_independent_compare_20261023T034756480637Z.json`.

| Fala | v0.2.1: segundos acima do piso | v0.2.2: segundos acima do piso | RMSE médio por segundo v0.2.1 | v0.2.2 |
|---|---:|---:|---:|---:|
| feminina (21 s completos) | 5 | **16** | 0,000122 | **0,000352** |
| masculina (17 s) | 7 | **17** | 0,000326 | **0,001116** |

**Observação:** a rampa DF da v0.2.2 melhorou o material original de 14 s,
mas piorou fortemente ambas as falas independentes. Ela **não deve ser
promovida** ao produto. A v0.2.1 continua como referência de pesquisa atual.
Os pisos destas duas falas foram estimados com três renders cada, portanto as
contagens exatas podem mudar com mais execuções; a diferença de RMSE entre
versões é suficientemente grande para rejeitar a rampa fixa neste corpus.

**Inferência:** a atenuação observada perto de 5 kHz nos segundos 4–5 do
arquivo `speech48.wav` depende do conteúdo ou de alguma etapa do original.
Uma curva fixa para todas as entradas não reproduz essa etapa. Ainda faltam
ruídos independentes e validação no VST3 em host real.
