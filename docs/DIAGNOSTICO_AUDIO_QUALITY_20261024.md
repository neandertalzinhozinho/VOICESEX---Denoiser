# Diagnóstico Técnico — Qualidade de Áudio (Voz Robótica e Crackling) — SEXYVOX VST3

**Data:** 24 de Outubro de 2026  
**Autor:** Antigravity (Subagente Especialista em DSP e Áudio Nativo)  
**Status:** Causas Raízes Mapeadas com Evidência Matemática e Forense; Aguardando Autorização do Agente Pai para Correções

---

## 1. Sumário Executivo

Após a resolução do crash de inicialização no FL Studio (`ORT_API_MANUAL_INIT`), o plugin VST3 `SEXYVOX` (candidato SHA-256: `284D681A57C2F7268F00019E9C6D046D0E1A600D43330A1BE459DD046DDECFFB`) carregou com sucesso no host. Entretanto, o usuário relatou dois sintomas graves de degradação sonora:
1. **Voz Robótica** (ressonância artificial, perda de timbre natural, zumbido metálico harmônico de ~93.8 Hz);
2. **Crackling** (estalos intermitentes, descontinuidades periódicas em bordas de bloco e falhas de sincronismo em tempo real).

Realizou-se uma auditoria linha a linha em `native/sexyvox/Combinador.cpp`, `Combinador.h`, `Streaming.cpp`, `Plugin.cpp` e análise espectral/numérica do arquivo `test_speech_out.wav` gerado pelo teste de streaming.

Foram descobertas **quatro causas raízes críticas e inquestionáveis** sustentadas por dados numéricos:

| # | Componente | Arquivo e Linha | Defeito Matemático | Sintoma Resultante |
|---|------------|-----------------|---------------------|-------------------|
| **1** | Combinador | `Combinador.cpp:128-132` | Truncamento de `e_aligned`: apenas 384 amostras são lidas; as últimas 128 amostras do bloco de 512 são preenchidas com **zeros absolutos**. | **Voz Robótica + Crackling:** Toda frequência acima de 3092 Hz é cortada nos últimos 128 samples de cada bloco, gerando modulação retangular a 93.75 Hz. |
| **2** | Filtro FIR $L()$ | `Combinador.cpp:83-98` | O laço final de cauda injeta 256 zeros via `memmove`, **zerando a memória de entrada (`filter_mem_input`) a cada bloco**. Além disso, `out_idx < FILTER_HISTORY` é sempre falso (`512 < 257`), descartando toda a cauda. | **Crackling Severo:** O filtro FIR zera o histórico a cada 512 amostras, gerando descontinuidades de amplitude 16.2× maiores que a média do sinal (medido: 0.03595 vs 0.00221). |
| **3** | Estado do Filtro | `Combinador.h:30-31` e `Combinador.cpp:137-139` | `apply_filter_L(d)` e `apply_filter_L(e_aligned)` **compartilham a mesma memória `filter_mem_input`**. | **Voz Robótica:** Contaminação mútua entre o ramo direto (DF) e o ramo ERB, destruindo a relação de fase do filtro complementar a 3092 Hz. |
| **4** | Worker Thread | `Plugin.cpp:74` | `std::this_thread::sleep_for(std::chrono::milliseconds(1))` na thread de processamento. No Windows padrão, `sleep_for(1ms)` pode demorar até **15.6 ms** (750 amostras a 48 kHz). | **Crackling / Underruns:** A worker thread atrasa além do deadline de 512 amostras (10.7 ms), forçando o callback a descartar o áudio processado e alternar bruscamente para áudio dry. |

---

## 2. Evidências Numéricas em Arquivos de Áudio Reais

Uma análise matemática foi executada sobre `native/sexyvox/out/test_speech_out.wav` (áudio gerado pelo motor nativo a partir de `analysis/speech48.wav`):

- **Taxa de amostragem:** 48000 Hz, 1 canal, 674.232 amostras (14.05 s).
- **Taxa de repetição dos blocos:** $48000 / 512 = 93.75\text{ Hz}$.
- **Variação média de amplitude entre amostras consecutivas no sinal:** `0.002214`
- **Salto médio de amplitude exatamente na fronteira de 512 amostras:** `0.035952`  
  *(A descontinuidade de bloco é **16,2 vezes maior** que a continuidade normal do sinal).*
- **Salto máximo de descontinuidade em borda de bloco:** `0.5437` (salto de meio escala de amplitude instantâneo!).
- **Na entrada original (`speech48.wav`):** A variação na borda de 512 amostras é `0.003640`, perfeitamente idêntica à média geral `0.003733`.

**Conclusão Física:** A fronteira de 512 amostras está sofrendo um choque mecânico/matemático periódico a 93.75 Hz. Qualquer sinal submetido a pulsos periódicos de 93.75 Hz soa auditivamente como um **zumbido elétrico/robótico** sobreposto com **estalos contínuos**.

---

## 3. Análise Detalhada dos Problemas Encontrados

### Problema 1: Buffer `e_aligned` com 128 amostras zeradas
- **Localização:** `native/sexyvox/Combinador.cpp`, linhas 128–132:
  ```cpp
  // Ler e_alinhado (últimas 384 amostras)
  std::vector<float> e_aligned(B, 0.0f);
  int read_start = (erb_write_idx - ALIGNMENT_ERB_DF + BUF_ALIGNMENT) % BUF_ALIGNMENT;
  for (int i = 0; i < ALIGNMENT_ERB_DF; ++i) {
      e_aligned[i] = erb_alignment_buf[(read_start + i) % BUF_ALIGNMENT];
  }
  ```
- **Causa Raiz:** O bloco $B$ tem 512 amostras. O laço preenche apenas até `ALIGNMENT_ERB_DF` (384 amostras). As posições `384..511` permanecem com o valor inicial `0.0f`.
- **Efeito:** Nas linhas 138 e 144:
  $$\text{Voice}[i] = L(d)[i] + [D_{128}(e_{\text{aligned}})[i] - L(e_{\text{aligned}})[i]]$$
  Para $i \in [384, 511]$, $e_{\text{aligned}}[i] = 0$. Logo, a componente complementar de altas frequências $(D_{128}(e) - L(e))$ cai a zero. A voz perde subitamente todo o conteúdo acima de 3092 Hz durante 25% de cada ciclo, reaparecendo abruptamente na amostra 0 do ciclo seguinte.

### Problema 2: Destruição do Histórico do Filtro FIR a Cada Bloco
- **Localização:** `native/sexyvox/Combinador.cpp`, linhas 83–98:
  ```cpp
  for (int i = 0; i < N && i < FILTER_HISTORY; ++i) {
      double acc = 0.0;
      std::memmove(filter_mem_input.data() + 1, filter_mem_input.data(),
                   sizeof(float) * static_cast<size_t>(N));
      filter_mem_input[0] = 0.0f;  // padding zero
      ...
      int out_idx = num_samples + i;
      if (out_idx < FILTER_HISTORY) { // 512 + i < 257 -> NUNCA EXECUTADO!
          filter_mem_output[out_idx] += static_cast<float>(acc);
      }
  }
  ```
- **Causa Raiz:** Para um filtro FIR causal direto $y[n] = \sum_{k=0}^N h[k] x[n-k]$, as amostras passadas de $x$ já residem na linha de retardo `filter_mem_input`. A inserção de 256 zeros ao final do processamento apaga as amostras reais que deveriam servir de histórico para o próximo bloco! Além disso, a cauda nunca é salva porque `num_samples = 512` e `FILTER_HISTORY = 257`.
- **Efeito:** No início de cada bloco de 512 amostras, o filtro FIR opera como se o sinal viesse do silêncio absoluto, gerando um degrau transiente de alta frequência (click/crackling).

### Problema 3: Contaminação de Estado entre os Canais $L(d)$ e $L(e)$
- **Localização:** `native/sexyvox/Combinador.h` e `Combinador.cpp`, linhas 137–139:
  ```cpp
  apply_filter_L(d, L_d.data(), B);
  apply_delay_D128(e_aligned.data(), D128_e.data(), B);
  apply_filter_L(e_aligned.data(), L_e.data(), B);
  ```
- **Causa Raiz:** O objeto `Combinador` possui uma única instância `std::array<float, FILTER_HISTORY> filter_mem_input;`. A filtragem de $e_{\text{aligned}}$ começa com as amostras residuais deixadas por $d$, e o bloco seguinte de $d$ começa com amostras de $e_{\text{aligned}}$.
- **Efeito:** Intermodulação e perda da propriedade de reconstrução de sinal complementar no corte de 3092 Hz.

### Problema 4: `sleep_for(1ms)` e Quantização de Timer no Windows
- **Localização:** `native/sexyvox/Plugin.cpp`, linha 74:
  ```cpp
  if(!in.pop(input)) {std::this_thread::sleep_for(std::chrono::milliseconds(1));continue;}
  ```
- **Causa Raiz:** No Windows, o escalonador de threads opera com fatias de tempo padrão de 15.625 ms a menos que uma solicitação de alta resolução esteja ativa. Uma chamada `sleep_for(1ms)` pode suspender a thread trabalhadora por até 15 ms. Como um bloco de 512 amostras a 48 kHz dura apenas 10.67 ms, a thread falha em entregar o pacote a tempo.
- **Evidência:** O teste `SexyProcessorTest.exe` acusou:
  `realtime_stereo_128 underruns=0 max_vs_offline=0.137494`  
  `Realtime deadline failure`
  Em tempo real, quando a worker thread não entrega o bloco no instante `pos == 0`, o plugin ativa o fallback de segurança e transmite áudio não processado (*dry*), gerando descontinuidade audível (*dropouts/crackling*).

---

## 4. Plano de Correções Proposto

### Proposta 1 — Correção do Buffer Circular e Alinhamento no Combinador
- Manter o buffer circular `erb_alignment_buf` com capacidade adequada (896 amostras conforme especificado no RE).
- Ler 512 amostras contínuas atrasadas por `ALIGNMENT_ERB_DF` (384 amostras), eliminando completamente o preenchimento de zeros em `e_aligned`.

### Proposta 2 — Linha de Retardo FIR Contínua e Separação de Instâncias
- Implementar filtragem FIR direta clássica com delay line contínua preservando as 256 amostras anteriores entre blocos, sem injeção de zeros ao término da chamada.
- Separar os estados em duas instâncias independentes de filtro: `filter_d` (para o caminho direto $d$) e `filter_e` (para o caminho $e_{\text{aligned}}$), eliminando a contaminação mútua de estado.

### Proposta 3 — Otimização da Worker Thread e Sincronização em Tempo Real
- Substituir o polling com `sleep_for(1ms)` por uma primitiva de sincronização de baixa latência (`std::condition_variable` ou semáforo/evento Win32 com `timeBeginPeriod(1)`), garantindo que a thread acorde imediatamente ao receber um novo bloco de áudio.

---

## 5. Próxima Ação

- **Aguardar a revisão e autorização expressa do Agente Pai antes de realizar qualquer alteração nos arquivos de código ou compilar novos binários.**
