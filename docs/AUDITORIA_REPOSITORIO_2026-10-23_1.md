# Auditoria do repositório SEXYVOX — 2026-10-23

Auditoria concluída somente em leitura. A equivalência completa ainda não está demonstrada, e a v0.2.3 não está integrada ao VST3.

Este arquivo preserva o relatório entregue na conversa. Seu salvamento foi autorizado posteriormente pelo usuário; não foi refeita a auditoria nem foram executadas novas análises para produzir este documento.

## Estado confirmado

- Existem quatro modelos ONNX, dumps preservados, candidatos Python, motor C++, build VST3 e resultados experimentais. Os hashes dos modelos e dos três dumps principais conferem com os registros. O relatório ONNX contém checker aprovado e 16 execuções sintéticas aprovadas; não repeti essas inferências.
- O CEDAR instalado corresponde à versão **2.0.26603**, com o hash documentado.
- A [v0.2.3 Python](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/build/v0.2.3/voicex_v023.py:21) implementa fusão dinâmica DF/ERB entre 2 e 5,2 kHz e recompõe Noise como entrada menos Voice. A fórmula continua sendo uma hipótese de reconstrução.
- Recalculei as métricas das formas de onda preservadas, confirmando:

| Sinal | Segundos acima do piso, com oito renders |
|---|---|
| Fala feminina | **11, 12, 13, 14, 16 e 18**; pior razão **2,51×** |
| Fala masculina | **1, 2, 5, 9 e 13**; pior razão **1,65×** |
| Ruído branco | **Nenhum dos cinco segundos** |

Os resultados por banda sustentam concentração dos resíduos em **3,5–10 kHz**. Os experimentos com expoente ERB e fusão aditiva apresentam regressões em outros casos, justificando sua não promoção.

## Estado real do VST3

| Exemplar | Evidência encontrada |
|---|---|
| Build disponível | Manifesto **0.1.1**; fonte declara **4096 amostras / 85,33 ms** |
| Instalado | Manifesto **0.1.0**; hash idêntico ao bundle histórico 0.1.0, cuja fonte declara **8192 amostras / 170,67 ms** |

O instalado e o build atual têm hashes diferentes. Não executei nem recompilei os binários nesta auditoria.

O [código do processador](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/native/sexyvox/Plugin.cpp:8) contém Voice/Noise de −50 a +20 dB, bypass, suavização de 20 ms e persistência XML. Aceita processamento somente em **48 kHz**; nas demais taxas retorna áudio sem processamento.

O [relatório de soak](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/analysis/sexyvox_v011_soak.json) registra dois casos de 30 segundos, estéreo, buffers 64/1024 e zero underruns. Isso sustenta aquele ensaio do processador; não comprova execução no FL Studio nem identifica por hash o binário testado.

## Pendências reais

- Confirmar a operação nativa de fusão e aplicação da máscara ERB.
- Explicar a variabilidade entre renders: confirmei cinco saídas distintas na fala inicial, cinco na feminina e quatro na masculina.
- Validar a implementação escolhida em streaming C++ e no VST3 contra o corpus completo. O [motor atual](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/native/sexyvox/Streaming.cpp:174) ainda usa DF até o bin 107 e máscara ERB acima disso, sem a fusão v0.2.3.
- Completar os casos da versão candidata atual: ruídos ambientais, estéreo distinto, silêncio, impulsos, outras durações e ganhos/automação.
- Testar no FL Studio: carregamento, reprodução contínua, automação, underruns e reabertura da sessão. Não localizei evidência suficiente desses testes.

## Divergências

1. O [PROGRESS_TRACKER.md](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/docs/PROGRESS_TRACKER.md:67) encerra a investigação como “erro intrínseco ao ONNX”. Essa conclusão não foi demonstrada e conflita com os resultados posteriores.
2. A alegação de chunk sweep “bit-idêntico em todos os casos” é incorreta: o resultado salvo para chunk 1 registra diferença máxima de aproximadamente **1,26e-7**.
3. O [ESTADO_ATUAL.md](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/docs/ESTADO_ATUAL.md) mistura o instalado 0.1.0, o build 0.1.1 e métricas históricas da pesquisa. A igualdade entre build e instalado não vale atualmente.
4. A v0.2.3 ainda tem **`repeat` como padrão**, enquanto os resultados mais recentes com cinco falhas masculinas usam explicitamente **`ramp`**.
5. A métrica escolhe o **menor erro do candidato** e o **maior piso entre referências** por segundo. É favorável ao candidato, não uma medida estatística de equivalência. Também exclui frações finais de segundo; portanto, os “79%” anteriores representam apenas 41/52 segundos completos desse ensaio.
6. Localizei o checklist dentro de `PLANO_DESENVOLVIMENTO.md` e a retomada em `RETOMADA_20261022_V021.md`. Não encontrei documentos separados de continuidade/checklist/prompt recentes. Referências a `NOMENCLATURA.md`, `AMBIENTE.md`, `PROGRESS_TRACKER_20261022.md` e duas retomadas antigas apontam para arquivos ausentes. `ARQUITETURA_SEXYVOX.md` está incompleto, terminando em “## Vis”.

## Próxima ação recomendada

**Rastrear pontualmente a aplicação nativa DF/ERB no segundo 13 da fala feminina**, onde o erro persiste em 2,51× após oito renders. Determinar qual espectro recebe a máscara, em que ordem e com qual correspondência temporal. Isso pode resolver a causa que os ajustes empíricos apenas deslocam entre casos.

Os arquivos centrais dessa tarefa são:

- [Referências e candidato preservados](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/analysis/v023_speech_eight_refs_20261023T040902127411Z.npz): reproduzir o resíduo.
- [Script de comparação](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/build/v0.2.3/speech_variation_v023.py:24): explicitar segmentação e critério.
- [Desassemblagem ERB](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/analysis/erb_interp_v024.txt): distinguir a interpolação já conhecida da fusão ainda desconhecida.
- [Dump preservado](C:/Users/nean/Documents/Projetos-claude/VOICESEX1/dumps/VoicExSupport_dll_from_memory.bin): verificar o caminho nativo sem nova extração.

## Uso do REA

Não foi necessário na auditoria. Na próxima tarefa, começar pela desassemblagem existente; recorrer ao REA apenas para esclarecer essa pergunta localizada, sem análise ampla.

Tempo contabilizado nesta auditoria: cerca de três minutos.
