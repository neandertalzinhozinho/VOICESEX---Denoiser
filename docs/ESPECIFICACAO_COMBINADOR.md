# SEXYVOX — Especificação do Combinador

**Data:** 2026-10-23  
**Status:** Especificação inicial baseada em evidências de instrumentação não-invasiva  
**Versão:** 0.1.0

## Resumo Executivo

Esta especificação define o combinador nativo observado no CEDAR VoicEX 2, responsável por sintetizar a saída Voice a partir das saídas dos modelos DF e ERB. A topologia implementa um filtro complementar que preserva a energia total do sinal enquanto aplica processamento seletivo em frequência.

## Topologia Confirmada

```
Voice = L(d) + [D128(e_alinhado) - L(e_alinhado)]
```

Onde:
- `d`: sinal de entrada (direto)
- `e_alinhado`: saída do modelo ERB após alinhamento temporal
- `L()`: filtro passa-baixas complementar (cutoff 3092 Hz)
- `D128()`: atraso direto de 128 amostras
- `Voice`: saída final do processamento

**Evidência:** Relatório de investigação de latência (docs/INVESTIGACAO_LATENCIA_INICIALIZACAO_20261023.md), seção de análise do combinador nativo.

## Mapa Sinal a Sinal

### Entrada

- **Sinal de entrada:** `input[0..N-1]`
- **Taxa de amostragem:** 48000 Hz
- **Tamanho de bloco (B):** 512 amostras (configurável)
- **Canais:** 2 (estéreo processado como dual-mono)

### Ramo DF (Deep FilterNet)

```
input → [buffer circular 512] → FFT 1024 → modelo DF → máscara DF → IFFT 1024 → df_output
```

**Parâmetros:**
- FFT: 1024 pontos
- Hop: 512 amostras
- Buffer histórico: df_input_history [512] float32
- Buffer FFT: df_fft_input [1024] float32 (não inicializado)
- Saída: df_output [512] float64

### Ramo ERB (Equivalent Rectangular Bandwidth)

```
input → [buffer circular 128] → FFT 256 → modelo ERB → máscara ERB → IFFT 256 → erb_output
```

**Parâmetros:**
- FFT: 256 pontos
- Hop: 128 amostras
- Buffer histórico: erb_input_history [128] float32
- Buffer FFT: erb_fft_input [256] float32 (inicializado em zero)
- Saída: erb_output [128] float64

### Alinhamento Temporal

```
erb_output → [buffer alinhamento 896] → e_alinhado
```

**Parâmetros:**
- Buffer: erb_alignment [896] float32 (não inicializado)
- Alinhamento ERB/DF: 384 amostras
- Latência total do alinhamento: 384 amostras

**Nota:** O buffer de 896 amostras acomoda histórico + alinhamento + margem de segurança.

### Filtro Complementar

```
e_alinhado → L() → L(e_alinhado)
e_alinhado → D128() → D128(e_alinhado)
D128(e_alinhado) - L(e_alinhado) → componente complementar
```

**Parâmetros do filtro L:**
- Tipo: FIR passa-baixas
- Cutoff: 3092 Hz
- FFT: 512 pontos
- Hop: 256 amostras
- Coeficientes: 257 pesos double
- Recorte: [128:384) (256 amostras centrais)
- Histórico: 256 amostras

**Atraso direto:**
- D128: 128 amostras de atraso puro

### Saída

```
L(d) + [D128(e_alinhado) - L(e_alinhado)] → voice_output
```

**Parâmetros:**
- Buffer: voice [512] float64
- Latência total: 640 amostras (algoritímica) + atrasos de buffering

## Convenções

### Indexação

- **Intervalos:** `[a:b)` = inclusivo em `a`, exclusivo em `b`
- **Índice absoluto:** posição no buffer circular global (não resetado entre chamadas)
- **Índice relativo:** posição dentro do bloco atual (0 a B-1)

### Tempo

- **Amostras negativas:** padding antes do início do sinal (preenchido com zeros)
- **Pre-roll:** processamento de blocos iniciais para preencher buffers
- **Origem de tempo:** primeira amostra do primeiro bloco processado

### Latência

- **Latência algorítmica:** 640 amostras (inerente ao processamento)
- **Latência de buffering:** depende do host e configuração
- **Latência total reportada:** 640 + 2×MMC(B, 512) amostras

**Para B=512:**
- Latência algorítmica: 640
- Buffering: 2×512 = 1024
- Total: 1664 amostras (34.67 ms @ 48kHz)

## Tabela de Evidência

| Constante | Valor | Evidência | Confiança |
|-----------|-------|-----------|-----------|
| F (fase inicial) | {320, 384, 448} | Observação em 5 PIDs distintos | Alta (observado) |
| Seletor de F | Desconhecido | Não determinístico | Baixa (não observado) |
| FFT DF | 1024 pontos | Código nativo analisado | Alta (confirmado) |
| Hop DF | 512 amostras | Código nativo analisado | Alta (confirmado) |
| FFT ERB | 256 pontos | Código nativo analisado | Alta (confirmado) |
| Hop ERB | 128 amostras | Código nativo analisado | Alta (confirmado) |
| Alinhamento ERB/DF | 384 amostras | Análise de latência | Alta (confirmado) |
| Cutoff filtro L | 3092 Hz | Análise espectral | Alta (confirmado) |
| FFT filtro L | 512 pontos | Código nativo analisado | Alta (confirmado) |
| Coeficientes L | 257 pesos | Código nativo analisado | Alta (confirmado) |
| Recorte L | [128:384) | Código nativo analisado | Alta (confirmado) |
| Histórico L | 256 amostras | Código nativo analisado | Alta (confirmado) |
| Atraso direto | 128 amostras | Código nativo analisado | Alta (confirmado) |
| Buffer erb_alignment | 896 amostras | Observação direta | Alta (observado) |
| Buffer df_fft_input | 1024 amostras | Observação direta | Alta (observado) |
| Inicialização erb_alignment | Não inicializado | Observação de valores lixo | Alta (confirmado) |
| Inicialização df_fft_input | Não inicializado | Observação de valores lixo | Alta (confirmado) |
| Inicialização erb_fft_input | Zero | Observação direta | Alta (confirmado) |
| Estados h/c modelos | Desconhecido | Não observável via API | Baixa (não observado) |

## Invariantes

### Dimensões

1. **Blocos de processamento:** sempre múltiplos de 128 amostras
2. **Buffers circulares:** capacidade fixa (df: 512, erb: 128, voice: 512)
3. **Saída ERB:** 128 amostras por chamada (após alinhamento)
4. **Saída DF:** 512 amostras por chamada

### Qualidade de Sinal

1. **Ausência de NaN/Inf:** todos os buffers devem conter valores finitos após processamento
2. **Continuidade entre chamadas:** buffers circulares mantêm histórico entre blocos
3. **Complementaridade:** `Voice + Noise ≈ entrada` (preservação de energia)

### Comportamento

1. **Entrada zerada:** saída deve ser zerada (exceto por estados residuais dos modelos)
2. **Impulso unitário:** resposta deve decair dentro de 2048 amostras
3. **Suporte a múltiplos F:** implementação deve validar F ∈ {320, 384, 448} para B=512

## Pseudocódigo

```python
class Combinador:
    def __init__(self, B=512, sample_rate=48000):
        self.B = B
        self.sample_rate = sample_rate
        
        # Buffers de entrada
        self.df_input_history = np.zeros(512, dtype=np.float32)
        self.erb_input_history = np.zeros(128, dtype=np.float32)
        
        # Buffers de processamento
        self.df_fft_input = np.zeros(1024, dtype=np.float32)  # Inicializado em zero (diferente do CEDAR)
        self.erb_fft_input = np.zeros(256, dtype=np.float32)
        self.erb_alignment = np.zeros(896, dtype=np.float32)  # Inicializado em zero (diferente do CEDAR)
        
        # Buffers de saída
        self.voice = np.zeros(512, dtype=np.float64)
        self.df_hist = np.zeros(768, dtype=np.float64)
        self.low_df = np.zeros(512, dtype=np.float64)
        self.erb_hist = np.zeros(768, dtype=np.float64)
        self.high_erb = np.zeros(512, dtype=np.float64)
        
        # Filtro complementar
        self.filter_history = np.zeros(256, dtype=np.float64)
        self.filter_coeffs = self._design_complementary_filter()
        
        # Estado dos modelos (inicialização lazy)
        self.df_model_state = None
        self.erb_model_state = None
    
    def _design_complementary_filter(self):
        """Projeta filtro FIR passa-baixas complementar."""
        # Cutoff: 3092 Hz, FFT: 512 pontos
        # Retorna 257 coeficientes double
        # Recorte [128:384) para obter 256 amostras centrais
        pass
    
    def process(self, input_block, F):
        """
        Processa um bloco de entrada.
        
        Args:
            input_block: np.array[B] float32
            F: fase inicial (320, 384 ou 448 para B=512)
        
        Returns:
            voice_output: np.array[B] float64
        """
        # 1. Atualizar buffers circulares de entrada
        self.df_input_history = np.roll(self.df_input_history, -self.B)
        self.df_input_history[-self.B:] = input_block
        
        self.erb_input_history = np.roll(self.erb_input_history, -self.B)
        self.erb_input_history[-self.B:] = input_block
        
        # 2. Preparar FFTs
        self.df_fft_input[:512] = self.df_input_history
        self.df_fft_input[512:] = input_block
        
        self.erb_fft_input[:128] = self.erb_input_history[-128:]
        self.erb_fft_input[128:] = input_block[:128]
        
        # 3. Executar modelos (inicialização lazy)
        if self.df_model_state is None:
            df_output = self._run_df_model_first_call(self.df_fft_input)
        else:
            df_output = self._run_df_model(self.df_fft_input, self.df_model_state)
        
        if self.erb_model_state is None:
            erb_output = self._run_erb_model_first_call(self.erb_fft_input)
        else:
            erb_output = self._run_erb_model(self.erb_fft_input, self.erb_model_state)
        
        # 4. Alinhamento temporal
        self.erb_alignment = np.roll(self.erb_alignment, -128)
        self.erb_alignment[-128:] = erb_output
        e_alinhado = self.erb_alignment[-384:]  # Últimas 384 amostras
        
        # 5. Filtro complementar
        L_e = self._apply_complementary_filter(e_alinhado)
        D128_e = np.roll(e_alinhado, 128)
        
        # 6. Síntese final
        L_d = self._apply_complementary_filter(input_block)
        voice = L_d + (D128_e - L_e)
        
        # 7. Atualizar históricos
        self.voice = voice
        self.filter_history = np.roll(self.filter_history, -256)
        self.filter_history[-256:] = e_alinhado
        
        return voice
    
    def _apply_complementary_filter(self, signal):
        """Aplica filtro passa-baixas complementar."""
        # FFT 512 pontos
        # Multiplica por coeficientes
        # IFFT
        # Recorta [128:384)
        # Retorna 256 amostras
        pass
    
    def _run_df_model_first_call(self, fft_input):
        """Executa modelo DF na primeira chamada (inicialização lazy)."""
        # Estado inicial: h=0, c=0
        # Retorna saída do modelo
        pass
    
    def _run_erb_model_first_call(self, fft_input):
        """Executa modelo ERB na primeira chamada (inicialização lazy)."""
        # Estado inicial: h=0, c=0
        # Retorna saída do modelo
        pass
```

## Estrutura de Estado

```
Combinador
├── Buffers de entrada
│   ├── df_input_history [512] float32
│   └── erb_input_history [128] float32
├── Buffers de processamento
│   ├── df_fft_input [1024] float32
│   ├── erb_fft_input [256] float32
│   └── erb_alignment [896] float32
├── Buffers de saída
│   ├── voice [512] float64
│   ├── df_hist [768] float64
│   ├── low_df [512] float64
│   ├── erb_hist [768] float64
│   └── high_erb [512] float64
├── Filtro complementar
│   ├── filter_history [256] float64
│   └── filter_coeffs [257] float64
└── Estados dos modelos
    ├── df_model_state (lazy)
    └── erb_model_state (lazy)
```

## Testes Mínimos

### Teste 1: Impulso Unitário

**Objetivo:** Validar resposta ao impulso e decaimento.

```python
def test_impulso_unitario():
    comb = Combinador(B=512)
    
    # Entrada: impulso unitário
    input_block = np.zeros(512, dtype=np.float32)
    input_block[0] = 1.0
    
    # Processar com F=320
    output = comb.process(input_block, F=320)
    
    # Verificar:
    # 1. Saída não é NaN/Inf
    assert np.all(np.isfinite(output))
    
    # 2. Energia decai após 2048 amostras
    # (requer processamento de múltiplos blocos)
```

### Teste 2: Entrada Zerada

**Objetivo:** Validar comportamento com silêncio.

```python
def test_entrada_zerada():
    comb = Combinador(B=512)
    
    # Entrada: silêncio
    input_block = np.zeros(512, dtype=np.float32)
    
    # Processar com F=320
    output = comb.process(input_block, F=320)
    
    # Verificar: saída deve ser próxima de zero
    assert np.allclose(output, 0, atol=1e-6)
```

### Teste 3: Múltiplos Valores de F

**Objetivo:** Validar suporte a diferentes fases iniciais.

```python
def test_multiplos_F():
    for F in [320, 384, 448]:
        comb = Combinador(B=512)
        input_block = np.random.randn(512).astype(np.float32)
        output = comb.process(input_block, F=F)
        
        # Verificar: saída não é NaN/Inf
        assert np.all(np.isfinite(output))
```

### Teste 4: Complementaridade Voice/Noise

**Objetivo:** Validar preservação de energia.

```python
def test_complementaridade():
    comb = Combinador(B=512)
    
    # Entrada: sinal arbitrário
    input_block = np.random.randn(512).astype(np.float32)
    
    # Processar
    voice = comb.process(input_block, F=320)
    noise = comb.process_noise(input_block, F=320)  # Implementação separada
    
    # Verificar: voice + noise ≈ input
    reconstructed = voice + noise
    assert np.allclose(reconstructed, input_block, atol=1e-3)
```

### Teste 5: Continuidade Entre Chamadas

**Objetivo:** Validar buffers circulares.

```python
def test_continuidade():
    comb = Combinador(B=512)
    
    # Processar 10 blocos consecutivos
    outputs = []
    for i in range(10):
        input_block = np.random.randn(512).astype(np.float32)
        output = comb.process(input_block, F=320)
        outputs.append(output)
    
    # Verificar: ausência de descontinuidades nas fronteiras
    for i in range(1, 10):
        diff = np.abs(outputs[i][0] - outputs[i-1][-1])
        assert diff < 0.1  # Threshold ajustável
```

## Lacunas e Trabalho Futuro

### Lacuna 1: Coeficientes do Filtro Complementar

**Status:** Parâmetros conhecidos (cutoff, FFT, recorte), mas coeficientes exatos não extraídos.  
**Próximo passo:** Engenharia reversa do código nativo ou síntese numérica dos coeficientes.

### Lacuna 2: Estados Iniciais dos Modelos

**Status:** Não observáveis via API.  
**Próximo passo:** Validação empírica com impulso unitário e ajuste iterativo.

### Lacuna 3: Seletor de F

**Status:** Desconhecido.  
**Próximo passo:** Implementar suporte a múltiplos valores e validar todos.

### Lacuna 4: Implementação do Modelo DF/ERB

**Status:** Especificação foca no combinador, não nos modelos internos.  
**Próximo passo:** Especificação separada dos modelos DF e ERB.

## Referências

- docs/INVESTIGACAO_LATENCIA_INICIALIZACAO_20261023.md
- docs/INVESTIGACAO_PRIMEIRO_FRAME_20261023.md
- docs/ANALISE_FASE_F_ESTADOS_RECORRENTES.md
- planejamento/05_DESENHO_DO_COMBINADOR.md
- planejamento/referencias/OBSERVACAO_PRIMEIRO_FRAME_20261023T204624.json

## Histórico de Versões

- **0.1.0 (2026-10-23):** Especificação inicial baseada em evidências de instrumentação não-invasiva.
