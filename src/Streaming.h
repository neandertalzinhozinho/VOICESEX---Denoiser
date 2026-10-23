#pragma once
#include <array>
#include <memory>

// One causal channel; each call consumes 512 samples and returns the preceding
// 512-sample voice block. The first returned block is preroll and is discarded.
class StreamChannel {
public:
    StreamChannel();
    ~StreamChannel();
    void process(const float* input, float* voice);
private:
    struct Impl;
    std::unique_ptr<Impl> p;
};
