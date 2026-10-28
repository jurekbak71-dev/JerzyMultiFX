#pragma once
#include "core/Project.h"
#include <atomic>
#include <condition_variable>
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
        { std::lock_guard lock(mutex); pending = std::move(snapshot); }
        changed.notify_one();
    }
    ~StateWriter() {
        { std::lock_guard lock(mutex); stopping = true; }
        changed.notify_one();
        if (worker.joinable()) worker.join();
    }
};
} // namespace multifx::rpi
