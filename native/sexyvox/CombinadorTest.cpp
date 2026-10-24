#include "Combinador.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <random>

namespace {
    constexpr int B = 512;

    bool test_zero_input() {
        std::cout << "Test 1: Zero input → zero output..." << std::endl;

        Combinador comb;
        float d[B] = {0};
        float erb[B] = {0};
        float voice[B] = {0};

        comb.process(d, erb, voice);

        // Verificar ausência de NaN/Inf
        for (int i = 0; i < B; ++i) {
            if (std::isnan(voice[i]) || std::isinf(voice[i])) {
                std::cerr << "  FAIL: NaN/Inf detected at index " << i << std::endl;
                return false;
            }
        }

        // Verificar que saída é próxima de zero
        float max_abs = 0.0f;
        for (int i = 0; i < B; ++i) {
            max_abs = std::max(max_abs, std::abs(voice[i]));
        }

        if (max_abs > 1e-4f) {
            std::cerr << "  FAIL: Max absolute value " << max_abs << " exceeds threshold" << std::endl;
            return false;
        }

        std::cout << "  PASS: Max absolute value = " << max_abs << std::endl;
        return true;
    }

    bool test_unit_impulse() {
        std::cout << "Test 2: Unit impulse response..." << std::endl;

        Combinador comb;
        float d[B] = {0};
        float erb[B] = {0};
        float voice[B] = {0};

        // Impulso unitário na entrada direta
        d[0] = 1.0f;

        comb.process(d, erb, voice);

        // Verificar ausência de NaN/Inf
        for (int i = 0; i < B; ++i) {
            if (std::isnan(voice[i]) || std::isinf(voice[i])) {
                std::cerr << "  FAIL: NaN/Inf detected at index " << i << std::endl;
                return false;
            }
        }

        // Verificar que resposta não é trivialmente zero
        float energy = 0.0f;
        for (int i = 0; i < B; ++i) {
            energy += voice[i] * voice[i];
        }

        if (energy < 1e-6f) {
            std::cerr << "  FAIL: Response energy too low: " << energy << std::endl;
            return false;
        }

        std::cout << "  PASS: Response energy = " << energy << std::endl;
        return true;
    }

    bool test_continuity() {
        std::cout << "Test 3: Continuity between calls..." << std::endl;

        Combinador comb;
        std::vector<float> voice_stream;

        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dist(-0.1f, 0.1f);

        // Processar 10 blocos consecutivos
        for (int block = 0; block < 10; ++block) {
            float d[B];
            float erb[B] = {0};
            float voice[B];

            for (int i = 0; i < B; ++i) {
                d[i] = dist(rng);
            }

            comb.process(d, erb, voice);

            // Verificar ausência de NaN/Inf
            for (int i = 0; i < B; ++i) {
                if (std::isnan(voice[i]) || std::isinf(voice[i])) {
                    std::cerr << "  FAIL: NaN/Inf detected at block " << block
                              << ", index " << i << std::endl;
                    return false;
                }
            }

            for (int i = 0; i < B; ++i) {
                voice_stream.push_back(voice[i]);
            }
        }

        // Verificar continuidade (sem descontinuidades grandes)
        int discontinuities = 0;
        for (size_t i = B; i < voice_stream.size(); ++i) {
            float diff = std::abs(voice_stream[i] - voice_stream[i - B]);
            if (diff > 1.0f) {  // Threshold generoso para evitar falsos positivos
                discontinuities++;
            }
        }

        if (discontinuities > 5) {
            std::cerr << "  FAIL: Too many discontinuities: " << discontinuities << std::endl;
            return false;
        }

        std::cout << "  PASS: Discontinuities = " << discontinuities << std::endl;
        return true;
    }

    bool test_reset() {
        std::cout << "Test 4: Reset functionality..." << std::endl;

        Combinador comb;
        float d[B] = {0};
        float erb[B] = {0};
        float voice1[B] = {0};
        float voice2[B] = {0};

        // Processar uma vez
        d[0] = 1.0f;
        comb.process(d, erb, voice1);

        // Resetar
        comb.reset();

        // Processar novamente com mesma entrada
        comb.process(d, erb, voice2);

        // Verificar que saídas são idênticas
        float max_diff = 0.0f;
        for (int i = 0; i < B; ++i) {
            float diff = std::abs(voice1[i] - voice2[i]);
            max_diff = std::max(max_diff, diff);
        }

        if (max_diff > 1e-6f) {
            std::cerr << "  FAIL: Reset did not restore initial state, max diff = "
                      << max_diff << std::endl;
            return false;
        }

        std::cout << "  PASS: Max difference after reset = " << max_diff << std::endl;
        return true;
    }
}

int main() {
    std::cout << "=== Combinador Unit Tests ===" << std::endl;

    int passed = 0;
    int total = 4;

    if (test_zero_input()) passed++;
    if (test_unit_impulse()) passed++;
    if (test_continuity()) passed++;
    if (test_reset()) passed++;

    std::cout << std::endl;
    std::cout << "Results: " << passed << "/" << total << " tests passed" << std::endl;

    return (passed == total) ? 0 : 1;
}
