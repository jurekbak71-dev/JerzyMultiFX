#include "core/Engine.h"
#include "core/Presets.h"
#include "core/Project.h"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <stdexcept>
static std::atomic<bool> tracking{false};
static std::atomic<unsigned long> allocations{0};
void *operator new(std::size_t n) {
    if (tracking.load())
        allocations.fetch_add(1);
    if (void *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n) {
    return ::operator new(n);
}
void operator delete(void *p) noexcept {
    std::free(p);
}
void operator delete[](void *p) noexcept {
    std::free(p);
}
void operator delete(void *p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void *p, std::size_t) noexcept {
    std::free(p);
}
void require(bool v, const char *what) {
    if (!v)
        throw std::runtime_error(what);
}
int main() {
    try {
        auto ir = std::make_unique<multifx::Convolver>();
        float impulse = 1;
        require(ir->load(&impulse, nullptr, 1), "IR load");
        for (int i = 0; i < 400; ++i) {
            auto v = ir->tick(i == 0 ? 1.f : 0, 0, 1, 1);
            require(std::abs(v[0] - (i == 128 ? 1.f : 0)) < .00001, "Convolution latency/identity");
        }
        ir->reset();
        std::array<float, 301> kernel{};
        kernel[0] = .5;
        kernel[150] = .25;
        kernel[300] = -.1;
        require(ir->load(kernel.data(), nullptr, int(kernel.size())), "Partitioned IR load");
        ir->reset();
        for (int i = 0; i < 600; ++i) {
            auto v = ir->tick(i == 0 ? 1.f : 0, 0, 1, 1);
            float expected = i == 128 ? .5f : i == 278 ? .25f : i == 428 ? -.1f : 0;
            require(std::abs(v[0] - expected) < .005, "Partitioned multi-block convolution");
        }
        auto &lay = multifx::layout();
        require(lay.parameters.size() < multifx::maxParameters, "Parameter capacity");
        std::set<uint32_t> hashes;
        for (auto &p : lay.parameters)
            require(hashes.insert(multifx::keyHash(p.key)).second, "Stable parameter key collision");
        multifx::Project project;
        for (size_t i = 0; i < lay.parameters.size(); ++i)
            project.state.values[i] = float(i % 101) / 100;
        project.irL = {1, .5, -.25};
        project.irR = {.25, .5, 1};
        project.irName = "test stereo embedded IR";
        auto bytes = multifx::encode(project);
        multifx::Project restored;
        require(multifx::decode(bytes, restored), "State roundtrip");
        require(restored.state.values == project.state.values && restored.irL == project.irL &&
                    restored.irR == project.irR && restored.irName == project.irName,
                "Exact parameter/MIDI/IR restore");
        bytes[50] ^= 1;
        require(!multifx::decode(bytes, restored), "Corrupt state rejected");
        bytes.resize(12);
        require(!multifx::decode(bytes, restored), "Truncated state rejected");
        std::set<std::array<int, 5>> permutations;
        for (int rank = 0; rank < 120; ++rank)
            permutations.insert(multifx::permutation(rank));
        require(permutations.size() == 120, "All chain orders");
        auto e = std::make_unique<multifx::Engine>();
        multifx::State state;
        std::array<float, 1024> l{}, r{}, a{}, b{};
        unsigned long processed = 0;
        auto begin = std::chrono::steady_clock::now();
        for (int rate : {44100, 48000, 96000}) {
            e->prepare(rate);
            for (int size : {32, 64, 128, 256, 1024})
                for (int block = 0; block < 8; ++block) {
                    for (int i = 0; i < size; ++i) {
                        l[i] = .1f * std::sin(float(i + block * size) * .031f);
                        r[i] = -l[i];
                    }
                    tracking.store(true);
                    e->process(l.data(), r.data(), a.data(), b.data(), size, state);
                    tracking.store(false);
                    for (int i = 0; i < size; ++i)
                        require(std::isfinite(a[i]) && std::isfinite(b[i]), "Finite stereo");
                    processed += size;
                }
        }
        e->prepare(48000);
        multifx::State cleanState;
        multifx::selectFactory(cleanState, 0);
        l.fill(0);
        r.fill(0);
        for (int block = 0; block < 12; ++block)
            e->process(l.data(), r.data(), a.data(), b.data(), 1024, cleanState);
        for (int offset = 0; offset < 8192; offset += 1024) {
            l.fill(0);
            r.fill(0);
            if (offset == 0)
                l[0] = r[0] = .25f;
            e->process(l.data(), r.data(), a.data(), b.data(), 1024, cleanState);
            for (int i = 0; i < 1024; ++i)
                require(std::abs(a[i] - ((offset + i) == multifx::Engine::latency ? .25f : 0)) < .0002,
                        "Actual chain latency matches host PDC");
        }
        e->prepare(48000);
        for (int j = 0; j < 180; ++j) {
            state.values[lay.order] = float(j % 120) / 119;
            state.values[lay.delayType[0]] = float(j % 5) / 4;
            state.values[lay.delayType[1]] = float((j + 2) % 5) / 4;
            state.values[lay.reverb] = float(j % 5) / 4;
            state.values[lay.reverb + 9] = .5;
            for (int c = 1; c < 4; ++c) {
                auto &g = lay.processors[c];
                for (unsigned k = 0; k < g.api->parameter_count; ++k)
                    if (std::string(g.api->parameters[k].id) == "bypass")
                        state.values[g.start + k] = 0;
            }
            for (int i = 0; i < 256; ++i) {
                l[i] = .08f * std::sin(float(j * 256 + i) * .04f);
                r[i] = l[i] * .6f;
            }
            tracking.store(true);
            e->process(l.data(), r.data(), a.data(), b.data(), 256, state, 120, j * 256 * 120 / (60. * 48000),
                       true);
            tracking.store(false);
            for (int i = 0; i < 256; ++i)
                require(std::isfinite(a[i]) && std::abs(a[i]) <= 16 && std::isfinite(b[i]),
                        "Routing/engine change stability");
            processed += 256;
        }
        state.values[lay.bypass] = 1;
        for (int block = 0; block < 40; ++block) {
            for (int i = 0; i < 1024; ++i)
                l[i] = r[i] = .125f;
            tracking.store(true);
            e->process(l.data(), r.data(), a.data(), b.data(), 1024, state);
            tracking.store(false);
        }
        require(std::abs(a.back() - .125f) < .0001, "Bypass preserves audio");
        require(allocations.load() == 0, "Zero allocations on audio thread");
        std::cout << "PASS: partitioned convolution, IR latency128 / chain4240, " << lay.parameters.size()
                  << " stable parameters, complete MIDI/IR state+CRC, 120 orders, mono-compatible stereo "
                     "44.1/48/96kHz, blocks32..1024, active engine/routing switches, bypass, zero RT "
                     "allocations. Processed "
                  << processed << " samples in "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - begin).count()
                  << " s (includes preparation; not an RPi benchmark).\n";
        return 0;
    } catch (const std::exception &e) {
        tracking.store(false);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
