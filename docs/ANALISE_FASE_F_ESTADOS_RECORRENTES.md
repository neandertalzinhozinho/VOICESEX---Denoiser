# SEXYVOX — Etapa 04: Análise de Fase F e Estados Recorrentes

**Data:** 2026-10-23  
**Status:** Completa (análise consolidada, sem novos experimentos)

## Resumo Executivo

Esta etapa analisa a fase F e estados recorrentes (h/c dos modelos) com base nas evidências já coletadas. **Conclusão principal:** o seletor de F permanece desconhecido e os estados h/c dos modelos não são observáveis diretamente via instrumentação não-invasiva.

## Fase F: Estado Atual do Conhecimento

### Observações Coletadas

| Data | PID | B (samples) | F (samples) | Observação |
|------|-----|-------------|-------------|------------|
| 2026-10-23 20:46 | 20688 | 512 | 320 | Observação original (Etapa 02) |
| 2026-10-23 21:08 | 25124 | 512 | 320 | Validação de buffers (Etapa 03) |
| 2026-10-23 21:08 | 25640 | 512 | 320 | Análise de padrões (Etapa 03) |
| 2026-10-23 21:10 | 21160 | 512 | 448 | Auditoria de buffers (Etapa 03) |
| 2026-10-23 21:10 | 25640 | 512 | 384 | Segunda execução análise |

**Total:** 5 observações em 4 PIDs distintos  
**Valores observados:** 320 (×3), 384 (×1), 448 (×1)  
**Valores únicos:** {320, 384, 448}

### Análise do Seletor de F

**Hipóteses testadas:**

1. **F é determinístico:** ❌ Refutada. Mesmo B=512 produz F diferentes.
2. **F é função do PID:** ❌ Refutada. PID 25640 produziu F=320 e F=384 em execuções diferentes.
3. **F é função do timestamp:** ⚠️ Não testável diretamente, mas improvável dado padrão irregular.
4. **F é aleatório:** ✅ Hipótese mais consistente com os dados.

**Possíveis origens (não verificáveis sem instrumentação invasiva):**
- Inicialização não-determinística de buffers internos
- Ordem de alocação de memória pelo sistema operacional
- Estado de threads internas no momento da inicialização
- Parâmetros de configuração não expostos via API pública

**Conclusão:** O seletor de F é **desconhecido**. A implementação deve suportar múltiplos valores de F (320, 384, 448 para B=512) e validar todos eles.

### Relação F ↔ Descriptors

**Observação crítica:** F não é calculado como `inp[1] % 512` conforme sugerido inicialmente. Os descriptors mostram:
- `input.index_plus8 = 832` (constante em todas observações)
- `output.index_plus8 = 1856` (constante em todas observações)
- F varia: 320, 384, 448

**Interpretação revisada:** F é um offset interno que afeta como os dados são lidos/escritos nos buffers circulares, mas não é diretamente derivável dos descriptors observáveis.

## Estados Recorrentes dos Modelos (h/c)

### Observabilidade

**Métodos testados:**
1. ✅ Leitura de buffers de áudio (df_input_history, erb_input_history, etc.) — sucesso
2. ✅ Leitura de descriptors (input/output queues) — sucesso
3. ❌ Leitura de estados internos dos modelos (h, c) — **não suportado pela API**

**Limitação técnica:** A função `snapshot(w)` retorna apenas:
- Arrays de áudio (históricos e buffers de processamento)
- Ponteiros para estruturas internas
- Não expõe estados de camadas ocultas (hidden states) ou células (cell states)

**Consequência:** Os estados iniciais dos modelos DF e ERB são **desconhecidos** e não podem ser capturados via instrumentação não-invasiva.

### Implicações para Implementação

**Estratégia recomendada:**

1. **Inicialização lazy:** Os modelos devem ser inicializados na primeira chamada de processamento, não na instanciação.
2. **Estado zero como fallback:** Inicializar h=0, c=0 se não houver evidência de outro padrão.
3. **Validação empírica:** Comparar saída do candidato vs. CEDAR em múltiplos cenários para detectar divergências causadas por estados iniciais incorretos.

**Teste mínimo:**
- Processar impulso unitário (1.0 seguido de zeros)
- Comparar saída do candidato vs. CEDAR nos primeiros 2048 samples
- Divergências persistentes indicam estados iniciais incorretos

## Estados dos Buffers de Áudio

### Buffers Inicializados em Zero

**Confirmado em todas observações (PIDs 20688, 21160, 25124, 25640):**

| Buffer | Shape | Estado Inicial |
|--------|-------|----------------|
| df_input_history | [512] | Zero |
| erb_input_history | [128] | Zero |
| voice | [512] | Zero |
| df_hist | [768] | Zero |
| low_df | [512] | Zero |
| erb_hist | [768] | Zero |
| high_erb | [512] | Zero |
| erb_fft_input | [256] | Zero |

**Total:** 8 buffers inicializados em zero.

### Buffers Não Inicializados

**Descoberta crítica da Etapa 03:**

| Buffer | Shape | Estado Inicial |
|--------|-------|----------------|
| erb_alignment | [896] | Lixo (10^33-10^38, valores inconsistentes) |
| df_fft_input | [1024] | Lixo (10^33-10^38, valores inconsistentes) |

**Interpretação:** Estes buffers são alocados mas não inicializados. O processamento real deve sobrescrever estes buffers antes de usá-los.

**Implicação para implementação:**
- Não assumir valores iniciais específicos
- Garantir que estes buffers sejam preenchidos antes da primeira FFT
- Usar inicialização explícita (zeros) na implementação candidata para evitar comportamento não-determinístico

## Lacunas Remanescentes

### Lacuna 1: Seletor de F

**Status:** Desconhecido  
**Impacto:** Baixo (implementação pode validar múltiplos valores)  
**Caminho para resolução:** Requer instrumentação invasiva ou engenharia reversa do código nativo

### Lacuna 2: Estados h/c dos Modelos

**Status:** Desconhecido  
**Impacto:** Médio (pode causar divergências em cenários específicos)  
**Caminho para resolução:** Validação empírica com impulso unitário e comparação CEDAR vs. candidato

### Lacuna 3: Inicialização de erb_alignment e df_fft_input

**Status:** Confirmado como não-inicializado  
**Impacto:** Baixo (implementação candidata deve inicializar explicitamente)  
**Caminho para resolução:** Já resolvido — usar inicialização explícita em zeros

## Recomendações para Etapa 05 (Especificação do Combinador)

1. **Documentar F como parâmetro variável:** Especificação deve listar valores observados (320, 384, 448 para B=512) e requerer validação de todos.

2. **Adotar inicialização lazy para modelos:** Estados h/c inicializados na primeira chamada de processamento, com valores default zero.

3. **Inicialização explícita de todos os buffers:** Mesmo que o CEDAR não inicialize erb_alignment e df_fft_input, a implementação candidata deve fazê-lo para evitar não-determinismo.

4. **Teste de robustez com múltiplos F:** Suite de testes deve executar cenários com F=320, F=384 e F=448 para garantir que a implementação suporta todos os casos observados.

## Conclusão

A análise de fase F e estados recorrentes revela:
- **F é não-determinístico** e deve ser tratado como parâmetro variável
- **Estados h/c não são observáveis** e devem ser validados empiricamente
- **Buffers de áudio são consistentemente inicializados** (8 em zero, 2 não-inicializados)

A implementação candidata deve adotar estratégias defensivas (inicialização explícita, validação de múltiplos F) para compensar estas lacunas de observabilidade.
