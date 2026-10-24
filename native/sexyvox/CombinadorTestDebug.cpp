#include "Combinador.h"
#include <iostream>
#include <cmath>
#include <vector>

namespace {
    constexpr int B = 512;

    bool test_simple() {
        std::cout << "Test: Simple construction and process..." << std::endl;

        try {
            std::cout << "  Creating Combinador..." << std::endl;
            Combinador comb;

            std::cout << "  Allocating buffers..." << std::endl;
            std::vector<float> d(B, 0.0f);
            std::vector<float> erb(B, 0.0f);
            std::vector<float> voice(B, 0.0f);

            std::cout << "  Calling process()..." << std::endl;
            comb.process(d.data(), erb.data(), voice.data());

            std::cout << "  Checking output..." << std::endl;
            float max_abs = 0.0f;
            for (int i = 0; i < B; ++i) {
                if (std::isnan(voice[i]) || std::isinf(voice[i])) {
                    std::cerr << "  FAIL: NaN/Inf at index " << i << std::endl;
                    return false;
                }
                max_abs = std::max(max_abs, std::abs(voice[i]));
            }

            std::cout << "  PASS: Max absolute value = " << max_abs << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "  EXCEPTION: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "  UNKNOWN EXCEPTION" << std::endl;
            return false;
        }
    }
}

int main() {
    std::cout << "=== Combinador Debug Test ===" << std::endl;

    if (test_simple()) {
        std::cout << std::endl << "[OK] Test passed" << std::endl;
        return 0;
    } else {
        std::cout << std::endl << "[FAIL] Test failed" << std::endl;
        return 1;
    }
}
