# SEXYVOX

Plugin VST3 experimental para equilibrar voz e ambiente em tempo real.
Interface vetorial própria, dois controles de ganho e bypass com áudio alinhado.

## Estado atual

- Windows x64, áudio mono ou estéreo em **48 kHz**.
- Voice e Noise: **−50 a +20 dB**, com automação e suavização.
- Estado dos controles salvo na sessão do host.
- Inferência em thread separada; callback realtime sem espera por inferência.
- Versão 0.1.1: **4096 amostras / 85,33 ms** de latência declarada ao host.
- Em outras taxas de amostragem, o áudio passa sem processamento e o editor avisa.
- Se a inferência não acompanhar o host, o plugin mantém áudio original alinhado.

**Os modelos não são distribuídos neste repositório.** Este código precisa de
quatro modelos compatíveis, fornecidos separadamente por quem possui autorização
para usá-los. Não há download automático nem pesos embutidos no código público.
O projeto não é um pacote pronto para instalar sem essas dependências.

## Compilar

Requisitos: compilador C++17 para Windows x64, CMake 3.22+, JUCE 8.0.12 e
ONNX Runtime 1.23.2 x64. Dependências externas mantêm suas próprias licenças.
Forneça os caminhos locais ao configurar; nenhum caminho pessoal é necessário
no código-fonte.

```sh
cmake -S . -B build -A x64 \
  -DJUCE_DIR="<juce-source>" \
  -DONNXRUNTIME_DIR="<runtime-sdk>" \
  -DSEXYVOX_MODELS_DIR="<compatible-models>" \
  -DSEXYVOX_BUILD_TESTS=ON
cmake --build build --config Release --parallel 4
```

No terminal do Windows, coloque a configuração em uma única linha ou use a
continuação de linha do seu shell. O bundle completo fica em
`build/SEXYVOX_artefacts/Release/VST3/SEXYVOX.vst3`. Mantenha a DLL de runtime
dentro do bundle. Feche as instâncias carregadas antes de atualizar o plugin.

## Controles

Voice ajusta a voz; Noise ajusta o ambiente. Arraste verticalmente, use Shift
para ajuste fino e dê dois cliques para voltar a 0 dB. Os textos e as dicas
seguem a cor de cada controle: laranja para voz e azul para ruído.

Para começar, experimente Voice em 0 dB e Noise em −20 dB. Com ambos em 0 dB,
a mistura recompõe a entrada. Ganhos altos podem saturar o áudio; não há limiter.

## Verificação

`SexyProcessorTest` verifica estado, bypass, latência, estéreo, tamanhos de bloco
e comparação de execução realtime cadenciada com a referência offline.
`SexyProcessorTest --soak <fala-48k.wav> <relatorio.json>` executa dois testes
de 30 segundos com canais diferentes e buffers 64/1024.

`SexyStreamTest <entrada-48k.wav> <nova-saida.wav>` é uma ferramenta de diagnóstico
do motor para comparação numérica; não faz parte da interface do plugin.

Veja [o contrato de modelos](docs/MODELS.md) e [as medições](docs/PERFORMANCE.md).
