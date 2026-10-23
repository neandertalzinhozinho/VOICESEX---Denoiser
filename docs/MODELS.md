# Contrato de integração

Forneça quatro arquivos ONNX: `df_enc.onnx`, `df_dec.onnx`, `erb_enc.onnx`
e `erb_dec.onnx`. Os nomes dos tensores são parte da interface deste motor.
Modelos arbitrários de redução de ruído não são intercambiáveis.

| Grafo | Entradas principais | Estado recorrente | Contexto convolucional |
|---|---|---|---|
| df_enc | feat_spec [1,2,T,108] | h [1,1,256] | conv_history [1,2,2,108] |
| df_dec | emb [1,T,512], c0 [1,64,T,108] | h [2,1,256] | conv_history [1,64,5,108] |
| erb_enc | feat_erb [1,1,T,56] | h [1,1,96] | conv_history [1,1,2,56] |
| erb_dec | emb [1,T,896], e0 [1,64,T,56], e1 [1,64,T,28], e2/e3 [1,64,T,14] | h [2,1,96] | nenhum |

Todos os grafos recebem `h` e devolvem `history` para o próximo bloco.
O encoder DF entrega `emb` e `c0`; o decoder DF entrega `coefs` [1,T,108,16].
O encoder ERB entrega `emb` e `e0` a `e3`; o decoder ERB entrega `m` [1,1,T,56].
Entradas e saídas numéricas são float32. As saídas devem preservar todos os
quadros de T, sem truncamento para somente o último quadro.

O motor usa DF com FFT 1024/hop 512 e ERB com FFT 256/hop 128, em 48 kHz.
O ramo ERB usa log-power por banda; o DF usa normalização de magnitude persistente.
Os coeficientes DF são oito taps complexos por bin: seis temporais, vizinho
superior e vizinho inferior. A saída de ruído é entrada alinhada menos voz.

Em cada hop de 512 amostras, quatro quadros ERB são inferidos em uma sequência;
o DF usa o primeiro desses quatro resultados. Os modelos devem ser causais,
compatíveis com esse agendamento e validados antes de uso realtime.
