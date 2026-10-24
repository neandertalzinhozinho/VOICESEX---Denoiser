#include "Combinador.h"
#include <iostream>
#include <vector>
#include <cstring>

int main() {
    std::cout << "=== Combinador Granular 2 ===" << std::endl;

    try {
        std::cout << "Step 1: Creating Combinador..." << std::endl;
        Combinador comb;
        std::cout << "  OK" << std::endl;

        std::cout << "Step 2: reset()" << std::endl;
        comb.reset();
        std::cout << "  OK" << std::endl;

        std::vector<float> zero_input(512, 0.0f);
        std::vector<float> output(512, 0.0f);

        std::cout << "Step 3: apply_delay(zero)" << std::endl;
        comb.apply_delay(zero_input.data(), output.data(), 512);
        std::cout << "  OK" << std::endl;

        std::cout << "Step 4: apply_filter(zero)" << std::endl;
        comb.apply_filter(zero_input.data(), output.data(), 512);
        std::cout << "  OK - filter done" << std::endl;

        // Verificar se a FFT interna funcionou
        std::cout << "Step 5: Checking filter output values..." << std::endl;
        float max_val = 0.0f;
        for (int i = 0; i < 512; ++i) {
            max_val = std::max(max_val, std::abs(output[i]));
        }
        std::cout << "  Max abs: " << max_val << std::endl;
        std::cout << "  OK" << std::endl;

        std::cout << "Step 6: Calling process() with zero input..." << std::endl;
        std::vector<float> d(512, 0.0f);
        std::vector<float> erb(512, 0.0f);
        std::vector<float> voice(512, 0.0f);
        comb.process(d.data(), erb.data(), voice.data());
        std::cout << "  OK - process done" << std::endl;

        // Verificar saída
        std::cout << "Step 7: Checking voice output..." << std::endl;
        bool has_nan = false, has_inf = false;
        float max_abs = 0.0f;
        for (int i = 0; i < 512; ++i) {
            if (std::isnan(voice[i])) has_nan = true;
            if (std::isinf(voice[i])) has_inf = true;
            max_abs = std::max(max_abs, std::abs(voice[i]));
        }
        std::cout << "  NaN: " << (has_nan ? "YES" : "NO") << std::endl;
        std::cout << "  Inf: " << (has_inf ? "YES" : "NO") << std::endl;
        std::cout << "  Max abs: " << max_abs << std::endl;
        std::cout << "  OK" << std::endl;

        std::cout << std::endl << "[SUCCESS] All steps completed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION at last printed step" << std::endl;
        return 1;
    }
}
