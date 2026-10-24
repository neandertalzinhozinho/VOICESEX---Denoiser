# Trilha B — transição dinâmica DF/ERB, 2026-10-23

## Evidência que motivou a alteração

O candidato v0.2.1 tinha diferença concentrada de ~3 a 5 kHz nos segundos
4–5 de `speech48.wav`. Uma rampa fixa reduziu essa diferença, mas regrediu
fortemente em duas falas independentes (`docs/CORPUS_INDEPENDENTE_20261023.md`).

O modelo ERB produz máscara dependente do conteúdo. Ponderada pela energia
da entrada no bin DF 107 (~5 kHz), a máscara foi **0,170 e 0,077** nos
segundos 4–5 de `speech48.wav`, mas ficou entre **0,908 e 0,948** nos mesmos
segundos das falas independentes. Resultado bruto:
`analysis/v021_erb_boundary_weighted_20261023T034928382556Z.json`.
Esses valores acompanham o ganho espectral que aproximou o candidato do
CEDAR; não provam a operação exata do original.

## Experimento e implementação

`build/v0.2.2/dynamic_mask_probe_v022.py` testou a máscara ERB sobre a
saída DF em cinco formas. Aplicá-la à banda DF inteira piorou todos os
casos. Uma transição gradual de 2 a 5,2 kHz, que vai de ganho 1 ao valor
da máscara, melhorou a média dos três sinais. Resultado:
`analysis/v022_dynamic_mask_probe_20261023T035158587808Z.json`.

`build/v0.2.3/voicex_v023.py` implementa essa transição **dentro do
processamento espectral**, com as mesmas inferências DF/ERB e streams
complementares. A versão é experimental; o mecanismo ainda não foi confirmado
por rastreamento nativo.

## Validação contra renders preservados do CEDAR

`py -3.10 build/v0.2.3/validate_v023.py` comparou v0.2.1 e v0.2.3 com oito
renders de `speech48.wav` e três renders preservados de cada fala independente.
Resultado bruto: `analysis/v023_validation_20261023T035310886363Z.json`.

| Sinal | v0.2.1: segundos acima do piso | v0.2.3 | RMSE médio por segundo v0.2.1 | v0.2.3 |
|---|---:|---:|---:|---:|
| `speech48.wav` (14 s) | 3 | **0** | 0,001721 | **0,000541** |
| Fala feminina (21 s) | 7 | **6** | 0,000160 | **0,000146** |
| Fala masculina (17 s) | 8 | **6** | 0,000326 | **0,000288** |

Invariantes: Voice+Noise reproduz a entrada (erro máximo estéreo
8,67e-19), canais estéreo iguais geram saídas iguais, e a diferença máxima
entre chunks 1 e 64 foi 5,38e-8 no caso curto. Todas as saídas avaliadas
foram finitas.

## Pendências e limites

A v0.2.3 **ainda não atinge equivalência completa**. Na fala feminina, os
segundos 11, 12, 13, 14, 16 e 18 excedem o piso; o segundo 13 chega a
2,51× (RMSE 0,000215 contra piso 0,000086). Na fala masculina, os segundos
0, 1, 2, 5, 9 e 13 excedem; o pior é o segundo 1, 1,65× (0,000510 contra
0,000309). Os pisos das falas independentes usam três renders, portanto
devem ser refinados com mais repetições.

Próximas ações: separar por banda os erros restantes, testar ruídos distintos,
confirmar a regra de fusão no código nativo e só então integrar ao VST3.

## Diagnóstico posterior por banda e interpolação

`analysis/v023_residual_bands_20261023T035414224067Z.json` separou os
resíduos das falas independentes em 0–2, 2–5 e 5–24 kHz. Nos piores segundos,
a banda 0–2 kHz ficou abaixo do piso (razões ~0,46–0,75), enquanto 2–5 kHz
atingiu 3,74× e 5–24 kHz atingiu 6,26×. As razões são altas porque o piso
por banda é pequeno; usar também RMSE absoluto ao priorizar correções.

`analysis/v023_erb_interpolation_20261023T035500695556Z.json` comparou
`repeat` e `ramp` em todos os três sinais. `ramp` reduziu levemente o RMSE
médio em todos; manteve 0 e 6 segundos acima do piso na fala inicial e
feminina, e reduziu de 6 para 5 na masculina. A desassemblagem anterior
(`analysis/erb_interp_v024.txt`, rotina `0x2f4b00`) mostra interpolação em
rampa, então `ramp` é a escolha técnica mais coerente para a próxima versão.

## Testes de grade e sincronização ERB

`analysis/v023_erb_grid_probe_20261023T035708538670Z.json` comparou rampa
direta na grade 1024, rampa na grade 256 seguida de repetição por quatro,
interpolação linear e centros de banda. As diferenças de RMSE foram mínimas;
nenhuma forma removeu os segundos restantes. A rampa direta manteve o menor
RMSE médio na fala inicial e a menor contagem na fala masculina (5).

`analysis/v023_erb_timing_probe_20261023T035903322084Z.json` deslocou a
máscara em -8 a +8 quadros ERB (128 amostras cada) sem repetir inferência.
Zero quadro foi o melhor compromisso nas três falas; +1 melhorou a média da
fala inicial em ~2%, mas não a das outras. Não há evidência para deslocar a
máscara na implementação.

## Dois ruídos adicionais

`build/v0.2.3/noise_corpus_v023.py` gerou sinais reproduzíveis de 5 s em
`analysis/noise_20261023/`: ruído branco e ruído rosa, ambos com RMS alvo
0,025, semente 20261023 e 48 kHz. Foram testados puros e misturados com
fala feminina independente, com três renders novos do CEDAR por caso.
Resultado bruto: `analysis/v023_noise_corpus_20261023T040153318883Z.json`.

| Caso | RMSE médio v0.2.1 | v0.2.3 com rampa | segundos acima do piso v0.2.1 → v0.2.3 |
|---|---:|---:|---:|
| Branco puro | 0,000339 | **0,000139** | 5 → 5 |
| Branco + fala | 0,000807 | **0,000702** | 4 → 1 |
| Rosa puro | 0,000330 | **0,000288** | 2 → 0 |
| Rosa + fala | 0,000646 | **0,000605** | 0 → 0 |

**Observação:** a fusão dinâmica com rampa reduziu o RMSE nos quatro casos.
O ruído branco puro segue fora do piso nos cinco segundos, apesar da queda
de 59% no RMSE médio. Os ruídos são sintetizados, não gravações ambientais;
faltam ruídos reais adicionais para aferir generalização.

### Reavaliação do ruído branco com oito renders

O diagnóstico por banda (`analysis/v023_white_bands_20261023T040402123873Z.json`)
mostrou que o piso estimado com três renders pode variar bastante entre
baterias: numa delas, acima de 5,2 kHz, o piso foi próximo de 3–4e-7; noutra,
chegou a 3–6e-5. A repetição do mesmo sinal no CEDAR produziu modos distintos.
Isso torna prematura a conclusão de falha sistemática no ruído branco.

`build/v0.2.3/white_variation_v023.py` preservou oito saídas do CEDAR e a
saída candidata em `analysis/v023_white_eight_refs_20261023T040607169535Z.npz`.
O resultado por segundo está em
`analysis/v023_white_variation_20261023T040607169535Z.json`:

| Segundo | RMSE mínimo do candidato | Maior variação entre renders CEDAR | Razão |
|---:|---:|---:|---:|
| 0 | 0,000344 | 0,000429 | 0,80× |
| 1 | 0,0000206 | 0,0000269 | 0,76× |
| 2 | 0,0000261 | 0,0000350 | 0,75× |
| 3 | 0,0000362 | 0,0000680 | 0,53× |
| 4 | 0,0000422 | 0,0000570 | 0,74× |

**Observação:** os cinco segundos ficam dentro do piso calculado com oito
renders. O resultado anterior de 5/5 segundos fora do piso permanece válido
para aquela amostra de três renders, mas não é robusto à ampliação da
referência. **Inferência:** a variabilidade multimodal do original exige mais
renders também nas duas falas independentes antes de atribuir seus resíduos
ao algoritmo candidato. Não usar apenas a média de RMSE como critério.

### Oito renders nas falas independentes

`build/v0.2.3/speech_variation_v023.py` ampliou de três para oito renders cada
fala; preservou as saídas e o candidato com máscara em rampa em
`analysis/v023_speech_eight_refs_20261023T040902127411Z.npz`. Resultado por
segundo: `analysis/v023_speech_variation_20261023T040902127411Z.json`.

| Sinal | Segundos fora do piso com 3 renders | Com 8 renders | Pior razão com 8 |
|---|---:|---:|---:|
| Fala feminina | 6 | **6** | 2,51× (segundo 13) |
| Fala masculina | 5 | **5** | 1,65× (segundo 1) |

**Observação:** ampliar a referência não eliminou os resíduos das falas. Os
segundos femininos 11, 12, 13, 14, 16, 18 e masculinos 1, 2, 5, 9, 13
persistem. A banda por segundo em
`analysis/v023_speech_residual_bands_eight_20261023T040925108546Z.json`
localiza o excesso sobretudo em 3,5–5,2 kHz (até 7,27× o piso) e 5,2–10 kHz
(até 5,89×). Nos mesmos segundos, as bandas abaixo de 2 kHz ficaram dentro
do piso. **Inferência:** a próxima investigação deve concentrar-se na
transição DF/ERB e no ganho ERB alto; a regra de fusão v0.2.3 ainda não foi
confirmada diretamente no código nativo.

Uma sonda diagnóstica (`build/v0.2.3/highband_tilt_probe_v023.py`) atenuou
depois da saída, em 1–6%, a faixa aproximada 3,5–10 kHz. O resultado bruto é
`analysis/v023_highband_tilt_probe_20261023T041022257137Z.json`. Com 2–3%
de atenuação, a fala feminina caiu de seis para um segundo acima do piso;
a masculina permaneceu em cinco, embora o RMSE médio tenha caído de 0,0002865
para 0,0002708 em 2%. **Inferência limitada:** há excesso médio de energia
nessa faixa, mas um ganho fixo não explica todos os resíduos. Esta sonda não
é uma correção proposta para o pipeline: foi aplicada ao WAV final e ainda
não foi confrontada com todos os casos, streaming ou regra nativa.

### Sonda de expoente da máscara ERB

`build/v0.2.3/erb_exponent_probe_v023.py` executou uma única inferência dos
modelos por sinal e reaplicou a máscara ERB em rampa elevada a expoentes
1,00–1,20 antes da fusão DF/ERB. Resultado bruto:
`analysis/v023_erb_exponent_probe_20261023T041227819377Z.json`.

| Sinal | Expoente 1,00: segundos acima / RMSE médio | Expoente 1,20 |
|---|---:|---:|
| `speech48.wav` | 0 / 0,0005185 | 0 / 0,0004719 |
| Fala feminina | 6 / 0,0001453 | 3 / 0,0001396 |
| Fala masculina | 5 / 0,0002865 | 5 / 0,0002773 |
| Ruído branco | 0 / 0,0000938 | 0 / 0,0000940 |

**Observação:** o expoente 1,20 melhora as três falas na média e reduz a
contagem de falhas da fala feminina, mas não resolve os cinco segundos da
masculina. O ruído branco permanece próximo do mesmo erro. **Inferência:**
uma transformação dependente da máscara é mais plausível que atenuação fixa
do WAV final, mas o expoente não foi demonstrado no código nativo e não foi
testado ainda nos dois outros casos de ruído. Não promover ao produto.

`build/v0.2.3/speech_phase_gain_probe_v023.py` separou ganho e deslocamento
temporal nas duas bandas altas dos onze segundos persistentes, confrontando
os oito renders por fala. Resultado:
`analysis/v023_speech_phase_gain_probe_20261023T041109739778Z.json`.
Em todos os 22 pares segundo/banda, o melhor deslocamento entre -4 e +4
amostras foi **zero**. O ganho escalar ótimo para o candidato variou de
0,929 a 0,985 e reduziu o RMSE da banda em 1,04–1,80×. **Observação:** parte
do desvio é de amplitude, mas não desaparece com um único escalar. Isso
reforça a prioridade de inspecionar a transformação da máscara e sua aplicação
por frequência/quadro, em vez de alterar a latência global.

**Revisão do expoente com ruídos:** `build/v0.2.3/erb_exponent_noise_probe_v023.py`
testou a mesma hipótese com três novos renders do CEDAR para ruído branco +
fala, rosa + fala e rosa puro. Referências preservadas em
`analysis/v023_erb_exponent_noise_refs_20261023T041502242428Z.npz` e
resultado em `analysis/v023_erb_exponent_noise_probe_20261023T041502242428Z.json`.
Do expoente 1,00 ao 1,20, o RMSE médio subiu de 0,0007024 para 0,0007264
em branco + fala e de 0,0006227 para 0,0006478 em rosa + fala. No rosa puro,
caiu de 0,0002475 para 0,0002375. **Decisão:** rejeitar o expoente global
1,20 para promoção; ele melhora as falas isoladas, mas regride nas duas
misturas. A transformação nativa exata segue desconhecida.

### Ganho de −50 dB conferido no original

Uma hipótese alternativa era uma diferença na conversão do parâmetro Noise
de decibéis para ganho linear. `build/v0.2.3/gain_complement_probe_v023.py`
configurou **Voice e Noise ambos em −50 dB** sobre ruído branco. Como as
streams são complementares, a saída esperada é a entrada multiplicada por
`10^(-50/20) = 0,0031622776601683794`, independente da separação. Em três
renders, o ganho ajustado diferiu do ideal por menos de 6e-13 e o RMSE da
saída contra a entrada escalada ficou em ~3,46e-12. Resultado bruto:
`analysis/v023_gain_complement_probe_20261023T203235099126Z.json`.
**Observação:** o mapeamento de −50 dB do CEDAR é o mesmo da fórmula usada no
candidato, nessa configuração. Portanto, não explica o excesso de amplitude
nas bandas altas; o desvio está na separação ou na síntese das streams.

### Fórmula de fusão DF/ERB testada

`build/v0.2.3/df_erb_fusion_probe_v023.py` comparou a fórmula atual,
`DF · (1−b+b·M)`, com uma transição entre o filtro DF e o espectro original
mascarado, `(1−b)·DF+b·(STFT·M)`, onde `b` vai de zero a um entre 2 e 5,2 kHz.
A inferência ONNX foi compartilhada dentro de cada sinal. Dados:
`analysis/v023_df_erb_fusion_probe_20261023T203333955113Z.json`.

| Caso | Multiplicação: segundos acima / RMSE médio | Transição |
|---|---:|---:|
| `speech48.wav` | 0 / 0,0005185 | 0 / 0,0005167 |
| Fala feminina | 6 / 0,0001453 | 5 / 0,0001430 |
| Fala masculina | 5 / 0,0002865 | 5 / 0,0002801 |
| Ruído branco | 0 / 0,0000938 | 0 / 0,0000938 |

Deslocar o começo da transição para 3 ou 3,5 kHz piorou todos os casos de
fala e o ruído branco. Com três novos renders por caso de ruído
(`analysis/v023_fusion_noise_probe_20261023T203521034683Z.json`), a
transição reduziu o RMSE médio de branco + fala de 0,0002896 para 0,0002604
e de rosa + fala de 0,0006227 para 0,0006111, sem alterar suas contagens.
No rosa puro, porém, **aumentou** o RMSE de 0,0002696 para 0,0002825 (~4,8%).
As referências foram preservadas em
`analysis/v023_fusion_noise_refs_20261023T203521034683Z.npz`.

**Decisão:** não promover a transição como correção geral ainda. Ela reduz
erros de fala, mas a regressão no rosa puro exige investigar a operação
nativa e a dependência do conteúdo. A forma exata da fusão segue incerta.
