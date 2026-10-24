#include "Combinador.h"
#include <iostream>
#include <vector>

int main() {
    std::cout << "=== Combinador Simple Test ===" << std::endl;

    try {
        std::cout << "Step 1: Creating Combinador..." << std::endl;
        Combinador comb;
        std::cout << "  OK - Combinador created" << std::endl;

        std::cout << "Step 2: Calling reset()..." << std::endl;
        comb.reset();
        std::cout << "  OK - reset() completed" << std::endl;

        std::cout << "Step 3: Testing apply_delay with zero input..." << std::endl;
        std::vector<float> input(512, 0.0f);
        std::vector<float> output(512, 0.0f);
        comb.apply_delay(input.data(), output.data(), 512);
        std::cout << "  OK - apply_delay completed" << std::endl;

        std::cout << "Step 4: Testing apply_filter with zero input..." << std::endl;
        comb.apply_filter(input.data(), output.data(), 512);
        std::cout << "  OK - apply_filter completed" << std::endl;

        std::cout << std::endl << "[SUCCESS] All individual methods work" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return 1;
    }
}
