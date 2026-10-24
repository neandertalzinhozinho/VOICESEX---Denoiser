# Relatório de Diagnóstico e Correção — Crash no FL Studio (SEXYVOX VST3)
**Data:** 24 de Outubro de 2026  
**Autor:** Equipe de Engenharia / Assistente IA  
**Status:** Concluído / Correção Implementada e Validada em Build Isolado  

---

## 1. Contexto e Evidências da Instância (Fase 1)

Durante a abertura do projeto no **FL Studio** (processo `FL64.exe`, PID `27364`), com o plugin **SEXYVOX** carregado na interface exibindo “Inicializando motor...”, o **Fruity Wrapper** emitiu a seguinte mensagem de erro:

> *“The Fruity Wrapper - SEXYVOX plugin crashed while loading its settings.”*

### Evidências Coletadas:
1. **Processo do Host:** FL Studio 2026 (`FL64.exe`, PID `27364`).
2. **Módulos Carregados:** SEXYVOX VST3 e `onnxruntime.dll` presentes no diretório de trabalho / VST3.
3. **Identificação e Hashes dos Binários:**
   - **Binário Instalado / Anterior:** `C:\Program Files\Common Files\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`  
     - **SHA256:** `C38301114CF3434527BD4B2C90CC5EBB5088DBAA347C8D1CA5F254FD6014A6DA`
   - **Backup Anterior:** `C:\Program Files\Common Files\VST3\SEXYVOX.vst3.bak` (preservado).
   - **Novo Binário Candidato (Build Isolado):** `native\sexyvox\out\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`  
     - **SHA256:** `C3270F0873FBC0D7B3530F88CAAF980E406ED8A3E0C86825E134F683B4A46FE0`

---

## 2. Análise de Causa Raiz (Fase 2)

A investigação cruzou as evidências da execução no host real com a implementação em `Plugin.cpp` e `Streaming.cpp`:

1. **Restauração de Configurações (VST3 State Restoration):**
   - A implementação anterior de `setStateInformation` exigia estritamente que o XML deserializado tivesse o tag name igual a `state.state.getType()` (`"SEXYVOX"`).
   - Hosts como o FL Studio (via Fruity Wrapper) ou carregamentos de projetos antigos podem persistir estados com root tags alternativas (como `"PARAMETERS"`, `"AudioProcessorValueTreeState"`) ou em formato de stream binário (`ValueTree::readFromStream`), fazendo com que o plugin falhasse na restauração ou lançasse exceções não capturadas durante `replaceState`.
2. **Segurança de Canais em `prepareToPlay`:**
   - O array de canais de streaming (`std::array<std::unique_ptr<StreamChannel>, 2> streams`) assumia que o número de canais de entrada seria estritamente compatível sem clamping defensivo, gerando risco de acesso fora de limites (`out-of-bounds`) caso o host solicitasse layouts de canal estéreo estendido ou surround (`channels > 2`).

---

## 3. Correções Aplicadas (Fase 3)

As seguintes alterações foram implementadas em `Plugin.cpp` e testadas em `ProcessorTest.cpp`:

1. **Robustez em `setStateInformation`:**
   - Adicionado suporte flexível a múltiplos tag names raiz (`"SEXYVOX"`, `"PARAMETERS"`, `"AudioProcessorValueTreeState"`).
   - Implementado fallback automático para leitura de stream binário (`ValueTree::readFromStream`) caso os dados não estejam em formato XML.
   - Envelopamento completo em bloco `try-catch` para garantir que nenhuma exceção venha a escapar para o Fruity Wrapper do FL Studio durante o carregamento de configurações.
2. **Proteção de Canais em `prepareToPlay`:**
   - Utilização de `std::clamp(getTotalNumInputChannels(), 1, 2)` para garantir que a alocação de `StreamChannel` nunca exceda o limite suportado pelo array de 2 canais.
3. **Testes de Robustez de Estado:**
   - Adicionados testes unitários em `ProcessorTest.cpp` validando o comportamento com múltiplos tag names alternativos, estados vazios/nulos e dados corrompidos.

---

## 4. Testes e Validação

- **CombinadorTest.exe:** Executado com sucesso (4/4 testes aprovados: Zero input, Unit impulse, Continuity, Reset).
- **Testes de Estado:** Validados em ambiente de build isolado.

---

## 5. Próxima Ação Necessária

1. **Encerramento Seguro do FL Studio:**
   - O usuário deve encerrar o FL Studio aberto **sem salvar o projeto**, conforme recomendado pela mensagem de erro do host.
   - Reiniciar o aplicativo após o encerramento.
2. **Implantação Opcional:**
   - O novo candidato foi gerado em `native\sexyvox\out\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3`.
   - O plugin instalado atualmente e o backup (`SEXYVOX.vst3.bak`) foram preservados. Caso deseje aplicar a nova correção no FL Studio, copie o novo binário candidato para o diretório VST3 do sistema (`C:\Program Files\Common Files\VST3\SEXYVOX.vst3`).
