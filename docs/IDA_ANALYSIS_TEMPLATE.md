# IDA Pro 9.3 — Analysis Template (VoicEX 2)

Carregue `CEDARVoicEX_unpacked.dll` no IDA Pro 9.3.

## Etapa 1 — Arquitetura e Dependências de IA

Procure por strings e imports relacionadas a frameworks de inferência:

```
ONNX Runtime: onnxruntime, OrtSession, OrtEnv, OrtRun
TensorFlow Lite: tflite, TfLiteInterpreter, tflite::Interpreter
PyTorch / LibTorch: torch, jit::script, at::Tensor
OpenVINO: ov::Core, ov::CompiledModel, openvino
Custom: matrizes, gemm, float*, __m128, __m256, AVX, SSE
```

Ações no IDA:
- `Shift+F12` (Strings) → filtre por "onnx", "tflite", "torch", "ov::", "tensor"
- `Imports window` → procure por DLLs como `onnxruntime.dll`, `tensorflowlite.dll`, `libtorch.dll`
- Procure por alocação de memória grande (`VirtualAlloc`, `malloc` com tamanho grande) — possivelmente carregamento de pesos

## Etapa 2 — Bifurcação de Streams (Voice vs Noise)

Localize a função de processamento principal do VST3. Em plugins VST3, isso costuma ser o método `process` da classe do processor.

Procure por:
- Função que recebe dois buffers de entrada/saída (input, output)
- Divisão do sinal em dois fluxos internos
- Ganhos de -50dB a +20dB (procure por multiplicação por constantes float como 0.00001 (-100dB) a 10.0 (+20dB))
- Máscara espectral: multiplicação elemento-a-elemento entre sinal e máscara
- Síntese direta: IFFT/overlap-add

Strings a procurar no IDA:
```
"Voice", "Noise", "voice", "noise", "gain", "dB", "mask", "stream"
```

## Etapa 3 — Topologia da Rede Neural

Procure por:
- Convoluções: loops com soma de produtos (MAC), filtros 1D/2D
- RNN/LSTM: loops recorrentes, camadas de porta (gate), sigmoid/tanh
- Transformers: attention, softmax, Q/K/V
- Camadas densas: GEMM (matrix multiply)

Padrões de assembly x64 a procurar:
- `vmovaps`, `vmovups`, `vmulps`, `vaddps`, `vfmadd` (AVX float ops)
- `cvtdq2ps`, `cvtps2dq` (conversão int/float)
- Loops com contadores em registradores (rcx, rdx, r8, r9)

## Etapa 4 — Extração de Pesos e Bias

Pesos e bias são tipicamente arrays de `float` (4 bytes cada) ou `double` (8 bytes).

No IDA:
- Procure por grandes arrays de dados constantes (`.rdata` ou `.data`)
- Procure por referências a `float` via `lea` ou `movss`
- Dump de memória: use `File → Produce file → Dump data to file` para extrair seções

Script Python para IDA (extrair floats):
```python
import idaapi
import struct

start_ea = 0xXXXXXXXX  # endereço do array de pesos
size = 1024  # número de floats

weights = []
for i in range(size):
    ea = start_ea + i * 4
    val = struct.unpack('<f', idaapi.get_bytes(ea, 4))[0]
    weights.append(val)

print(weights[:20])  # primeiros 20 pesos
```

## Etapa 5 — Clonagem em Python/PyTorch

Estrutura esperada do clonado:
1. Carregar áudio → STFT (espectrograma)
2. Passar pela rede neural (máscara de voz/ruído)
3. Aplicar máscara ao sinal
4. ISTFT → reconstruir áudio
5. Aplicar ganhos Voice (-50dB a +20dB) e Noise (-50dB a +20dB)

Template PyTorch:
```python
import torch
import torch.nn as nn

class DenoiserMask(nn.Module):
    def __init__(self):
        super().__init__()
        # Preencher com camadas extraídas do binário
        self.layers = nn.Sequential(...)
    
    def forward(self, x):
        # x: espectrograma de entrada
        mask = torch.sigmoid(self.layers(x))
        return mask  # 0 = ruído, 1 = voz
```

## Validação
- Latência < 10ms (processamento em blocos pequenos ou arquitetura streaming)
- Saída deve separar voz e ruído conforme os controles do plugin