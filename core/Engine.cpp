#include "Engine.h"
#include "Convolver.h"
#include "effects/Native.h"
#include <stdexcept>
namespace multifx {
namespace {
struct Runner {
    const JAModule *api = nullptr;
    void *instance = nullptr;
    int offset = 0;
    std::array<float, 256> parameters{};
    ~Runner() {
        if (instance)
            api->destroy(instance);
    }
    void prepare(Group g, double sr) {
        if (instance)
            api->destroy(instance);
        api = g.api;
        offset = g.start;
        instance = api->create(sr, 1024, nullptr);
        if (!instance)
            throw std::runtime_error("Cannot prepare " + g.name);
        if (api->parameter_count > parameters.size())
            throw std::runtime_error("Parameter capacity");
    }
    void run(float *l, float *r, int n, const State &s, double sr, double bpm, double beat, bool playing,
             const JAMidi *midi, unsigned midiCount) {
        for (unsigned i = 0; i < api->parameter_count; ++i) {
            const auto &d = api->parameters[i];
            parameters[i] = d.minimum + (d.maximum - d.minimum) * clean(s.values[offset + i]);
        }
        JAProcess b{sizeof(JAProcess), uint32_t(n), sr,       bpm, beat, uint32_t(playing), {l, r}, {l, r},
                    parameters.data(), midi,        midiCount};
        api->process(instance, &b);
    }
};
} // namespace
struct Engine::Impl {
    double sr = 48000;
    std::array<Runner, 4> processor;
    std::array<std::array<Runner, 4>, 2> delay;
    Analog analog;
    Reverb reverb;
    Convolver ir;
    std::array<Digital, 2> digital;
    std::array<std::array<float, 1024>, 2> work{}, dry{}, a{}, b{}, saved{};
    std::array<std::array<float, 128>, 2> masterDry{};
    int dryPos = 0;
    State smooth;
    std::array<int, 5> order{0, 1, 2, 3, 4};
    int rank = 0;
    float graphFade = 1, master = 0;
    bool switching = false;
    void delaySlot(int slot, float *l, float *r, int n, const State &s, double bpm, double beat,
                   bool playing) {
        const auto &layout = multifx::layout();
        int type = int(plain(layout.delayType[slot], s.values[layout.delayType[slot]])),
            p = layout.digital[slot];
        float mix = s.values[p + 2], gain = db(plain(p + 3, s.values[p + 3]));
        for (int i = 0; i < n; ++i) {
            saved[0][i] = l[i];
            saved[1][i] = r[i];
        }
        if (type == 0) {
            for (int i = 0; i < n; ++i) {
                auto v = digital[slot].tick(l[i], r[i], s, p, bpm);
                l[i] = v[0];
                r[i] = v[1];
            }
        } else
            delay[slot][type - 1].run(l, r, n, s, sr, bpm, beat, playing, nullptr, 0);
        for (int i = 0; i < n; ++i) {
            l[i] = (saved[0][i] * (1 - mix) + l[i] * mix) * gain;
            r[i] = (saved[1][i] * (1 - mix) + r[i] * mix) * gain;
        }
    }
};
Engine::Engine() : impl(std::make_unique<Impl>()) {}
Engine::~Engine() = default;
void Engine::prepare(double rate, int block) {
    if (!std::isfinite(rate) || rate < 8000 || rate > 192000 || block < 1)
        throw std::invalid_argument("Unsupported sample rate or block size");
    auto &p = *impl;
    p.sr = rate;
    p.analog.prepare(rate);
    p.reverb.prepare(rate);
    p.ir.reset();
    for (auto &d : p.digital)
        d.prepare(rate);
    const auto &l = layout();
    for (int j = 0; j < 4; ++j)
        p.processor[j].prepare(l.processors[j], rate);
    for (int s = 0; s < 2; ++s)
        for (int j = 0; j < 4; ++j)
            p.delay[s][j].prepare(l.delays[s][j], rate);
    p.smooth = State{};
    p.rank = 0;
    p.order = {0, 1, 2, 3, 4};
    p.graphFade = 1;
    p.master = 0;
    p.masterDry = {};
    p.dryPos = 0;
}
bool Engine::loadIR(const float *l, const float *r, int frames) {
    return impl->ir.load(l, r, frames);
}
void Engine::process(const float *left, const float *right, float *outL, float *outR, int frames,
                     const State &target, double bpm, double beat, bool playing, const JAMidi *midi,
                     unsigned midiCount) noexcept {
    if (!outL || !outR || frames <= 0)
        return;
    auto &p = *impl;
    const auto &lay = layout();
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400)
        bpm = 120;
    float inPeak = 0, outPeak = 0;
    for (int start = 0; start < frames; start += 1024) {
        int n = std::min(1024, frames - start);
        State &s = p.smooth;
        float smoothing = float(1 - std::exp(-double(n) / (p.sr * .01)));
        for (size_t i = 0; i < lay.parameters.size(); ++i) {
            float v = clean(target.values[i]);
            if (lay.parameters[i].steps && int(i) < lay.midiLearn) {
                if (v != s.values[i])
                    p.switching = true;
            } else
                s.values[i] += smoothing * (v - s.values[i]);
        }
        int rank = int(plain(lay.order, target.values[lay.order]));
        if (rank != p.rank)
            p.switching = true;
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c) {
                auto input = c ? right : left;
                float x = input ? input[start + i] : (left ? left[start + i] : 0);
                x = std::isfinite(x) ? std::clamp(x, -16.f, 16.f) : 0;
                p.work[c][i] = p.dry[c][i] = x;
                inPeak = std::max(inPeak, std::abs(x));
            }
        for (int block : p.order) {
            for (int c = 0; c < 2; ++c)
                std::copy_n(p.work[c].data(), n, p.b[c].data());
            auto *l = p.work[0].data();
            auto *r = p.work[1].data();
            if (block == 0) {
                for (int i = 0; i < n; ++i)
                    p.analog.preamp(l[i], r[i], s);
                p.processor[0].run(l, r, n, s, p.sr, bpm, beat, playing, nullptr, 0);
                for (int i = 0; i < n; ++i) {
                    p.analog.drive(l[i], r[i], s);
                    p.analog.comp(l[i], r[i], s);
                }
            } else if (block == 1) {
                for (int j = 1; j < 4; ++j)
                    p.processor[j].run(l, r, n, s, p.sr, bpm, beat, playing, midi, midiCount);
            } else if (block == 2) {
                int mode = int(plain(lay.delayMode, s.values[lay.delayMode]));
                if (mode == 0) {
                    p.delaySlot(0, l, r, n, s, bpm, beat, playing);
                    p.delaySlot(1, l, r, n, s, bpm, beat, playing);
                } else {
                    for (int c = 0; c < 2; ++c)
                        std::copy_n(p.work[c].data(), n, p.a[c].data());
                    p.delaySlot(0, l, r, n, s, bpm, beat, playing);
                    p.delaySlot(1, p.a[0].data(), p.a[1].data(), n, s, bpm, beat, playing);
                    float blend = s.values[lay.reverb - 1];
                    for (int i = 0; i < n; ++i) {
                        l[i] = mode == 1 ? .5f * (l[i] + p.a[0][i]) : l[i] * (1 - blend) + p.a[0][i] * blend;
                        r[i] = mode == 1 ? .5f * (r[i] + p.a[1][i]) : r[i] * (1 - blend) + p.a[1][i] * blend;
                    }
                }
            } else if (block == 3) {
                for (int i = 0; i < n; ++i) {
                    auto v = p.reverb.tick(l[i], r[i], s);
                    l[i] = v[0];
                    r[i] = v[1];
                }
            } else {
                for (int i = 0; i < n; ++i) {
                    auto v = p.ir.tick(l[i], r[i], s.values[lay.ir] * (1 - s.values[lay.blockBypass[4]]),
                                       db(plain(lay.ir + 1, s.values[lay.ir + 1])));
                    l[i] = v[0];
                    r[i] = v[1];
                }
            }
            // IR always contributes the declared fixed latency, including bypass.
            if (block != 4) {
                float bypass = s.values[lay.blockBypass[block]];
                for (int i = 0; i < n; ++i) {
                    l[i] = l[i] * (1 - bypass) + p.b[0][i] * bypass;
                    r[i] = r[i] * (1 - bypass) + p.b[1][i] * bypass;
                }
            }
        }
        for (int i = 0; i < n; ++i) {
            float step = float(1 / (p.sr * .01));
            if (p.switching)
                p.graphFade = std::max(0.f, p.graphFade - step);
            else
                p.graphFade = std::min(1.f, p.graphFade + step);
            p.master += (clean(target.values[lay.bypass]) - p.master) * .002f;
            float wet = p.graphFade * (1 - p.master);
            for (int c = 0; c < 2; ++c) {
                float dry = p.masterDry[c][p.dryPos];
                p.masterDry[c][p.dryPos] = p.dry[c][i];
                float x = p.work[c][i] * wet + dry * (1 - wet);
                x = std::isfinite(x) ? std::clamp(x, -16.f, 16.f) : 0;
                (c ? outR : outL)[start + i] = x;
                outPeak = std::max(outPeak, std::abs(x));
            }
            p.dryPos = (p.dryPos + 1) % 128;
        }
        if (p.switching && p.graphFade == 0) {
            p.rank = rank;
            p.order = permutation(rank);
            for (size_t i = 0; i < lay.parameters.size(); ++i)
                if (lay.parameters[i].steps)
                    s.values[i] = clean(target.values[i]);
            p.switching = false;
        }
        beat += n * bpm / (60 * p.sr);
    }
    inputPeak.store(inPeak, std::memory_order_relaxed);
    outputPeak.store(outPeak, std::memory_order_relaxed);
    gainReduction.store(p.analog.gr, std::memory_order_relaxed);
}
} // namespace multifx
