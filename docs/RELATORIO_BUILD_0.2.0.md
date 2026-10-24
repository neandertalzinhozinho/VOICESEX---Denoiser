# Relatório da build 0.2.0 — VoicEX experimental

**Versão: `0.2.0`** (candidato de pesquisa, não é clone certificado nem binário compilado)

## O que esta versão é

Pipeline offline em Python que reproduz a separação voz/ruído do CEDAR VoicEX 2 usando os quatro modelos ONNX extraídos do plugin, com a arquitetura reconstruída por análise estática e medição.

Não houve compilação nem empacotamento nesta etapa, conforme instrução. "Build 0.2.0" designa o conjunto versionado de código e resultados.

## Como rodar

```
py build/v0.2.0/voicex_v020.py entrada.wav saida.wav --voice-db 0 --noise-db -50
```

Saídas opcionais: `--voice-out`, `--noise-out`, `--report`.
Entrada: WAV PCM 48 kHz. Ganhos: −50 a +20 dB.

## Resultado principal

| Medida | Valor |
|---|---|
| Correlação voz (fala+ruído) | **0,99964** |
| RMSE voz (fala+ruído) | **0,00184** |
| Piso de repetibilidade do plugin | 0,00208 |
| Razão candidato/piso | **1,14×** |
| Ganhos iguais (0/0, 20/20, −50/−50) | RMSE **0,0** |
| Silêncio | erro **0,0** |
| RTF (CPU, 1 thread) | 0,54 |

## Descobertas que definiram a versão

1. **A separação é paralela por frequência**, não em cascata:
   bins 0–107 pelo filtro DF; bins ≥108 pela máscara ERB. A cascata errava 4,5× mais.
2. **A voz é limitada a ~5 kHz.** Acima disso o plugin entrega 0,01% da energia; a máscara ERB exportada já reflete isso.
3. **Ganhos são aplicados depois da separação**, sobre streams complementares — por isso pares de ganho iguais reproduzem a entrada exatamente.
4. **Janela Vorbis** e normalização DF confirmadas no código nativo.

## Erros corrigidos em relação à 0.1.x

| Versão | Correção | Efeito |
|---|---|---|
| 0.1.1 | Remoção da normalização ERB indevida | voz deixou de ser atenuada a 0,0004 |
| 0.1.2 | Direção dos vizinhos espectrais | correção pontual |
| 0.1.3 | Interpolação ERB em rampa | melhora marginal |
| 0.2.0 | Split paralelo DF/ERB | RMSE 0,0084 → 0,0018 |

## Limitações desta build

- Offline apenas; sem streaming, sem VST3, sem tempo real.
- Um único arquivo de fala como material de validação.
- Variação do próprio plugin entre execuções não explicada; as comparações usam o piso de repetibilidade.
- Não há prova de equivalência bit a bit; há proximidade medida no limite da repetibilidade do original.

## Arquivos

- Código: `build/v0.2.0/voicex_v020.py`
- Testes: `build/v0.2.0/test_v020.py` (todos passam)
- Benchmarks: `benchmark_v020.py`, `host_compare_v020.py`, `noise_floor_v020.py`
- Documentação: `docs/VALIDACAO_V020.md`
- Evidência: `analysis/v020_*.json`, `analysis/split_sweep_v020_*.json`
