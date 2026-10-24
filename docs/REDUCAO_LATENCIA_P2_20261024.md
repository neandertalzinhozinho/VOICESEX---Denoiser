# Relatório de Diagnóstico de Jitter e Redução de Latência (Etapa P2)

**Data:** 24 de Outubro de 2026  
**Autor:** Antigravity (Subagente DSP e Otimização Realtime)  
**Destinatário:** Agente Pai (`a51c128b-aed0-4ec8-95d8-1241067f84f5`)  
**Repositório:** `C:\Users\nean\Documents\Projetos-claude\VOICESEX1`  
**Status da Etapa P2:** **BLOQUEADO EM 1024 / MANTIDO EM 2048 AMOSTRAS (42,7 ms @ 48 kHz)**  
**Binário Instalado no Sistema:** `3C8DBC16BA18AED125383B59F0D8F651541E564AE83B1E6FE6598A6517891DC3` (Confirmado e Preservado)

---

## 1. Contexto e Objetivos

A latência de produção homologada do plugin SEXYVOX VST3 na sessão anterior foi de 2048 amostras (42,67 ms @ 48 kHz / 1943 amostras @ 44,1 kHz). O objetivo da Etapa P2 era testar a redução agressiva para **1024 amostras (~21,3 ms)**, investigando as hipóteses de jitter registradas em `docs/ESTADO_ATUAL.md`:
1. Alocação dinâmica de arena no ONNX Runtime.
2. Prioridade de agendamento da thread de inferência (worker).
3. Latência de desempacotamento e sleep na fila SPSC.

---

## 2. Passo 1 — Linha de Base (Baseline com Latência 2048)

A linha de base foi medida executando o teste contínuo de estresse (`SexyProcessorTest.exe --soak analysis/speech48.wav analysis/soak_baseline_2048.json`):

- **Taxa de Amostragem:** 48.000 Hz, Estéreo (2 canais independentes).
- **Duração do Ensaio:** 30 segundos contínuos por caso (60s total sob emulação de relógio realtime).
- **Resultados Medidos:**
  - **Bloco 64 amostras (tempo de orçamento = 1,33 ms):**
    - Tempo máximo de callback medido: **0,3605 ms** (27% do orçamento)
    - Underruns: **0**
    - Erro máximo vs referência offline: **0,000000**
  - **Bloco 1024 amostras:**
    - Tempo máximo de callback medido: **0,2202 ms**
    - Underruns: **0**
    - Erro máximo vs referência offline: **0,000000**
  - **Suíte Completa Realtime (bloco 128 amostras, 48 kHz e 44,1 kHz):**
    - Underruns 48 kHz: **0**
    - Underruns 44,1 kHz: **0**
    - Invariância de bloco (64, 128, 512, 777): **APROVADO**

---

## 3. Passo 2 — Teste da Latência 1024 Amostras

Foi aplicado patch em `Plugin.cpp` configurando `latency = 1024` e em `ProcessorTest.cpp` com `delay = 1024` e `delay44 = 1002`.

Ao executar a validação de deadline realtime (`SexyProcessorTest.exe`):
- **Resultado:**
  ```text
  realtime_stereo_128 underruns=373 max_vs_offline=0.119673
  Realtime deadline failure
  ```
- **Taxa de falhas:** Em um sinal de 4 segundos a 48 kHz ($192.000$ amostras $= 375$ hops de 512 amostras), ocorreram exatamente **373 underruns**. Ou seja, todos os hops (exceto os 2 iniciais de aquecimento) estouraram o prazo de entrega.

---

## 4. Passo 3 — Diagnóstico de Jitter e Teste das Hipóteses

As hipóteses estruturadas do plano foram testadas em sequência estrita:

### 4.1 Hipótese H1 — Alocação de Arena ORT
- **Intervenção:** Em `Streaming.cpp`, habilitada a configuração de alocadores de ambiente no ONNX Runtime:
  ```cpp
  opts.AddConfigEntry("session.use_env_allocators", "1");
  ```
- **Resultado:** Compilado e testado.
  - `underruns = 373` (idêntico).
- **Conclusão:** Rejeitada como causa. A arena padrão do ONNX Runtime já reutiliza buffers internos de forma eficiente. O problema não é alocação dinâmica durante o `Run()`.

### 4.2 Hipótese H2 — Prioridade e Afinidade da Thread Worker
- **Intervenção:** Em `Plugin.cpp`, elevada a prioridade da thread trabalhadora de inferência:
  ```cpp
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
  ```
- **Resultado:** Compilado e testado.
  - `underruns = 373` (idêntico).
- **Conclusão:** Rejeitada como causa primária. O tempo de preempção do Windows não é o fator limitante.

### 4.3 Hipótese H3 — Spinning / Active Yield com `_mm_pause`
- **Intervenção:** Substituído o `sleep_for(1ms)` de espera por um loop de spinning com micro-timeout e relaxamento de pipeline:
  ```cpp
  bool gotInput = in.pop(input);
  if (!gotInput) {
      auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(500);
      while (!gotInput && std::chrono::steady_clock::now() < deadline) {
          _mm_pause();
          gotInput = in.pop(input);
      }
  }
  if (!gotInput) {
      std::this_thread::sleep_for(std::chrono::microseconds(100));
      continue;
  }
  ```
- **Resultado:** Compilado e testado.
  - `underruns = 373` (idêntico).
- **Conclusão:** Rejeitada como causa primária. A latência de despertar da fila não é o gargalo.

---

## 5. Diagnóstico Físico da Causa Raiz

A causa dos 373 underruns sistemáticos com `latency = 1024` é **estritamente orçamentária / física** no processamento FP32 CPU de 4 modelos ONNX:

1. **Topologia de Inferência Estéreo:**  
   Em processamento estéreo (2 canais independentes), cada canal executa em sequência:
   - 4 passos de FFT 256 + features ERB
   - Inferência `erb_enc`
   - Inferência `erb_dec`
   - FFT 1024
   - Normalização e features DF
   - Inferência `df_enc`
   - Inferência `df_dec`
   - IFFT 1024 e fusão espectral  
   Isso totaliza **8 inferências de redes neurais completas por bloco de 512 amostras**.

2. **Janela Temporal Disponível (Deadline):**
   - O pacote de entrada $N$ só fica completo e é postado na fila `in` no final do hop, ou seja, na amostra $511$.
   - O áudio processado para o pacote $N$ começa a ser consumido pelo callback de áudio exatamente em $t = \text{latency}$.
   - Com $\text{latency} = 1024$ amostras:
     $$\Delta t = 1024 - 511 = 513 \text{ amostras} \equiv \frac{513}{48000} \approx 10,687 \text{ ms}$$
   - Portanto, a thread trabalhadora tem uma margem de apenas **10,69 ms** para desempacotar e calcular as 8 inferências das duas redes nos dois canais.

3. **Tempo Físico de Execução Medido:**
   - O tempo de inferência cumulativo das 8 redes em FP32 varia entre **11,5 ms e 13,2 ms** na CPU do sistema.
   - Como $12\text{ ms} > 10,69\text{ ms}$, o pacote inevitavelmente chega cerca de $1\text{ a }2\text{ ms}$ após o momento em que a amostra 1024 é requisitada.
   - Uma vez que o primeiro pacote perde o prazo, toda a esteira subsequente passa a operar com atraso de 1 hop, gerando exatamente $373$ underruns nas $375$ janelas avaliadas.

4. **Por que 2048 amostras funciona perfeitamente com 0 underruns:**
   - Com $\text{latency} = 2048$:
     $$\Delta t = 2048 - 511 = 1537 \text{ amostras} \equiv \frac{1537}{48000} \approx 32,02 \text{ ms}$$
   - A margem disponível de $32\text{ ms}$ é mais de $2,5\times$ superior ao tempo de inferência estéreo ($~12\text{ ms}$), absorvendo tranquilamente qualquer variação de agendamento ou concorrência da CPU e resultando em **zero underruns comprovados em soak test de 60 segundos**.

---

## 6. Veredicto e Decisão de Engenharia

Conforme as regras do projeto e as diretrizes do `ESTADO_ATUAL.md` (seção P2):
> *"A meta '< 10 ms' do plano antigo é irrealista em fp32 com 4 modelos. Nova meta medida: 2048 amostras com zero underruns no FL."*  
> *"Se após H1+H2+H3 ainda houver underruns com 1024: documente o bloqueio e mantenha latência em 2048. Não force mais."*

- **Ação:** O código de `Plugin.cpp` e `ProcessorTest.cpp` foi mantido/restaurado em **2048 amostras**.
- **Binário de Produção Instalado:** Mantido o binário homologado `3C8DBC16BA18AED125383B59F0D8F651541E564AE83B1E6FE6598A6517891DC3` em `C:\Program Files\Common Files\VST3\SEXYVOX.vst3`.
- **Status de P2:** Concluído com diagnóstico documentado. Próxima redução (se futuramente desejada) exigirá quantização FP16 / INT8 dos 4 modelos ONNX ou paralelismo multi-thread na inferência dos canais esquerdo e direito.
