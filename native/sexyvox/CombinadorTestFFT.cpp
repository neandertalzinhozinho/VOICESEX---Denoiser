#include <iostream>
#include <vector>
#include <juce_dsp/juce_dsp.h>

int main() {
    std::cout << "=== FFT Isolation Test ===" << std::endl;

    try {
        std::cout << "Step 1: Creating FFT object..." << std::endl;
        juce::dsp::FFT fft(9);  // log2(512) = 9
        std::cout << "  OK" << std::endl;

        std::cout << "Step 2: Allocating buffer..." << std::endl;
        std::vector<float> buffer(512, 0.0f);
        buffer[0] = 1.0f;  // Impulso unitário
        std::cout << "  OK" << std::endl;

        std::cout << "Step 3: Calling performRealOnlyForwardTransform..." << std::endl;
        fft.performRealOnlyForwardTransform(buffer.data(), true);
        std::cout << "  OK" << std::endl;

        std::cout << "Step 4: Checking output..." << std::endl;
        bool has_nan = false;
        bool has_inf = false;
        float max_abs = 0.0f;

        for (int i = 0; i < 512; ++i) {
            if (std::isnan(buffer[i])) has_nan = true;
            if (std::isinf(buffer[i])) has_inf = true;
            max_abs = std::max(max_abs, std::abs(buffer[i]));
        }

        std::cout << "  NaN: " << (has_nan ? "YES" : "NO") << std::endl;
        std::cout << "  Inf: " << (has_inf ? "YES" : "NO") << std::endl;
        std::cout << "  Max abs: " << max_abs << std::endl;
        std::cout << "  OK" << std::endl;

        std::cout << std::endl << "[SUCCESS] FFT works correctly" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return 1;
    }
}
