# Relatório de Validação no FL Studio (Etapa 09)

**Data:** 2026-10-24  
**Projeto:** `SEXYVOX` (Plugin VST3 realtime para FL Studio)  
**Escopo:** Validação do candidato integrado no ambiente do FL Studio e protocolo de teste.

---

## 1. Identidade e Hash do Candidato VST3

- **Caminho do Build (Artefato):**  
  `native\sexyvox\build_combinador\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3`
- **Caminho do Binário Interno (`x86_64-win`):**  
  `native\sexyvox\build_combinador\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3\Contents\x86_64-win\SEXYVOX.vst3`
- **Hash SHA-256 do Binário VST3:**  
  `C38301114CF3434527BD4B2C90CC5EBB5088DBAA347C8D1CA5F254FD6014A6DA`
- **Latência Declarada ao Host:**  
  `8192` amostras (170,67 ms a 48 kHz).
- **Parâmetros Expostos:**  
  - `Voice` (Ganho da voz processada)
  - `Noise` (Ganho do ruído atenuado/residual)
  - `Bypass` (Bypass do processamento)

---

## 2. Configurações e Resultados dos Testes de Integração Pré-Host

Antes da hospedagem no DAW, a integridade funcional do pipeline C++ foi validada rigorosamente na linha de comando (`SexyStreamTest.exe`):

- **Executável de Teste:** `native\sexyvox\out\SexyStreamTest_artefacts\Release\SexyStreamTest.exe`
- **Entrada de Áudio:** `analysis\speech48.wav` (48 kHz, estéreo/dual-mono, ~14 segundos)
- **Código de Saída:** `0` (Sucesso)
- **Fator de Tempo Real (RTF):** `0.229` (Processamento em 3.22 s, altamente otimizado para tempo real)
- **Estabilidade Numérica:** Saídas estritamente finitas (`NaN / Inf` ausentes), sem exceções ou *Access Violations*.
- **Caminhos Executados:** Caminho DF (Deep Filtering), caminho ERB (Equivalent Rectangular Bandwidth), buffer de alinhamento temporal (384 amostras), atraso circular direto ($D_{128}$) e filtro FIR passa-baixas complementar ($L()$).

---

## 3. Estratégia de Implantação Segura e Reversível

Para evitar sobrescrever cegamente o plugin estável previamente instalado em `C:\Program Files\Common Files\VST3\SEXYVOX.vst3`, adota-se o seguinte protocolo:
1. **Backup da Versão Anterior:**  
   `Copy-Item "C:\Program Files\Common Files\VST3\SEXYVOX.vst3" "C:\Program Files\Common Files\VST3\SEXYVOX.vst3.bak" -Recurse -Force`
2. **Implantação do Candidato:**  
   `Copy-Item "native\sexyvox\build_combinador\SEXYVOX_artefacts\Release\VST3\SEXYVOX.vst3" "C:\Program Files\Common Files\VST3\SEXYVOX.vst3" -Recurse -Force`
3. **Rollback (se necessário):**  
   Restaurar a partir de `SEXYVOX.vst3.bak`.

---

## 4. Instruções e Protocolo de Validação Manual no FL Studio

Como o FL Studio (`C:\Program Files\Image-Line\FL Studio 2026\FL64.exe`) é um host DAW de interface gráfica interativa, a validação completa no host segue o roteiro abaixo:

1. **Atualização do Plugin no Host:**
   - Fechar o FL Studio (se aberto).
   - Aplicar a cópia do candidato para `C:\Program Files\Common Files\VST3\SEXYVOX.vst3` conforme o procedimento seguro acima.
2. **Verificação de Reconhecimento:**
   - Abrir o FL Studio 2026.
   - Ir em `Options > Manage plugins` e executar `Find plugins` (ou verificar se o SEXYVOX é detectado como VST3).
3. **Teste de Inserção e Áudio:**
   - Inserir o `SEXYVOX` em um canal do Mixer (ex: Insert 1) com uma trilha de voz de teste a 48 kHz.
   - Iniciar reprodução contínua.
   - Verificar se há áudio audível, ausência de cliques, silêncio inesperado, travamentos ou *underruns* de CPU.
4. **Verificação de Controles e Automação:**
   - Girar os botões `Voice`, `Noise` e acionar `Bypass` na interface gráfica do VST3.
   - Confirmar se a atenuação e separação de voz/ruído respondem em tempo real.
   - Testar gravação de automação e reabertura do projeto (`.flp`) para validar o salvamento de estado XML/estado interno.
5. **Teste de Variação de Buffer do Host:**
   - Testar o desempenho com tamanhos de buffer de ASIO/Audio Settings em `128`, `256`, `512`, `1024` e `2048` amostras.
   - Verificar se a latência reportada (`8192` amostras) é compensada adequadamente pelo motor de compensação de latência de plugin (*Plugin Delay Compensation - PDC*) do FL Studio.

---

## 5. Limitações Conhecidas Registradas

- **Frequência de Amostragem:** O motor opera nativamente em `48 kHz`. Taxas como 44,1 kHz atualmente passam o sinal em *bypass* (dry), conforme arquitetura estabelecida.
- **Fidelidade do Filtro FIR:** O filtro de 257 taps baseado em janela de Hamming aproxima o filtro complementar $L()$, mas a equivalência numérica bit a bit exata com o binário original do CEDAR não foi demonstrada por ausência de coeficientes nativos originais (conforme Etapa 08.3).
- **Latência Fixa:** A latência declarada de 8192 amostras prioriza estabilidade e ausência de *underruns*, exigindo PDC ativo no host.

---

## 6. Estado Atual das Pendências e Próximos Passos

- **Identidade do Build:** Candidato compilado e verificado via `SexyStreamTest.exe` (Exit Code 0).
- **Implantação e Roteiro de Host:** Prontos para execução no FL Studio 2026.
- **Próxima Ação:** Executar a validação interativa no FL Studio seguindo o protocolo acima e registrar os resultados finais de host.
