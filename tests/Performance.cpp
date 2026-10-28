#include "core/Engine.h"
#include "core/Presentation.h"
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#endif
using namespace multifx;
long peakMemory() {
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return long(counters.PeakWorkingSetSize / 1024);
#else
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line))
        if (line.starts_with("VmHWM:"))
            return std::stol(line.substr(6));
#endif
    return 0;
}
int main(int argc, char **argv) {
    std::ostream *out = &std::cout;
    std::ofstream file;
    if (argc > 1) {
        file.open(argv[1]);
        out = &file;
    }
    auto &l = layout();
    *out << "{\"sampleRate\":48000,\"buffer\":128,\"profiles\":[";
    const char *names[]{"Clean",          "TS808",      "Muff circuit Supa", "Chorus",         "Attack Decay",
                        "Atomic Cluster", "MX Volante", "Reverb Plate",      "IR 16384 frames", "Tape Warm", "Cloud", "Shimmer", "Memory Delay"};
    for (int profile = 0; profile < 13; ++profile) {
        State s;
        auto set = [&](const char *key, float value) {
            const int id = parameterId(key);
            if (id >= 0)
                s.values[id] = value;
        };
        if (profile == 1) {
            set("drive.type", .5);
            set("drive.bypass", 0);
        }
        if (profile == 2) {
            set("drive.bypass", 0);
            set("drive.type", 0);
            set("fuzz.model", 1);
        }
        if (profile == 3)
            set("clone.bypass", 0);
        if (profile == 4)
            set("attack.bypass", 0);
        if (profile == 5)
            set("atomic.bypass", 0);
        if (profile == 6) {
            set("delayA.type", .25);
            set("delayA.mix", 1);
            set("delayB.bypass", 1);
        }
        if (profile == 7)
            set("reverb.mix", .3);
        if (profile == 8)
            set("ir.mix", 1);
        if(profile==9){set("td.bypass",0);set("td.sat",.65);set("td.wowFlutter",.5);}
        if(profile==10 || profile==11){set("reverb.engine",profile==10?.75f:1.f);set("reverb.mix",.5);set("reverb.pitch",1);}
        if(profile==12){set("delayA.type",.5);set("delayA.mix",1);}
        Engine engine;
        engine.prepare(48000);
        if (!engine.prepareState(s))
            return 1;
        if (profile == 8) {
            std::array<float, 16384> ir{};
            ir[0] = .5;
            ir[100] = .1;
            engine.loadIR(ir.data(), nullptr, int(ir.size()));
        }
        std::array<float, 128> input{}, a{}, b{};
        for (int i = 0; i < 128; ++i)
            input[i] = .1f * std::sin(i * .07f);
        for (int i = 0; i < 50; ++i)
            engine.process(input.data(), input.data(), a.data(), b.data(), 128, s);
        const auto start = std::chrono::steady_clock::now();
        const auto cpu = std::clock();
        constexpr int blocks = 375;
        for (int i = 0; i < blocks; ++i)
            engine.process(input.data(), input.data(), a.data(), b.data(), 128, s);
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double used = double(std::clock() - cpu) / CLOCKS_PER_SEC;
        if (profile)
            *out << ',';
        *out << "{\"name\":\"" << names[profile] << "\",\"audioSeconds\":1,\"wallSeconds\":" << elapsed
             << ",\"processCpuSeconds\":" << used << ",\"latencySamples\":" << engine.currentLatency()
             << ",\"peakProcessRSSKiB\":" << peakMemory() << "}";
    }
    *out << "]}\n";
    return 0;
}
