#pragma once
#include "Model.h"
#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
namespace multifx {
// Only selected native delays exist. Construction and reclamation never run on audio.
struct DelayInstance {
    const JAModule *api;
    void *instance;
    int type, offset;
    std::array<float, 256> parameters{};
    DelayInstance(Group group, int selected, double rate)
        : api(group.api), instance(api->create(rate, 1024, nullptr)), type(selected), offset(group.start) {
        if (!instance)
            throw std::bad_alloc();
    }
    ~DelayInstance() {
        api->destroy(instance);
    }
    void run(float *l, float *r, int frames, const State &s, double rate, double bpm, double beat,
             bool playing) {
        for (unsigned i = 0; i < api->parameter_count; ++i) {
            const auto &d = api->parameters[i];
            parameters[i] = d.minimum + (d.maximum - d.minimum) * clean(s.values[offset + i]);
        }
        JAProcess b{sizeof(JAProcess),
                    uint32_t(frames),
                    rate,
                    bpm,
                    beat,
                    uint32_t(playing),
                    {l, r},
                    {l, r},
                    parameters.data(),
                    nullptr,
                    0,
                    {nullptr, nullptr},
                    nullptr};
        api->process(instance, &b);
    }
};
class DelayRack {
    struct Slot {
        std::atomic<int> requested{0}, loadedType{0};
        std::atomic<DelayInstance *> published{nullptr}, hazard{nullptr};
    };
    std::array<Slot, 6> slots;
    std::atomic<bool> stopping{false};
    std::thread worker;

  public:
    ~DelayRack() {
        stop();
    }
    void stop() {
        stopping.store(true);
        if (worker.joinable())
            worker.join();
        for (auto &s : slots) {
            delete s.published.exchange(nullptr);
            s.hazard.store(nullptr);
            s.requested.store(0);
            s.loadedType.store(0);
        }
    }
    void prepare(double rate) {
        stop();
        stopping.store(false);
        worker = std::thread([this, rate] {
            while (!stopping.load()) {
                for (int i = 0; i < 6; ++i) {
                    auto &s = slots[i];
                    const int want = s.requested.load();
                    auto *current = s.published.load();
                    if ((current ? current->type : 0) == want)
                        continue;
                    std::unique_ptr<DelayInstance> next;
                    try {
                        if (want > 0)
                            next = std::make_unique<DelayInstance>(i < 2 ? layout().delays[i][want - 1]
                                                                         : layout().processors[i - 2],
                                                                   want, rate);
                    } catch (...) {
                        continue;
                    }
                    if (stopping.load() || want != s.requested.load())
                        continue;
                    auto *old = s.published.exchange(next.release());
                    s.loadedType.store(want);
                    while (old && s.hazard.load() == old && !stopping.load())
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    delete old;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(4));
            }
        });
    }
    void request(int slot, int type) {
        slots[slot].requested.store(type);
    }
    DelayInstance *acquire(int slot) {
        auto &s = slots[slot];
        DelayInstance *p;
        do {
            p = s.published.load();
            s.hazard.store(p);
        } while (p != s.published.load());
        return p;
    }
    void release(int slot) {
        slots[slot].hazard.store(nullptr);
    }
    bool ready() const {
        for (const auto &s : slots) {
            if (s.loadedType.load() != s.requested.load())
                return false;
        }
        return true;
    }
};
} // namespace multifx
