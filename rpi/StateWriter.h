#pragma once
#include "core/Project.h"
#include <atomic>
#include <algorithm>
#include <cstdint>
#include <condition_variable>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <sys/resource.h>
namespace multifx::rpi {
// Only the non-audio control thread submits snapshots. Encoding and SD-card I/O
// must not serialize parameter requests or hold the audio callback up.
class StateWriter {
    std::mutex mutex;
    std::condition_variable changed;
    std::optional<Project> pending;
    bool stopping = false;
    uint64_t revision = 0;
    std::string path;
    std::thread worker;
  public:
    std::atomic<bool> failed{false};
    explicit StateWriter(std::string destination) : path(std::move(destination)), worker([this] {
        setpriority(PRIO_PROCESS, 0, 10);
        for (;;) {
            std::optional<Project> next;
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [this] { return stopping || pending.has_value(); });
                if (!pending && stopping) return;
                // Coalesce knob movements: avoid dozens of SD writes/second.
                // Continuous motion is still checkpointed at most every 750ms;
                // shutdown flushes immediately rather than waiting for debounce.
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(750);
                while(!stopping){
                    const auto observed=revision;
                    const auto quiet=std::min(deadline,std::chrono::steady_clock::now()+std::chrono::milliseconds(150));
                    if(!changed.wait_until(lock,quiet,[&]{return stopping || revision!=observed;}))break;
                    if(std::chrono::steady_clock::now()>=deadline)break;
                }
                next.swap(pending);
            }
            try {
                const auto temporary = path + ".tmp";
                const bool ok = saveProject(temporary, *next) && std::rename(temporary.c_str(), path.c_str()) == 0;
                failed.store(!ok);
            } catch (...) { failed.store(true); }
        }
    }) {}
    void submit(Project snapshot) {
        { std::lock_guard lock(mutex); pending = std::move(snapshot); ++revision; }
        changed.notify_one();
    }
    ~StateWriter() {
        { std::lock_guard lock(mutex); stopping = true; }
        changed.notify_one();
        if (worker.joinable()) worker.join();
    }
};
} // namespace multifx::rpi
