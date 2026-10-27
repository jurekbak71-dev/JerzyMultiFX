#include "core/Engine.h"
#include "core/Presentation.h"
#include "core/Project.h"
#include "vendor/atomic/Dsp.h"
#include "vendor/clone/Dsp.h"
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace multifx;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
State isolated(int block) {
    State s;
    for (int i = 0; i < 5; ++i)
        s.values[layout().blockBypass[i]] = i == block ? 0 : 1;
    s.values[layout().outputLimiter] = 0;
    return s;
}
void impulse(State s, int expected, float amplitude = 1) {
    Engine e;
    e.prepare(48000);
    require(e.prepareState(s), "Warm active source modules outside audio");
    float kernel = .25f;
    require(e.loadIR(&kernel, nullptr, 1), "Load non-identity IR");
    std::array<float, 128> input{}, l{}, r{};
    for (int k = 0; k < 40; ++k)
        e.process(input.data(), input.data(), l.data(), r.data(), 128, s);
    float largest = 0;
    int peak = -1;
    for (int offset = 0; offset < 8192; offset += 128) {
        input.fill(0);
        if (offset == 0)
            input[0] = .1f;
        e.process(input.data(), input.data(), l.data(), r.data(), 128, s);
        for (int i = 0; i < 128; ++i)
            if (std::abs(l[i]) > largest) {
                largest = std::abs(l[i]);
                peak = offset + i;
            }
    }
    require(peak == expected, "Actual impulse latency must match active route");
    require(Engine::latencyFor(s) == expected && e.currentLatency() == expected,
            "Reported latency matches impulse");
    if (amplitude > 0)
        require(std::abs(largest - .1f * amplitude) < .0001f, "IR changes actual audio amplitude");
}
int main() try {
    auto &lay = layout();
    {
        Project old;
        std::vector<uint8_t> bytes;
        u32(bytes, 0x31464d4a);
        u32(bytes, 1);
        u32(bytes, 919);
        for (int i = 0; i < 919; ++i) {
            u32(bytes, keyHash(lay.parameters[i].key));
            float v = old.state.values[i];
            if (i == lay.midiMap + 74)
                v = float(lay.preamp + 2) / lay.midiLearn;
            u32(bytes, std::bit_cast<uint32_t>(v));
        }
        u32(bytes, 0);
        u32(bytes, 48000);
        u32(bytes, 0);
        u32(bytes, crc32(bytes.data(), bytes.size()));
        Project migrated;
        require(decode(bytes, migrated), "Read actual version-1 layout");
        require(int(plain(lay.midiMap + 74, migrated.state.values[lay.midiMap + 74])) == lay.preamp + 1,
                "Preserve old CC target after appending parameters");
    }
    State clean;
    impulse(clean, 0);
    State ir = isolated(4);
    ir.values[lay.ir] = 1;
    impulse(ir, 128, .25);
    State spectral = isolated(1);
    spectral.values[lay.processorBypass[2]] = 0;
    spectral.values[parameterId("attack.blend")] = 0;
    impulse(spectral, attackLatency, -1);
    spectral = isolated(1);
    spectral.values[lay.processorBypass[3]] = 0;
    spectral.values[parameterId("atomic.blend")] = 0;
    impulse(spectral, atomicLatency, -1);
    spectral.values[lay.blockBypass[1]] = 1;
    impulse(spectral, 0);
    // Independent original engine renders verify that the adapter and chain preserve the source signal.
    for (int engine : {1, 3}) {
        State s = isolated(1);
        s.values[lay.processorBypass[engine]] = 0;
        Engine integrated;
        integrated.prepare(48000);
        require(integrated.prepareState(s), "Warm original comparison module");
        mf_clone::Engine chorus;
        chorus.reset(48000, mf_clone::defaults());
        atomiccluster::Engine atomic;
        atomic.reset(48000, atomiccluster::defaults());
        mf_clone::Values cp{};
        atomiccluster::Values ap{};
        const auto &g = lay.processors[engine];
        for (unsigned k = 0; k < g.api->parameter_count; ++k) {
            if (engine == 1)
                cp[k] = s.values[g.start + k];
            else
                ap[k] = s.values[g.start + k];
        }
        std::array<float, 128> input{}, l{}, r{};
        double maximum = 0;
        for (int offset = 0; offset < 16384; offset += 128) {
            for (int i = 0; i < 128; ++i)
                input[i] = .08f * std::sin((offset + i) * .057f);
            integrated.process(input.data(), input.data(), l.data(), r.data(), 128, s);
            for (int i = 0; i < 128; ++i) {
                auto reference = engine == 1 ? chorus.process(input[i], input[i], cp, 120, NAN)
                                             : atomic.process(input[i], input[i], ap, 120, NAN);
                maximum = std::max(maximum, std::abs(double(l[i]) - reference[0]));
            }
        }
        require(maximum < .00001, "Integrated Chorus/Atomic differs from original DSP at matching controls");
    }
    // Every block must return the same zero-latency clean signal when bypassed.
    for (int block = 0; block < 5; ++block) {
        State s = isolated(block);
        s.values[lay.blockBypass[block]] = 1;
        s.values[lay.drive + 5] = 0;
        s.values[lay.reverb + 9] = .8f;
        s.values[lay.ir] = 1;
        impulse(s, 0);
    }
    // Limiter must protect physical outputs, while clean signals remain unchanged.
    {
        Engine e;
        e.prepare(48000);
        State s;
        std::array<float, 128> in{}, l{}, r{};
        in.fill(2);
        e.process(in.data(), in.data(), l.data(), r.data(), 128, s);
        for (float x : l)
            require(std::abs(x) <= .98001f, "Output limiter ceiling");
        require(e.clips.load() > 0, "Clips before limiter counted");
    }
    std::cout << "PASS real signal: clean0 / IR128 / Attack2064 / Atomic2048, all block bypasses0; original "
                 "Chorus/Atomic sample comparison; physical-output limiter\n";
    return 0;
} catch (const std::exception &e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}
