# VoicEX 2 — Plano de desenvolvimento e retomada

Registrado em: 2026-10-21.

## Objetivo e alvo

Realizar engenharia reversa do denoiser CEDAR VoicEX 2, compreender sua separação de voz/ruído e implementar o processamento em C++ ou Python/PyTorch.

Local informado do plugin:
`C:\Program Files\Common Files\VST3\CEDARVoicEX.vst3\Contents\x86_64-win`

## Plano anterior preservado

### Preparação — Dump de memória (análise dinâmica)

Antes de tudo, executar a abordagem de dump de memória. O código precisa estar descriptografado em RAM para ser executado pela CPU.

Com o plugin instalado, ativado e carregado no FL Studio, processando áudio, utilizar Scylla ou x64dbg para fazer um dump da imagem em memória para disco. Carregar o binário obtido no IDA Pro 9.3 e reconstruir as importações (IAT), se necessário.

Nota de validação: um dump existente não comprova, sozinho, que o PE esteja reconstruído, integralmente desempacotado ou adequado para execução independente.

### Etapa 1 — Arquitetura e dependências (modelo previsto: V3)

Analisar o binário, procurar bibliotecas de inferência de IA (TensorFlow Lite, ONNX Runtime ou implementações proprietárias de matrizes), identificar onde os pesos são carregados em memória e localizar a estrutura da DNN que separa voz e ruído.

### Etapa 2 — Bifurcação de streams (modelo previsto: Coder)

Localizar a função principal de processamento e o ponto exato de divisão do sinal em Voice stream e Noise stream. Mapear as funções de ganho de -50 dB a +20 dB em cada fluxo. Identificar máscara espectral versus síntese direta.

### Etapa 3 — Modelo de inferência (modelo previsto: Coder/Reasoning)

Analisar tensores e matrizes; identificar convoluções, RNNs ou Transformers; extrair topologia e mecanismo de classificação das frequências como voz ou ruído. Investigar blocos pequenos e processamento em streaming.

A latência inferior a 10 ms era uma premissa do plano anterior, não um resultado confirmado. Medir separadamente atraso algorítmico, buffering do host e tempo de execução.

### Etapa 4 — Pesos e bias (modelo previsto: Coder)

Utilizar REA/debugger para extrair arrays constantes de pesos e bias, converter dados hexadecimais em floats quando necessário e documentar dimensões para reconstrução em Python/PyTorch.

Se os ONNX já contiverem os tensores completos, validar seus initializers antes de repetir extração manual ou conversão hex → float.

### Etapa 5 — Implementação (modelo previsto: Reasoning)

Implementar em C++ ou Python: receber áudio, processar pela rede reconstruída, separar os dois streams e aplicar ganhos de Voice e Noise de -50 dB a +20 dB. Validar qualidade e latência contra o original.

## Checklist de retomada

Legenda: itens marcados representam artefatos encontrados no levantamento do projeto; não significam validação funcional nova. Itens pendentes exigem confirmação ou implementação.

### Preparação

- [x] Estrutura do projeto e documentação inicial presentes.
- [x] Dumps presentes em `dumps/`, incluindo módulo principal e VoicExSupport.
- [x] Script de dump presente em `scripts/dump_module_mem.py`.
- [ ] Registrar origem, versão do plugin, endereços-base e hashes dos dumps/modelos.
- [ ] Verificar se reconstrução de IAT/PE e carregamento no IDA ainda são necessários ao trabalho restante.

### Etapa 1 — Arquitetura

- [x] Quatro arquivos ONNX presentes em `models/`.
- [x] Listagens de nós presentes em `analysis/model_*_nodes.txt`.
- [x] Configurações extraídas e código de referência DeepFilterNet presentes em `analysis/`.
- [ ] Consolidar evidência do runtime efetivamente utilizado pelo plugin; formato ONNX não comprova ONNX Runtime.
- [ ] Documentar caminho de carregamento dos modelos e papel de cada um.
- [ ] Separar correspondências confirmadas com DeepFilterNet de hipóteses arquiteturais.

### Etapa 2 — Streams e ganhos

- [x] Scripts de teste de streams, resposta e latência presentes em `scripts/`.
- [x] Saídas de disassembly do pipeline presentes em `analysis/`.
- [ ] Confirmar endereço e fluxo da função principal de processamento.
- [ ] Documentar ponto exato da separação e reconstrução Voice/Noise.
- [ ] Revalidar complementaridade dos streams com alinhamento temporal e ganhos conhecidos.
- [ ] Mapear ganho, conversão dB → linear, limites e eventual suavização.
- [ ] Confirmar máscara espectral, deep filtering e ordem dos estágios.

### Etapa 3 — Inferência

- [x] Scripts `describe_models.py` e `probe_models.py` presentes.
- [ ] Validar todos os modelos com checker e execução de referência.
- [ ] Consolidar entradas, saídas, tipos, dimensões e estados recorrentes.
- [ ] Determinar janelas, FFT, hop, bandas ERB e normalização de cada estágio.
- [ ] Determinar ordem de execução, histórico, lookahead e alinhamento entre estágios.
- [ ] Reproduzir a medição indicada no levantamento: 1664 amostras a 48 kHz, aproximadamente 34,7 ms.
- [ ] Distinguir atraso do sinal e custo de inferência; revisar a meta original de 10 ms com evidência.

### Etapa 4 — Pesos e bias

- [x] Contêineres ONNX extraídos presentes em `models/`.
- [ ] Conferir integridade, initializers e eventual dependência de arquivos externos.
- [ ] Inventariar nome, dtype e shape de todos os pesos e bias.
- [ ] Exportar para PyTorch somente se necessário à implementação escolhida.
- [ ] Comparar numericamente qualquer conversão com a execução ONNX de referência.

### Etapa 5 — Implementação e validação

- [x] Template presente em `build/denoiser_clone_template.py`.
- [ ] Construir primeiro pipeline offline executável com os modelos validados.
- [ ] Implementar pré-processamento, estados persistentes e reconstrução do áudio.
- [ ] Implementar Voice/Noise e respectivos ganhos.
- [ ] Comparar saídas alinhadas contra o plugin em sinais e gravações de referência.
- [ ] Testar silêncio, impulsos, voz com ruído, mudanças de ganho e limites de blocos.
- [ ] Medir atraso, tempo por bloco e comportamento contínuo.
- [ ] Somente depois integrar streaming e eventual empacotamento VST3.

## Onde retomar

O projeto está entre análise arquitetural e reconstrução do pipeline: já existem dumps, modelos e análises, mas não há implementação ponta a ponta validada no levantamento disponível.

Próxima tarefa: validar os quatro ONNX e produzir um contrato de I/O e estados para cada modelo. Em seguida, fechar pré/pós-processamento e encadeamento dos estágios antes de implementar o pipeline offline.

Não repetir dump por padrão. Primeiro verificar se os artefatos existentes atendem à próxima análise.

## Documentos relacionados

- `docs/PROGRESS_TRACKER.md`: checklist anterior, desatualizada em relação aos artefatos encontrados; mantida sem alteração.
- `docs/DUMP_WORKFLOW.md`: procedimento de dump.
- `docs/IDA_ANALYSIS_TEMPLATE.md`: roteiro de análise no IDA.

Ao retomar em outra sessão: ler este arquivo, conferir os artefatos citados e atualizar somente os itens comprovados por resultados registrados.
