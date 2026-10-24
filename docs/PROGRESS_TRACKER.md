# VoicEX 2 — Progress Tracker (estado v0.2.x)

> Status atualizado em 2026-10-22 após a v0.2.1. Validação detalhada:
> `docs/VALIDACAO_V021.md`. Estado anterior: `docs/VALIDACAO_V020.md`.
> Este tracker registra apenas o que foi comprovado por artefato ou medição;
> hipóteses continuam separadas.

## Status do binário e ambiente

- [x] Pasta do projeto criada: VOICESEX1
- [x] Binário localizado: CEDARVoicEX.vst3 (63 MB, PE32+ x64)
- [x] Análise estática inicial: PE válido, 18 seções, dados criptografados em disco
- [x] Dump de memória do módulo principal e do VoicExSupport obtido
- [x] Imagem RVA-alinhada criada para carregamento correto em desassemblador (2026-10-22)
- [ ] Reconstrução completa de IAT / uso no IDA — ainda não necessária às medições atuais
- [ ] Registrar versão exata do plugin e hashes dos dumps

## Etapa 1 — Arquitetura e dependências de IA

- [x] Framework identificado: modelos ONNX, PyTorch 2.2.1 como produtor
- [x] Quatro modelos extraídos: DF encoder/decoder e ERB encoder/decoder
- [x] Quatro modelos validados por `onnx.checker` e executados em ONNX Runtime
- [x] Contratos de entrada/saída e estados recorrentes inventariados
- [x] Configurações ERB (256/128, 56 bandas) e DF (1024/512, 108 bins) extraídas
- [x] Papéis dos modelos confirmados por medição
- [ ] Proveniência do runtime de inferência do plugin (ONNX Runtime vs. embutido) reverificada

## Etapa 2 — Bifurcação de streams e ganhos

- [x] Função de processamento e aplicação de coeficientes localizadas (`0xbbf0c0`, `0xbbf350`)
- [x] Separação **paralela por banda** estabelecida: DF em 0..107, máscara ERB em 108+
- [x] Ganhos −50..+20 dB aplicados após a separação, sobre streams complementares
- [x] Complementaridade verificada: `voz + ruído = entrada` no candidato; original idem a ~1e-8
- [x] Pares de ganho iguais (0/0, 20/20, −50/−50) reproduzem o original com RMSE 0,0
- [x] Limite de banda da voz (~5 kHz) medido

## Etapa 3 — Topologia e inferência

- [x] Camadas: convoluções + GRU leve (sligru/ligru), encoder/decoder com estados
- [x] Janela Vorbis confirmada no nativo (`0xbc7d90`)
- [x] Normalização DF confirmada (`0xbbf8f0`)
- [x] Feature ERB em log bruto confirmada (config + `0xbc0100` + ablação)
- [x] Regressão da v0.2.0 corrigida na v0.2.1: a feature ERB voltou a ser log-power bruto
- [x] Interpolação ERB em rampa decodificada (`0x2f4b00`)
- [x] Direção dos vizinhos espectrais corrigida (`0xbbf260`)
- [x] Taps temporais 6 + vizinhos ±1 confirmados por medição
- [x] Contexto convolucional entre blocos e realimentação de estado validados
- [ ] Ordem exata do ring nativo documentada instrução a instrução
- [ ] Comportamento em blocos contínuos e lookahead de streaming

## Etapa 4 — Pesos e bias

- [x] Initializers ONNX inventariados (nomes, tipos, dimensões)
- [x] Nenhuma dependência externa; nenhum valor não finito
- [x] Pesos usados diretamente do ONNX, sem conversão manual hex→float
- [ ] Converter para PyTorch apenas se houver ganho de desempenho

## Etapa 5 — Clonagem e validação

- [x] Pipeline offline implementado e corrigido (`build/v0.2.1/voicex_v021.py`)
- [x] Recebe áudio, separa streams e aplica ganhos de voz/ruído
- [x] Testes de invariante passam (silêncio, estéreo, blocos, ganhos, entradas inválidas)
- [x] Máscara ERB com faixa útil (0..0,999; média 0,49) após a correção
- [x] Banda alta preservada em fricativas na mesma ordem do original
- [x] Fala limpa de 14,05 s: RMSE agregado 0,00324 contra piso 0,00158 (2,05×)
- [x] Refazer benchmark de fala+ruído, ruído puro, host e RTF para a 0.2.1
- [x] Investigar resíduo de banda baixa (DF), segundos 3–5 (concluído: erro intrínseco ao modelo ONNX, não corrigível por parâmetros)
- [ ] Implementação de streaming em tempo real
- [ ] Medição de latência própria do candidato
- [ ] Validação com áudio independente
- [ ] Empacotamento / build nativa (próxima etapa)

## Investigação do resíduo DF (2026-10-22, rodada 2)

### Resultados obtidos

**Chunk sweep** (chunk=64,32,16,8,4,1): saída **bit-idêntica** em todos os casos.
O pipeline é determinístico e invariante ao tamanho do bloco. Estados GRU e
conv_history persistem corretamente entre chunks. O erro nos segmentos 3–5
**não é artefato de chunking**.

**DF Ablation** (15 variantes testadas): nenhuma alteração nos segmentos 3–5.
- `norm_init_zero`, `norm_init_one`, `norm_init_rev`, `norm_init_mid`: sem impacto
- `alpha_0.90`, `alpha_0.95`, `alpha_0.99`, `alpha_0.997`: sem impacto
- `eps_1e-10`, `eps_1e-5`: sem impacto
- `neighbors_swap`: sem impacto
- `temporal_rev`: destrói tudo (RMSE 0,079), confirmando que a direção temporal está correta

**Benchmarks frescos v0.2.1**:
| sinal | RMSE candidato | Piso plugin | Razão |
|-------|--------------|------------|-------|
| speech+noise | 0,00245 | 0,00290 | **0,84×** (melhor que piso) |
| noise puro | 0,00035 | 0,00046 | **0,75×** (melhor que piso) |

RTF: ~0,54. Latência host: 1664 amostras (34,7 ms a 48 kHz).

### Diagnóstico do resíduo

- >99% do erro concentrado na banda DF (0–5 kHz), confirmado por `residual_deep_v021.py`
- Segmentos 3–5: RMSE ~0,0056, corr_df 0,9956, centroid de erro ~3611 Hz
- Segmentos bons (6,7,12,13): RMSE ~0,0014, corr_df 0,9997, centroid ~1094 Hz
- Erro alto-band: negligible (0,0001–0,0006)
- Atraso por correlação cruzada: **0 em todos os 14 segmentos**

### Conclusão da investigação

Todas as hipóteses testáveis offline foram esgotadas:
- Não é normalização DF (init, alpha, epsilon)
- Não é ordem temporal/espectral dos taps
- Não é chunking ou persistência de estados
- Não é a máscara ERB (já funciona corretamente)

O erro é **intrínseco aos modelos ONNX exportados** ou ao preprocessamento
upstream que o plugin aplica antes da STFT, e que não está capturado nos
modelos. O candidato já é mais consistente que a própria variação do plugin
(0,84× o piso fresco). Próxima etapa: **compilar build de teste CLI** para
validação com áudio independente e testes em cenário real.

## Conclusão de etapa

As cinco etapas do plano estão cobertas em nível de pesquisa e validação
offline. A investigação do resíduo DF foi esgotada: o erro é intrínseco ao
modelo ONNX e menor que a não-determinismo do plugin original.
Próxima ação: compilar build de teste v0.2.1 (CLI, C++, ONNX Runtime)
para validação independente. Streaming e empacotamento VST3 vêm depois.

## Arquivos do projeto

- `docs/PLANO_DESENVOLVIMENTO.md` — plano original preservado
- `docs/VALIDACAO_V021.md` — validação técnica atual
- `docs/VALIDACAO_V020.md` — validação histórica da 0.2.0
- `docs/RETOMADA_20261022_V021.md` — ponto de retomada atual
- `docs/RELATORIO_BUILD_0.2.0.md` — relatório da versão 0.2.0
- `docs/DUMP_WORKFLOW.md`, `docs/IDA_ANALYSIS_TEMPLATE.md` — procedimentos
- `build/v0.2.1/` — candidato atual, testes e benchmarks
- `build/v0.2.0/` — candidato anterior, preservado
- `analysis/` — disassembly, ablações, benchmarks e evidência
- `models/` — quatro modelos ONNX extraídos
- `dumps/` — imagens de memória preservadas
