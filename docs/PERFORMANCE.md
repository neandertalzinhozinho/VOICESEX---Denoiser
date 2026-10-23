# Medições da versão 0.1.1

Medições locais de desenvolvimento; não são garantia de desempenho em toda máquina.

| Medida | 0.1.0 | 0.1.1 |
|---|---:|---:|
| Latência declarada ao host, 48 kHz | 8192 amostras / 170,67 ms | 4096 amostras / 85,33 ms |
| Chamadas ONNX por hop e canal | 10 | 4 |
| Fator de tempo real mono no trecho de referência | 0,4602 | 0,2597 |

Nesse ensaio, o tempo de processamento caiu aproximadamente 43,6%. Isso mede
tempo total de inferência, não utilização de CPU exibida pelo host.
O agrupamento usa somente os quatro quadros ERB já presentes no mesmo hop;
não acrescenta lookahead e mantém os estados entre chamadas.

Diferença contra a versão anterior em um trecho de fala de 14,05 segundos:
RMS 9,78e-9; erro máximo 1,19e-7 após a mesma quantização PCM24.

Teste contínuo com canais diferentes: 30 segundos por caso, buffers 64 e 1024,
48 kHz; zero atrasos de worker e saída idêntica à referência offline.
Maior duração observada do callback: 1,2598 ms (64) e 0,2889 ms (1024).
O sistema operacional, outros plugins e o dispositivo de áudio podem alterar
a margem de tempo disponível. A latência do dispositivo soma-se à do plugin
no monitoramento ao vivo, mesmo com compensação de atraso na reprodução.
