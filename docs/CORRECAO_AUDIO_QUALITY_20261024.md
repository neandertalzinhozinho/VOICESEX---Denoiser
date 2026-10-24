# Relatório de Correção — Qualidade de Áudio (Voz Robótica e Crackling) — SEXYVOX VST3

**Data:** 24 de Outubro de 2026  
**Autor:** Antigravity (Subagente Especialista em DSP e Áudio Nativo)  
**Destinatário:** Agente Pai (`a51c128b-aed0-4ec8-95d8-1241067f84f5`)  
**Status:** Implementado, Compilado e 100% Aprovado nos Testes Automatizados  

---

## 1. Sumário das Modificações Realizadas

Seguindo estritamente as diretrizes de engenharia reversa e as instruções autorizadas pelo agente pai, foram aplicadas as seguintes intervenções cirúrgicas:

### 1.1 Backups Criados
- `Combinador.cpp.bak_20261024B`
- `Combinador.h.bak_20261024B`
- `Plugin.cpp.bak_20261024B`

### 1.2 `Combinador.h`
- Removidos os arrays únicos compartilhados `filter_mem_input` e `filter_mem_output`.
- Adicionados estados independentes de histórico para cada canal: `filter_mem_d` (`std::array<float, 256>`) e `filter_mem_e` (`std::array<float, 256>`).
- Assinatura atualizada: `void apply_filter_L(const float* input, float* output, int num_samples, float* mem);`.

### 1.3 `Combinador.cpp`
- **FIR Causal Puro:** Removido completamente o laço espúrio de "padding zero" (`memmove` com `0.0f` no histórico e escrita em `filter_mem_output`). O filtro FIR causal direto preserva exatamente as últimas 256 amostras de entrada em `mem[0..255]` entre blocos consecutivos.
- **Leitura Completa de `e_aligned`:** Corrigida a leitura do buffer circular para ler 512 amostras completas (384 atrasadas + 128 mais recentes), eliminando a modulação retangular de 93.75 Hz causada pelo preenchimento com zeros.
- **Isolamento de Canais em `process()`:** As chamadas de `apply_filter_L` utilizam independentemente `filter_mem_d.data()` e `filter_mem_e.data()`, extinguindo a contaminação mútua entre o ramo DF e o ramo ERB.
- **Reset:** `reset()` zera `filter_mem_d` e `filter_mem_e`.

### 1.4 `Plugin.cpp`
- **Precisão de Timer do Windows:** Adicionados `<timeapi.h>`, `#pragma comment(lib, "winmm.lib")`, envolvendo a thread trabalhadora com `timeBeginPeriod(1)` e `timeEndPeriod(1)`.
- **Robustez de Estado APVTS:** Corrigido o manipulador de `setStateInformation` para rejeitar tags inválidas (`<garbage>`), impedindo a corrupção do estado do processador em testes de estresse de recall de parâmetros.

---

## 2. Resultados dos Testes Automatizados

### 2.1 `CombinadorTest.exe`
- **Status:** **PASS** (4/4 testes aprovados)
- Test 1 (Zero input): `PASS (max abs = 0)`
- Test 2 (Unit impulse): `PASS (energy = 0.125756)`
- Test 3 (Continuity): `PASS (discontinuities = 0)`
- Test 4 (Reset): `PASS (max diff = 0)`

### 2.2 `SexyStreamTest.exe`
- **Comando:** `SexyStreamTest.exe analysis\speech48.wav out\test_out.wav`
- **Status:** **PASS**
- **Métricas:** `RTF = 0.268452` (Tempo real acelerado: 3.77 s para 14.05 s de áudio)
- **Código de Saída:** `0`

### 2.3 `SexyProcessorTest.exe`
- **Status:** **PASS**
- **Métricas:**
  - `underruns = 0`
  - `max_vs_offline = 0`
- **Saída:** `PASS unity, reported latency, stereo, separation, blocks 64/128/512/777, state recall, bypass, realtime`
- **Código de Saída:** `0`

---

## 3. Identificação do Binário Candidato

- **Arquivo:** `native/sexyvox/out/SEXYVOX_artefacts/Release/VST3/SEXYVOX.vst3/Contents/x86_64-win/SEXYVOX.vst3`
- **SHA-256:**
  `E667FAB906F91060508561D7116623ABD4F9C582F9CCC4D76E5CE2B86B73CC3C`

*(Observação: Em conformidade com as restrições estritas, o binário NÃO foi copiado para o diretório de produção do sistema `C:\Program Files\Common Files\VST3\SEXYVOX.vst3`).*
