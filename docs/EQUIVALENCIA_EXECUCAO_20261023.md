# Trilha B — execução autônoma de 2026-10-23

## Observação reproduzível

Com `py -3.10 build/v0.2.1/noise_floor_v021.py`, o CEDAR instalado foi
carregado pelo Pedalboard e processou dois sinais de 1 s a 48 kHz, com 1 s
de preroll, buffer de 512 amostras e ganho Voice 0 dB / Noise -50 dB.
Resultado bruto: `analysis/v021_noise_floor_20261023T031819262537Z.json`.

| sinal | RMSE candidato × CEDAR | RMSE CEDAR × CEDAR | razão | correlação |
|---|---:|---:|---:|---:|
| fala + ruído | 0,0027387 | 0,0027895 | 0,982 | 0,999154 |
| ruído puro | 0,0004073 | 0,0004980 | 0,818 | 0,806575 |

Esses números são observações restritas a dois sinais, um segundo cada.
A razão abaixo de 1 significa que o erro do candidato ficou menor que a
diferença entre estes dois renders do original. Não demonstra identidade
funcional ou perceptiva, principalmente no ruído puro, cuja correlação é baixa.

## Próximo experimento

Executar comparação por segundo da fala limpa inteira com múltiplos renders
do CEDAR sob configuração idêntica. Para cada segundo, registrar distribuição
do piso entre pares de renders, diferença do candidato e bandas DF/ERB.
Priorizar os segundos 3–5, onde o erro anterior excedeu o piso. Depois repetir
com sinais independentes; o corpus atual ainda depende de `speech48.wav`.

## Comparação por segundo executada

O script novo `build/v0.2.1/segment_floor_v021.py` fez três renders completos
do original e um do candidato, com configuração idêntica. Resultado:
`analysis/v021_segment_floor_20261023T032114887582Z.json`. Registra hashes
da entrada e do binário, latência, todos os pares de piso e todos os erros
do candidato, permitindo auditoria da seleção.

**Observação:** os dois primeiros renders do CEDAR foram bit-idênticos; o
terceiro diferiu. Mesmo usando o maior piso observado e o menor erro do
candidato (critério favorável ao candidato), os segundos **1, 3, 4, 5, 7 e 8**
excederam o piso. Nos segundos 3–5, os menores erros do candidato foram
0,00557, 0,00530 e 0,00519, contra pisos máximos de 0,00189, 0,00177 e
0,00151, respectivamente.

**Inferência:** a diferença 3–5 persiste além da variação observada do
original nesta execução; a equivalência completa continua refutada nesse
material. A alternância entre dois renders idênticos e um diferente sugere
que o piso precisa ser caracterizado com mais repetições e histórico de
carregamento antes de receber interpretação probabilística.

**Próxima ação:** instrumentar a diferença espectral nos segundos 3–5 e
testar as hipóteses de pré-ênfase e estado inicial da GRU com a mesma bateria.

## Revisão com oito renders do original

`build/v0.2.1/reference_modes_v021.py` produziu oito renders completos,
preservados em `analysis/v021_reference_modes_20261023T032340965375Z.npz`
com hashes e matriz de RMSE no JSON de mesmo nome. Foram **cinco saídas
bit a bit distintas**: os renders 2=4, 3=5 e 1=7 se repetiram exatamente.
O RMSE entre pares variou de 0 a 0,002303 no arquivo inteiro.

`build/v0.2.1/reference_envelope_v021.py` comparou o candidato com essas oito
referências por segundo e por banda. Resultado:
`analysis/v021_reference_envelope_20261023T032419105331Z.json`.
Usando o menor erro do candidato e o maior piso entre pares em cada segundo,
**4, 5 e 7** ainda excedem o piso, por fatores **2,49×, 3,16× e 1,17×**.
Nos segundos 4 e 5, o erro DF é 0,00526 e 0,00517, enquanto o erro ERB é
0,00066 e 0,00045 na referência mais próxima. A prioridade técnica é DF.

**Correção da interpretação anterior:** os segundos 1, 3 e 8 ultrapassaram
o piso calculado com três renders, mas não com os oito. O número de modos do
CEDAR torna insuficiente inferir uma distribuição completa a partir de três
execuções. Cinco modos observados não prova que só existam cinco.

**Incerteza:** a razão da alternância entre saídas do original ainda é
desconhecida. Pode envolver estado interno ou outro fator do host; os hashes
mostram repetição exata, mas não identificam a causa.

## Hipótese de pré-ênfase no DF testada

`build/v0.2.1/preemphasis_probe_v021.py` aplicou o filtro de primeira ordem
`x[n] - alpha*x[n-1]` **somente às features DF**, mantendo a STFT original
para a filtragem e a reconstrução. Foram testados alpha=0 (controle), 0,90,
0,93, 0,95 e 0,97 contra os oito renders salvos do CEDAR. Resultado:
`analysis/v021_preemphasis_probe_20261023T032527029506Z.json`.

| alpha | RMSE médio dos segundos 4–5 | média dos RMSE mínimos por segundo |
|---:|---:|---:|
| 0 | 0,005245 | 0,001721 |
| 0,90 | 0,005828 | 0,003273 |
| 0,93 | 0,005747 | 0,003412 |
| 0,95 | 0,005730 | 0,003547 |
| 0,97 | 0,005757 | 0,003786 |

**Observação:** todas as variantes testadas pioraram os trechos 4–5 e a
comparação geral. Esta forma específica de pré-ênfase nas features DF não
explica o resíduo. Isso não elimina outros filtros ou posições no pipeline.
Próxima hipótese: estado inicial/warm-up da GRU e ordem do ring nativo.

## Hipótese de warm-up curto testada

`build/v0.2.1/warmup_probe_v021.py` reiniciou estados de rede e DSP em
quatro pontos da fala, todos alinhados ao hop da STFT, mantendo 1 s de zeros
antes de cada trecho. Comparou os mesmos segundos absolutos 4 e 5 com os
oito renders salvos do CEDAR. Resultado:
`analysis/v021_warmup_probe_20261023T032618586173Z.json`.

| início da fala processada | RMSE médio mínimo, segundos 4–5 |
|---:|---:|
| 0 s (controle) | 0,005245 |
| 1,024 s | 0,005283 |
| 2,048 s | 0,006611 |
| 3,072 s | 0,006859 |

**Observação:** encurtar o histórico não melhorou os trechos críticos; as
reinicializações mais próximas deles pioraram o resultado. Este teste não
equivale a medir diretamente o estado inicial da GRU no CEDAR. Ele descarta
apenas a explicação simples de que menos warm-up no candidato resolveria o
resíduo. Próximo passo: examinar a ordem do ring nativo e/ou estados internos
com evidência de execução ou desassemblagem focada.

## Ordem do ring DF examinada no dump

`analysis/dis_ring_write_v026.py` gerou `analysis/ring_write_v026.txt` a
partir do dump preservado de `VoicExSupport`. Em `0x2f32d0`, a gravação usa
o índice atual do ring (`[r14]`) para o destino e, depois da cópia, calcula
`(indice + 1) % capacidade` em `0x2f34c8–0x2f34d4`. Em `0xbbf350`, a leitura
dos taps começa em `([rcx] + i) % capacidade` (`0xbbf3b0–0xbbf3c9`). O caller
`0xbbf5e0` chama a gravação em `0xbbf617` antes da aplicação em `0xbbf748`.

**Inferência:** se a capacidade do ring é a ordem temporal 6 indicada pela
configuração, a leitura após a gravação percorre do quadro mais antigo até o
mais recente. Isso é coerente com a ordem dos seis taps do candidato. A
desassemblagem ainda não prova todos os detalhes de alinhamento e inicialização
do ring nem o conteúdo exato da estrutura em tempo de execução. Não há, nesta
evidência, motivo para inverter os taps no candidato.

## Variação dentro da mesma instância e agenda dos coeficientes

`build/v0.2.1/reference_lifecycle_v021.py` processou a fala inteira três
vezes em cada uma de duas instâncias do CEDAR, sempre com `reset=True` e a
mesma entrada. Resultado e formas de onda:
`analysis/v021_reference_lifecycle_20261023T032834199535Z.{json,npz}`.
Os três renders de cada instância tiveram hashes diferentes. Logo, recriar
o plugin não é condição necessária para a variação. Em um par, a primeira
diferença acima de 1e-10 surge na amostra 1666 da fala, próxima da latência
declarada de 1664 amostras. A divergência aparece desde o início do sinal
processado, não somente após acúmulo prolongado de histórico. O experimento
antigo `analysis/lifecycle_v013_20261022T030951205810Z/report.json` também
registrara variação com `reset()` explícito.

`build/v0.2.1/df_schedule_probe_v021.py` deslocou os coeficientes DF em
-2, -1, +1 e +2 quadros em relação ao espectro. Resultado:
`analysis/v021_df_schedule_probe_20261023T032955696918Z.json`.

| deslocamento em quadros | RMSE médio mínimo, segundos 4–5 | média por segundo |
|---:|---:|---:|
| -2 | 0,006383 | 0,003520 |
| -1 | 0,005583 | 0,002480 |
| 0 | **0,005245** | **0,001721** |
| +1 | 0,006039 | 0,002812 |
| +2 | 0,009135 | 0,005166 |

**Observação:** nenhum deslocamento inteiro testado melhora o resíduo.
**Incerteza:** ainda pode haver diferença de estados recorrentes,
pré-processamento ou agendamento subquadro que esse sweep não mede.

## Estado inicial uniforme da GRU DF testado

`build/v0.2.1/df_hidden_init_probe_v021.py` preencheu os estados iniciais
do encoder e decoder DF com constantes 0, ±0,01 e ±0,1. Resultado:
`analysis/v021_df_hidden_init_probe_20261023T033049035636Z.json`.

| valor inicial | RMSE médio mínimo, segundos 4–5 | média por segundo |
|---:|---:|---:|
| 0 | **0,005245** | **0,001721** |
| +0,01 | 0,005264 | 0,001814 |
| -0,01 | 0,005272 | 0,001813 |
| +0,1 | 0,005419 | 0,002289 |
| -0,1 | 0,006172 | 0,003081 |

**Observação:** valores uniformes não nulos não melhoraram os trechos 4–5.
Isso não descarta um estado inicial estruturado ou persistente específico do
CEDAR; exige evidência interna para testar essa hipótese de forma dirigida.

## Entrega de blocos em tempo real testada

`build/v0.2.1/pacing_probe_v021.py` enviou a fala inteira ao CEDAR em blocos
de 512 amostras, uma vez sem pausa e duas vezes no ritmo de 48 kHz. Resultado:
`analysis/v021_pacing_probe_20261023T033830863310Z.{json,npz}`.
O render rápido e o primeiro ritmado foram bit-idênticos (RMSE 0); o segundo
ritmado produziu outro modo já visto nos renders anteriores (RMSE 0,001897
entre modos). Os erros médios dos segundos 4–5 permaneceram 0,005516 e
0,005245. **Observação:** a cadência de entrega, por si só, não eliminou a
variação nem o resíduo DF nesta execução. A hipótese de agendamento assíncrono
como causa única fica enfraquecida, sem estar descartada para outros hosts.

## Análise nativa automatizada

O dump `VoicExSupport_rva_aligned.bin` foi aberto no REA/Ghidra (SHA-256
`d8beb024d230e910bb20ecc34ec95f91a3e6b986af8182835481d5872a1af2f3`).
A importação e autoanálise não terminaram dentro do limite de inicialização
de 330 s do provedor (`provider_timeout`); portanto não foi obtido
pseudocódigo. Repetir a mesma chamada sem ajustar perfil/tempo não acrescenta
evidência. A desassemblagem local focada continua disponível para perguntas
pontuais, e o próximo uso de Ghidra exige perfil de análise mais restrito ou
um limite de inicialização maior.

## Descoberta de diferença espectral na borda DF

Um sweep de atrasos fracionários de -2 a +2 amostras (`analysis/v021_fractional_delay_probe_20261023T034001062409Z.json`)
encontrou mínimo em **0 amostra** nos segundos 4, 5, 6 e 7. Um deslocamento
global fracionário não explica o resíduo.

Uma transferência complexa estimada no segundo 4 e aplicada ao segundo 5
reduziu o RMSE mínimo deste último de 0,00519 para 0,00259; treinada no
segundo 5, reduziu o segundo 4 para 0,00183. Artefato:
`analysis/v021_df_transfer_probe_20261023T034037709453Z.json`. A inspeção
por bin (`analysis/v021_df_transfer_inspect_20261023T034103822099Z.json`)
mostrou ganho próximo de 1 até ~2 kHz e queda progressiva acima de ~3 kHz,
chegando a 0,12–0,23 nos bins 105–107 (~4,9–5,0 kHz). A fase estimada nesses
bins é pequena. **Inferência:** há uma diferença sistemática de magnitude perto
do limite DF/ERB. A operação nativa exata ainda não está provada.

Um sweep de rampas simples (`analysis/v021_df_taper_probe_20261023T034150564922Z.json`)
reduziu o número de segundos acima do piso de 3 para 2 na melhor configuração
(ganho 1 em 2 kHz caindo linearmente para 0,4 em 5,2 kHz). Os segundos 5 e 7
continuaram acima, por 1,54× e 1,25×. Em sinais adicionais de 1 s, essa
rampa manteve fala+ruído quase igual (RMSE 0,002328→0,002325) e reduziu
o erro em ruído puro de 0,000567 para 0,000417, ambos comparados com renders
novos do CEDAR (`analysis/v021_df_taper_external_20261023T034316222603Z.json`).

## Candidato experimental v0.2.2

`build/v0.2.2/voicex_v022.py` aplica a rampa na stream Voice e recompõe
Noise como entrada menos Voice. `build/v0.2.2/validate_v022.py` confirmou
complementaridade (erro máximo 6,94e-18), canais estéreo idênticos, saída
finita e diferença máxima 9,38e-8 entre chunks 1 e 64 no caso curto.
Comparação da fala inteira com os oito renders: segundos 5 e 7 ainda acima
do piso, por 1,549× e 1,240×. Resultado:
`analysis/v022_validation_20261023T034403256139Z.json`.

**Status:** melhoria experimental reproduzida, ainda insuficiente para
equivalência completa. A rampa é uma hipótese de reconstrução, não uma
operação comprovada no CEDAR. Falta validar com mais falas independentes e
descobrir a regra nativa que produz a transição DF/ERB.

**Revisão após validação independente:** duas falas reais de vozes distintas
em 48 kHz foram adquiridas e comparadas. A v0.2.2 passou de 5 para 16 segundos
acima do piso na fala feminina e de 7 para 17 na masculina; o RMSE médio por
segundo subiu ~2,9× e ~3,4×. Portanto, a rampa fixa está **rejeitada para
promoção**. Ver `docs/CORPUS_INDEPENDENTE_20261023.md` e
`analysis/v022_independent_compare_20261023T034756480637Z.json`. A melhora
nos segundos 4–5 do arquivo inicial continua sendo evidência de uma diferença
local, não de uma correção geral.

## Limitações do ambiente

O comando `python` resolve para o alias da Microsoft Store; `py -3.10` é o
interpretador funcional usado nesta execução. A pasta não contém `.git`, logo
os artefatos novos foram preservados por nome versionado, sem commit.
