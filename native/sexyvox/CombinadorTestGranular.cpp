#include "Combinador.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Combinador Granular Debug Test ===" << std::endl;

    try {
        std::cout << "Step 1: Creating Combinador..." << std::endl;
        Combinador comb;
        std::cout << "  OK" << std::endl;

        std::cout << "Step 2: Allocating test buffers..." << std::endl;
        std::vector<float> d(512, 0.0f);
        std::vector<float> erb(512, 0.0f);
        std::vector<float> voice(512, 0.0f);
        std::cout << "  OK" << std::endl;

        std::cout << "Step 3: Testing reset()..." << std::endl;
        comb.reset();
        std::cout << "  OK" << std::endl;

        std::cout << "Step 4: Calling process() with zero input..." << std::endl;
        comb.process(d.data(), erb.data(), voice.data());
        std::cout << "  OK" << std::endl;

        std::cout << "Step 5: Checking output..." << std::endl;
        bool has_nan = false;
        bool has_inf = false;
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

        std::cout << std::endl << "[SUCCESS] All steps completed" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return 1;
    }
}
