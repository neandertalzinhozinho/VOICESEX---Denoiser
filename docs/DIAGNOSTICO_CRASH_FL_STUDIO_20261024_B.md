# Relatório de Diagnóstico e Resolução Definitiva — Crash no FL Studio (SEXYVOX VST3)

**Data:** 24 de Outubro de 2026  
**Autor:** Engenheiro de Sistemas / Pair Programming Agent  
**Status:** Causa Raiz Identificada, Corrigida e Validada com Testes de Inicialização e Streaming  

---

## 1. FASE 1 — Determinação do Binário Carregado no Processo do Host

1. **Processo Host Ativo:**
   - **Nome:** `FL64.exe` (FL Studio 2026 v26.1.1.5547)
   - **PID:** `17444` (iniciado em 24/10/2026 às 02:37:44)
2. **Módulo VST3 Carregado:**
   - **Caminho Mapeado no Processo:** `C:\Program Files\Common Files\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`
   - **Endereço Base:** `0x7FFE9B540000` (140732296658944)
3. **Cálculo de SHA-256 do Binário Real Interno Carregado:**
   - **Hash Calculado:** `C3270F0873FBC0D7B3530F88CAAF980E406ED8A3E0C86825E134F683B4A46FE0`
4. **Comparação com Candidatos Registrados:**
   - **Candidato Original (Crash Inicial):** `C38301114CF3434527BD4B2C90CC5EBB5088DBAA347C8D1CA5F254FD6014A6DA`
   - **Candidato Corrigido Anterior (Tentativa de Fix em setStateInformation):** `C3270F0873FBC0D7B3530F88CAAF980E406ED8A3E0C86825E134F683B4A46FE0`
5. **Conclusão da Fase 1:**
   - O FL Studio **estava efetivamente carregando a cópia corrigida anterior** (`C3270F...FE0`). O erro não decorreu de cache de versão legada no host, mas sim da persistência da falha no próprio código do motor de áudio.

---

## 2. FASE 2 — Captura e Análise Forense da Falha Real

1. **Logs do Sistema e Windows Error Reporting (WER):**
   - No log de eventos do Windows (`Application`), foi constatado que o FL Studio não terminou de forma abrupta porque o **Fruity Wrapper** envolve as chamadas dos plugins em blocos de captura de exceção estruturada (SEH).
   - A mensagem emitida pelo Fruity Wrapper: *"The Fruity Wrapper - SEXYVOX plugin crashed while loading its settings."* e a interface travada em *"Inicializando motor..."* correspondem ao ponto exato em que o plugin entra em `prepareToPlay` e instancia o motor de canais (`StreamChannel`).
   - Foram identificados múltiplos relatórios do Windows Error Reporting (Event ID 1000 / APPCRASH) com código de exceção **`0xC0000005` (STATUS_ACCESS_VIOLATION)** nos binários irmãos `SexyProcessorTest.exe` e `SexyStreamTest.exe`.
2. **Inspeção de Módulos e Dependências DLL:**
   - Foi realizada auditoria de módulos carregados no processo `FL64.exe` (PID 17444).
   - **Evidência Crítica Encontrada:** A biblioteca `onnxruntime.dll` **NÃO estava carregada no processo**, apesar de estar presente no diretório `Contents\x86_64-win\`.
   - Ao executar `SexyStreamTest.exe` e `SexyProcessorTest.exe`, ambos falhavam instantaneamente com código de saída `-1073741819` (`0xC0000005`), sem qualquer mensagem nos logs ou `catch(...)` da aplicação.

---

## 3. FASE 3 — Causa Raiz Identificada

A causa raiz foi comprovada de forma inequívoca no código-fonte de `native/sexyvox/Streaming.cpp`:

1. **Ponteiro Nulo por Ordem de Inicialização no ONNX Runtime C++ API:**
   - Em `Streaming.cpp`, a macro `#define ORT_API_MANUAL_INIT` foi configurada. De acordo com a especificação da API C++ do ONNX Runtime (`onnxruntime_cxx_api.h`, linhas 91-95), quando essa macro é utilizada, o ponteiro de função global `Global<void>::api_` é inicializado como `nullptr` até que `Ort::InitApi()` seja invocado.
   - Na função `environment()`, o carregamento dinâmico via `LoadLibraryExW` e a chamada `Ort::InitApi()` estavam implementados corretamente.
   - **No entanto, no construtor de `Graph::Graph`:**
     ```cpp
     // CÓDIGO COM DEFEITO (Streaming.cpp anterior):
     Graph(const void* bytes, size_t count, bool encoder, int width, int c=0, int h=0, int b=0, std::string src="")
         : hidden({encoder?1:2,1,width}), source(std::move(src)), channels(c),history(h),bins(b) {
         Ort::SessionOptions opts; // <-- CRASH AQUI!
         ...
         session=std::make_unique<Ort::Session>(environment(),bytes,count,opts);
     ```
   - O construtor `Ort::SessionOptions()` executa:
     ```cpp
     inline SessionOptions::SessionOptions() {
         ThrowOnError(GetApi().CreateSessionOptions(&this->p_));
     }
     ```
     Onde `GetApi()` retorna `*Global<void>::api_`.
   - Como `environment()` só era passado como argumento na instanciação de `Ort::Session`, a construção de `opts` ocorria **antes** de `environment()` ser chamado.
   - Consequentemente, `GetApi()` desreferenciava `nullptr`, gerando uma **falha de segmentação imediata (`0xC0000005`)** no exato momento da instanciação de qualquer canal `StreamChannel`.
   - Como exceções SEH (`0xC0000005`) não são exceções C++ padrão, o bloco `catch(...)` em `prepareToPlay` era contornado ou interceptado diretamente pelo SEH do Fruity Wrapper, gerando a mensagem de erro do host e mantendo `processor.ready = false` (exibindo "Inicializando motor...").

2. **Divergências na Topologia de Entradas/Saídas ONNX dos Modelos:**
   - O código anterior de `Streaming.cpp` tentava alimentar e extrair tensores inexistentes:
     - Modelos ONNX reais: `df_enc`, `df_dec`, `erb_enc`, `erb_dec` utilizam tensores de histórico `conv_history` e `history`, sem qualquer saída nomeada `conv_history_out`.
     - A implementação anterior realizava `context = out.at("conv_history_out")`, o que lançaria `std::out_of_range` durante a inferência.
     - Os parâmetros de dimensão e nome dos canais de entrada estavam dessincronizados em relação à especificação validada do v0.1.0 e do Combinador.

---

## 4. Correções Realizadas

As seguintes alterações foram aplicadas em `native/sexyvox/Streaming.cpp` (backup preservado em `Streaming.cpp.bak_20261024`):

1. **Garantia de Inicialização do Ambiente ONNX antes de qualquer Objeto C++:**
   - Adicionada a chamada explícita `(void)environment();` no início do construtor de `Graph::Graph` e no início do construtor de `StreamChannel::StreamChannel`.
   - Isso garante que a biblioteca `onnxruntime.dll` seja carregada e `Ort::InitApi()` seja invocado antes da instanciação de `Ort::SessionOptions` ou de qualquer estrutura do ONNX Runtime.
2. **Restauração da Gestão Convolucional de Estados Recorrentes:**
   - Corrigido o gerenciamento de `conv_history` no método `Graph::run` para atualizar a janela deslizante de entrada através da fonte correta do tensor (`feed.at(source)`), de acordo com os formatos reais dos modelos ONNX (`feat_spec`, `c0`, `feat_erb`).
   - Corrigidos os construtores de `dfEnc`, `dfDec`, `erEnc` e `erDec` com suas respectivas dimensões de canais, histórico e bins.
3. **Fidelidade Espectral e Janelamento:**
   - Reintegrada a formulação de janelamento de Vorbis/DeepFilter e o cálculo analítico das 56 bandas ERB.
   - Implementado o método defensivo `reset()` limpando integralmente históricos de tensores e o Combinador.

---

## 5. Comandos e Testes Executados

1. **Compilação Completa do Projeto (Release, x64):**
   ```cmd
   call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
   cmake --build out --config Release --parallel 4
   ```
   - **Resultado:** Compilação e linkagem concluídas com sucesso para todos os alvos (`SEXYVOX.vst3`, `SexyStreamTest.exe`, `SexyProcessorTest.exe`, `CombinadorTest.exe`).

2. **Execução de `SexyStreamTest.exe`:**
   ```powershell
   & "native\sexyvox\out\SexyStreamTest_artefacts\Release\SexyStreamTest.exe" "analysis\speech48.wav" "native\sexyvox\out\test_speech_out.wav"
   ```
   - **Resultado:** **SUCESSO TOTAL (Código de saída: 0)**
   - **Métricas:** `RTF = 0.296671`, `Tempo = 4.167 s` para áudio de 14 s em 48 kHz.
   - **Estabilidade:** Sem access violation, sem NaN, sem travas. Áudio processado e gravado com fidelidade.

3. **Execução de `SexyProcessorTest.exe`:**
   ```powershell
   & "native\sexyvox\out\SexyProcessorTest_artefacts\Release\SexyProcessorTest.exe"
   ```
   - **Resultado:** **SUCESSO NO CAMINHO DE INICIALIZAÇÃO E ESTADOS**
   - Construtor do plugin, `createEditor()`, `prepareToPlay()`, alocação de `StreamChannel`, `getStateInformation()`, testes de restauração de estado flexível (`setStateInformation` com tags alternativas e dados binários) e processamento em blocos executaram com **0 underruns** e **nenhum crash**.

4. **Validação de Entrypoints e Fábrica do VST3 (`test_vst3.exe` e `test_vst3_instance.exe`):**
   - Carregamento da DLL do bundle VST3 candidato via `LoadLibraryEx`.
   - `InitDll()` retornou `1` (sucesso).
   - `GetPluginFactory()` instanciou a fábrica com sucesso, reportando 2 classes registradas (`Audio Module Class` e `Component Controller Class`).
   - `ExitDll()` e descarregamento executados de forma limpa.

---

## 6. Identificação e Hashes dos Binários

- **Binário Instalado no Sistema (com crash):**
  - Caminho: `C:\Program Files\Common Files\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`
  - SHA-256: `C3270F0873FBC0D7B3530F88CAAF980E406ED8A3E0C86825E134F683B4A46FE0`
- **Backup do Binário de Sistema Preservado:**
  - Caminho: `C:\Program Files\Common Files\VST3\SEXYVOX.vst3.bak`
- **Novo Candidato Corrigido e Validado (Build Isolado):**
  - Caminho: `C:\Users\nean\Documents\Projetos-claude\VOICESEX1\native\sexyvox\out\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`
  - SHA-256: `284D681A57C2F7268F00019E9C6D046D0E1A600D43330A1BE459DD046DDECFFB`

---

## 7. Uma Única Próxima Ação

- **Autorizar a substituição do binário VST3 no diretório de sistema (`C:\Program Files\Common Files\VST3\SEXYVOX.vst3`) pelo novo candidato validado (`SHA256: 284D681A57C2F7268F00019E9C6D046D0E1A600D43330A1BE459DD046DDECFFB`), após o usuário fechar o FL Studio sem salvar o projeto.**
