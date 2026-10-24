#include "Combinador.h"
#include <cmath>
#include <cstring>
#include <algorithm>

namespace {
    constexpr double PI = 3.14159265358979323846;
    constexpr double SAMPLE_RATE = 48000.0;
    constexpr double CUTOFF_FREQ = 3092.0;
}

Combinador::Combinador()
    : erb_write_idx(0)
    , delay_write_idx(0)
{
    erb_alignment_buf.fill(0.0f);
    filter_mem_d.fill(0.0f);
    filter_mem_e.fill(0.0f);
    delay_line.fill(0.0f);

    // Projetar filtro passa-baixas FIR via janela de Hamming
    // Método sinc truncada + janela
    const int N = FILTER_TAPS - 1;
    const double wc = 2.0 * PI * CUTOFF_FREQ / SAMPLE_RATE;

    for (int n = 0; n < FILTER_TAPS; ++n) {
        double x = n - static_cast<double>(N) / 2.0;
        double h;
        if (std::abs(x) < 1e-10) {
            h = wc / PI;
        } else {
            h = sin(wc * x) / (PI * x);
        }
        // Janela de Hamming
        double w = 0.54 - 0.46 * cos(2.0 * PI * n / N);
        filter_coeffs[n] = h * w;
    }

    // Normalizar para ganho unity na banda passante
    double gain = 0.0;
    for (int n = 0; n < FILTER_TAPS; ++n) {
        gain += filter_coeffs[n];
    }
    if (gain > 0.001) {
        for (int n = 0; n < FILTER_TAPS; ++n) {
            filter_coeffs[n] /= gain;
        }
    }
}

Combinador::~Combinador() = default;

void Combinador::apply_filter_L(const float* input, float* output, int num_samples, float* mem) {
    // Filtro FIR causal direto tipo I: y[n] = sum_{k=0}^{N} h[k] * x[n-k]
    // Onde N = FILTER_TAPS - 1 = 256
    const int N = FILTER_TAPS - 1;

    for (int i = 0; i < num_samples; ++i) {
        double acc = filter_coeffs[0] * input[i];

        for (int k = 1; k <= N; ++k) {
            acc += filter_coeffs[k] * mem[k - 1];
        }

        output[i] = static_cast<float>(acc);

        // Atualizar linha de atraso: mem[0] guarda amostra anterior mais recente
        std::memmove(mem + 1, mem, sizeof(float) * static_cast<size_t>(N - 1));
        mem[0] = input[i];
    }
}

void Combinador::apply_delay_D128(const float* input, float* output, int num_samples) {
    // Buffer circular de tamanho DELAY_DIRECT (128)
    // Cada chamada sobrescreve os dados mais antigos
    // A saída é o dado mais antigo na posição atual

    for (int i = 0; i < num_samples; ++i) {
        // Lê dado que está na posição atual (o mais antigo)
        output[i] = delay_line[delay_write_idx];

        // Sobrescreve com entrada nova
        delay_line[delay_write_idx] = input[i];

        // Avança índice circular
        delay_write_idx = (delay_write_idx + 1) % DELAY_DIRECT;
    }
}

void Combinador::process(const float* d, const float* erb_output, float* voice) {
    // Topologia observada no CEDAR VoicEX 2:
    // Voice = L(d) + [D128(e) - L(e)]
    // Em Streaming.cpp, tanto d quanto erb_output sao sintetizados via dfFFT (hop 512),
    // portanto ja estao temporalmente sincronizados. O atraso adicional de 384 amostras
    // causava desfasamento temporal e filtro pente severo no crossover (~3092 Hz).

    std::array<float, B> L_d;
    std::array<float, B> D128_e;
    std::array<float, B> L_e;

    apply_filter_L(d, L_d.data(), B, filter_mem_d.data());
    apply_delay_D128(erb_output, D128_e.data(), B);
    apply_filter_L(erb_output, L_e.data(), B, filter_mem_e.data());

    // Sintese complementar
    for (int i = 0; i < B; ++i) {
        voice[i] = L_d[i] + (D128_e[i] - L_e[i]);
    }
}

void Combinador::reset() {
    erb_alignment_buf.fill(0.0f);
    filter_mem_d.fill(0.0f);
    filter_mem_e.fill(0.0f);
    delay_line.fill(0.0f);
    erb_write_idx = 0;
    delay_write_idx = 0;
}
