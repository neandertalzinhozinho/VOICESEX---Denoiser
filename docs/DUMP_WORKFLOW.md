# VoicEX 2 — Memory Dump Workflow (Análise Dinâmica)

## Alvo
`C:\Program Files\Common Files\VST3\CEDARVoicEX.vst3\Contents\x86_64-win\CEDARVoicEX.vst3`
Host: FL Studio (VST3). Plugin rodando, processando áudio.

## Ferramentas
- x64dbg (64-bit)
- Scylla (IAT reconstruction)
- IDA Pro 9.3 (análise estática do dump)

---

## Fase 1 — Preparar o host

1. Abra FL Studio.
2. Carregue CEDARVoicEX.vst3 em um canal de mixer.
3. Carregue áudio com voz + ruído para forçar o plugin a processar (ativa o caminho de inferência de IA).
4. Deixe o plugin rodando por alguns segundos para garantir que todos os módulos estejam carregados na RAM (incluindo eventuais estágios de descriptografia/unpacking).

---

## Fase 2 — Anexar o x64dbg ao FL Studio

1. Abra x64dbg (x64).
2. File → Attach → selecione o processo `FL64.exe` (ou `FL.exe`, 32-bit se for a versão x86 do FL Studio).
3. O processo vai pausar. Preste atenção: o áudio pode travar enquanto anexado — isso é esperado.

---

## Fase 3 — Localizar o módulo do plugin na memória

No x64dbg:

```
Modules tab → procure por CEDARVoicEX.vst3 (ou o nome interno do módulo carregado)
```

Anote:
- Base address do módulo
- Entry point (se houver)
- Tamanho da imagem

Se o plugin for carregado como DLL dentro do FL Studio, ele aparecerá na lista de módulos do processo.

---

## Fase 4 — Fazer o dump da memória

### Opção A: x64dbg (direto)

No x64dbg, com o processo anexado e pausado:

1. Abra o painel `Memory Map`.
2. Clique com o botão direito sobre a região de memória do módulo CEDARVoicEX.
3. Selecione "Dump memory to file" → salve como `CEDARVoicEX_dump.bin` na pasta `VOICESEX1\dumps\`.

### Opção B: Process Hacker / Process Dump

Se o x64dbg travar ao anexar ao FL Studio:

1. Abra Process Hacker (ou Task Manager).
2. Clique com o botão direito no processo FL Studio → Create Dump → Full Dump.
3. O dump completo conterá todo o espaço de endereçamento, incluindo o plugin.

---

## Fase 5 — Reconstruir o IAT com Scylla

O dump direto terá importações quebradas (IAT vazia ou com endereços de runtime). Scylla conserta isso.

1. Abra Scylla.
2. Selecione o processo FL Studio (ou carregue o dump se Scylla suportar off-line).
3. Clique em "IAT Autosearch" → Scylla procura o IAT do módulo CEDARVoicEX.
4. Quando encontrar, clique em "Get Imports" → Scylla resolve as funções importadas.
5. Clique em "Dump" → selecione o módulo → marque "Rebuild IAT" → salve como `CEDARVoicEX_unpacked.dll`.

Resultado: um PE válido com IAT reconstruído, pronto para o IDA Pro 9.3.

---

## Fase 6 — Carregar no IDA Pro 9.3

1. Abra `CEDARVoicEX_unpacked.dll` no IDA Pro 9.3.
2. Aguarde o auto-análise terminar.
3. Se houver funções não reconhecidas, use "Create function" (P) manualmente.
4. Salve a base de dados IDA (`.i64`).

---

## Pontos de atenção

- **Anti-debug / anti-VM**: se o plugin detectar o debugger e travar, procure por instruções `CPUID`, `RDTSC`, `IsDebuggerPresent`, `CheckRemoteDebuggerPresent` no dump e NOP-as antes de re-dump.
- **Obfuscação**: se o código ainda estiver criptografado após o dump, você precisa fazer o dump no momento exato após o unpacking stub terminar (use breakpoint no `VirtualAlloc`/`VirtualProtect` seguido de `memcpy`/`memmove` para a região descriptografada).
- **IAT**: se Scylla não encontrar o IAT automaticamente, use "IAT Scanner" ou procure manualmente por thunks de importação.

---

## Próximo passo após o dump

Com o `CEDARVoicEX_unpacked.dll` no IDA, execute as 5 etapas do plano de RE (arquitetura, bifurcação de streams, extração de pesos, clonagem, validação).