# Sessão Autônoma 2026-10-24 — Resolução de Qualidade de Áudio e Redução de Latência (SEXYVOX VST3)

**Data:** 24 de Outubro de 2026  
**Subagente:** DSP & C++ Audio Specialist  
**Agente Chamador:** `a51c128b-aed0-4ec8-95d8-1241067f84f5`  
**Escopo:** Resolução dos artefatos de degradação de áudio (voz robótica + "zumzum" metálico de filtro pente) e redução de latência no SEXYVOX VST3.

---

## 1. Diagnóstico e Causa-Raiz Comprovada por Evidências

### 1.1 Falso Offset de Alinhamento de 384 Amostras no Combinador
- **Evidência Primária:** No CEDAR VoicEX 2 original (`docs/INVESTIGACAO_LATENCIA_INICIALIZACAO_20261023.md`), o ramo ERB opera nativamente com hop 128 (FFT 256) e o ramo DF com hop 512 (FFT 1024). A diferença algorítmica de latência entre os dois no original é de $512 - 128 = 384$ amostras, exigindo que o CEDAR atrasasse o ERB em 384 amostras para alinhar com o DF.
- **Defeito no SEXYVOX:** No código nativo do SEXYVOX (`Streaming.cpp`), tanto o ramo DF quanto a máscara ERB são sintetizados através da mesma IFFT de 1024 pontos (`dfFFT`) com hop 512 e janela de Vorbis. Ambos os sinais `d_samples` e `e_samples` já emergem **perfeitamente síncronos**.
- **Impacto do Bug:** Em `Combinador.cpp`, a variável `ALIGNMENT_ERB_DF = 384` forçava um atraso espúrio de 384 amostras (8.0 ms) apenas no ramo ERB. A síntese complementar:
  $$\text{Voice} = L(d) + [D_{128}(e_{\text{alinhado}}) - L(e_{\text{alinhado}})]$$
  somava frequências baixas em $t$ e frequências altas em $t-384$, gerando cancelamento de fase destrutivo e filtro pente violento com vales a cada 125 Hz ($48000 / 384$). Isso resultava em RMSE de **0.0907** (9,07% de erro sobre `speech48.wav`).

### 1.2 Latência de Grupo FIR vs. Desalinhamento com Canal Dry no Plugin
- O filtro linear de fase $L$ (257 coeficientes) e a linha $D_{128}$ introduzem exatamente 128 amostras de latência de grupo.
- No `Plugin.cpp`, o canal `dry` era atrasado apenas por `latency` (múltiplo de 512). Com o Combinador no domínio do tempo, o canal `voice` chegava atrasado por `latency + 128`.
- No cálculo do ruído complementar $(d - v)$, ocorria subtração de amostras com 128 amostras de desfasamento temporal ($x(t) - x(t-128)$), gerando um filtro pente secundário a cada 375 Hz ($48000 / 128$), audível como ressonância metálica ("zumzum") quando o knob de ruído ou voz era movimentado.

### 1.3 Fusão Espectral Direta (Single-IFFT com Rampa Dinâmica v0.2.3)
- Na Trilha B de pesquisa (`build/v0.2.3/voicex_v023.py` e `history/v0.1.0/Streaming.cpp`), a recombinação DF/ERB ocorre **no domínio da frequência** antes de uma única IFFT:
  - Bins 0..107: DF com rampa suave de transição (2.000 Hz a 5.200 Hz).
  - Bins 108..512: Máscara ERB direta.
- **Vantagens Físicas e Matemáticas:**
  1. Preserva a partição da unidade de Overlap-Add (janela de Vorbis, erro de complementaridade de $3.72 \times 10^{-9}$).
  2. Elimina a segunda IFFT (redução de 50% no custo de FFT).
  3. Elimina a convolução FIR de 257 taps (redução de ~260.000 MACs por bloco).
  4. Latência algorítmica exata de 512 amostras (1 hop inteiro), eliminando o desvio de 128 amostras e sincronizando `dry` e `voice` com precisão de subamostra.

---

## 2. Resultados Numéricos de Equivalência com o CEDAR VoicEX 2

Comparação contra os 8 renders de referência preservados do plugin CEDAR original (`analysis/v021_reference_modes_20261023T032340965375Z.npz`):

| Configuração | Lag Ótimo vs CEDAR | RMSE Mínimo | RMSE Médio | Observação |
|---|:---:|:---:|:---:|---|
| Combinador anterior (align = 384) | -128 | 0.007512 | 0.090540 | Distorção grave, voz robótica |
| Combinador corrigido (align = 0) | -128 | 0.002171 | 0.002475 | Exige atraso extra de 128 em dry |
| **Fusão Espectral com Rampa (Single-IFFT)** | **0 (bit-alinhado)** | **0.002016** | **0.002208** | **Paridade excelente, sem latência residual** |
| **SEXYVOX VST3 via Pedalboard (Host real)** | **0 (bit-alinhado)** | **0.002025** | **0.002235** | **Testado em tempo real via host** |

- **Complementaridade:** Erro máximo $\|(Voice + Noise) - Input\|_\infty = \mathbf{3.72 \times 10^{-9}}$ (nível de ruído de quantização fp32).
- **Separação Reconstruída:** $\mathbf{5.96 \times 10^{-8}}$ na soma linear normalizada com Voice em 0 dB e Noise em -50 dB.

---

## 3. Redução de Latência Validada (P2: 4096 → 2048 Amostras)

- `Plugin.cpp`: `constexpr int latency = 2048;` (redução de 85.3 ms para **42.6 ms** @ 48 kHz).
- `ProcessorTest.cpp`: `constexpr int delay = 2048;`.
- **Testes Unitários JUCE (`SexyProcessorTest.exe`):**
  - `unity`: **PASS** (erro < 1e-6)
  - `reported latency`: **PASS** (2048 samples)
  - `stereo`: **PASS**
  - `separation`: **PASS**
  - `block invariance` (blocos 64, 128, 512, 777): **PASS**
  - `state recall / robustness`: **PASS**
  - `bypass`: **PASS**
  - `realtime stereo 128`: **PASS** (`underruns = 0`, `max_vs_offline = 0`)
- **Teste de Estresse Contínuo (`SexyProcessorTest.exe --soak`):**
  - Bloco 64: **0 underruns**, tempo máximo de callback = **0.144 ms** (orçamento: 1.33 ms).
  - Bloco 1024: **0 underruns**, tempo máximo de callback = **0.093 ms** (orçamento: 21.3 ms).
  - Erro máximo entre streaming em tempo real e offline: **0.000000**.
  - `passed: true`.

---

## 4. Backups e Arquivos de Código

Conforme as diretrizes, todos os arquivos modificados receberam backup `.bak_20261024C`:
- `native/sexyvox/Combinador.h.bak_20261024C`
- `native/sexyvox/Combinador.cpp.bak_20261024C`
- `native/sexyvox/Streaming.cpp.bak_20261024C`
- `native/sexyvox/Streaming.h.bak_20261024C`
- `native/sexyvox/Plugin.cpp.bak_20261024C`
- `native/sexyvox/ProcessorTest.cpp.bak_20261024C`

Binários gerados no diretório do projeto:
- `native/sexyvox/out/SEXYVOX_artefacts/Release/VST3/SEXYVOX.vst3` (NÃO copiado para a pasta do sistema `C:\Program Files\Common Files\VST3`).
