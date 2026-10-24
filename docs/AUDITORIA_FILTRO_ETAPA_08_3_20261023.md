# Auditoria Matemática do Filtro — Etapa 08.3 (`SEXYVOX`)

**Data:** 2026-10-23  
**Escopo:** Auditoria somente leitura do filtro FIR complementar $L()$ em `Combinador.cpp` e `Combinador.h`.

## 1. Análise dos Coeficientes e Domínio Temporal (FIR)
- **Domínio:** Os `257` coeficientes (`FILTER_TAPS = 257`) calculados em `Combinador.cpp` são coeficientes temporais no domínio do tempo (taps de um filtro FIR direto tipo I).
- **Projeto:** Utiliza método de truncagem por sinc com cutoff de 3092 Hz a 48 kHz, ponderado por janela de Hamming ($w[n] = 0.54 - 0.46 \cos(2\pi n / N)$), normalizado para ganho unitário na banda passante.
- **Correspondência com o Nativo:** Representa a aproximação analítica de alta fidelidade do filtro complementar $L()$ do CEDAR VoicEX 2 identificado na investigação de latência.

## 2. Verificação de Limites e Memória (`FILTER_HISTORY` e `memmove`)
- **Tamanho do Histórico (`FILTER_HISTORY = 257`):** Correto e seguro. Com `N = FILTER_TAPS - 1 = 256`, o índice máximo acessado no delay line `filter_mem_input` é $k = 256$. O vetor dimensionado com 257 elementos cobre exatamente os índices $[0 \dots 256]$.
- **Operação `memmove`:**
  ```cpp
  std::memmove(filter_mem_input.data() + 1, filter_mem_input.data(),
               sizeof(float) * static_cast<size_t>(N));
  ```
  Desloca 256 elementos (antigos $x[0 \dots 255]$) para as posições $1 \dots 256$. A posição `filter_mem_input[0]` recebe a nova amostra `input[i]`. Como o vetor tem capacidade 257 (índices $0 \dots 256$), a escrita em `index = 256` é estritamente válida e não gera estouro de buffer (*buffer overflow*).

## 3. Conclusão da Auditoria
A implementação do filtro $L()$ e da linha de atraso no `Combinador` é matematicamente consistente, causal, segura quanto aos limites de memória e opera corretamente no pipeline de tempo real.
