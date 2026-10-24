# Validação — candidato 0.2.1

Registrado em: 2026-10-22.
Versão: `build/v0.2.1/voicex_v021.py`.
Status: candidato de pesquisa; sem build compilada e sem alegação de paridade
com o original. As métricas abaixo são medições contra o próprio plugin, não
uma prova de equivalência bit a bit.

## 1. O que mudou em relação a 0.2.0

A v0.2.0 chamava `core.features(..., 'erb')` do módulo 0.1.0. Essa função
ainda aplicava a normalização antiga por média móvel e a escala `/40`, que
havia sido substituída na v0.1.1 por log-power bruto. Ou seja: a v0.2.0
herdou uma regressão silenciosa na feature ERB.

A v0.2.1 define `erb_features()` localmente e usa:

```
potência por banda ERB -> 10*log10(potência + 1e-10), sem normalização
```

Isso é coerente com:

- configuração do modelo com `normalisation=false`;
- função nativa em `0xbc0100`;
- ablações anteriores que já haviam fixado log-power bruto.

### Efeito medido na máscara ERB

Fala limpa de 14,05 s, 56 bandas, 5.269 quadros:

| métrica | v0.2.0 (feature normalizada) | v0.2.1 (log bruto) |
|---|---|---|
| mínimo | 5,96e-08 | 6,37e-05 |
| máximo | 0,0434 | 0,9989 |
| média | ~1,2e-04 | 0,4890 |
| média faixa baixa (0–27) | ~3,5e-04 | 0,6380 |
| média faixa alta (27–56) | ~1,2e-04 | 0,3500 |

A máscara da v0.2.0 era degenerada (praticamente zero em toda a faixa). A da
v0.2.1 tem faixa útil e varia com o conteúdo, como esperado de uma máscara de
separação.

Consequência direta: a banda alta (>5 kHz) deixou de ser zerada durante
fricativas. Em segmentos com energia alta, o candidato 0.2.0 tinha ~0,007% da
energia acima de 5 kHz; o original mantinha 0,29%–0,52%. A v0.2.1 passou a
reter 0,32%–0,58%, na mesma ordem do original.

## 2. Medições na fala limpa de 14,05 s

Condição: voz 0 dB, ruído −50 dB, 48 kHz, estéreo idêntico, buffer 512.
Referência: render novo do plugin. O piso é a diferença entre dois renders
idênticos do próprio plugin.

| segundo | RMSE v0.2.0 | RMSE v0.2.1 | piso do plugin |
|---:|---:|---:|---:|
| 0 | 0,00120 | 0,00145 | 0,00150 |
| 1 | 0,00269 | 0,00321 | 0,00102 |
| 2 | 0,00195 | 0,00426 | 0,00244 |
| 3 | 0,00578 | 0,00588 | 0,00149 |
| 4 | 0,00575 | 0,00559 | 0,00134 |
| 5 | 0,00536 | 0,00545 | 0,00139 |
| 6 | 0,00069 | 0,00076 | 0,00095 |
| 7 | 0,00355 | 0,00175 | 0,00100 |
| 8 | 0,00193 | 0,00159 | 0,00094 |
| 9 | 0,00139 | 0,00127 | 0,00114 |
| 10 | 0,00165 | 0,00151 | 0,00142 |
| 11 | 0,00274 | 0,00206 | 0,00243 |
| 12 | 0,00555 | 0,00234 | 0,00266 |
| 13 | 0,00087 | 0,00080 | 0,00084 |
| **RMS agregado** | **0,00347** | **0,00324** | **0,00158** |

Razão agregada candidato/piso: **2,03×** na v0.2.0 e **2,05×** na v0.2.1.
A melhora agregada é pequena porque o erro dominante continua sendo a banda
baixa (DF), não a banda alta.

### O que melhorou de fato

- Segmento 7: 0,00355 → 0,00175 (piso 0,00100), com a energia >5 kHz
  preservada na mesma proporção do original.
- Segmento 12: 0,00555 → 0,00234 (piso 0,00266), agora abaixo do piso.
- A fração do erro acima de 5 kHz caiu de 20%–76% para 2%–4% nesses
  segmentos.

### O que continua em aberto

- Segmentos 3–5 concentram o maior erro (~0,0055) com o piso em ~0,0014.
  É erro de banda baixa, portanto do filtro DF, não da máscara ERB.
- A comparação por segmento usa renders novos em cada versão; a
  não-determinismo do plugin adiciona incerteza da ordem do piso.
- A v0.2.1 ainda não refez o benchmark de fala+ruído, o ruído puro, a
  comparação via host nem o RTF. As métricas de 1,14× o piso da v0.2.0
  **não devem ser transferidas automaticamente** para a 0.2.1.

## 3. Análise do arquivo inteiro: modo de máscara, atraso e nível

`whole_analysis_v021.py` foi executado sobre os 14,05 s e salvou as formas de
onda para diagnóstico posterior.

| modo | RMSE agregado | correlação |
|---|---:|---:|
| `repeat` | 0,00322959 | 0,998694 |
| `ramp` | 0,00322900 | 0,998694 |

A diferença entre os modos é de ~6e-07, muito abaixo do piso do plugin
(0,00158). Decisão: manter `repeat` como padrão; `ramp` continua disponível e
sem ganho mensurável neste material.

Atraso por correlação cruzada (±64 amostras): **0 em todos os 14 segmentos**.
Não há desalinhamento temporal residual.

Razões de nível candidato/referência:

- 0–5 kHz: 0,952 a 1,013. Os segundos 1 e 2 ficam em 0,967 e 0,952
  (candidato levemente mais baixo); os segundos 3–5 ficam em 0,991–1,005.
- 5–24 kHz: 1,03 a 1,43. A banda alta ainda fica um pouco mais quente em
  alguns trechos de baixa energia; nos trechos de fricativa (7 e 12) a razão
  fica em ~1,06, próxima do original.

Nos segundos 3–5, a correlação por segmento é 0,9945–0,9962, contra ≥0,9977
nos demais. Como o atraso é zero e a razão de nível 0–5 kHz é ~1, o resíduo
não é ganho nem desalinhamento: é diferença de forma de onda/fase dentro da
banda DF.

## 4. Testes de invariante

`build/v0.2.1/test_v021.py` passa integralmente:

- silêncio → saída zero;
- complementaridade `voz + ruído = entrada`;
- estéreo independente;
- processamento em blocos com contexto;
- limites de ganho −50..+20 dB e rejeição de valores inválidos;
- impulso com pico limitado;
- máscaras `repeat` e `ramp` dentro do piso.

Artefato: `analysis/v021_tests_20261022T104324435509Z.json`.

## 5. Investigação do resíduo DF (rodada 2 — 2026-10-22)

### Chunk sweep

`chunk_sweep_v021.py` comparou chunk=64,32,16,8,4,1. **Saída bit-idêntica**
em todos os casos. O pipeline é determinístico e invariante ao tamanho do
bloco. Estados GRU e conv_history persistem corretamente entre chunks.
Conclusão: o erro nos segmentos 3–5 não é artefato de chunking.

### DF Ablation

`df_ablation_v021.py` testou 15 variantes:
- `norm_init_zero`, `norm_init_one`, `norm_init_rev`, `norm_init_mid`: sem impacto nos BADs
- `alpha_0.90`, `alpha_0.95`, `alpha_0.99`, `alpha_0.997`: sem impacto
- `eps_1e-10`, `eps_1e-5`: sem impacto
- `neighbors_swap`: sem impacto nos BADs
- `temporal_rev`: destrói tudo (RMSE 0,079), confirmando direção temporal correta
- Combinações `rev_temp_zero`, `rev_temp_one`: idem

**Nenhuma configuração melhora os segmentos 3–5.** Todas as hipóteses de
parâmetros da normalização DF, ordem temporal/espectral e epsilon foram
esgotadas.

### Benchmarks frescos

| sinal | RMSE candidato | Piso plugin | Razão |
|-------|--------------|------------|-------|
| speech+noise | 0,00245 | 0,00290 | **0,84×** |
| noise puro | 0,00035 | 0,00046 | **0,75×** |

RTF: ~0,54 (CPU, 1 thread). Latência host: 1664 amostras (34,7 ms @ 48 kHz).
Correlação speech+noise: 0,9994.

### Diagnóstico conclusivo do resíduo

- >99% do erro na banda DF (0–5 kHz)
- BADs (3–5): RMSE 0,0056, corr_df 0,9956, centroid erro 3611 Hz
- GOODs (6,7,12,13): RMSE 0,0014, corr_df 0,9997, centroid erro 1094 Hz
- Erro alto-band: negligible (0,0001–0,0006)
- Atraso: 0 em todos os 14 segmentos

### Veredicto

O resíduo DF é **intrínseco aos modelos ONNX exportados** ou ao
preprocessamento upstream do plugin não capturado nos modelos. Não é
corrigível por ajuste de parâmetros no pipeline atual. O candidato já
é mais consistente que a própria variação interna do plugin.

## 6. Comandos de reprodução

```
cd build\v0.2.1
py test_v021.py
py whole_diag_v021.py
py whole_analysis_v021.py
py benchmark_v021.py
py noise_floor_v021.py
py host_compare_v021.py
py residual_deep_v021.py
py df_ablation_v021.py
py chunk_sweep_v021.py
```

## 7. Artefatos desta rodada

### Rodada 1 (v0.2.1 inicial)
- `build/v0.2.1/voicex_v021.py` — candidato 0.2.1
- `build/v0.2.1/test_v021.py` — suíte de invariantes
- `build/v0.2.1/whole_diag_v021.py` — diagnóstico por segundo
- `build/v0.2.1/whole_analysis_v021.py` — análise de modos, atraso e nível
- `build/v0.2.1/benchmark_v021.py` — benchmark de referências salvas
- `build/v0.2.1/host_compare_v021.py` — comparação fresca via pedalboard
- `build/v0.2.1/noise_floor_v021.py` — piso de repetibilidade
- `analysis/v021_tests_20261022T104324435509Z.json`
- `analysis/v021_whole_diag_20261022T104357907620Z.json`
- `analysis/v021_whole_analysis_20261022T104825451129Z.json`
- `analysis/v021_waveforms_20261022T104825451129Z.npz` — entrada + 2 renders + 2 modos
- `analysis/v020_whole_file_20261022T103903124488Z.json` — medição agregada da 0.2.0
- `analysis/v020_whole_diag_20261022T104029977047Z.json` — diagnóstico da 0.2.0
- `analysis/v020_mask_stats_20261022T104212028225Z.json` — máscara degenerada da 0.2.0

### Rodada 2 (investigação do resíduo DF — 2026-10-22)
- `build/v0.2.1/residual_deep_v021.py` — decomposição do erro por banda
- `build/v0.2.1/df_ablation_v021.py` — 15 variantes de normalização/ordem
- `build/v0.2.1/chunk_sweep_v021.py` — sweep de chunk size (64→1)
- `analysis/v021_residual_deep_20261022T232729363944Z.json`
- `analysis/v021_benchmark_20261022T232125274811Z.json`
- `analysis/v021_noise_floor_20261022T232211226316Z.json`
- `analysis/v021_host_20261022T232429143584Z.json`
- `analysis/df_ablation_20261022T232945840611Z.json`
- `analysis/chunk_sweep_20261022T235219833267Z.json`
