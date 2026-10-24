# Validação ONNX — ponto de retomada atualizado

Evidência: `analysis/onnx_validation_20261022T024117900311Z.json`.
Reprodução: `py analysis/validate_onnx_contracts.py`.
Ambiente: ONNX 1.23.2, ONNX Runtime 1.23.2, NumPy 2.2.6; CPU, uma thread.

## Checklist comprovada nesta execução

- [x] Quatro modelos aprovados por `onnx.checker` com `full_check=True`.
- [x] Quatro modelos carregados e executados no ONNX Runtime CPU.
- [x] Inferência sintética para B=1 e S=1, 3, 6, 16: 16 execuções sem erro.
- [x] Inventário de initializers com nomes, tipos e dimensões registrado no JSON.
- [x] Nenhum initializer externo; nenhum valor não finito encontrado nos initializers.
- [x] SHA-256 dos quatro arquivos registrado no JSON.
- [x] Entradas e saídas observadas registradas para cada execução.
- [ ] Executar pares encoder/decoder conectados com realimentação dos estados.
- [ ] Verificar histórico convolucional e equivalência entre blocos.
- [ ] Reconstruir pré/pós-processamento e comparar áudio contra o original.

## Contratos observados

Dimensões abaixo para B=1 e S=1; todos os inputs são float32. Os papéis DF/ERB são as designações de trabalho da análise anterior.

| Modelo | Initializers | Entradas | Saídas |
|---|---:|---|---|
| 0x02BACC00 — DF encoder | 124 | feat_spec [1,2,1,108]; h [1,1,256] | emb [1,1,512]; c0 [1,64,1,108]; lsnr [1,1,1]; history [1,1,256] |
| 0x02D060E0 — DF decoder | 171 | emb [1,1,512]; c0 [1,64,1,108]; h [2,1,256] | coefs [1,1,108,16]; history [2,1,256] |
| 0x02F48EA0 — ERB encoder | 176 | feat_erb [1,1,1,56]; h [1,1,96] | e0 [1,64,1,56]; e1 [1,64,1,28]; e2/e3 [1,64,1,14]; emb [1,1,896]; lsnr [1,1,1]; history [1,1,96] |
| 0x02F8FF00 — ERB decoder | 171 | emb [1,1,896]; e3/e2 [1,64,1,14]; e1 [1,64,1,28]; e0 [1,64,1,56]; h [2,1,96] | m [1,1,1,56]; history [2,1,96] |

## Limites e próxima ação

As execuções usam entradas sintéticas independentes e estados inicialmente zerados. Não comprovam qualidade de áudio, equivalência ao plugin, comportamento contínuo ou latência. A execução em ONNX Runtime não identifica o runtime utilizado pelo plugin original. Initializers incluem constantes do grafo, não apenas pesos treináveis.

Próximo passo: conectar cada encoder ao decoder correspondente, testar realimentação de `history` em `h` e investigar contexto temporal das convoluções. Compatibilidade de shapes não basta para garantir equivalência do streaming.

Este registro complementa `PLANO_DESENVOLVIMENTO.md` sem alterar o plano preservado.
