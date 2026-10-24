# Checkpoint da investigação — 23/10/2026

## Estado e escopo

Registro das descobertas obtidas na investigação de latência e inicialização do SEXYVOX. Não representa equivalência completa. Código e binários permanecem sem alterações; não houve recompilação, patch ou novos dumps. Instâncias novas não estabelecem os estados das oito referências históricas.

Dois subagentes revisaram em paralelo, somente leitura, a latência anunciada e a inicialização DF/ERB. Ambos concluíram sem executar novos ensaios ou alterar arquivos. Este checkpoint incorpora seus resultados e a consolidação abaixo. O relatório separado docs/INVESTIGACAO_LATENCIA_INICIALIZACAO_20261023.md ainda está pendente.

## Latência anunciada: cálculo confirmado

A expressão experimental 640 + 2 × MMC(B,512) foi REFUTADA como regra geral. Coincide nos cinco tamanhos inicialmente testados, mas falha em outros três. B é maxSamplesPerBlock configurado no setup; não foi demonstrado que equivale ao tamanho efetivo de todo callback variável.

O helper VST3 RVA 0x24CD3E0–0x24CD5A2 aplica contratos 64 → 512 → 32 → 32 → B. Para k a partir de1, calcula n=kB e arredonda z=n sucessivamente para baixo a múltiplos desses contratos. Acumula C=max(C,n−z) e termina no primeiro déficit zero. Retorna três qwords: B,640,C. Laço de divisões inteiras: 0x24CD500–0x24CD552.

VST3 0x2026FE0–0x2027037 chama o helper em 0x2027008, calcula extra=2B+2C e grava em completeAdapter+0x38; soma640 e grava total em completeAdapter+0x30; segue para inicialização dos buffers em 0x2025B30. Getter VST3 0x300070 retorna o DWORD processor+0x3C, quando positivo. O caminho estático completo entre esses dois campos ainda não foi demonstrado: a igualdade é observada dinamicamente, não uma cadeia de setters integralmente rastreada.

O getter 0x2026F60 lê DWORD completeAdapter+0x28; a interface em engine+0x218 tem deslocamento RTTI0x1D8 em relação ao objeto completo. Setup do host fornece B e o getter retorna B; endereço exato da escrita original em +0x28 ainda é lacuna.

| B | C | Extra | Total anunciado | Fórmula antiga MMC |
|---:|---:|---:|---:|---:|
|128|384|1024|1664|1664|
|512|0|1024|1664|1664|
|640|640|2560|3200|5760|
|768|768|3072|3712|3712|
|960|960|3840|4480|16000|
|1000|1000|4000|4640|128640|
|1024|0|2048|2688|2688|
|2048|0|4096|4736|4736|

Support 0xBC2B30 declara latência nativa512+128=640. O extra vem do adaptador de blocos e seus buffers, não de uma medição de tempo ONNX.

## Buffers circulares

Estado em completeAdapter+0x40. Duas vistas sobre armazenamento compartilhado têm descritores state+0x228 (entrada) e state+0x450 (saída). Primeiros qwords: capacidade, cursor, campo+0x10 e canais; o campo+0x10 permaneceu zero, sem semântica de índice comprovada. Ponteiros de canais começam em+0x20.

Inicialização 0x2025B30: capacidade=R+extra, com R1024 a48kHz. Cursor de saída recebe deslocamento R módulo capacidade em 0x2025C7C–0x2025CBD. Distância circular (entrada−saída) módulo capacidade=extra. Ambos os cursores avançam pelo número real de amostras do callback. Armazenamento de áudio zerado nos dois canais antes da entrada.

PID14532: B512/capacidade2048/cursores448 e1472/extra1024; B768/capacidade4096/cursores640 e1664/extra3072; B640/capacidade3584/cursores640 e1664/extra2560. Não interpretar a distância modular inversa como quantidade disponível, nem as vistas como duas FIFOs independentes.

## Primeiros frames e alinhamento

Configuração confirmada: DF FFT1024/hop512/batch1; ERB FFT256/hop128/batch4;48kHz. Wrapper+0x7C guarda alinhamento384. Ordem Support 0xBC2C40: ERB em0xBC2C69, DF em0xBC2C78, combinador em0xBC2CB8. Prefixo ERB384 zerado em0xBC3027–0xBC3059; transporte da cauda384 em0xBC2CBD–0xBC2D0E. Reset do combinador:0xBBE810.

Históricos de áudio e cinco arrays do combinador estavam zerados nas capturas válidas. Isso não comprova todos os estados GRU, convolucionais e de normalização originais. Scratch pós-FFT não representa janela temporal; valores residuais/NaN de wrappers inativos foram descartados.

F é a fase inicial, equivalente ao cursorEntrada inicial módulo512 nos casos conjuntamente validados. PID2568: F448 produz primeiro bloco DF[-448..63], com448zeros e entrada[0..63]; F0 produz[0..511]. A tentativa F320 tem active=[] e foi excluída. PID5644: F384 produz[-384..127], depois[128..639],[640..1151], com vetores completos exatos e estáveis após50ms.

WAV PID23880/F64, wrapper ativo0x53ece5acc80:

| Callback | Entrada do host | DF atual | Último hop ERB |
|---:|---|---|---|
|0|0..511|−64..447|320..447|
|1|512..1023|448..959|832..959|
|2|1024..1535|960..1471|1344..1471|
|3|1536..2047|1472..1983|1856..1983|

Os16 registros avançam512, sem deriva; erros integrais DF/ERB e transporte ERB iguais a zero. Correção da revisão: o último hop ERB do callback2 começa em1344, não1280. O wrapper0x53ece5ada80 é inativo e não sustenta conclusões sobre estado neural.

Inferência a partir desses vetores e do código: primeira janela DF[-576..447]; primeiras quatro janelas ERB[-192..63],[-64..191],[64..319],[192..447]. Não houve captura direta dessas janelas antes da FFT. A origem da seleção de F permanece aberta. Frames preenchidos ainda executam inferência e atualizam estados.

## Combinador já confirmado

Voice=L(d)+[D128(e_alinhado)−L(e_alinhado)]. Filtro complementar cutoff3092Hz, FFT512, passo256,257pesos double, recorte[128:384], histórico256 e atraso direto128. Support: filtro0xBC22F0, complemento0xBC2550, soma0xBBEB77. Reconstrução dos pesos: erro máximo4,44e−16;611segmentos estáveis de256amostras com erro inferior1e−12.

No WAV,16comparações do transporte do prefixo384 tiveram erro zero. Coordenada lógica inicial do combinado:−F−640, deduzida de alinhamento e recortes; não define o primeiro valor não zero da saída bruta. Igualdade de uma renderização com referência histórica não prova estados internos iguais.

## Saída bruta versus latência anunciada

Host VST3 ctypes, IAudioProcessor::process direto, ciclo de vida normal, float32 estéreo duplicado,48kHz, sem preset ou automação. Pedalboard apenas carrega módulo. Entrada e saída mantêm N amostras, sem compensação/corte do host. Harness estável mantém buffers vivos até fechar.

WAV PID23880/B512: sem48000zeros iniciais;1.014.510amostras reais,8192zeros finais e arredondamento512, total1.022.976. Latência1664; primeiro |Y|>1e−12 em1077; pico antes de1664=3,617719812609721e−7. Comparação Y[1664:] com X[:-1664]: erro máximo1,4901161193847656e−8; RMSE1,4738985531201325e−9. A cauda cobre todo o WAV real.

Marcadores PID6060/B640: N16384, latência3200; primeiro |Y|>1e−12 em2341; pico do prefixo1,1832631571451202e−5. Erro máximo após alinhamento3200=4,470348358154297e−8; RMSE1,6638066011579644e−9. Marcador entrada0 reaparece em3200; entrada4096 em7296. Comparação cobre13184amostras; últimas3200 não foram validadas com flush nesse ensaio.

Primeiro não zero, prefixo transitório, preenchimento neural, latência nativa640 e deslocamento dominante são grandezas distintas. Ganhos padrão quase identidade validam recomposição/alinhamento, não fidelidade neural isolada Voice. Controlador novo: Voice e Noise normalizados0,7142857313156128, Bypass0; normalizedToPlain genérico não demonstrou dB.

## Consequências no C++ atual — somente leitura

native/sexyvox/Plugin.cpp: linha8 fixa hop512/latência4096; linha62 anuncia4096 e usa dry4096; linhas76–78 fazem output.index=input.index−1 e descartam negativo; linhas99–100 agendam wanted=(counter−latency)/512 após4096. Alterar só latência anunciada quebraria relação com agendamento físico.

native/sexyvox/Streaming.cpp: linhas103–110 inicializam buffers/contextos; linha151 usa os primeiros56valores de máscara ERB do batch4; linha174 mascara bins DF>=108; linha179 apenas síntese DF escreve Voice. Faltam síntese ERB separada, alinhamento384 e combinador complementar com recorte128. Descartar um pacote512 não resolve origem−640 nem fases adicionais. Controlar índices por amostra/pré-roll separadamente da margem do worker.

SHA256 Plugin.cpp:2f35a11eb6930e7fc4898d8185813155b9f0e9073bf61664f4a3e9ca1899be62

SHA256 Streaming.cpp:4b5c5f58bf81ba4932688c3baa8ecf12a8bcfd8935daad022c55431b200831e9

## Evidências, identidade e exclusões

Diretório temporário existente: C:\Users\nean\AppData\Local\Temp\sexyvox_latency_20261023_94e6e31bd14642ff866b81c2161e33ea

Arquivos: latency_decomposition.json/PID12892; latency_decomposition_counterexamples.json/PID12408; queue_initialization.json/PID14532; final_validation.json/PID2568; exact_mapping.json/PID5644; wav_512.json/PID23880; marker_counterexample_640.json/PID6060; parameters_default.json. Instrumentação existente: host_probe.py, host_probe_stable.py, trial_v3.py, decompose.py, decompose_counterexamples.py, queue_probe.py, final_validation.py, exact_mapping.py, parameters_probe.py.

Dump preexistente dumps/CEDARVoicEX_from_memory.bin: nas regiões verificadas offsetArquivo=RVA−0x40000; não generalizar sem comparar. final_validation.json confirma igualdade com memória em300070/15bytes,2026FE0/88,24CD3E0/451,2025B30/384. Nenhum novo dump.

VST3 SHA256:17ea5325bafcbafaf0589f55dfced09e109bf4f95e881304c27ffcc602774537

WAV preservado SHA1:a6a0265063f9e7b1ba830a35dda2f05399dec3c1

WAV entrada float32+cauda SHA256:f5771777a013fd04acb431b37b8d8079c94c926fc5c3c3327731776265f2e8ad

WAV saída SHA256:b96367f0a45e459fe7da758ad940da4a7d189edc0b40de306eb9cd9888efa997

Marcadores entrada SHA256:6f3c2b3bc3caf27cbd3ae1d1541d36da4b827592ad96c1426f20bfdae3ec1dc3

Marcadores saída SHA256:68d8a64649acabb4731cdd8acb08e4699f927b1d53b809ffe9584f2a0b764be2

Watchpoints que capturaram SingleStep sem escrita confirmada foram rejeitados. Watchpoint de execução filtrado nos setters não capturou eventos; filho terminou normalmente. Não fundamentam a causa da latência. Não houve investigação DRM/ativação.

Questões abertas: escolha da fase inicial; estados neurais originais completos; relação causal com diferenças históricas; callbacks variáveis; prazos reais/FL Studio; caminhos estáticos completos de propagação dos campos de latência. NPZ histórico não contém os estados necessários. Nenhum percentual global novo de equivalência decorre destes ensaios.

## Consolidação das duas revisões

Não há contradição entre o cálculo recuperado e os oito registros. A contradição é com a generalização anterior por MMC. O cálculo do extra e sua materialização em armazenamento circular estão sustentados por código e leituras; a propagação estática completa até o campo consultado pelo host continua incompleta. A configuração de B pelo setup e a igualdade dos getters são fatos dinâmicos, não rastreamento integral de setter.

A segunda revisão corrigiu o intervalo ERB do terceiro callback para1344..1471. Nenhuma evidência válida sustenta F320, janelas temporais extraídas de scratch pós-FFT ou estados neurais integralmente zerados. Os primeiros blocos armazenados, transporte384 e atraso global da recomposição estão confirmados; as janelas pré-FFT são inferências e o seletor deF continua desconhecido.

Âncora adicional da primeira revisão: dumps/VoicExSupport_rva_aligned.bin, Support0xBC2B30–0xBC2B62, calcula config[0x114]×config[0x78]×config[0x3C]+(2^expoente_do_combinador)/4. Na configuração observada,512×1×1+512/4=640. O helper constrói contratos e acumula latências em0x24CD442–0x24CD4DF. Esses endereços distinguem o contrato nativo do adaptador.

## Próxima ação recomendada — menor ensaio ainda útil

Uma única primeira chamada nativa de uma instância nova, B512, entrada identificável e F registrado, com observação de leitura imediatamente antes da primeira FFT/inferência: registrar somente vetores temporais e tensores iniciais de estado cujos ponteiros, formas e propriedades tenham sido previamente validados. Isso testa diretamente as janelas hoje deduzidas e os estados hoje não observados, sem repetir a curva de latência ou ensaios longos. Não foi executado. Não recupera os estados históricos nem explica sozinho o seletor deF. A montagem posterior por índices de amostra permanece uma consequência de implementação, não uma alteração autorizada nesta revisão somente leitura.
