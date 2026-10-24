#include <iostream>
#include <vector>
#include <cstring>
#include <juce_dsp/juce_dsp.h>

int main() {
    std::cout << "=== FFT Double Loop Test ===" << std::endl;

    try {
        juce::dsp::FFT fft(9);  // log2(512)
        std::vector<float> history(256, 0.0f);
        float buffer[512];

        std::cout << "Step 1: One iteration memcpy + FFT..." << std::endl;
        std::memcpy(buffer, history.data(), 256 * sizeof(float));
        std::memset(buffer + 256, 0, 256 * sizeof(float));
        fft.performRealOnlyForwardTransform(buffer, true);
        std::cout << "  OK" << std::endl;

        std::cout << "Step 2: Apply coefficients manually (k=1)..." << std::endl;
        float re = buffer[2];
        float im = buffer[3];
        buffer[2] = re * 1.0f;
        buffer[3] = im * 1.0f;
        std::cout << "  OK" << std::endl;

        std::cout << "Step 3: Inverse FFT..." << std::endl;
        fft.performRealOnlyInverseTransform(buffer);
        std::cout << "  OK" << std::endl;

        std::cout << "Step 4: Loop with local float[512]..." << std::endl;
        for (int hop = 0; hop < 2; ++hop) {
            std::cout << "  Hop " << hop << "...";
            float local_buf[512] = {0};
            std::memcpy(local_buf, history.data(), 256 * sizeof(float));
            fft.performRealOnlyForwardTransform(local_buf, true);
            fft.performRealOnlyInverseTransform(local_buf);
            std::cout << " OK" << std::endl;
        }
        std::cout << "  OK" << std::endl;

        std::cout << std::endl << "[SUCCESS] All steps completed!" << std::endl;
        return 0;

    } catch (const std::exception& e) {
        std::cerr << "EXCEPTION: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "UNKNOWN EXCEPTION" << std::endl;
        return 1;
    }
}
