# Validação técnica — VoicEX experimental 0.2.0

Data: 2026-10-22. Candidato: `build/v0.2.0/voicex_v020.py`.
Escopo: processamento offline em Python/ONNX Runtime. **Sem compilação e sem empacotamento.**

## Arquitetura estabelecida

```
entrada 48 kHz
   │
   ├─ STFT 1024 / hop 512 ──────────────────────────────┐
   │     bins   0..107  → filtro DF (6 taps temporais   │
   │                       + vizinhos ±1)               │
   │     bins 108..512  → máscara ERB                   │
   │                       (modelo 256 / hop 128,       │
   │                        mapeada por frequência)     │
   └─ ISTFT 1024 / hop 512 → stream de voz              │
                            stream de ruído = entrada − voz
```

Evidência que sustenta cada escolha:

| Elemento | Evidência |
|---|---|
| Split paralelo por banda (não cascata) | Medição: RMSE 0,0084 (cascata) → **0,00179** (paralelo) |
| Corte em `nb_df = 108` | Config extraída `deepfilternet_df`; platô ótimo medido entre 64 e 108; ≥128 degrada 2,7× |
| Máscara ERB vinda do modelo 256/128 | Contrato ONNX `feat_erb [B,1,S,56]`, `m [B,1,S,56]`; hop 512 = 4 × 128 |
| Filtro DF só em 0..107 | `filter_df` nativo escreve apenas `nb_df` bins; máscara cobre o restante |
| Janela Vorbis | `0xbc7d90`: `sin(π/2 · sin²(π(i+0,5)/n))` |
| Normalização DF | `0xbbf8f0`: estado `α·s + (1−α)·|x|`, saída `x/√s`, α = exp(−hop/sr) |
| Feature ERB em log bruto | Config `normalisation = false`; função `0xbc0100`; ablação |
| Interpolação ERB | `0x2f4b00`: rampa `m[b] + k·(m[b+1]−m[b])/w`, última banda constante |
| Direção dos vizinhos | `0xbbf260`: `bin + t − order` com salto do bin central |
| Taps temporais 0..5 + vizinhos 6,7 | Melhor RMSE medido; ordem invertida degrada 45× em fala |
| Ganhos pós-separação | `(0,0)`, `(20,20)`, `(−50,−50)` reproduzem o original com RMSE **0,0** |

## Testes de invariante — `build/v0.2.0/test_v020.py`

Todos passam:

- silêncio → saída exatamente zero;
- complementaridade `voz + ruído = entrada` (< 1e-12);
- independência entre canais em estéreo (< 1e-9);
- consistência entre blocos `chunk=1` e `chunk=64` (< 1e-6);
- modos de máscara `repeat` e `ramp` finitos e próximos;
- ganhos: 0 dB unitário, +20 dB = ×10, rejeição de 21 dB, −51 dB e NaN;
- entradas inválidas rejeitadas (mono 1-D, vazia, NaN);
- impulso limitado e complementar.

## Precisão medida

### Referência salva (mesma sessão de benchmark)

| Caso | Voz RMSE | Ruído RMSE |
|---|---:|---:|
| `speech_noise` | 0,001844 | 0,001951 |
| `noise` | 0,000463 | 0,000620 |
| `stereo_distinct` | 0,004530 | 0,004998 |
| `impulse` | 0,000109 | 0,000134 |
| `silence` | 0,0 | 0,0 |
| ganhos `unity` / `both20` / `both_low` | 0,0 | 0,0 |

### Execução nova do plugin (não reutiliza referência salva)

| Caso | Voz RMSE | Ruído RMSE |
|---|---:|---:|
| `speech_noise` | 0,002748 | 0,002315 |
| `speech_clean` | 0,001052 | 0,001832 |
| `noise` | 0,000751 | 0,000566 |
| `stereo_distinct` | 0,003230 | 0,003230 |

### Piso de ruído do próprio plugin, mesma fatia de 1 s

| Caso | Repetibilidade do original | Erro do candidato | Razão |
|---|---:|---:|---:|
| `speech_noise` | 0,002082 | 0,002371 | **1,14×** |
| `noise` | 0,000260 | 0,000579 | 2,22× |

Conclusão: em fala+ruído o candidato está a 14% do piso de repetibilidade do plugin; em ruído puro o erro absoluto é 0,00058 sobre RMS 0,025 (2,3%).

## Desempenho

- RTF ≈ **0,54** em CPU, uma thread (≈ 1,9× mais rápido que tempo real) para 2 canais com dois pares de modelos.
- Latência de entrega observada no host: **1664 amostras = 34,7 ms** a 48 kHz, buffers 128 e 512.
- O candidato é **offline**: não há caminho de streaming implementado. O RTF não é latência.

## Distribuição do erro residual

Caso `speech_noise`, energia do erro por banda: 0–100 Hz 14,9%; 100–300 Hz 20,7%; 300–500 Hz 13,0%; 500–800 Hz 13,0%; 800–1200 Hz 6,0%; demais < 8%.

O erro está concentrado na banda do filtro DF (abaixo de ~5 kHz). Acima de 5 kHz a saída do plugin é praticamente nula: 0,01% da energia. O candidato reproduz esse comportamento (máscara ERB ≈ 0 nessa faixa).

## Limitações e incógnitas restantes

1. **Streaming não implementado.** O filtro DF usa 6 quadros de histórico; hoje o padding inicial é zerado por arquivo.
2. **Um único áudio de fala** (14 s) fornecido como material independente; dois trechos não são duas gravações.
3. **Variação do original** entre execuções idênticas não foi explicada; por isso a comparação é feita contra o piso de repetibilidade.
4. **Proveniência exata do runtime** no plugin (ONNX Runtime vs. embutido) segue como achado anterior, não reverificado nesta rodada.
5. **Interpolação ERB no domínio 1024** é uma reconstrução por frequência; `repeat` e `ramp` diferem menos que o piso de ruído.
6. **`mask_mode`** permanece configurável; o padrão é `repeat`.
7. **Ganho em tempo real/automação contínua** não testado (apenas valores estáticos).

## Reprodução

```
py build/v0.2.0/test_v020.py
py build/v0.2.0/benchmark_v020.py
py build/v0.2.0/host_compare_v020.py
py build/v0.2.0/noise_floor_v020.py
```

CLI:

```
py build/v0.2.0/voicex_v020.py entrada.wav saida.wav --voice-db 0 --noise-db -50 \
   --voice-out voz.wav --noise-out ruido.wav --report relatorio.json
```

Artefatos de medição: `analysis/v020_benchmark_*.json`, `analysis/v020_host_*.json`,
`analysis/v020_noise_floor.json`, `analysis/v020_residual.json`,
`analysis/split_sweep_v020_*.json`, `analysis/align_v020_*.json`, `analysis/v020_tests.json`.
