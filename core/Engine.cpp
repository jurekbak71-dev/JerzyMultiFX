#include "Engine.h"
#include "Convolver.h"
#include "DelayRack.h"
#include "effects/Native.h"
#include <stdexcept>
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
#include <xmmintrin.h>
#endif
namespace multifx {
namespace {
struct NoDenormals {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    unsigned previous = _mm_getcsr();
    NoDenormals() {
        _mm_setcsr(previous | 0x8040);
    }
    ~NoDenormals() {
        _mm_setcsr(previous);
    }
#elif defined(__aarch64__)
    uint64_t previous;
    NoDenormals() {
        asm volatile("mrs %0, fpcr" : "=r"(previous));
        uint64_t value = previous | (1ull << 24);
        asm volatile("msr fpcr, %0" ::"r"(value));
    }
    ~NoDenormals() {
        asm volatile("msr fpcr, %0" ::"r"(previous));
    }
#endif
};
} // namespace
namespace {
struct Plan {
    int rank = 0, latency = 0;
    unsigned models = 0;
    bool master = false;
    std::array<bool, 5> block{};
    std::array<bool, 4> processor{};
    std::array<int, 2> delay{};
    bool operator==(const Plan &) const = default;
};
Plan planFor(const State &s) {
    const auto &l = layout();
    Plan p;
    p.rank = int(plain(l.order, s.values[l.order]));
    p.master = s.values[l.bypass] >= .5f;
    for (int i = 0; i < 5; ++i)
        p.block[i] = !p.master && s.values[l.blockBypass[i]] < .5f;
    for (int j = 0; j < 4; ++j)
        p.processor[j] =
            p.block[j ? 1 : 0] && (l.processorBypass[j] < 0 || s.values[l.processorBypass[j]] < .5f);
    p.block[4] = p.block[4] && s.values[l.ir] > .00001f;
    if (p.block[0] && s.values[l.drive + 5] < .5f)
        p.latency += driveLatency;
    if (p.processor[2])
        p.latency += attackLatency;
    if (p.processor[3])
        p.latency += atomicLatency;
    if (p.block[4])
        p.latency += convolutionLatency;
    for (int i = 0; i < 2; ++i)
        p.delay[i] = p.block[2] && s.values[l.slotBypass[i]] < .5f
                         ? int(plain(l.delayType[i], s.values[l.delayType[i]]))
                         : -1;
    auto model = [&](int id) {
        p.models = p.models * 33u +
                   unsigned(std::round(clean(s.values[id]) * std::max(1, l.parameters[id].steps))) + 1;
    };
    if (p.block[0] && s.values[l.drive + 5] < .5f) {
        model(l.drive);
        if (s.values[l.drive] < .25f)
            model(l.fuzzModel);
    }
    if (p.processor[1])
        model(l.processors[1].start + 5);
    if (p.processor[3])
        model(l.processors[3].start + 10);
    if (p.block[3] && s.values[l.reverb + 9] > .00001f)
        model(l.reverb);
    for (int bank = 0; bank < 2; ++bank)
        if (p.delay[bank] == 1 && s.values[l.digital[bank] + 2] > .00001f)
            for (int id : l.mxAlgorithms[bank])
                model(id);
    return p;
}
} // namespace
struct Engine::Impl {
    double sr = 48000;
    DelayRack delays;
    Analog analog;
    Reverb reverb;
    Convolver ir;
    std::array<Digital, 2> digital;
    std::array<std::array<float, 1024>, 2> work{}, dry{}, a{}, saved{};
    State smooth;
    Plan plan;
    std::array<int, 5> order{0, 1, 2, 3, 4};
    std::atomic<int> reportedLatency{0};
    float graphGain = 1, outputGain = 1, limiterGain = 1, limiterRelease = 0;
    bool initialized = false, triggerHeld = false, waiting = false;
    void runProcessor(int index, float *l, float *r, int n, const State &s, double bpm, double beat,
                      bool playing, const JAMidi *midi, unsigned count, const float *retL = nullptr,
                      const float *retR = nullptr) {
        auto *instance = delays.acquire(2 + index);
        if (instance) {
            for (unsigned k = 0; k < instance->api->parameter_count; ++k) {
                const auto &d = instance->api->parameters[k];
                instance->parameters[k] =
                    d.minimum + (d.maximum - d.minimum) * clean(s.values[instance->offset + k]);
            }
            JAProcess process{sizeof(JAProcess),
                              uint32_t(n),
                              sr,
                              bpm,
                              beat,
                              uint32_t(playing),
                              {l, r},
                              {l, r},
                              instance->parameters.data(),
                              midi,
                              count,
                              {retL, retR},
                              s.values.data()};
            instance->api->process(instance->instance, &process);
        } else
            waiting = true;
        delays.release(2 + index);
    }
    std::array<float, 2> slotFade{};
    std::array<bool, 2> slotWasDry{true, true};
    bool reverbWasDry = true;
    void delaySlot(int slot, float *l, float *r, int n, const State &s, double bpm, double beat,
                   bool playing) {
        const auto &lay = layout();
        const int type = plan.delay[slot], base = lay.digital[slot];
        if (type < 0) {
            delays.request(slot, 0);
            slotFade[slot] = 0;
            return;
        }
        const float mix = s.values[base + 2], gain = db(plain(base + 3, s.values[base + 3]));
        delays.request(slot, mix > .00001f ? type : 0);
        if (mix <= .00001f) {
            if (!slotWasDry[slot])
                digital[slot].reset();
            slotWasDry[slot] = true;
            slotFade[slot] = 0;
            for (int i = 0; i < n; ++i) {
                l[i] *= gain;
                r[i] *= gain;
            }
            return;
        }
        slotWasDry[slot] = false;
        for (int c = 0; c < 2; ++c)
            std::copy_n(c ? r : l, n, saved[c].data());
        bool ready = type == 0;
        if (type == 0) {
            digital[slot].configure(s, base, bpm);
            for (int i = 0; i < n; ++i) {
                auto v = digital[slot].tick(l[i], r[i]);
                l[i] = v[0];
                r[i] = v[1];
            }
        } else {
            auto *native = delays.acquire(slot);
            if (native && native->type == type) {
                native->run(l, r, n, s, sr, bpm, beat, playing);
                ready = true;
            }
            delays.release(slot);
        }
        const float step = 1.f / float(sr * .01);
        for (int i = 0; i < n; ++i) {
            slotFade[slot] = ready ? std::min(1.f, slotFade[slot] + step) : 0;
            float wet = mix * slotFade[slot];
            l[i] = (saved[0][i] * (1 - wet) + l[i] * wet) * gain;
            r[i] = (saved[1][i] * (1 - wet) + r[i] * wet) * gain;
        }
    }
};
Engine::Engine() : impl(std::make_unique<Impl>()) {}
Engine::~Engine() = default;
int Engine::latencyFor(const State &s) {
    return planFor(s).latency;
}
int Engine::currentLatency() const {
    return impl->reportedLatency.load();
}
bool Engine::delaysReady() const {
    return impl->delays.ready();
}
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
    p.delays.prepare(rate);
    p.smooth = State{};
    p.plan = Plan{};
    p.order = {0, 1, 2, 3, 4};
    p.graphGain = p.outputGain = p.limiterGain = 1;
    p.limiterRelease = float(1 - std::exp(-1 / (rate * .05)));
    p.initialized = false;
    p.triggerHeld = false;
    p.slotFade = {};
    p.reportedLatency.store(0);
    clips.store(0);
}
bool Engine::prepareState(const State &s) {
    auto &p = *impl;
    const auto desired = planFor(s);
    const auto &l = layout();
    for (int j = 0; j < 4; ++j)
        p.delays.request(2 + j, desired.processor[j] ? 1 : 0);
    for (int bank = 0; bank < 2; ++bank)
        p.delays.request(bank, desired.delay[bank] > 0 && s.values[l.digital[bank] + 2] > .00001f
                                   ? desired.delay[bank]
                                   : 0);
    for (int attempt = 0; attempt < 2500; ++attempt) {
        if (p.delays.ready())
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return false;
}
bool Engine::loadIR(const float *l, const float *r, int frames) {
    return impl->ir.load(l, r, frames);
}
void Engine::process(const float *left, const float *right, float *outL, float *outR, int frames,
                     const State &target, double bpm, double beat, bool playing, const JAMidi *midi,
                     unsigned midiCount, const float *returnL, const float *returnR) noexcept {
    if (!outL || !outR || frames <= 0)
        return;
    NoDenormals noDenormals;
    auto &p = *impl;
    const auto &lay = layout();
    if (!std::isfinite(bpm) || bpm < 20 || bpm > 400)
        bpm = 120;
    float inPeak = 0, outPeak = 0;
    for (int start = 0; start < frames; start += 1024) {
        const int n = std::min(1024, frames - start);
        const Plan desired = planFor(target);
        if (!p.initialized) {
            p.plan = desired;
            p.order = permutation(desired.rank);
            p.smooth = target;
            p.initialized = true;
            p.reportedLatency.store(desired.latency);
        }
        // Fade to silence before a routing/latency change; do not comb-filter dry with delayed wet.
        const bool changing = !(desired == p.plan);
        const float step = 1.f / float(p.sr * .005);
        const float smoothing = float(1 - std::exp(-double(n) / (p.sr * .01)));
        for (size_t i = 0; i < lay.parameters.size(); ++i) {
            float v = clean(target.values[i]);
            if (lay.parameters[i].steps) {
                if (!changing)
                    p.smooth.values[i] = v;
            } else if (int(i) >= lay.midiLearn)
                p.smooth.values[i] = v;
            else
                p.smooth.values[i] += smoothing * (v - p.smooth.values[i]);
        }
        State &s = p.smooth;
        p.waiting = false;
        for (int j = 0; j < 4; ++j)
            p.delays.request(2 + j, p.plan.processor[j] ? 1 : 0);
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c) {
                const auto *input = c ? right : left;
                float x = input ? input[start + i] : (left ? left[start + i] : 0);
                x = std::isfinite(x) ? std::clamp(x, -16.f, 16.f) : 0;
                p.work[c][i] = p.dry[c][i] = x;
                inPeak = std::max(inPeak, std::abs(x));
            }
        if (p.plan.block[0])
            p.analog.configure(s);
        if (p.plan.block[3] && s.values[lay.reverb + 9] > .00001f) {
            p.reverb.configure(s);
            p.reverbWasDry = false;
        } else if (!p.reverbWasDry) {
            p.reverb.reset();
            p.reverbWasDry = true;
        }
        for (int block : p.order) {
            if (!p.plan.block[block]) {
                if (block == 2)
                    for (int k = 0; k < 2; ++k)
                        p.delays.request(k, 0);
                continue;
            }
            auto *l = p.work[0].data(), *r = p.work[1].data();
            if (block == 0) {
                if (s.values[lay.cpdBypass] < .5f)
                    for (int i = 0; i < n; ++i)
                        p.analog.preamp(l[i], r[i], s);
                if (p.plan.processor[0])
                    p.runProcessor(0, l, r, n, s, bpm, beat, playing, nullptr, 0);
                for (int i = 0; i < n; ++i) {
                    p.analog.noiseGate(l[i], r[i]);
                    p.analog.drive(l[i], r[i], s);
                    p.analog.comp(l[i], r[i], s);
                }
            } else if (block == 1) {
                std::array<JAMidi, 128> events{};
                unsigned count = 0;
                for (unsigned k = 0; k < midiCount && count < events.size(); ++k)
                    if (midi[k].frame >= unsigned(start) && midi[k].frame < unsigned(start + n)) {
                        events[count] = midi[k];
                        events[count++].frame -= unsigned(start);
                    }
                const bool held = target.values[lay.attackTrigger] >= .5f;
                if (held && !p.triggerHeld && count < events.size())
                    events[count++] = {0, 3, {144, 60, 100}};
                p.triggerHeld = held;
                for (int j = 1; j < 4; ++j)
                    if (p.plan.processor[j])
                        p.runProcessor(j, l, r, n, s, bpm, beat, playing, events.data(), count,
                                       returnL ? returnL + start : nullptr,
                                       returnR ? returnR + start : nullptr);
            } else if (block == 2) {
                const int mode = int(plain(lay.delayMode, s.values[lay.delayMode]));
                if (mode == 0) {
                    p.delaySlot(0, l, r, n, s, bpm, beat, playing);
                    p.delaySlot(1, l, r, n, s, bpm, beat, playing);
                } else {
                    for (int c = 0; c < 2; ++c)
                        std::copy_n(p.work[c].data(), n, p.a[c].data());
                    p.delaySlot(0, l, r, n, s, bpm, beat, playing);
                    p.delaySlot(1, p.a[0].data(), p.a[1].data(), n, s, bpm, beat, playing);
                    const float blend = s.values[lay.reverb - 1];
                    for (int i = 0; i < n; ++i) {
                        l[i] = mode == 1 ? .5f * (l[i] + p.a[0][i]) : l[i] * (1 - blend) + p.a[0][i] * blend;
                        r[i] = mode == 1 ? .5f * (r[i] + p.a[1][i]) : r[i] * (1 - blend) + p.a[1][i] * blend;
                    }
                }
            } else if (block == 3) {
                if (s.values[lay.reverb + 9] > .00001f)
                    for (int i = 0; i < n; ++i) {
                        auto v = p.reverb.tick(l[i], r[i], s);
                        l[i] = v[0];
                        r[i] = v[1];
                    }
            } else
                for (int i = 0; i < n; ++i) {
                    auto v =
                        p.ir.tick(l[i], r[i], s.values[lay.ir], db(plain(lay.ir + 1, s.values[lay.ir + 1])));
                    l[i] = v[0];
                    r[i] = v[1];
                }
        }
        if (p.waiting)
            p.graphGain = 0;
        const float wantedGain = db(plain(lay.masterGain, target.values[lay.masterGain]));
        unsigned clipped = 0;
        for (int i = 0; i < n; ++i) {
            p.graphGain = p.waiting  ? 0
                          : changing ? std::max(0.f, p.graphGain - step)
                                     : std::min(1.f, p.graphGain + step);
            p.outputGain += (wantedGain - p.outputGain) * .002f;
            float l = p.work[0][i] * p.graphGain * p.outputGain,
                  r = p.work[1][i] * p.graphGain * p.outputGain;
            const float peak = std::max(std::abs(l), std::abs(r));
            if (peak > .98f)
                ++clipped;
            if (target.values[lay.outputLimiter] >= .5f) {
                const float needed = peak > .98f ? .98f / peak : 1.f;
                p.limiterGain = std::min(needed, p.limiterGain + p.limiterRelease * (1 - p.limiterGain));
                l *= p.limiterGain;
                r *= p.limiterGain;
            } else
                p.limiterGain = 1;
            outL[start + i] = std::isfinite(l) ? l : 0;
            outR[start + i] = std::isfinite(r) ? r : 0;
            outPeak = std::max(outPeak, std::max(std::abs(outL[start + i]), std::abs(outR[start + i])));
        }
        if (clipped)
            clips.fetch_add(clipped, std::memory_order_relaxed);
        if (changing && p.graphGain == 0) {
            if (desired.block[3] && !p.plan.block[3])
                p.reverb.reset();
            if (desired.block[4] && !p.plan.block[4])
                p.ir.reset();
            for (int bank = 0; bank < 2; ++bank)
                if (desired.delay[bank] != p.plan.delay[bank])
                    p.digital[bank].reset();
            p.plan = desired;
            p.order = permutation(desired.rank);
            p.reportedLatency.store(desired.latency);
            p.slotFade = {};
            for (size_t id = 0; id < lay.parameters.size(); ++id)
                if (lay.parameters[id].steps)
                    p.smooth.values[id] = clean(target.values[id]);
        }
        beat += n * bpm / (60 * p.sr);
    }
    inputPeak.store(inPeak, std::memory_order_relaxed);
    outputPeak.store(outPeak, std::memory_order_relaxed);
    gainReduction.store(p.plan.block[0] ? p.analog.gr : 0, std::memory_order_relaxed);
}
} // namespace multifx
