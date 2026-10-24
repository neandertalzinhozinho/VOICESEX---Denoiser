#pragma once
#include <array>
#include <vector>

class Combinador {
public:
    Combinador();
    ~Combinador();

    // Processa um bloco de 512 amostras
    void process(const float* d, const float* erb_output, float* voice);

    // Reseta todos os buffers para estado inicial
    void reset();

    // Métodos expostos para testes
    void apply_filter_L(const float* input, float* output, int num_samples, float* mem);
    void apply_filter_L(const float* input, float* output, int num_samples) {
        apply_filter_L(input, output, num_samples, filter_mem_d.data());
    }
    void apply_delay_D128(const float* input, float* output, int num_samples);

private:
    static constexpr int B = 512;
    static constexpr int DELAY_DIRECT = 128;
    static constexpr int ALIGNMENT_ERB_DF = 384;
    static constexpr int BUF_ALIGNMENT = 896;
    static constexpr int FILTER_TAPS = 257;   // Coeficientes L()
    static constexpr int FILTER_HISTORY = 256; // Tamanho do histórico (N = 256)

    // Filtro complementar L(): FIR passa-baixas ~3092 Hz
    std::array<double, FILTER_TAPS> filter_coeffs;
    std::array<float, FILTER_HISTORY> filter_mem_d;   // Linha de atraso independente para ramo d (256)
    std::array<float, FILTER_HISTORY> filter_mem_e;   // Linha de atraso independente para ramo e_alinhado (256)

    // Buffer de alinhamento ERB
    std::array<float, BUF_ALIGNMENT> erb_alignment_buf;
    int erb_write_idx;

    // Delay D128 circular
    std::array<float, DELAY_DIRECT> delay_line;
    int delay_write_idx;
};
