# ESTADO ATUAL — fonte única de verdade

> **Este é o único arquivo que responde "o que fazer agora".**
> Todos os `RETOMADA_*.md` são históricos datados. Se este arquivo contradiz
> um deles, este vence. Última atualização: 2026-10-23.

## Instrução vigente

- **Entrega de produto:** plugin **SEXYVOX VST3** realtime para FL Studio.
  Não é app offline, não é import/export WAV.
- **Foco principal: equivalência completa com o CEDAR VoicEX 2.** A Trilha B
  está ativa e orienta as prioridades. O SEXYVOX VST3 é a forma de entrega
  dessa equivalência; sua validação no host integra o objetivo.
- Não sobrescrever artefatos: criar arquivos novos e versionados.
- Não fazer bypass de DRM/ativação.
- Distinguir observação, inferência e incerteza em toda doc nova.

## Convenção de nomes

Ver `docs/NOMENCLATURA.md`. Resumo:

- **CEDAR VoicEX 2** → o plugin original alvo.
- **candidato vX.Y.Z** → clonagem de pesquisa (Python).
- **SEXYVOX** → o VST3 de produto.

## Status das duas trilhas

### Trilha A — Produto (SEXYVOX VST3)

| Item | Estado |
|---|---|
| Fonte nativa compila (`native/sexyvox`) | ✅ |
| VST3 instalado em `C:/Program Files/Common Files/VST3/SEXYVOX.vst3` | ✅ |
| SHA256 conferido (original = instalado) | ✅ |
| Carregado via Pedalboard, parâmetros enumerados, áudio alterado | ✅ |
| **Carregado e testado no FL Studio de verdade** | ⚠️ **PENDENTE** |
| Taxa 48 kHz | ✅ |
| Outras taxas (44,1 kHz etc.) | ❌ passa dry |
| Latência declarada ao host | 8192 amostras (170,67 ms) — **alta** |
| Zero underruns em realtime estéreo/128 | ✅ com 8192 |
| Automação Voice/Noise em tempo real | ⚠️ não validado no FL |
| Estado XML salvando/restaurando no FL | ⚠️ não validado no FL |

### Trilha B — Pesquisa (candidato vs CEDAR)

| Item | Estado |
|---|---|
| Arquitetura reconstruída | ✅ |
| 4 modelos ONNX validados e executados | ✅ |
| Candidato v0.2.1 offline funcional | ✅ |
| Candidato v0.2.2 com rampa DF | ❌ rejeitado para promoção: regressão em duas falas independentes |
| Candidato v0.2.3 com fusão dinâmica DF/ERB | ⚠️ experimental; com interpolação em rampa melhora três falas e quatro casos de ruído; 11 segundos das falas independentes persistem acima do piso de 8 renders; ruído branco 0/5 acima do piso com 8 renders |
| RMSE speech+noise vs piso do plugin | 0,84× (melhor que piso) |
| RMSE noise puro vs piso do plugin | 0,75× (melhor que piso) |
| Resíduo DF (segundos 4–5) | **INVESTIGAÇÃO ATIVA**; 2,49× e 3,16× o maior piso entre 8 renders — ver `EQUIVALENCIA_EXECUCAO_20261023.md` |
| Streaming C++ vs v0.2.1 Python | paridade 0,9999999999998063 |

## Próximas ações, em ordem de prioridade

### P0 — Plano ativo de equivalência (Trilha B)

1. **Fixar referência reproduzível:** registrar versão e hash do CEDAR,
   modelos e dumps; taxa, tamanho de bloco, ganhos, alinhamento e configuração
   do host. Medir a variação entre execuções idênticas do original por sinal.
   **Parcial:** versão 2.0.26603 e hashes registrados em
   `docs/REFERENCIA_CEDAR_20261023.md`; oito renders da fala mostram cinco
   saídas distintas. Falta caracterizar a variação no corpus completo.
2. **Ampliar corpus:** pelo menos duas falas independentes (vozes distintas),
   dois ruídos distintos, fala limpa, fala+ruído, ruído puro, silêncio,
   impulsos, mono e estéreo. Preservar entradas e saídas versionadas.
   **Parcial:** duas falas reais independentes, feminina e masculina, 48 kHz,
   em `analysis/independent_20261023/`; origem e hashes em
   `docs/CORPUS_INDEPENDENTE_20261023.md`. Dois ruídos sintéticos reproduzíveis
   (branco e rosa) foram testados puros e misturados com fala, com três renders
   do CEDAR por caso. Faltam ruídos ambientais independentes, silêncio,
   impulsos, estéreo e demais combinações.
3. **Comparar por caso e por trecho:** alinhar saídas e registrar RMSE, erro
   relativo ao piso do original, correlação, energia por banda, latência,
   complementaridade Voice+Noise e comportamento dos ganhos. Separar DF
   (0–5 kHz) e ERB; identificar os piores trechos antes de alterar o pipeline.
4. **Investigar o resíduo DF 3–5 s:** testar pré-ênfase upstream e estado
   inicial/warm-up da GRU; depois rastrear pré/pós-processamento e ordem do
   ring nativo com evidência dinâmica. Registrar resultado negativo também.
   A hipótese de erro intrínseco ao ONNX continua hipótese, não conclusão.
   **Novo achado:** a máscara ERB ponderada pela energia acompanha a diferença
   de magnitude de ~3 a 5 kHz. A v0.2.3 aplica fusão dinâmica e melhorou as
   três falas, zerando os segundos acima do piso em `speech48.wav`. Com a
   interpolação em rampa, ainda há seis segundos acima do piso na fala
   feminina e cinco na masculina, mesmo com oito renders de referência.
   O ruído branco passou de cinco segundos acima do piso (três renders) a zero
   (oito renders), mostrando que o piso era subamostrado. Ver
   `docs/EQUIVALENCIA_V023_20261023.md`. Os resíduos persistentes das falas
   concentram-se em 3,5–10 kHz; confirmar a regra nativa de fusão e ganho ERB
   nessa faixa antes de promover a v0.2.3 ao produto. Uma sonda de expoente
   ERB 1,20 reduziu as falhas da fala feminina de seis para três e melhorou
   o RMSE médio das três falas, mas deixou as cinco falhas masculinas e
   **regrediu nas duas misturas de fala com ruído**. Rejeitado para promoção;
   ainda é necessário descobrir a transformação nativa exata. Uma transição
   aditiva DF/ERB melhorou as três falas e duas misturas, mas regrediu ~4,8%
   no ruído rosa puro; também não foi promovida. O ganho de −50 dB do CEDAR
   foi conferido contra `10^(-50/20)` com erro ~3,5e-12, portanto não explica
   o resíduo alto.
5. **Corrigir e revalidar:** cada mudança candidata deve melhorar o corpus
   completo sem regressão material em casos antes aprovados. Repetir a
   comparação em streaming C++ e no VST3, com blocos e automação reais.

**Critério de conclusão:** em todos os casos do corpus, o erro alinhado deve
ficar dentro do piso de variação medido do original, sem divergência audível
identificada em escuta cega dos piores trechos; preservar complementaridade,
ganhos, latência e comportamento contínuo. Se o original for determinístico
em algum caso, definir tolerância numérica antes do teste e documentar sua
justificativa. Publicar resultados por caso, incluindo falhas; não declarar
equivalência com base apenas em médias agregadas.

### P1 — Validar o produto no FL

1. **Abrir o FL com as ferramentas disponíveis, escanear, inserir o SEXYVOX e rodar 3 testes:**
   - sessão 48 kHz mono e estéreo, 30 s de fala, verificar indicador de
     underruns do FL;
   - automação dos knobs Voice/Noise durante playback (checar zipper);
   - salvar sessão, fechar, reabrir, confirmar estado restaurado.
2. **Se qualquer um dos 3 falhar**, parar e documentar o modo de falha antes
   de tentar corrigir.

### P2 — Reduzir latência do VST3 (objetivo: ≤ 42 ms, ou seja 2048 amostras)

A meta "< 10 ms" do plano antigo é irrealista em fp32 com 4 modelos. Nova
meta medida: **2048 amostras com zero underruns no FL**.

Ordem de ataque:
1. **Diagnóstico do jitter** que causou os 117 atrasos no teste de 2048.
   Hipóteses a testar explicitamente: alocação no ORT durante run, contenção
   de lock no worker, jitter de scheduling de thread, tamanho de arena.
   Sem esse diagnóstico, reverter para 8192 foi decisão cega.
2. **FP16 nos 4 modelos ONNX** + validação de paridade antes de adotar.
3. **Session options do ORT**: `graph_optimization_level=ORT_ENABLE_ALL`,
   threads fixas, `enable_cpu_mem_arena` verificado.
4. Re-testar 2048. Se passar com folga, tentar 1024.

### P3 — Suporte a 44,1 kHz com resample explícito

Resample entrada → 48k, processar, resample saída → taxa do host.
Usar `juce::ResamplingAudioSource`. Custo de CPU baixo vs inferência.
Sem isso, metade do público-alvo (produção musical em 44,1) fica de fora.

### P4 — Validação com áudio independente

Já há duas gravações independentes e dois tipos de ruído sintético, com
resultados do candidato em `docs/EQUIVALENCIA_V023_20261023.md`. Acrescentar
ruídos ambientais e usar o corpus completo também para validar o SEXYVOX.

### P5 — Melhorias de qualidade adicionais

- Reduzir banda alta "quente" (razão 1,03–1,43 no 5–24 kHz em trechos de
  baixa energia do candidato).
- Incorporar correções adicionais identificadas pela validação do corpus.

## Decisão reaberta: resíduo DF nos segundos 3–5

Investigação rodada 2 esgotou as hipóteses de parâmetros testadas (15
ablações, chunk sweep, norm_init, alpha, epsilon, ordem temporal/espectral).
Permanece sem demonstração se a diferença vem dos modelos ONNX exportados ou
de pré-processamento upstream não capturado neles.

**Status: investigação ativa na Trilha B.** Priorizar:
- caracterizar a origem dos cinco modos observados em oito renders do CEDAR;
- investigar estados internos da GRU ou outro pré-processamento ainda não
  reproduzido, com foco nos segundos 4–5;
- testar correções contra todo o corpus e os oito renders preservados.

Os testes recentes de pré-ênfase nas features DF (0,90–0,97), reinício com
histórico mais curto, deslocamento inteiro de coeficientes DF (±1–2 quadros)
e estados iniciais uniformes da GRU DF (±0,01; ±0,1) pioraram o resultado.
A variação do CEDAR ocorre até dentro da mesma instância
com `reset=True` e persistiu com entrega de blocos em ritmo real. A
desassemblagem da gravação e leitura do ring é coerente com
a ordem cronológica de taps do candidato, mas não resolve todos os detalhes de
inicialização. Evidência e comandos em
`docs/EQUIVALENCIA_EXECUCAO_20261023.md`.

## Arquivos que este documento substitui como "próxima ação"

- `RETOMADA_20261022.md` (histórico, aponta para v0.1.2)
- `RETOMADA_20261022_V021.md` (histórico, aponta para v0.2.1)
- `RETOMADA_SEXYVOX_VST3_20261022.md` (histórico, aponta para build 0.1.0)

Todos os três continuam válidos como registro; nenhum deles manda mais.
