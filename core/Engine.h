#pragma once
#include "Latency.h"
#include "Model.h"
#include <atomic>
#include <memory>
namespace multifx {
class Engine {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    std::atomic<float> inputPeak{0}, outputPeak{0}, gainReduction{0};
    Engine();
    ~Engine();
    Engine(const Engine &) = delete;
    Engine &operator=(const Engine &) = delete;
    void prepare(double rate, int maxBlock = 1024);
    void process(const float *l, const float *r, float *ol, float *or_, int frames, const State &,
                 double bpm = 120, double beat = 0, bool playing = false, const JAMidi *midi = nullptr,
                 unsigned midiCount = 0) noexcept;
    bool loadIR(const float *l, const float *r, int frames);
    static constexpr int latency = chainLatency;
};
} // namespace multifx
